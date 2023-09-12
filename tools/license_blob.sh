#!/bin/bash
# (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

#!/bin/bash

# Input and output file paths
input_file="../LICENSE"
output_file="license_blob.h"

# Check if the input file exists
if [ ! -f "$input_file" ]; then
  echo "Error: $input_file not found."
  exit 1
fi

# Convert the file to a C byte array with newlines
echo "static const char license_blob[] = {" > "$output_file"
hexdump -v -e '1/1 "  0x%02x,\n"' "$input_file" >> "$output_file"
echo "0x00};" >> "$output_file"

echo "Conversion completed. The C byte array is stored in $output_file."

