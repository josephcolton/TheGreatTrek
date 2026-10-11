# CLAUDE.md

Guidance for Claude Code when working in this repository.

## What this is

**The Great Trek / Die Groot Trek** is an Oregon-Trail-style survival game about the 1830s Great Trek in South Africa, written in **C17 + SDL3**. It is a personal project meant for **family to play**, released on GitHub under **GPL-3.0** (`LICENSE`).

Read these before designing or writing content:
- `docs/planning/03_development_plan.md`: decisions, the two sides, route, systems, architecture, milestones. **This is the source of truth.**
- `docs/planning/02_research_resources.md`: sourced historical facts with links.
- `docs/planning/01_gemini_review.md`: early Gemini design ideas worth keeping, and the factual errors in them.
- `docs/planning/04_game_design.md`: how the game works: screens, dialog, map, timing, stores and barter, events format, hunting, saves, core API.

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
src/main.c  SDL init + main loop.
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

Status: **the build is not set up yet.** It is milestone 1 in the plan. The intended setup:
- CMake ≥ 3.24 with `FetchContent` for SDL3, SDL3_ttf (vendored FreeType) and SDL3_image, pinned to release tags.
- An option to use system SDL3 packages instead.

Intended commands once it exists:
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
```

### This machine (Windows 11), checked 2026-10-11
- **First on PATH:** `gcc` 13.2 (MinGW-w64 UCRT), `cmake` 3.29 and `ninja`, all from **Strawberry Perl** (`C:\Strawberry\c\bin`).
- MSYS2 is installed at `C:\msys64` (ucrt64 has gcc and make, but not cmake or SDL).
- Visual Studio 2022 Community is installed.
- SDL is **not** installed system-wide. FetchContent avoids needing it.
- **Open question for the user:** which toolchain to standardise on.
  - The Strawberry gcc works, but it is an unusual choice.
  - MSYS2 UCRT64 can use `pacman -S mingw-w64-ucrt-x86_64-{cmake,ninja,sdl3,sdl3-ttf,sdl3-image}`.
  - MSVC works through VS 2022.
- The repo lives in Google Drive (`My Drive`). Keep `build/` out of git, and consider building outside Drive if sync slows it down.
- Shells: both Git Bash and PowerShell are available. Paths contain spaces, so quote them.

## Git

- Main branch: `main`. Remote: `origin` (GitHub).
- **Claude may commit and push to `main` without asking.** Commit at sensible checkpoints, with clear messages.
- **Work only on `main`.** Do not create side branches, feature branches or worktrees.
- Never force-push or rewrite published history.
- Make sure the build and tests pass before pushing, once a build exists.
- Add `build/` and editor folders to `.gitignore` when the build is created. The current file is GitHub's C template.
