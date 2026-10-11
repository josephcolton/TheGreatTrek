# The Great Trek / Die Groot Trek

A survival game in the spirit of *The Oregon Trail*, set on the Great Trek in South Africa in 1837. Choose to play a Dutch (Voortrekker) or English (settler/trader) family. Outfit your wagon in Grahamstown and lead your family north across the Orange and Vaal rivers to the Highveld.

Written in C with SDL3. Licensed under the GNU GPL v3 (see `LICENSE`).

**Status:** early development. The build skeleton works; the game loop is next. See `docs/planning/` for the design and the historical research.

## Building

You need CMake 3.24 or newer, a C17 compiler (GCC, Clang or MSVC) and Ninja (or another CMake generator). SDL3, SDL3_ttf and SDL3_image are downloaded and built automatically.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
./build/bin/TheGreatTrek
```

Options: `-DTREK_USE_SYSTEM_SDL=ON` to use SDL3 packages already installed on your system; `-DTREK_BUILD_GAME=OFF` to build only the simulation core, tools and tests.
