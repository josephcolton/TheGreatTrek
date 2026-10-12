# Development Plan: Die Groot Trek / The Great Trek

Status: **decisions made (2026-10-10; platform changed to C + SDL3 on 2026-10-11). Next: build skeleton.**
Builds on `01_gemini_review.md` and `02_research_resources.md`.

---

## 1. Decisions

| Question | Decision |
|---|---|
| Whose story? | **One trek north.** Everyone starts at the same place. Each playable family has a backstory explaining how they got there. |
| Dutch or English? | **The side you play.** It changes the wagons on offer, starting money and goods, prices, how people react to you, and some events. (The interface is in English. A Dutch/Afrikaans translation can come later; see §6.) |
| Audience | Personal project and **family play**. |
| Distribution | **Public GitHub repo, GPL-3.0 licence** (repo: `TheGreatTrek`). |
| Platform (decided 2026-10-11) | **Compiled game in C with SDL3**, not a web game. Windows first; keep it portable to Linux and macOS. |
| Characters | All playable families are **fictional** (see §4). Historical public figures appear as themselves. |

### Still open (minor; defaults chosen)
- **Start year and place.** Default: **Grahamstown, April 1837.** That is when the Uys party camped outside town and received the Dutch Bible from the English settlers. It is a natural opening scene where both sides meet. The alternative is Graaff-Reinet in 1836, which is more Dutch and lets Vegkop happen live.
- **End point.** Default: **the Mooi River (Potchefstroom area)**, founded 1838–39.
- **More roads (decided 2026-10-11):**
  - **Natal**, forking at the Vet River. Goal: the Bloukrans/Bushman's River camps or Port Natal.
  - **Far North**, forking at the Vaal drift. Goal: the Soutpansberg or Delagoa Bay.
  - The trip ends on arrival, followed by a historical epilogue.
  - See `06_research_routes.md`.
- **Dutch spellings.** Default: period Dutch words in English text (laager, smous, voorloper, kakebeenwa, drift).

---

## 2. Design pillars

1. **Oregon Trail's loop, South African substance.** Travel, consume supplies, face events, decide, then arrive or die. The mechanics come from documented realities: oxen, laager, drifts, fever and nagana, barter, flintlocks.
2. **Your side matters.** Dutch and English players see different prices, wagons, welcomes and dangers on the same road.
3. **Every event has a source,** or is clearly labeled as invented.
4. **People, not obstacles.** The Barolong, Griqua, Basotho and Ndebele, Khoikhoi servants, missionaries and British officials are named actors with their own reasons.
5. **Honest, not graphic.** The audience is adults who remember Oregon Trail.
   - Death and disease are present and honest.
   - Violence is told in the text and off-screen, not graphically.
   - A full run takes 30–60 minutes.
6. **Text kept separate from code** from the start, so a translation is easy later.

---

## 3. The route north (draft, to be checked on a map)

```
GRAHAMSTOWN (start, Apr 1837)  — outfitting, Uys Bible scene, backstories
   │  Eastern Cape frontier districts
CRADOCK / Great Fish River     — last colonial town, last cheap supplies
   │  Karoo: dry, water scarce
COLESBERG area                 — edge of the Colony
   │
ORANGE RIVER DRIFT             — river crossing set-piece (flood season!)
   │  Transorangia: Griqua, Basotho, Barolong lands
PHILIPPOLIS (Griqua) [optional stop]
   │
THABA NCHU                     — Moroka's Barolong & Wesleyan mission; trekker gathering;
   │                             news of Vegkop; council vote; [future fork → Drakensberg/Natal]
SAND / VET RIVER (Winburg area)
   │  Highveld: open grassland, winter frost, huge game herds, lions
VAAL RIVER DRIFT               — second big crossing; entering Ndebele-claimed land
   │
MOOI RIVER (end)               — settle; epilogue
```
- Length: about 540 mi in a straight line, ~650–700 mi on the ground (`05_research_pass2.md` §2). The Uys party took ~87 days from Grahamstown to the Sand River in 1837, so a full trip is ~100–110 in-game days.
- **Historical tension of 1837:** Mzilikazi's Ndebele held the land north of the Vaal until trekker commandos drove them north in Jan and Nov 1837 (Mosega, then the Marico campaign). A 1837 start puts the player in that tense period. Events can show it through news, refugees, envoys and the decision to wait or press on, without making the family fight battles.

