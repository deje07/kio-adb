#!/bin/sh
set -eu

if ! command -v makepkg >/dev/null 2>&1; then
    echo 'makepkg is required; run this script on an Arch-based system.' >&2
    exit 1
fi

arch_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
source_dir=$(dirname -- "$arch_dir")
build_dir=$(mktemp -d)
trap 'rm -rf -- "$build_dir"' EXIT
trap 'exit 1' HUP INT TERM

pkgver=$(sed -n 's/^pkgver=//p' "$arch_dir/PKGBUILD")
mkdir "$build_dir/kio-adb"
cp -- "$source_dir/CMakeLists.txt" "$source_dir/LICENSE" "$source_dir/README.md" \
    "$source_dir/adb.desktop" "$source_dir/adb.json" "$source_dir/adbworker.cpp" "$build_dir/kio-adb/"
cp -R -- "$source_dir/tests" "$build_dir/kio-adb/"
tar -czf "$build_dir/kio-adb-$pkgver.tar.gz" -C "$build_dir" kio-adb
cp -- "$arch_dir/PKGBUILD" "$build_dir/PKGBUILD"

mkdir -p -- "$arch_dir/dist"
(
    cd "$build_dir"
    # Replace SKIP with the checksum of this checkout's source snapshot.
    checksum=$(sha256sum "kio-adb-$pkgver.tar.gz")
    checksum=${checksum%% *}
    sed -i "s/sha256sums=('SKIP')/sha256sums=('$checksum')/" PKGBUILD
    PKGDEST="$arch_dir/dist" makepkg --syncdeps "$@"
)
