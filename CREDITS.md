# Credits

## Code libraries
| Library | Licence | Use |
|---|---|---|
| [SDL3](https://github.com/libsdl-org/SDL) 3.4.16 | zlib | window, input, rendering, audio |
| [SDL3_ttf](https://github.com/libsdl-org/SDL_ttf) 3.2.2 (with vendored FreeType) | zlib (FreeType: FTL) | text rendering |
| [SDL3_image](https://github.com/libsdl-org/SDL_image) 3.4.6 | zlib | PNG loading and screenshots |
| [cJSON](https://github.com/DaveGamble/cJSON) 1.7.19 | MIT (`third_party/cjson/LICENSE`) | JSON data and saves |

SDL libraries are fetched by CMake at build time and are not stored in this repository.

## Fonts
| File | Author | Licence |
|---|---|---|
| `assets/fonts/VT323-Regular.ttf` | The VT323 Project Authors (Peter Hull) | SIL Open Font License 1.1 (`assets/fonts/VT323-OFL.txt`) |

## Images and sounds
| File | Author | Licence |
|---|---|---|
| `assets/sprites/wagons.txt` (wagons, oxen, the voorloper) | drawn for this game (Claude, for the project) | GPL-3.0, like the code |
| `assets/sprites/scenery.txt` (houses, churches, huts, trees, the camp) | drawn for this game (Claude, for the project) | GPL-3.0, like the code |
| `assets/sprites/animals.txt` (the game animals, and the hyena and wild dog for the night guard) | drawn for this game (Claude, for the project) | GPL-3.0, like the code |
| `assets/sprites/portraits.txt` (portraits of the families, their servants, Moroka and an old trekker) | drawn for this game (Claude, for the project); Moroka's is an impression, not a likeness | GPL-3.0, like the code |
| `assets/sprites/towns.txt` (Karoo houses, a Karoo church, willows, Griqua houses, mat houses, a kraal) | drawn for this game (Claude, for the project) | GPL-3.0, like the code |

The sprites are text files (one character per pixel), so they can be edited in any text editor. Skies, hills, rivers and the map are drawn in code (`src/ui/scene.c`, `src/ui/screen_overlay.c`), and the sound effects are synthesised in `src/ui/sound.c`. Every image or sound file added later must be listed here with its author, source and licence.

## Historical sources
See `docs/planning/02_research_resources.md` and `docs/planning/05_research_pass2.md`.
