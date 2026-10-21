#! /bin/bash

set -e

if which inkscape &>/dev/null; then
    renderer=inkscape
elif which rsvg-convert &>/dev/null; then
    renderer=rsvg
else
    echo "Error: this tool requires inkscape or rsvg-convert" >&2
    exit 1
fi

cd $(dirname "$0")

in_file=$(readlink -f $(dirname "$0"))/appimage-controller-icon.svg

# create directory structure
mkdir -p hicolor/scalable/apps
if [ "$renderer" = inkscape ]; then
    inkscape --export-plain-svg=hicolor/scalable/apps/AppImageController.svg "$in_file"
else
    cp "$in_file" hicolor/scalable/apps/AppImageController.svg
fi

for res in 16 32 64 128 160 192 256 384 512; do
    fullres="${res}x${res}"
    echo "Generating PNG for resolution $fullres"

    mkdir -p hicolor/"$fullres"/apps

    if [ "$renderer" = inkscape ]; then
        inkscape -w "$res" -h "$res" -e hicolor/"$fullres"/apps/AppImageController.png "$in_file"
    else
        rsvg-convert -w "$res" -h "$res" "$in_file" > hicolor/"$fullres"/apps/AppImageController.png
    fi
done
