#!/bin/sh
# Builds a Release and packs the game into dist/TheGreatTrek-<version>-<os>.zip:
# the program, data/, assets/, LICENSE, README.md and CREDITS.md.
# Usage: tools/package.sh [build_dir]
set -e
cd "$(dirname "$0")/.."
BUILD="${1:-build-release}"
VERSION=$(sed -n 's/.*TREK_VERSION_STRING "\(.*\)".*/\1/p' src/core/version.h)
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) OS=windows; EXE=TheGreatTrek.exe ;;
    Darwin) OS=macos; EXE=TheGreatTrek ;;
    *) OS=linux; EXE=TheGreatTrek ;;
esac
cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD"
ctest --test-dir "$BUILD" --output-on-failure
NAME="TheGreatTrek-$VERSION-$OS"
STAGE="dist/$NAME"
rm -rf "$STAGE" "dist/$NAME.zip"
mkdir -p "$STAGE"
cp "$BUILD/bin/$EXE" "$STAGE/"
cp -r data assets "$STAGE/"
cp LICENSE README.md CREDITS.md "$STAGE/"
if [ "$OS" = linux ]; then
    cp packaging/linux/thegreattrek.desktop "$STAGE/"
fi
(cd dist && cmake -E tar cf "$NAME.zip" --format=zip "$NAME")
echo "dist/$NAME.zip"
