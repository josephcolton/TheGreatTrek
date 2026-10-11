# CLAUDE.md

Guidance for Claude Code when working in this repository.

## What this is

**The Great Trek / Die Groot Trek** is an Oregon-Trail-style survival game about the 1830s Great Trek in South Africa, written in **C17 + SDL3**. It is a personal project meant for **family to play**, released on GitHub under **GPL-3.0** (`LICENSE`).

Read these before designing or writing content:
- `docs/planning/03_development_plan.md`: decisions, the two sides, route, systems, architecture, milestones. **This is the source of truth.**
- `docs/planning/02_research_resources.md`: sourced historical facts with links.
- `docs/planning/01_gemini_review.md`: early Gemini design ideas worth keeping, and the factual errors in them.
- `docs/planning/04_game_design.md`: how the game works: screens, dialog, map, timing, stores and barter, events format, hunting, saves, core API.
- `docs/planning/05_research_pass2.md`: distances, 1837 calendar, period prices and wages, checked facts, family origin stories, name lists, glossary.

## Settled decisions (don't re-open without the user)

- **One trek north.**
  - Default start: Grahamstown, April 1837. Opening scene: the English settlers' Bible gift to the Uys party.
  - Route: Cradock → Colesberg → Orange River drift → Thaba Nchu → Sand/Vet River → Vaal drift → ending at the Mooi River (Potchefstroom area).
  - Natal/Drakensberg is a possible later fork at Thaba Nchu.
- **"Dutch or English" is the side you play, not the interface language.** The side changes:
  - wagons and items on offer
  - starting money and livestock
  - prices (`base × side_modifier[seller_group] × distance × relationship`)
  - welcome at trekker laagers, missionaries and others
  - some events
- **Everyone starts at the same place.** Each playable family has a **backstory** explaining how they got there; backstories replace Gemini's "classes".
- **All playable families and named characters are fictional** (e.g. the English settler family is the fictional *Ashworth* family). Do not base characters on, or name, real private families. Historical public figures (Retief, Uys, Potgieter, Moroka, Mzilikazi, Harris, etc.) may appear as themselves.
- **The interface is in English**, with period Dutch words (laager, smous, voorloper, kakebeenwa, drift, sanna).
- All player-facing text lives in locale files, so a Dutch or Afrikaans translation can be added later.
- **Family-friendly:** danger and death are present, but violence is told in text and kept non-graphic.
- **Educational realism:** odds are not shown (a "Show odds" setting exists, off by default) but should match history where sources allow. Every rate and price carries a source or is marked invented.
- **Money:** whole rixdollars (Dutch side) or pounds and shillings (English side), priced as close to 1837 as sources allow. Historical events move prices, and sellers cite them.
- **Saves are permadeath:** one save per trip, written after every step, listed newest first; no going back to an earlier point. Difficulty (Easy/Normal/Hard) is chosen on the main menu before a trip.
- **Graves:** only the current party's graves are shown, never graves from other trips.
- **English side earns the group laager** through events; it is never offered at the start.

## Content rules