---

## 4. The two sides

### Shared start
Both sides outfit in Grahamstown. The opening scene is the April 1837 gift of the Dutch Bible from the English settlers to the Uys party. It is played from your side's point of view.

### Side comparison (first-pass numbers, to be balanced later)
| | **Dutch (Voortrekker family)** | **English (settler / trader family)** |
|---|---|---|
| Why they go | Grievances against British rule, frontier-war losses, land, independence | Trade with the trekkers and the interior, hunting, cheap land, a fresh start after frontier-war losses |
| Starting wealth | Little cash (rixdollars), **lots of livestock** (cattle, sheep, horses) | **More cash** (sterling) and merchant credit, few animals |
| Wagons offered | Kakebeenwa (light, sturdy, standard); a second family wagon cheaper through kin | Kakebeenwa at a markup; **heavy transport wagon** (more cargo, needs more oxen, slower); Scotch cart (light two-wheeler, cheap, fragile). *Built: a wagonmaker at Grahamstown trades wagons for English families; the 1837 prices of the transport wagon and cart are estimates.* |
| Guns | Sanna/roer (flintlock); family members can shoot | Same, or a pricier English-made gun. *Percussion guns were in use by English hunters in the 1830s (Steedman's "copper caps", Harris's "percussion gun"); built as an English-side item that cuts misfires.* |
| Trekker laagers | Welcome. Can join a group laager (safer nights), vote in council, trade at fair prices | Tolerated or suspected ("Are you the Governor's man?"). Higher prices; joining a laager isn't guaranteed |
| Grahamstown / colonial merchants | Normal prices | **Cheaper.** Credit available |
| Missionaries (Wesleyan, LMS) | Neutral to cool | Warmer (English-speaking). Medicine and news easier to get |
| Griqua / Barolong / Basotho | Relations depend on choices; land competition | Relations depend on choices; existing trade ties make barter slightly easier |
| Smous | Normal | Normal (often English-speaking themselves) |
| Language events | Understands trekker council and Dutch-speaking servants | Needs an interpreter in some Dutch-only scenes; some events are locked or harder |
| Unique events | Council elections, leader quarrels (Potgieter/Maritz/Retief), news of Vegkop | Debt collectors, British officials asking for news, demand for trade goods, distrust tests |

How it works in code: every price is calculated as `base_price × side_modifier[seller_group] × distance_modifier × relationship_modifier`. Wagon and item catalogues are filtered by side.

### Backstories (pick a family; this replaces Gemini's "classes")
Each family is a set of perks and penalties from the Gemini classes, plus a short intro story.

**Dutch side**
1. **The Frontier Farmer** (*Grensboer*). His farm was raided in the 1834–35 frontier war (the Sixth Frontier War), the cattle are gone, and no compensation came when the conquered land was returned. Perks: good with livestock; Penalty: little cash.
2. **The Blacksmith** (*Smid*). Perks: repairs; Penalty: heavy forge load.
3. **The Hunter** (*Jagter*). Perks: hunting yield, tracking; Penalty: lower family morale.
4. **The Predikant's household.** Inspired by Susanna and Erasmus Smit. Perks: morale, nursing; Penalty: few fighting men, little money.

**English side**
1. **The 1820 Settler Family** (the fictional *Ashworths*). They held a farm in the frontier districts and lived through the frontier wars, so they know the tension and fear on the frontier at first hand. Perks: wagon upkeep, colonial credit; Penalty: distrusted by trekkers.
2. **The Trader.** Carries trade goods north to sell to the trekkers and interior peoples, like the smouse and English traders. Perks: barter; Penalty: a heavy wagon, a target for theft.
3. **The Hunter-Traveller.** Inspired by **W. Cornwallis Harris's** real 1836–37 hunting trip from Grahamstown across the Orange to Mzilikazi's country. Perks: guns and horses; Penalty: small family, little knowledge of farming.

