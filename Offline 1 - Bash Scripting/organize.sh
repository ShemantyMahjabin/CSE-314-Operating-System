#!/bin/bash
if [ "$#" -lt 4 ]; then
    echo "Usage: $0 <submissions_folder> <TARGET_FOLDER> <test_folder> <answer_folder> [optional flags]"
    exit 1
fi

SUBMISSIONS_FOLDER=$1
TARGET_FOLDER=$2
TEST_FOLDER=$3
ANSWER_FOLDER=$4

VERBOSE=false
EXECUTE=true
INCLUDE_LC=true
INCLUDE_CC=true
INCLUDE_FC=true

shift 4

for arg in "$@"; do
    case "$arg" in
        -v) VERBOSE=true ;;
        -noexecute) EXECUTE=false ;;
        -nolc) INCLUDE_LC=false ;;
        -nocc) INCLUDE_CC=false ;;
        -nofc) INCLUDE_FC=false ;;
    esac
done

mkdir -p "$TARGET_FOLDER/C"
mkdir -p "$TARGET_FOLDER/C++"
mkdir -p "$TARGET_FOLDER/Python"
mkdir -p "$TARGET_FOLDER/Java"

declare -A FILE_MAP
for f in "$SUBMISSIONS_FOLDER"/*.zip
do
    file=$(basename -- "$f")
    filename="$file"
    student_id="${file%.zip}"        
    student_id="${student_id: -7}"
    FILE_MAP["$student_id"]="$filename"

    
    if [ -z "$student_id" ]; then
        echo "No student ID found in $file"
        continue
    fi

    tempname="tempo_$student_id"
    mkdir "$tempname"
    unzip -q "$f" -d "$tempname"

    
    source_file=$(find "$tempname"\
        -type f \( \
        -name "*.c" \
        -o -name "*.cpp" \
        -o -name "*.py" \
        -o -name "*.java" \
        \) | head -n 1)

    if [ -z "$source_file" ]; then
        $VERBOSE && echo "No source file for $student_id"
        rm -rf "$tempname"
        continue
    fi

    ext="${source_file##*.}"

    

    case "$ext" in 
        c)
            mkdir -p "$TARGET_FOLDER/C/$student_id"
            cp "$source_file" "$TARGET_FOLDER/C/$student_id/main.c"
            ;;
        cpp)
            mkdir -p "$TARGET_FOLDER/C++/$student_id"
            cp "$source_file" "$TARGET_FOLDER/C++/$student_id/main.cpp"
            ;;
        py)
            mkdir -p "$TARGET_FOLDER/Python/$student_id"
            cp "$source_file" "$TARGET_FOLDER/Python/$student_id/main.py"
            ;;
        java)
            mkdir -p "$TARGET_FOLDER/Java/$student_id"
            cp "$source_file" "$TARGET_FOLDER/Java/$student_id/Main.java"
            ;;
        *)
            $VERBOSE && echo "Unknown file type for $student_id, skipping."
            ;;
    esac
     rm -rf "$tempname"

done



RESULT_CSV="$TARGET_FOLDER/result.csv"
echo -n "student_id,student_name,language" > "$RESULT_CSV"
$EXECUTE && echo -n ",matched,not_matched" >> "$RESULT_CSV"
$INCLUDE_LC && echo -n ",line_count" >> "$RESULT_CSV"
$INCLUDE_CC && echo -n ",comment_count" >> "$RESULT_CSV"
$INCLUDE_FC && echo -n ",function_count" >> "$RESULT_CSV"
echo "" >> "$RESULT_CSV"


count_lines() {
    wc -l < "$1"
}

count_comments() {
    comment_count=0
    case "$1" in 
        *.c)
            comment_count=$(grep -E '//|/\*' "$1" | wc -l);;
        *.cpp)
            comment_count=$(grep -E '//|/\*' "$1" | wc -l);;
        *.py)
            comment_count=$(grep -E '#' "$1" | wc -l);;
        *.java)
            comment_count=$(grep -E '//|/\*' "$1" | wc -l);;
        *);;

    esac
    echo $comment_count

}


count_functions() {
    function_count=0
    case "$1" in 
        *.c)
            function_count=$(grep -cE '^\s*(static\s+|extern\s+|inline\s+|const\s+|volatile\s+|restrict\s+)*((unsigned|signed|short|long)\s+)*((int|void|float|double|char|bool|[A-Za-z_][A-Za-z0-9_]*|struct\s+[A-Za-z_][A-Za-z0-9_]*|union\s+[A-Za-z_][A-Za-z0-9_]*|enum\s+[A-Za-z_][A-Za-z0-9_]*))\s+\**[A-Za-z_][A-Za-z0-9_]*\s*\(' "$1");;
        *.cpp)
            function_count=$(grep -cE '^\s*(static\s+|extern\s+|inline\s+|const\s+|volatile\s+|restrict\s+)*((unsigned|signed|short|long)\s+)*((int|void|float|double|char|bool|[A-Za-z_][A-Za-z0-9_]*|struct\s+[A-Za-z_][A-Za-z0-9_]*|union\s+[A-Za-z_][A-Za-z0-9_]*|enum\s+[A-Za-z_][A-Za-z0-9_]*))\s+\**[A-Za-z_][A-Za-z0-9_]*\s*\(' "$1");;
        *.py)
            function_count=$(grep -cE '^\s*def\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\(' "$1");;
        *.java)
            function_count=$(grep -cE '^\s*(public|private|protected)?\s*(static\s+)?(final\s+|abstract\s+|synchronized\s+|native\s+|strictfp\s+)?(int|void|float|double|char|boolean|String|[A-Za-z_][A-Za-z0-9_<>,\s]*?)\s+[A-Za-z_][A-Za-z0-9_]*\s*\(' "$1");;
        *);;

    esac
    echo $function_count

}

execute_match(){
    local studentdir=$1
    local language=$2
    local student_id=$3
    local matched=0
    local not_matched=0

    for test in "$TEST_FOLDER"/test*.txt
    do
        file=$(basename -- "$test")              
        testcasenum="${file%.txt}" 
        #testcasenum="${testcasenum: -1}"                 
        testcasenum="${testcasenum//[!0-9]/}"
        outputfile="$studentdir/out${testcasenum}.txt"
        answerfile="$ANSWER_FOLDER/ans${testcasenum}.txt"

        case "$language" in
            C)
                if [[ -f "$studentdir/main.c" ]]
                then
                    gcc "$studentdir/main.c" -o "$studentdir/main.out"
                    "$studentdir/main.out" < "$test" > "$outputfile"
                else
                    echo "C source file missing for $student_id"
                fi;;
            C++)
                if [[ -f "$studentdir/main.cpp" ]]
                then
                    g++ "$studentdir/main.cpp" -o "$studentdir/main.out"
                    "$studentdir/main.out" < "$test" > "$outputfile"
                else
                    echo "C++ source file missing for $student_id"
                fi;;
            Java)
                if [[ -f "$studentdir/Main.java" ]]
                then
                    javac "$studentdir/Main.java"
                    java -cp "$studentdir" Main < "$test" > "$outputfile"
                else
                    echo "Java source file missing for $student_id"
                fi;;
            Python)
                if [[ -f "$studentdir/main.py" ]]
                then
                    python3 "$studentdir/main.py" < "$test" > "$outputfile"
                else
                    echo "Python source file missing for $student_id"
                fi;;
            *);;
        esac


            if diff -q "$outputfile" "$answerfile" > /dev/null
            then
                ((matched++))
            else 
                ((not_matched++))
            fi
    done
     echo "$matched $not_matched"
}



for typedir in "$TARGET_FOLDER"/*
do
    for studentdir in "$typedir"/*
    do
        if [[ -f "$studentdir/main.c" ]]
        then
            main="$studentdir/main.c"
            extension="C"
        elif [[ -f "$studentdir/main.cpp" ]]
        then    
            main="$studentdir/main.cpp"
            extension="C++"
        elif [[ -f "$studentdir/main.py" ]]
        then 
            main="$studentdir/main.py"
            extension="Python"
        elif [[ -f "$studentdir/Main.java" ]]
        then 
            main="$studentdir/Main.java"
            extension="Java"
        else
            $VERBOSE && echo "No code file found for student in $student_dir"
            continue
        fi


        student_id=$(basename "$studentdir")
        filename="${FILE_MAP[$student_id]}"
        namepart="${filename%%_submission_*}"
        fullname="${namepart%_*}"
        fullname="${fullname//_/ }"


        line_count=0
        comment_count=0
        function_count=0
        matched=0
        not_matched=0

        $INCLUDE_LC && line_count=$(count_lines "$main")
        $INCLUDE_CC && comment_count=$(count_comments "$main")
        $INCLUDE_FC && function_count=$(count_functions "$main")

        if $EXECUTE; then
            read matched not_matched < <(execute_match "$studentdir" "$extension" "$student_id")
        fi

        row="$student_id,$fullname,$extension"
        $EXECUTE && row+=",${matched:-0},${not_matched:-0}"
        $INCLUDE_LC && row+=",$line_count"
        $INCLUDE_CC && row+=",$comment_count"
        $INCLUDE_FC && row+=",$function_count"

        echo "$row" >> "$RESULT_CSV"
    done
done



