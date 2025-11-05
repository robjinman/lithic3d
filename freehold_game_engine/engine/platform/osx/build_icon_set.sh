#!/bin/bash

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <path_to_source_image> <destination>"
  exit 1
fi

SOURCE_IMAGE=$1
DESTINATION_DIR=$2
ICONSET_DIR="app.iconset"
ICNS_FILE="${DESTINATION_DIR}/app.icns"

if [ ! -f "$SOURCE_IMAGE" ]; then
  echo "Error: Source image '$SOURCE_IMAGE' not found!"
  exit 1
fi

mkdir -p $ICONSET_DIR

ICON_SIZES=(16 32 64 128 256 512 1024)

for SIZE in "${ICON_SIZES[@]}"; do
  OUTPUT_FILE="$ICONSET_DIR/icon_${SIZE}x${SIZE}.png"
  echo "Generating $OUTPUT_FILE..."
  convert "$SOURCE_IMAGE" -resize ${SIZE}x${SIZE} "$OUTPUT_FILE"

  if [ "$SIZE" -lt 1024 ]; then
    OUTPUT_FILE_2X="$ICONSET_DIR/icon_${SIZE}x${SIZE}@2x.png"
    echo "Generating $OUTPUT_FILE_2X..."
    convert "$SOURCE_IMAGE" -resize $((SIZE * 2))x$((SIZE * 2)) "$OUTPUT_FILE_2X"
  fi

done

iconutil -c icns $ICONSET_DIR -o $ICNS_FILE

if [ $? -eq 0 ]; then
  echo "Successfully created $ICNS_FILE"
else
  echo "Failed to create .icns file"
  exit 1
fi

echo "Done."