- **Every event has a `sources` field.** Cite something from `02_research_resources.md` or a new source, or mark it `"invented"`. Don't present invented numbers or people as history. When a fact is uncertain, say so in the docs rather than guessing.
- **People, not obstacles.** African polities and people are named actors with their own motives, never generic "tribe" encounters. This includes the Barolong (Moroka), Griqua, Basotho (Moshoeshoe), Ndebele (Mzilikazi), and Khoikhoi and other servants/herders travelling with the families. Servants are named party members with skills (voorloper, herder, interpreter).
- **Known Gemini errors to avoid:**
  - "baviansboud": use *sanna*/*roer*
  - inconsistent currency: from 1825, 1 rixdollar = 1s 6d, so £1 ≈ 13⅓ Rds
  - Kuruman as a stop on the route
  - "70–75% success rate"
  - a Griqua guide in Zululand
  - vertical-cliff wagon dismantling

## Architecture (planned; see plan §6)

```
src/core/   pure C simulation — MUST NOT include SDL headers. Deterministic given a seed.
src/ui/     SDL3 front end: screen state machine, text rendering/wrapping, input.
src/ui/main.c SDL init + main loop.
tools/      trekbot (headless balance runs), datacheck (validate JSON + locale keys) — link core only.
tests/      CTest unit tests for core.
third_party/ vendored cJSON (MIT).
data/       events/*.json, items/wagons/route/families/prices.json, locale/en.json
assets/     fonts (SIL OFL only), images, sounds — every asset listed in CREDITS.md.
```

Conventions:
- C17, warnings on (`-Wall -Wextra` / `/W4`). No compiler-specific extensions in `core`.
- Use the project RNG (PCG32 or similar, passed explicitly). **Never `rand()`** and never hidden global RNG state. Runs must reproduce from a seed.
- No player-facing string literals in C. Look text up by locale key.
- Game numbers (prices, probabilities, yields) go in `data/`, not hard-coded.
- Saves are JSON in `SDL_GetPrefPath("TheGreatTrek", "TheGreatTrek")`.
- Rendering: low-resolution logical canvas (e.g. 640×360) via `SDL_SetRenderLogicalPresentation`. Mostly text screens; choices with keys 1–4 or the mouse.

## Build

Milestones 1 (build skeleton) and 3 (core loop) are done; the game is playable from title screen to ending. CMake ≥ 3.24 fetches SDL3 3.4.16, SDL3_ttf 3.2.2 (vendored FreeType) and SDL3_image 3.4.6 with `FetchContent`, all as static libraries, so the game is one exe that needs no extra DLLs. `-DTREK_USE_SYSTEM_SDL=ON` uses installed SDL3 packages instead; `-DTREK_BUILD_GAME=OFF` builds only the core, tools and tests (no SDL).

Programs, `data/` and `assets/` all land in `<build>/bin/`.

```sh
B="$LOCALAPPDATA/TheGreatTrek-build/debug"      # outside Google Drive
cmake -S . -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "$B"
ctest --test-dir "$B" --output-on-failure
"$B/bin/datacheck.exe" "$B/bin/data"
"$B/bin/trekbot.exe" --runs 200 --data data         # balance stats per family (also --policy, --difficulty)
"$B/bin/TheGreatTrek.exe" --pref "$TEMP/p" --seed 7 --keys "2 1 1 enter up enter" --screenshot shot.png
```

Testing the front end without a person: `--keys` replays key presses (1-9, a-z, enter, esc, arrows, `text:Word`, `wait:N`), `--seed` fixes the new trip's seed, `--pref` keeps saves out of the real user folder, and `--screenshot` saves a PNG you can look at. `trekbot --dump FILE --stop-at grave|river|store|route|ending` writes a save at that moment; copy it into `<pref>/trips/` to open it in the game.

### This machine (Windows 11), checked 2026-10-11
- **Toolchain in use:** `gcc` 13.2 (MinGW-w64 UCRT), `cmake` 3.29 and `ninja` from **Strawberry Perl** (`C:/Strawberry/c/bin`), first on PATH. Chosen because it works with no setup; the CMake files are portable to MSVC and other platforms.
- Also installed: MSYS2 at `C:/msys64`, Visual Studio 2022 Community **without the C++ workload** (no `cl.exe`), so MSVC builds cannot be tested here until "Desktop development with C++" is added. SDL is not installed system-wide (FetchContent fetches it).
- **Build outside Google Drive**, in `%LOCALAPPDATA%\TheGreatTrek-build\`, so sync doesn't slow the build.
- Shells: both Git Bash and PowerShell are available. Paths contain spaces, so quote them.
- Backslash escapes written through Bash heredocs or Python strings can get collapsed (`'\\'` became `'\'`); prefer the Write/Edit tools for source files.
- `gh` (GitHub CLI) is not installed.

## Git

- Main branch: `main`. Remote: `origin` (GitHub).
- **Claude may commit and push to `main` without asking.** Commit at sensible checkpoints, with clear messages.
- **Work only on `main`.** Do not create side branches, feature branches or worktrees.
- Never force-push or rewrite published history.
- Make sure the build and tests pass before pushing, once a build exists.
