#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <unistd.h>
#include <string>

using namespace std;

int N, M, x, y;
int num_units;
int completed_operations = 0;

auto start_time = chrono::high_resolution_clock::now();

const int NUM_STATIONS = 4;
pthread_mutex_t station_locks[NUM_STATIONS];
pthread_cond_t station_conds[NUM_STATIONS];
bool station_available[NUM_STATIONS];
int station_waiting_count[NUM_STATIONS]; 

// Unit coordination
pthread_mutex_t *unit_locks;
sem_t *unit_semaphores;
int *unit_completion_count;

// Logbook synchronization 
sem_t logbook_access;
sem_t reader_access;
sem_t reader_priority;
pthread_mutex_t logbook_lock;
pthread_mutex_t reader_count_lock;
pthread_mutex_t waiting_readers_lock;
int active_readers = 0;
int waiting_readers = 0;


FILE *output_file;
pthread_mutex_t output_lock;

void init_semaphores() {
    for (int i = 0; i < NUM_STATIONS; i++) {
        pthread_mutex_init(&station_locks[i], 0);
        pthread_cond_init(&station_conds[i], 0);
        station_available[i] = true;
        station_waiting_count[i] = 0;
    }

    num_units = N / M;
    unit_locks = new pthread_mutex_t[num_units];
    unit_semaphores = new sem_t[num_units];
    unit_completion_count = new int[num_units];

    for (int i = 0; i < num_units; i++) {
        pthread_mutex_init(&unit_locks[i], 0);
        sem_init(&unit_semaphores[i], 0, 0);
        unit_completion_count[i] = 0;
    }

    sem_init(&logbook_access, 0, 1);
    sem_init(&reader_access, 0, 1);
    sem_init(&reader_priority, 0, 1);
    pthread_mutex_init(&logbook_lock, 0);
    pthread_mutex_init(&reader_count_lock, 0);
    pthread_mutex_init(&waiting_readers_lock, 0);

    pthread_mutex_init(&output_lock, 0);

    start_time = chrono::high_resolution_clock::now();
}

long long get_time() {
    auto current_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(current_time - start_time);
    return (int)(duration.count() / 100);
}

int get_random_number() {
    static random_device rd;
    static mt19937 generator(rd());
    double lambda = 1000.0;
    poisson_distribution<int> poissonDist(lambda);
    return poissonDist(generator);
}

void print_message(const char* message) {
    pthread_mutex_lock(&output_lock);
    fprintf(output_file, "%s\n", message);
    fflush(output_file);
    pthread_mutex_unlock(&output_lock);
}

int get_station_id(int operative_id) {
    return (operative_id - 1) % NUM_STATIONS;
}

int get_unit_id(int operative_id) {
    return (operative_id - 1) / M;
}

bool is_leader(int operative_id) {
    return (operative_id % M == 0);
}

void wait_for_station(int operative_id, int station_id) {
    pthread_mutex_lock(&station_locks[station_id]);

    if (!station_available[station_id]) {
        char message[200];
        sprintf(message, "Operative %d is waiting for station TS%d at time %lld",
                operative_id, station_id + 1, get_time());
        print_message(message);
    }

    station_waiting_count[station_id]++;
    while (!station_available[station_id]) {
        pthread_cond_wait(&station_conds[station_id], &station_locks[station_id]);
    }
    station_waiting_count[station_id]--;
    station_available[station_id] = false;

    pthread_mutex_unlock(&station_locks[station_id]);
}

void release_station_access(int operative_id, int station_id) {
    pthread_mutex_lock(&station_locks[station_id]);
    station_available[station_id] = true;
    pthread_cond_broadcast(&station_conds[station_id]);
    pthread_mutex_unlock(&station_locks[station_id]);
}

struct Operative {
    int id;
    int unit_id;
    int station_id;
};

void logbook_entry(int unit_id) {
    sem_wait(&reader_priority);
    pthread_mutex_lock(&waiting_readers_lock);
    if (waiting_readers > 0) {
        pthread_mutex_unlock(&waiting_readers_lock);
        sem_post(&reader_priority);
        usleep(100000);
        sem_wait(&reader_priority);
        pthread_mutex_lock(&waiting_readers_lock);
    }
    pthread_mutex_unlock(&waiting_readers_lock);
    sem_wait(&logbook_access);
    pthread_mutex_lock(&logbook_lock);
    sleep(y);
    completed_operations++;
    char message[200];
    sprintf(message, "Unit %d has completed intelligence distribution at time %lld",
            unit_id + 1, get_time());
    print_message(message);
    pthread_mutex_unlock(&logbook_lock);
    sem_post(&logbook_access);
    sem_post(&reader_priority);
}

