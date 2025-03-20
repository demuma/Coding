#!/usr/bin/env bash
#
# Usage: ./rename_cpp_hpp.sh [target_directory]
#
# If no directory is specified, the script will look in the current directory (.)

TARGET_DIR="${1:-.}"

# Use 'find' to locate files ending with .cpp or .hpp, 
# then rename them by replacing the extension with .txt.
find "$TARGET_DIR" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.yaml'  \) -print0 | \
while IFS= read -r -d '' FILE; do
  # Construct the new name by stripping the old extension and adding .txt
  NEW_FILE="${FILE%.*}.txt"
  echo "Renaming '$FILE' to '$NEW_FILE'"
  mv "$FILE" "$NEW_FILE"
done

