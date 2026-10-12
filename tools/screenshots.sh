#!/bin/sh
# Retakes the README screenshots in docs/screenshots/. trekbot plays a trip to a chosen moment and
# saves it; the game opens that save in a throwaway --pref folder and takes the picture.
# Usage: tools/screenshots.sh [build_dir]   (a built game; Python with Pillow shrinks the PNGs)
set -e
cd "$(dirname "$0")/.."
BIN="${1:-build}/bin"
OUT=docs/screenshots
TMP="${TMPDIR:-${TEMP:-/tmp}}/trek-screenshots"
rm -rf "$TMP"
mkdir -p "$OUT" "$TMP"

# shot NAME FAMILY SEED STOP KEYS   (STOP '-': no save, the title or new-trip screens)
shot() {
    pref="$TMP/$1"
    mkdir -p "$pref/trips"
    if [ "$4" != "-" ]; then
        "$BIN/trekbot" --runs 1 --family "$2" --seed "$3" --data "$BIN/data" \
            --dump "$pref/trips/trip-20370101-120000-0001.json" --stop-at "$4" >/dev/null
    fi
    "$BIN/TheGreatTrek" --pref "$pref" --seed "$3" --keys "$5" --screenshot "$OUT/$1.png"
    python -c "import sys; from PIL import Image; Image.open(sys.argv[1]).convert('RGB').save(sys.argv[1], optimize=True)" \
        "$OUT/$1.png" 2>/dev/null || true
    echo "$OUT/$1.png"
}

shot title   settler   7  -                'wait:30'
shot family  settler   7  -                '2 2 2 wait:20'
shot cradock settler   5  landmark:cradock '1 enter wait:20'
shot store   settler   5  store            '1 enter wait:20'
shot road    mission   12 speaker          '1 enter wait:20'
shot river   trader    4  river            '1 enter wait:20'
shot hunt    jagter    8  hunt             '1 enter wait:90'
shot night   smid      9  night            '1 enter wait:20'
shot map     hunter    2  landmark:thaba_nchu '1 enter wait:10 m wait:10'
