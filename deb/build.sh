#!/bin/sh
set -eu

deb_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
source_dir=$(dirname -- "$deb_dir")
build_dir=$(mktemp -d)
trap 'rm -rf -- "$build_dir"' EXIT
trap 'exit 1' HUP INT TERM

mkdir "$build_dir/kio-adb"
cp -- "$source_dir/CMakeLists.txt" "$source_dir/LICENSE" "$source_dir/README.md" \
    "$source_dir/adb.desktop" "$source_dir/adb.json" "$source_dir/adbworker.cpp" "$build_dir/kio-adb/"
cp -R -- "$source_dir/tests" "$deb_dir/debian" "$build_dir/kio-adb/"

(
    cd "$build_dir/kio-adb"
    dpkg-buildpackage -b -us -uc
)

mkdir -p -- "$deb_dir/dist"
for artifact in "$build_dir"/*.deb "$build_dir"/*.buildinfo "$build_dir"/*.changes; do
    cp -- "$artifact" "$deb_dir/dist/"
done
