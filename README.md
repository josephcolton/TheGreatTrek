# The Great Trek / Die Groot Trek

A survival game in the spirit of *The Oregon Trail*, set on the Great Trek in South Africa in 1837. Choose to play a Dutch (Voortrekker) or English (settler/trader) family. Outfit your wagon in Grahamstown and lead your family north across the Orange and Vaal rivers to the Highveld.

Written in C with SDL3. Licensed under the GNU GPL v3 (see `LICENSE`).

**Status:** playable from start to finish, still being tested and balanced. See `docs/planning/` for the design and the historical research.

## Playing

Pick a side and one of seven fictional families, read how they came to be in Grahamstown, name your party, and set out in April 1837. On the road you choose the pace, the rations and how to keep watch at night; you buy supplies in the colonial towns, barter beyond the Orange River, hunt, cross rivers, and decide what to do when things go wrong. The trip saves itself after every step, and there is one save per trip, so what happens on the road stays happened.

| Key | What it does |
|---|---|
| 1-9 or the mouse | choose |
| Enter | confirm or continue |
| Esc | the menu (save and quit) |
| M / J / G | map / journal / glossary of period words |
| H | the history behind an event |
| P / S | party / supplies (in an event) |
| F11 | full screen |

Every event says where its history comes from; press H to read it. The families are invented; the places, prices and much of what happens are taken from period sources such as Captain Harris (1836-37) and James Backhouse (1838-39).

## Building

You need CMake 3.24 or newer, a C17 compiler (GCC, Clang or MSVC) and Ninja (or another CMake generator). SDL3, SDL3_ttf and SDL3_image are downloaded and built automatically.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
./build/bin/TheGreatTrek
```

Options: `-DTREK_USE_SYSTEM_SDL=ON` to use SDL3 packages already installed on your system; `-DTREK_BUILD_GAME=OFF` to build only the simulation core, tools and tests.

`tools/package.sh` builds a Release and packs a zip with the game, its data, the licence and the credits into `dist/`.

Tools: `datacheck` checks the data files and text, and `trekbot` plays hundreds of games to check balance (`trekbot --runs 200`).
