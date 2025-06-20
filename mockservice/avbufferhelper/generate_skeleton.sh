#!/bin/bash

HEADER_FILE="$1"
OUTPUT_CPP="$2"

if [[ ! -f "$HEADER_FILE" ]]; then
  echo "Header file not found!"
  exit 1
fi

CLASS_NAME=$(grep -Po 'class\s+\K\w+' "$HEADER_FILE" | head -1)
SKELETON_CLASS="${CLASS_NAME}Skeleton"
HEADER_BASENAME=$(basename "$HEADER_FILE")

echo "#include \"$HEADER_BASENAME\"" > "$OUTPUT_CPP"
echo "#include <iostream>" >> "$OUTPUT_CPP"
echo "" >> "$OUTPUT_CPP"
echo "class $SKELETON_CLASS : public $CLASS_NAME {" >> "$OUTPUT_CPP"
echo "public:" >> "$OUTPUT_CPP"

# Process each pure virtual method line
grep -E 'virtual .*=\s*0\s*;' "$HEADER_FILE" | while read -r line; do
    # Clean line from comments and spaces
    clean_line=$(echo "$line" | sed -E 's/\/\/.*//' | tr -d '\r')

    # Remove "virtual" and "= 0;"
    sig=$(echo "$clean_line" | sed -E 's/virtual\s+//' | sed -E 's/= *0 *; *//')

    # Extract function name
    func_name=$(echo "$sig" | grep -oP '\w+(?=\s*\()')

    # Extract return type (everything before function name)
    ret_type=$(echo "$sig" | sed -E "s/\s*$func_name\s*\(.*//" | xargs)

    # Extract argument list
    args=$(echo "$sig" | grep -oP '\(.*\)')

    # Set default return value
    case "$ret_type" in
        bool) ret_stmt="return false;" ;;
        void) ret_stmt="" ;;
        int|uint32_t|size_t) ret_stmt="return 0;" ;;
        float|double) ret_stmt="return 0.0;" ;;
        *\*) ret_stmt="return nullptr;" ;;
        *) ret_stmt="return {};" ;;
    esac

    echo "    $ret_type $func_name$args override {" >> "$OUTPUT_CPP"
    echo "        std::cout << \"$func_name called\" << std::endl;" >> "$OUTPUT_CPP"
    [[ -n "$ret_stmt" ]] && echo "        $ret_stmt" >> "$OUTPUT_CPP"
    echo "    }" >> "$OUTPUT_CPP"
    echo "" >> "$OUTPUT_CPP"
done

echo "};" >> "$OUTPUT_CPP"
echo "" >> "$OUTPUT_CPP"

# Handle free functions like: IAVBufferHelper* getAVBufferHelperInstance();
grep -E "^\s*[^/].*\*\s*get.*\(\)\s*;" "$HEADER_FILE" | grep -v 'virtual' | while read -r line; do
    ret_type=$(echo "$line" | sed -E 's/\*.*//g' | xargs)
    func_name=$(echo "$line" | grep -oP '\w+(?=\s*\()')
    echo "$ret_type* $func_name() {" >> "$OUTPUT_CPP"
    echo "    return new $SKELETON_CLASS();" >> "$OUTPUT_CPP"
    echo "}" >> "$OUTPUT_CPP"
    echo "" >> "$OUTPUT_CPP"
done