Each family brings named people, including **servants and herders** (Khoikhoi, formerly enslaved, Mfengu). They have their own names, skills (voorloper, herder, interpreter), and their own decisions (they can leave if badly treated).

### Frontier tension in the English backstory
The tense relationship between settlers and local people on the Eastern Cape frontier fits the **English settler backstory** and the **frontier events** in the first section of the route (Grahamstown → Cradock). Handle it with care:
- Show the fear and the raids that families lived through.
- Also show what drove Xhosa communities: land taken in the frontier wars, the killing of King Hintsa in 1835, and the annexation of Queen Adelaide Province and its reversal.


---

## 5. Core game loop

```
OUTFIT (Grahamstown) → choose side → choose family → buy wagon(s), oxen, supplies
TRAVEL TURN (1 day; batch several quiet days)
  pace: easy / normal / hard     rations: full / meagre / starving
  night stance: Rest | Form Laager | Guard herd   (Dutch side may join a group laager)
  daily: distance, food/water, ox stamina, grazing (season), health, morale
  event check (weighted by region, season, weather, stance, SIDE, relations)
LANDMARK: set-piece choices, trade, rest, council (Dutch), news
SIDE ACTIONS: hunt · trade · repair · tend sick · scout (voorloper)
END: arrival → epilogue (survival + relations + choices) | death → tombstone
```

Systems: time and season, travel, oxen, wagon condition, party and health, economy (coin → barter), hunting (real-time, aim with the mouse, Oregon Trail style; slow-loading flintlock), relations (per group, plus the trekker council and the British), events, endings. (Details are in `01_gemini_review.md` §2.)

---

## 6. Technical plan (C + SDL3)

### Stack
- **C17, built with CMake** (3.24 or newer).
  - Libraries: **SDL3** for windows, input, rendering and basic audio; **SDL3_ttf** for text; **SDL3_image** for PNG art.
  - Audio: start with SDL's built-in audio (WAV files). Add SDL3_mixer only if music needs it.
- **Dependencies are fetched by CMake** using `FetchContent`, pinned to release tags, with SDL_ttf's own bundled copy of FreeType. Anyone who clones the repo can build with no separate SDL install. A developer with system SDL3 packages (MSYS2, apt, brew) can use them instead.
- **Data files are JSON**, read with a vendored **cJSON** (MIT licence, GPL-compatible) in `third_party/`. They are easy to edit by hand and easy to check.
- **Our own random number generator** (PCG32 or xorshift), seeded per game. Never `rand()`. Runs must be reproducible from a seed.
- **Saves** are JSON files in `SDL_GetPrefPath("TheGreatTrek", "TheGreatTrek")`.

### Architecture: the core never touches SDL
```
src/core/   pure C simulation (no SDL): state, rng, travel, economy, events, hunting,
            party, relations, endings, data loading. Deterministic given a seed.
src/ui/     SDL front end: screens (title, side, family, store, travel, event,
            landmark, hunt, tombstone, epilogue), text rendering/wrapping, input.
src/main.c  SDL init + main loop + screen state machine.
tools/      headless programs linking only src/core:
              trekbot   — plays N runs per side, prints survival/balance stats
              datacheck — validates every data/*.json and that all text keys exist
tests/      unit tests for core (CTest)
```
Keeping SDL out of the core means the balance bot and the tests run without a window. It also means the simulation could get another front end later if wanted, such as a terminal version.

### Presentation
- **Low-resolution logical canvas** (e.g. 640×360) scaled up with `SDL_SetRenderLogicalPresentation`. This gives a crisp retro look on any screen.
- **Mostly text screens**, like Oregon Trail. Choices are made with the number keys 1–4 or the mouse. Each event screen gets a status bar (date, weather, oxen, food, health) and later a small illustration.
- **Text wrapping** uses SDL3_ttf's wrapped rendering. The font must be SIL OFL licensed.
- Names are typed in with SDL text input.

