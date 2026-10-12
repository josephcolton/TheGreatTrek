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

# Windows: also the installer, if Inno Setup is installed (winget install JRSoftware.InnoSetup).
if [ "$OS" = windows ]; then
    ISCC="${ISCC:-}"
    for c in "$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe" "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" \
             "/c/Program Files/Inno Setup 6/ISCC.exe"; do
        if [ -z "$ISCC" ] && [ -f "$c" ]; then ISCC="$c"; fi
    done
    if [ -n "$ISCC" ]; then
        "$ISCC" -Q "-DAppVersion=$VERSION" "-DSourceDir=$(cygpath -w "$PWD/$STAGE")" "-DOutputDir=$(cygpath -w "$PWD/dist")" \
            "$(cygpath -w "$PWD/packaging/windows/TheGreatTrek.iss")"
        echo "dist/$NAME-setup.exe"
    else
        echo "Inno Setup not found; no installer made (set ISCC=path/to/ISCC.exe)"
    fi
fi