void* operative_thread(void* arg) {
    Operative* op = (Operative*)arg;
    int arrival_delay = (get_random_number() % 20) + 1;
    usleep(arrival_delay * 1000);
    char message[200];
    int display_station = op->station_id + 1;
    sprintf(message, "Operative %d has arrived at typewriting station TS%d at time %lld",
            op->id, display_station, get_time());
    print_message(message);
    wait_for_station(op->id, op->station_id);
    sprintf(message, "Operative %d has started document recreation at station TS%d at time %lld",
            op->id, display_station, get_time());
    print_message(message);
    sleep(x);
    sprintf(message, "Operative %d has completed document recreation at time %lld",
            op->id, get_time());
    print_message(message);
    release_station_access(op->id, op->station_id);
    pthread_mutex_lock(&unit_locks[op->unit_id]);
    unit_completion_count[op->unit_id]++;
    if (unit_completion_count[op->unit_id] == M) {
        sprintf(message, "Unit %d has completed document recreation phase at time %lld",
                op->unit_id + 1, get_time());
        print_message(message);

        // Notify 
        sem_post(&unit_semaphores[op->unit_id]);
    }
    pthread_mutex_unlock(&unit_locks[op->unit_id]);
    // Leader unit completion wait kore and performs intelligence distribution
    if (is_leader(op->id)) {
        sem_wait(&unit_semaphores[op->unit_id]);
        logbook_entry(op->unit_id);
    }
    return NULL;
}

void* staff_thread(void* arg) {
    int staff_id = *(int*)arg;
    while (completed_operations < num_units) {
        int read_interval = (get_random_number() % 5) + 1;
        sleep(read_interval);
        pthread_mutex_lock(&waiting_readers_lock);
        waiting_readers++;
        pthread_mutex_unlock(&waiting_readers_lock);
        sem_wait(&reader_priority);
        sem_wait(&reader_access);
        pthread_mutex_lock(&reader_count_lock);
        active_readers++;
        if (active_readers == 1) {
            sem_wait(&logbook_access);
        }
        pthread_mutex_unlock(&reader_count_lock);
        pthread_mutex_lock(&waiting_readers_lock);
        waiting_readers--;
        pthread_mutex_unlock(&waiting_readers_lock);
        sem_post(&reader_access);
        sem_post(&reader_priority);
        pthread_mutex_lock(&reader_count_lock);
        int current_ops = completed_operations;
        pthread_mutex_unlock(&reader_count_lock);
        char message[200];
        sprintf(message, "Intelligence Staff %d began reviewing logbook at time %lld. Operations completed = %d",
                staff_id, get_time(), current_ops);
        print_message(message);
        usleep(500000);
        sem_wait(&reader_access);
        pthread_mutex_lock(&reader_count_lock);
        active_readers--;
        if (active_readers == 0) {
            sem_post(&logbook_access);
        }
        pthread_mutex_unlock(&reader_count_lock);
        sem_post(&reader_access);
        if (current_ops >= num_units) break;
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }
    FILE* input_file = fopen(argv[1], "r");
    if (!input_file) {
        printf("Error: Cannot open input file\n");
        return 1;
    }
    fscanf(input_file, "%d %d %d %d", &N, &M, &x, &y);
    fclose(input_file);
    output_file = fopen(argv[2], "w");
    if (!output_file) {
        printf("Error: Cannot open output file\n");
        return 1;
    }
    init_semaphores();
    Operative* operatives = new Operative[N];
    for (int i = 0; i < N; i++) {
        operatives[i].id = i + 1;
        operatives[i].unit_id = get_unit_id(i + 1);
        operatives[i].station_id = get_station_id(i + 1);
    }
    pthread_t* operative_threads = new pthread_t[N];
    pthread_t staff_threads[2];

    int staff_ids[2] = {1, 2};
    pthread_create(&staff_threads[0], NULL, staff_thread, &staff_ids[0]);
    pthread_create(&staff_threads[1], NULL, staff_thread, &staff_ids[1]);

    for (int i = 0; i < N; i++) {
        pthread_create(&operative_threads[i], NULL, operative_thread, &operatives[i]);
        usleep(100000);  
    }
    for (int i = 0; i < N; i++) pthread_join(operative_threads[i], NULL);
    pthread_join(staff_threads[0], NULL);
    pthread_join(staff_threads[1], NULL);
    delete[] operatives;
    delete[] operative_threads;
    delete[] unit_locks;
    delete[] unit_semaphores;
    delete[] unit_completion_count;
    fclose(output_file);
    return 0;
}
