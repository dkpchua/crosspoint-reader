#!/bin/bash

set -e

cd "$(dirname "$0")"

READER_FONT_STYLES=("Regular" "Italic" "Bold" "BoldItalic")
NOTOSERIF_FONT_SIZES=(12 14 16 18)
NOTOSANS_FONT_SIZES=(12 14 16 18)

for size in ${NOTOSERIF_FONT_SIZES[@]}; do
  for style in ${READER_FONT_STYLES[@]}; do
    font_name="notoserif_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/NotoSerif/NotoSerif-${style}.ttf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path --2bit --compress --pnum > $output_path
    echo "Generated $output_path"
  done
done

for size in ${NOTOSANS_FONT_SIZES[@]}; do
  for style in ${READER_FONT_STYLES[@]}; do
    font_name="notosans_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/NotoSans/NotoSans-${style}.ttf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path --2bit --compress --pnum > $output_path
    echo "Generated $output_path"
  done
done

# Small UI chrome (button devices, uiScale 1.0). Rendered 1-bit: crisp at these
# sizes and half the flash of 2-bit.
UI_FONT_SIZES=(10 12)
# Larger UI chrome substituted in on touch/high-density boards via the uiScale
# remap (see src/main.cpp setupFonts). Rendered 2-bit so it stays smooth when
# enlarged. Touch boards use uiScale 1.2, so UI_12 -> 14.4 -> 14 is the size the
# remap actually lands on; add larger sizes here if a board adopts a higher scale.
UI_FONT_SIZES_LARGE=(14)
UI_FONT_STYLES=("Regular" "Bold")

# Ubuntu lacks the Latin Extended Additional block (U+1EA0-U+1EF9) used for
# Vietnamese tone marks. Append a Vietnamese-only Ubuntu cut so those glyphs are
# filled from it while every glyph Ubuntu already has stays unchanged (fontstack
# is ordered by descending priority). NotoSansHebrew fills U+05D0-U+05EA so the
# Hebrew UI translation renders in menus and settings.
generate_ui_font() {
  local size="$1" style="$2" extra_flags="$3"
  local font_name="ubuntu_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
  local font_path="../builtinFonts/source/Ubuntu/Ubuntu-${style}.ttf"
  local hebrew_path="../builtinFonts/source/NotoSansHebrew/NotoSansHebrew-${style}.ttf"
  local viet_path="../builtinFonts/source/Ubuntu/Ubuntu-Vietnamese-${style}.ttf"
  local output_path="../builtinFonts/${font_name}.h"
  python fontconvert.py $font_name $size $font_path $hebrew_path $viet_path \
    --additional-intervals 0x05D0,0x05EA $extra_flags > $output_path
  echo "Generated $output_path"
}

for size in ${UI_FONT_SIZES[@]}; do
  for style in ${UI_FONT_STYLES[@]}; do
    generate_ui_font $size $style ""
  done
done

for size in ${UI_FONT_SIZES_LARGE[@]}; do
  for style in ${UI_FONT_STYLES[@]}; do
    generate_ui_font $size $style "--2bit --compress"
  done
done

python fontconvert.py notosans_8_regular 8 \
  ../builtinFonts/source/NotoSans/NotoSans-Regular.ttf \
  ../builtinFonts/source/NotoSansHebrew/NotoSansHebrew-Regular.ttf \
  --additional-intervals 0x05D0,0x05EA > ../builtinFonts/notosans_8_regular.h

echo ""
echo "Running compression verification..."
python verify_compression.py ../builtinFonts/
