"""Turns the public-domain hymn and folk settings in tools/music/*.abc into data/music.json.

The sources are Open Hymnal Project files (public domain; see CREDITS.md). Only the small part
of ABC notation they use is read: voices, notes with accidentals and octave marks, lengths,
rests, ties and bar lines; words, decorations, slurs and layout are skipped. Each voice becomes
a list of [midi note, length in beats] (note 0 is a rest). The game plays the result in a
tracker style (src/ui/music.c).
Usage: python tools/abc_to_music.py
"""
import json
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "tools", "music")

# id, file, voices (melody first), beats per minute, what it is, where it plays
TUNES = [
    ("old_100th", "old_100th.abc", ["S1V1", "S1V2", "S2V1", "S2V2"], 84,
     "Old Hundredth: the Genevan Psalter tune of 1551 (Psalm 134 in the Dutch psalm books, Psalm 100 in the English)",
     "Melody Genevan Psalter c. 1551, attr. Louis Bourgeois; setting Sternhold and Hopkins' Psalter, 1561. "
     "Open Hymnal Project, public domain. Cape congregations sang the Genevan tunes from Datheen's psalm book and "
     "the Statenberyming, and the trekkers carried their psalm books north (NWU repository, hymnody study)."),
    ("tallis_canon", "tallis_canon.abc", ["S1V1", "S1V2", "S2V1", "S2V2"], 76,
     "Tallis' Canon: Thomas Tallis, c. 1567, sung to the evening hymn 'Glory to thee, my God, this night'",
     "Thomas Tallis c. 1567; setting from 'A Hymnal' (Episcopal), 1916. Open Hymnal Project, public domain."),
    ("st_anne", "st_anne.abc", ["S1V1", "S1V2", "S2V1", "S2V2"], 66,
     "St Anne: William Croft, 1708, the tune of 'O God, our help in ages past'",
     "William Croft 1708; setting from 'The Lutheran Hymnary', 1913. Open Hymnal Project, public domain."),
    ("o_waly_waly", "o_waly_waly.abc", ["S2V1", "S2V2", "S3V1", "S3V2"], 100,
     "O Waly Waly ('The Water is Wide'): a traditional English and Scots folk tune",
     "Traditional; setting by Tobin Strong, released to the public domain in 2014. Open Hymnal Project."),
]

# where each tune plays (the game picks one by what is on screen)
SCENES = {"title": "old_100th", "church": "old_100th", "ending": "old_100th", "night": "tallis_canon",
          "grave": "st_anne", "place": "o_waly_waly"}

KEYS = {"C": {}, "G": {"F": 1}, "D": {"F": 1, "C": 1}, "F": {"B": -1}, "Bb": {"B": -1, "E": -1}}
BASE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NOTE = re.compile(r"(\^\^|\^|__|_|=)?([A-Ga-gzx])([,']*)(\d*)(/*)(\d*)")


def length(num, slashes, den):
    n = int(num) if num else 1
    if not slashes:
        return float(n)
    d = int(den) if den else 2 ** len(slashes)
    return n / d


def parse(path, voices):
    text = open(path, encoding="latin-1").read()
    key = "C"
    lines = {v: [] for v in voices}
    for raw in text.splitlines():
        line = raw.split("%")[0].rstrip() if not raw.startswith("%%") else ""
        m = re.match(r"K:\s*(\S+)", line)
        if m:
            key = m.group(1)
        m = re.match(r"\[V:\s*(\S+)\]\s*(.*)", line)
        if m and m.group(1) in lines:
            lines[m.group(1)].append(m.group(2))
    keysig = KEYS[key]
    out = []
    for v in voices:
        body = " ".join(lines[v])
        body = re.sub(r"\[[A-Z]:[^\]]*\]", " ", body)   # inline fields such as [Q:1/4=120]
        body = re.sub(r"![^!]*!", " ", body)             # decorations
        body = re.sub(r'"[^"]*"', " ", body)             # chord names and annotations
        notes = []
        bar_acc = {}
        tie = False
        i = 0
        while i < len(body):
            c = body[i]
            if c == "|":
                bar_acc = {}
                i += 1
                continue
            if c == "-":
                tie = True
                i += 1
                continue
            m = NOTE.match(body, i)
            if not m:
                i += 1  # spaces, slurs, ']' of an end bar
                continue
            acc, letter, octs, num, slashes, den = m.groups()
            dur = length(num, slashes, den)
            if letter in "zx":
                notes.append([0, dur])
            else:
                up = letter.upper()
                semis = BASE[up] + (12 if letter.islower() else 0) + 60
                semis += 12 * octs.count("'") - 12 * octs.count(",")
                octave_key = (up, octs, letter.islower())
                if acc:
                    shift = {"^": 1, "^^": 2, "_": -1, "__": -2, "=": 0}[acc]
                    bar_acc[octave_key] = shift
                semis += bar_acc.get(octave_key, keysig.get(up, 0))
                if tie and notes and notes[-1][0] == semis:
                    notes[-1][1] += dur
                else:
                    notes.append([semis, dur])
            tie = False
            i = m.end()
        out.append(notes)
    totals = [sum(d for _, d in n) for n in out]
    assert max(totals) - min(totals) < 1e-6, (path, totals)
    return out


def main():
    tunes = []
    for tid, fname, voices, bpm, title, source in TUNES:
        parts = parse(os.path.join(SRC, fname), voices)
        tunes.append({"id": tid, "title": title, "source": source, "bpm": bpm, "voices": parts})
        print(f"{tid}: {len(parts)} voices, {sum(d for _, d in parts[0]):g} beats")
    doc = {
        "_about": "Music: public-domain psalm, hymn and folk tunes (sources in each tune), played by the game in a "
                  "retro tracker style. Made from tools/music/*.abc by tools/abc_to_music.py; edit those, not this. "
                  "Each voice is [midi note, beats]; note 0 is a rest. The melody is the first voice.",
        "scenes": SCENES,
        "tunes": tunes,
    }
    with open(os.path.join(ROOT, "data", "music.json"), "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps(doc, ensure_ascii=False, separators=(",", ":"))
                .replace('{"id"', '\n    {"id"').replace('"tunes":[', '\n  "tunes":[') + "\n")


if __name__ == "__main__":
    main()