### Data and text
- `data/events/*.json`. Each event has `id`, `sides`, `regions`, `months`, `weight`, `requires`, `sources`, and choices with weighted outcomes and effects. (The format is drafted in `04_game_design.md` §8.)
- Other data files: `data/items.json`, `data/wagons.json`, `data/route.json`, `data/families.json`, `data/prices.json` (side × seller-group modifiers).
- **All player-facing text lives in `data/locale/en.json`** and is looked up by key. A Dutch or Afrikaans file can be added later without changing code.

### Repository layout
```
CMakeLists.txt
CLAUDE.md, README.md, LICENSE (GPL-3.0), CREDITS.md
src/core/  src/ui/  src/main.c
tools/  tests/  third_party/ (cJSON)
data/   events/ locale/ *.json
assets/ fonts/ images/ sounds/   (licence noted per file in CREDITS.md)
docs/planning/                   (these planning docs)
```

### Licensing notes
- The repo is **GPL-3.0** (`LICENSE`). Game text and data use the same licence.
- **CC BY-SA 4.0** content would be compatible if wanted for art.
- **Art and images:** use only your own work, public-domain historical images (e.g. Harris's 1840 animal plates, period engravings), or CC BY / BY-SA works. Record every asset in `CREDITS.md`.
- **Fonts:** SIL OFL fonts are fine.
- **Libraries:** SDL3 and its satellites use the zlib licence; cJSON is MIT. Both are GPL-compatible.

---

## 7. Milestones

Status on 2026-10-11 (in brackets after each milestone).

| # | Milestone | Done when |
|---|---|---|
| 1 | **Build skeleton** [done] | CMake + FetchContent builds a window with text on Windows. `core` library, `trekbot` and `datacheck` stubs, and CTest all run. |
| 2 | **Research pass 2** [done; the Tregardt diary was found free on DBNL and read in part in pass 3, `05_research_pass2.md` section 14] | Route checked on a real map with distances. Calendar for 1837. Glossary of period terms. Tregardt diary and Harris's *Narrative* skimmed for events. |
| 3 | **Core loop** [done] | Outfit → travel → landmark → end works with placeholder text, both sides, seeded RNG, save/load. The bot can finish runs. |
| 4 | **Vertical slice** [built; the family playtest is still to do] | Grahamstown → Orange River playable for **both sides**: about 15 events, shop with side pricing, laager, oxen, illness, tombstone. Family playtest. |
| 5 | **Full route** [done: both roads to the Mooi River, hunting, barter, 54 events, five endings, bot-balanced; more events welcome] | Through to the Mooi River: Thaba Nchu, Vaal crossing, hunting, smous, barter, about 50 events, endings. Balance with the bot. |
| 6 | **Accuracy & perspective pass** [history notes written for every event; a careful second read by a person is still needed] | Check that African groups, servants and the 1837 conflicts are shown fairly. "History behind this event" notes. |
| 7 | **Polish & release** [pixel-art sprites for wagons, oxen, buildings, plants and animals, layered scenes, a parchment route map, graves, glossary, synthesised sound effects, portraits of the named people and a release zip done; a GitHub release to do] | Pixel art, route map, sound, tombstone screen. GitHub release with a Windows build (zip with the exe and SDL DLLs). |
| 8 | *(Later)* | Natal fork over the Drakensberg; Dutch or Afrikaans translation; Linux and macOS builds. |

---

## 8. Next steps
1. A family playtest (milestone 4), then balance from what the family finds.
2. A second reader for the history notes and the origin stories (milestone 6).
3. A first GitHub release (milestone 7). An MSVC build test needs the C++ tools added to Visual Studio.
4. Research still open: shop prices of powder, coffee, sugar and trade goods; isiXhosa names; see `05_research_pass2.md` sections 13 and 14.
