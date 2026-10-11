# Game Design: How the Game Works

Status: **first draft (2026-10-11).** Builds on `03_development_plan.md`, which stays the source of truth for the settled decisions. This file says how those decisions turn into screens, systems, data files and code.

**About the numbers.** Unless a number is marked **[sourced]**, it is an *invented starting value for balancing*. These are game tuning, not history, and they all live in `data/` so the trekbot can tune them. Facts marked **[recall, verify]** must be checked against a source before they go into event text.

---

## 1. A session at a glance

```
Title ─▶ Side ─▶ Family ─▶ Name party ─▶ Opening scene (Bible gift, Grahamstown)
                                               │
                                               ▼
                                      Outfitting store
                                               │
          ┌────────────────────────────────────▼───────────────────────────┐
          │ TRAVEL (one day per tick; quiet days fast-forward)             │
          │   ├─▶ Event (dialog)            ├─▶ Hunt (turn-based)          │
          │   ├─▶ Landmark ─▶ store / barter / talk / council / rest       │
          │   ├─▶ River crossing (set piece)                               │
          │   └─▶ Grave (a party member dies) ─▶ back to travel            │
          └──────────────┬──────────────────────────────┬──────────────────┘
                         ▼                              ▼
              Arrival at the Mooi River        Everyone lost / stranded
                         ▼                              ▼
              Epilogue + family score            Ending screen + grave
```

Target length: **30–60 minutes** for a full run (plan §2). Rough budget on Normal:

| Part | Count | Time each | Total |
|---|---|---|---|
| Travel days (fast-forward) | ~85 | ~1 s | ~1.5 min |
| Events | ~25 | ~30 s | ~12 min |
| Landmarks (incl. stores) | 7–8 | ~2 min | ~15 min |
| Hunts | 3–5 | ~1.5 min | ~6 min |
| Outfitting + opening + ending | 1 | ~8 min | ~8 min |
| | | | **≈ 40 min** |

---

## 2. Screens

### 2.1 Screen list

| Screen | Purpose | Reached from |
|---|---|---|
| **Title** | New game, Continue, Load, Graves, Settings, Credits, Quit | start |
| **Side select** | Dutch or English, with a short list of what changes | Title |
| **Family select** | Backstory cards: portrait, story, perks/penalties, party list | Side |
| **Name party** | Accept or edit names (default fictional names supplied) | Family |
| **Dialog** | All events and conversations (§3) | many |
| **Store** | Coin purchases at colonial towns and missions (§7) | Landmark |
| **Barter** | Two-column trade north of the Orange (§7.4) | Landmark, event |
| **Travel** | Main loop screen (§2.3) | after outfitting |
| **Supplies** | Inventory, weight, wagon parts, livestock | Travel menu |
| **Party** | Members, health, conditions, skills, loyalty | Travel menu |
| **Map** | Full route map (§5) | Travel menu, `M` |
| **Journal** | Scrolling diary of everything that happened | Travel menu, `J` |
| **Standing** | Relations in words ("Moroka's people remember your help") | Travel menu |
| **Landmark** | Picture, description, actions | arrival |
| **River crossing** | Read the river, choose a method, watch the result | landmark/edge |
| **Hunt** | Turn-based flintlock hunt (§9) | Travel menu, event |
| **Grave** | Name, dates, typed epitaph; saved to the graves file | a death |
| **Ending / Epilogue** | Assembled epilogue (§12), score, "History behind the game" | end |
| **Pause** (overlay) | Save, Settings, Glossary, Quit to title | `Esc` |
| **Glossary** (overlay) | Period words: laager, smous, voorloper, drift, sanna… | `G`, or clicking a highlighted word |
| **History note** (overlay) | "The history behind this event" + sources | `H` on an event |

### 2.2 Screen state machine (UI side)

- The UI keeps a **screen stack**. Full screens replace the top; overlays (Pause, Glossary, History note, Supplies, Party, Map, Journal) are **pushed** and popped with `Esc`.
- Each screen is a small vtable:
  ```c
  typedef struct Screen {
      void (*enter)(struct Screen *, App *);
      void (*leave)(struct Screen *, App *);
      void (*event)(struct Screen *, App *, const SDL_Event *);
      void (*update)(struct Screen *, App *, double dt);
      void (*render)(struct Screen *, App *, SDL_Renderer *);
  } Screen;
  ```
- **Which full screen is showing is decided by the core's phase, not by the UI.** After every command, `main.c` asks the core `game_phase(g)` and switches screens if needed. This keeps the UI dumb and makes save/load trivial: load the state, read the phase, show the matching screen.

```c
typedef enum {
    PHASE_OUTFIT, PHASE_TRAVEL, PHASE_DIALOG, PHASE_LANDMARK,
    PHASE_STORE, PHASE_BARTER, PHASE_RIVER, PHASE_HUNT,
    PHASE_GRAVE, PHASE_ENDED
} GamePhase;
```

### 2.3 Layout (640×360 logical canvas)

Travel screen:
```
┌──────────────────────────────────────────────────────────────────────────┐
│ Mon 15 May 1837 · Autumn · Clear, cold        Karoo, near the Brak River │ status bar
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│        (scene: veld layers scroll; wagon + span of oxen walk;            │ ~180 px
│         herd and herders behind; sky colour by time/weather)             │
│                                                                          │
├──────────────────────────────────────────────────────────────────────────┤
│ Grahamstown ●───●───●──▶🐂─────○──────○────○────○ Mooi River              │ trail strip
│ Next: Colesberg, 38 mi              Travelled: 212 mi                    │
├──────────────────────────────────────────────────────────────────────────┤
│ Food 640 lb   Water 2 vats   Oxen 14 (tired)   Health: fair   Morale: ok │
│ 1 Travel on   2 Rest a day   3 Pace: normal   4 Rations: full            │
│ 5 Night: laager   6 Hunt   7 Supplies   8 Party   9 Map    Esc menu      │
└──────────────────────────────────────────────────────────────────────────┘
```

Dialog screen:
```
┌──────────────────────────────────────────────────────────────────────────┐
│ status bar                                                               │
├───────────────┬──────────────────────────────────────────────────────────┤
│               │ KLAAS, YOUR VOORLOPER                                    │
│  (portrait or │ "The water here stands still and green, baas. The oxen   │
│   scene art)  │ will drink it, but I would not let the children."        │
│               │                                                    ▼     │
├───────────────┴──────────────────────────────────────────────────────────┤
│ 1 Let everyone drink                                                     │
│ 2 Boil it first (costs firewood and half a day)                          │
│ 3 Push on to the next spring                                             │
│ 4 Ask Klaas where the next spring is   [needs: voorloper healthy]        │
│                                                     H history  G glossary│
└──────────────────────────────────────────────────────────────────────────┘
```

Rules:
- Event choices: **at most 4**, keys `1`–`4` or mouse. Menus (travel, store) may use `1`–`9`.
- Mouse hover highlights a choice; click selects. Keyboard and mouse are both full paths through the game.
- Font: one SIL OFL pixel-friendly font at two sizes (body and headings). Scale the font in Settings for younger or older eyes.

---

## 3. Dialog and text

### 3.1 Everything is a locale key

- No player-facing literals in C. Every string is a key in `data/locale/en.json`.
- **Lookup order** so text can differ by side without code: `key@<side>.<family>` → `key@<side>` → `key`. Example: `event.smous.intro@english` can greet the English family in English while the default greets in Dutch.
- **Substitution:** `{name}` style placeholders filled from an argument list the core supplies.
  - Party references: `{m.name}`, `{m.he}`, `{m.him}`, `{m.his}` for the member an event picked (each member has a pronoun set in `families.json`).
  - Plurals: `{n:ox|oxen}` picks the form by `n`. A translation can define its own rules later.
  - Money: `{money:amount}` formats per side (§7.1).
- **Light markup** inside strings:
  - `[[laager]]` marks a glossary term: drawn in a highlight colour; clicking it (or `G`) opens the glossary entry.
  - `*word*` is emphasis (a second colour, not italics, so the pixel font stays readable).
  - `\n\n` is a paragraph break.

### 3.2 Rendering

- Do **our own line layout** with `TTF_MeasureString` so we can (a) colour runs inside a line and (b) reveal text a character at a time. Lines are rendered to cached textures and only rebuilt when the text changes.
- **Typewriter reveal** at the speed from Settings (instant, fast, normal). Any key or click finishes the page; a second press goes on.
- Long text is **paged** to fit the box; `▼` marks more.
- The journal keeps the final text of every page and outcome, so the player can scroll back.

### 3.3 Dialog model (core side)

An event is a **small graph of nodes**. A simple event has one node; a set piece (the Bible scene, meeting Moroka, the trekker council) has several.

```
Node:    speaker, image, text, choices[≤4]
Choice:  text, requires (shown greyed with a reason if unmet), outcomes[]
Outcome: weight (+ skill modifiers), text, effects[], next node (or end)
```

- **Locked choices are shown, greyed out, with a reason** ("Needs an interpreter", "The council only hears its own members"). This is how the player *sees* that their side matters.
- The game **never shows odds as numbers**. Choice text hints at risk in words ("risky", "slow but safe"). An optional Settings toggle "Show odds" is for curious players and testing.
- The core exposes the current node as a plain struct; the UI draws it:

```c
typedef struct {
    const char *key;            /* locale key */
    TextArg     args[8];
    int         n_args;
} LocText;

typedef struct {
    LocText text;
    bool    enabled;
    LocText locked_reason;      /* empty when enabled */
} ChoiceView;

typedef struct {
    LocText    speaker, body;
    const char *image_id;
    ChoiceView choices[4];
    int        n_choices;
    const char *history_note_key; /* NULL if none */
} DialogView;

bool game_dialog_view(const Game *g, DialogView *out);
void game_choose(Game *g, int choice, Report *rep);
```

`Report` is a list of `LocText` lines ("Two oxen drowned.") plus stat deltas the UI animates in the status bar. The same lines go to the journal.

### 3.4 The party talks

Party members (especially the servants and older children) are speakers in events, not just names on a list. An event can pick "the member with the best herding skill" or "the voorloper" as its speaker. This keeps the people who did the work at the centre of the story (plan §2, pillar 4).

---

## 4. Time, calendar and weather

### 4.1 Calendar

- One tick = **one day**. The date is stored as a day count from 1 Jan 1837; the weekday and month are computed (1 Jan 1837 was a Sunday, so the default start, **Monday 3 April 1837**, is computed, not hard-coded).
- **Seasons (southern hemisphere):** Apr autumn · May–Aug winter · Sep–Oct spring · Nov–Mar summer.
- **Sundays:** a standing order "Keep the Sabbath" (rest on Sundays) defaults to *on*. It costs distance but gives morale, and trekker parties expect it. *Sabbath rest on trek* is **[recall, verify]**.

### 4.2 Weather

Each day rolls a weather state from a per-region × per-month table in `data/weather.json`:

`clear · hot · windy/dust · cold · frost · rain · thunderstorm`

- **Karoo:** dry, hot days and cold nights; rain rare. **Highveld:** winter frost and dry grass; summer thunderstorms. **Eastern Cape:** rain possible in any month. *(General climate, [recall, verify] against a climate source.)*
- Weather affects: travel distance, ox stamina, illness, misfires when hunting (§9), river depth (§6), and some events.
- Weather is "sticky": tomorrow is more likely to match today (a simple Markov table), so storms come in spells.

### 4.3 The daily tick (fixed order, for determinism)

```
1. Morning: apply standing orders (pace, rations, Sabbath)
2. Weather roll
3. Travel: distance = base_pace × oxen × terrain × weather × wagon × voorloper × load
   (0 if resting; stop at a landmark or river even if the day isn't used up)
4. Consumption: food per person, water, firewood
5. Livestock: grazing and water → ox stamina; straying check
6. Health: illness progression, recovery, new illness checks
7. Morale and servant loyalty update
8. Daytime event check (§8.3)
9. Night: stance resolves (§10); night event check
10. Checks: death → Grave; no oxen able to pull → stranded event; arrival → Landmark
```

### 4.4 Fast-forward

"Travel on" runs days until something needs the player: an event, a landmark, a river, a death, or a **warning** (food under 7 days, water under 2 days, an ox dying, someone falling ill). Each quiet day plays ~1 second of the travel animation, with a "skip" key to go faster.

### 4.5 Time costs of actions

| Action | Time (starting values) |
|---|---|
| Rest | 1 day |
| Hunt | half a day (less travel that day) |
| Repair a wheel / axle | half a day / 1 day (Blacksmith: halved) |
| Form a laager at night | costs ~1 mi of the day's travel |
| Trading at a landmark | free (but the day passes if you stay the night) |
| Wait at a flooded drift | 1 day per wait |

---

## 5. The route and the map

### 5.1 Route data (`data/route.json`)

The route is a **graph**, not a line, so optional stops (Philippolis) and the later Natal fork are just more edges.

```json
{
  "nodes": [
    { "id": "grahamstown", "name": "place.grahamstown", "lat": -33.31, "lon": 26.53,
      "kind": "town", "shops": ["colonial_merchant"], "map_xy": [412, 300] },
    { "id": "orange_drift", "name": "place.orange_drift", "kind": "river",
      "river": "orange", "map_xy": [300, 168] }
  ],
  "edges": [
    { "from": "grahamstown", "to": "cradock", "miles": 95,
      "region": "eastern_frontier", "terrain": "hills",
      "water": "streams", "optional": false }
  ]
}
```

- `region` drives events, weather, grazing and animal tables.
- `water` is `streams` (water most days), `springs` (sometimes), or `dry` (Karoo stretches: carry water).
- Real lat/lon is stored so the route can be checked against a real map in research pass 2.

### 5.2 Rough distances (to verify, milestone 2)

These are **my estimates from modern road distances**; ox-wagon tracks differed. They total more than the plan's 450–500 mi estimate, so research pass 2 must settle this.

| Leg | Region | ~Miles |
|---|---|---|
| Grahamstown → Cradock | Eastern frontier | 95 |
| Cradock → Colesberg | Karoo | 145 |
| Colesberg → Orange River drift | Karoo / river | 25 |
| Orange drift → Thaba Nchu (direct, or via Philippolis) | Transorangia | 125 (direct) |
| Thaba Nchu → Sand/Vet River | Highveld | 60 |
| Sand/Vet River → Vaal drift | Highveld | 95 |
| Vaal drift → Mooi River | Highveld | 30 |
| | **Total** | **≈ 575** |

At 6–12 mi/day **[sourced: about 10 km/day with herds]**, with rest days, that is roughly 70–100 days, inside the plan's 60–120.

### 5.3 Two map views

1. **Trail strip** (always on the travel screen): a horizontal line of landmarks with the wagon icon moving along it. Shows distance to the next landmark.
2. **Map screen** (`M`): a hand-drawn-style pixel map of the eastern Cape to the Highveld on the 640×360 canvas.
   - Rivers drawn and labelled: Great Fish, Orange, Caledon, Vet, Sand, Vaal, Mooi.
   - Landmarks: visited (filled), known (outline), rumoured (a "?" until someone tells you about it).
   - The route already travelled is a dotted line; each grave is a small cross.
   - **Peoples are labelled softly across areas** (Griqua, Barolong, Basotho, Ndebele), with no hard borders. Land claims were contested, and the map shouldn't imply lines that didn't exist. Hovering a label shows a short note.
   - Map positions come from `map_xy` in `route.json`; the background is one PNG in `assets/images/`.

---

## 6. River crossings (set pieces)

The Orange and the Vaal are the two big drifts; smaller rivers (Great Fish, Caledon, Vet, Sand) are mini-crossings.

**Reading the river.** The crossing screen describes depth, current and banks in words, from:
`depth = base_depth[river][month] + recent_rain_bonus + random_spread`.
Summer rains raise the rivers; an April–July crossing of the Orange is usually lower. *(Seasonal flooding: [recall, verify].)*

| Choice | Needs | Effect |
|---|---|---|
| Ford it | depth below a threshold | fast; risk rises steeply with depth |
| Double-span the oxen | ≥ 2 spans' worth of oxen | lower risk of sticking mid-stream |
| Unload and carry goods over | half a day + people | protects cargo; wagon still at risk |
| Caulk and float the wagon | tallow, hides; deep water | slow; risk of tipping |
| Wait for the river to drop | — | 1 day, eats food; depth re-rolls |
| Get help from people at the drift | relations, payment or barter | lowers risk; who is there depends on the drift *(verify who lived at or used each drift in 1837)* |

Outcomes: safe crossing; goods lost (random items, weight-weighted); ox drowns; wagon damaged; a person in danger (a rescue choice, with a named helper). Deaths at crossings are possible but rare on Easy.

---

## 7. Money, stores and trading

### 7.1 Currency

- **One internal unit: the farthing** (¼ d), stored as a 64-bit integer.
- **[sourced]** From 1825, 1 rixdollar = 1s 6d, so 1 Rd = 18 d = **72 farthings**, and £1 = 960 farthings ≈ 13⅓ Rds.
- **Display by side:** English players see `£3 4s 6d`; Dutch players see `48 Rds 3 sk`. *(1 Rd = 8 skillings, so 1 sk = 9 farthings: **[recall, verify]**.)*
- Prices in `data/` are written in farthings, or in pence with a helper, so the numbers are exact.

### 7.2 Prices

From the plan:

```
price = base_price[item]
      × side_modifier[side][seller_group]
      × distance_modifier[node]
      × relationship_modifier(relation with seller_group)
      × scarcity_modifier(stock left / stock at start)
```

- `scarcity_modifier` is new: each shop has limited stock; the price rises as you buy it out (e.g. ×1.0 full → ×1.5 nearly empty). It stops one player from buying all the lead in Cradock.
- **Selling** pays `price × sell_spread[seller_group]` (e.g. 0.5 at a merchant, better with traders who want that good).
- **Credit (English side):** in Grahamstown the merchant offers credit up to a limit. Debt is a flag plus an amount; it can bring a debt-collector event later and shows up in the epilogue.

### 7.3 The store screen

```
┌ HOLLAND & SONS, GRAHAMSTOWN (fictional) ──────────── You have £42 6s 0d ┐
│  Item                     Price      Have   Buy                         │
│  1 Oxen                   £3 0s       0     [ 14 ]                      │
│  2 Meal / flour (100 lb)  12s 0d      0     [  6 ]                      │
│  3 Gunpowder (lb)         2s 6d       0     [ 10 ]                      │
│  …                                                                      │
│  Spare parts: wheel, axle, disselboom                                   │
├─────────────────────────────────────────────────────────────────────────┤
│  Bill: £38 2s 0d     Load: 2,850 / 3,000 lb  (heavy)                    │
│  Mr Holland: "You'll want a spare axle before the Karoo, sir."          │
│  ←/→ change amount   Enter buy   Esc leave                              │
└─────────────────────────────────────────────────────────────────────────┘
```

- Items are grouped: **Animals · Food · Water & cooking · Guns & shot · Wagon & parts · Trade goods · Medicine**.
- The shopkeeper (a fictional named person) gives **one advice line** about the most dangerous gap in your supplies (no spare axle, too few oxen, no water vats before the Karoo). This is the Oregon Trail "Matt's General Store" lesson.
- Running totals show **money and wagon load**. Overloading slows travel and wears the wagon.
- The catalogue is filtered by side (`items.json` and `wagons.json` have a `sides` field).

### 7.4 Barter (north of the Orange)

North of the Orange, coin is worth little. **Trade goods become money**: gunpowder, lead, tobacco, beads, iron, cloth, coffee, and livestock (`01_gemini_review.md` §2; confirm each good against a period source).

```
┌ TRADING WITH THE BAROLONG AT THABA NCHU ─────────────────────────────────┐
│  You offer                         │  They offer                        │
│  ▸ Tobacco      4 lb               │  ▸ Oxen          2                 │
│    Beads        2 strings          │    Sorghum       100 lb            │
│    Cloth        1 bolt             │    Milk/curds    (food)            │
│                                    │                                    │
│  Balance: ███████░░░  "They think this is a little short."              │
│  1 Offer   2 Add more   3 Ask what they need   4 Leave                  │
└─────────────────────────────────────────────────────────────────────────┘
```

- Each partner (group + place) has a **wants table**: value per good, and how much they want of it today.
- The balance meter compares your offer to their goods. An offer is accepted when `your_value ≥ their_value × fairness_threshold`.
- **Fair trades build relations; squeezing a partner hurts them.** "Ask what they need" costs nothing and reveals the top two wants. A good interpreter improves the deal.
- Barter partners are **named** (a headman, a trader, a missionary's agent), with a line of their own reason for trading.

### 7.5 Other sellers

- **Smous** (event, any region): mixed coin and barter, higher prices, sometimes rare goods, always news and rumours (which can reveal "?" places on the map).
- **Mission stations** (Wesleyan at Thaba Nchu): medicine, news, letters, a little food. Warmer to the English side (plan §4).
- **Trekker laagers** (Dutch side welcome; English side tolerated): oxen, food, gunpowder; fair prices if your council standing is good.

---

## 8. Events

### 8.1 File format (`data/events/*.json`)

One file per region or theme. The format the plan referred to:

```json
{
  "id": "karoo_stagnant_water",
  "sides": ["dutch", "english"],
  "regions": ["karoo"],
  "months": [1,2,3,4,5,6,7,8,9,10,11,12],
  "time": "day",
  "weight": 10,
  "once": false,
  "cooldown_days": 20,
  "requires": [ { "edge_water": "dry" } ],
  "weight_mods": [ { "if": { "weather": "hot" }, "mul": 1.5 } ],
  "speaker": { "pick": "role", "role": "voorloper" },
  "image": "karoo_pool",
  "text": "event.karoo_stagnant_water.body",
  "history": "history.karoo_water",
  "sources": [
    { "kind": "invented", "note": "Gemini seed 1. Dysentery risk is plausible but not sourced; see 01_gemini_review.md §3." }
  ],
  "choices": [
    {
      "text": "event.karoo_stagnant_water.drink",
      "outcomes": [
        { "weight": 60, "text": "event.karoo_stagnant_water.drink.ok",
          "effects": [ { "water": 2 } ] },
        { "weight": 40, "text": "event.karoo_stagnant_water.drink.sick",
          "effects": [ { "water": 2 }, { "illness": "flux", "who": "random", "count": 2 } ] }
      ]
    },
    {
      "text": "event.karoo_stagnant_water.boil",
      "outcomes": [
        { "weight": 100, "text": "event.karoo_stagnant_water.boil.ok",
          "effects": [ { "water": 2 }, { "item": "firewood", "add": -1 }, { "time_days": 0.5 } ] }
      ]
    },
    {
      "text": "event.karoo_stagnant_water.ask_voorloper",
      "requires": [ { "member_role_healthy": "voorloper" } ],
      "locked_reason": "lock.need_voorloper",
      "outcomes": [
        { "weight": 70, "weight_mods": [ { "skill": "voorloper", "per_point": 5 } ],
          "text": "event.karoo_stagnant_water.ask.spring",
          "effects": [ { "water": 4 }, { "morale": 5 } ] },
        { "weight": 30, "text": "event.karoo_stagnant_water.ask.none",
          "effects": [ { "time_days": 0.5 } ] }
      ]
    }
  ]
}
```

Multi-node events add a `"nodes": { ... }` map and a `"start"` node; each outcome may have `"next": "node_id"`.

### 8.2 Conditions and effects (a fixed vocabulary, not a scripting language)

`requires` is a list of clauses that must **all** be true; `{ "any": [ ... ] }` gives OR, `{ "not": {...} }` negates.

| Condition clauses | Effect clauses |
|---|---|
| `side`, `family` | `item` add/remove (food, powder, lead, parts, trade goods…) |
| `region`, `node`, `edge_water`, `month`, `season`, `weather` | `money` add/remove |
| `flag`, `not_flag`, `counter_gte` | `oxen`, `livestock` (cattle, sheep, horses) add/remove |
| `item_gte`, `money_gte`, `oxen_gte` | `ox_stamina`, `wagon_damage` (part, amount) |
| `member_role`, `member_role_healthy`, `member_skill_gte` | `health`, `illness`, `injury` (who: random/role/named/all) |
| `relation_gte` / `relation_lt` (group, value) | `morale`, `loyalty` (servant) |
| `stance` (tonight's night stance), `in_group_laager` | `relation` (group, delta) |
| `days_since_start_gte`, `distance_gte` | `flag` set/clear, `counter` add |
| `chance` (extra random gate) | `time_days`, `distance` (lost or gained miles) |
| | `schedule` (event id, in N days) — chains and follow-ups |
| | `start_hunt` (table), `start_barter` (partner), `start_river` |
| | `death` (who) — rare, always with its own text; see §11.4 |
| | `reveal_node` (map), `journal` (extra line), `history_unlock` |

`datacheck` validates every clause name, every locale key, every `next` node, and that **`sources` is present and non-empty** for every event.

### 8.3 How events are picked

Each day (and again at night, with `"time": "night"` events):

1. **Does anything happen?** `p = base_rate[region] × difficulty × pace_mod` (starting value: about 1 event every 3 days).
2. **Which event?** Gather all events whose `sides`, `regions`, `months` and `requires` match and which aren't on cooldown or used up (`once`). Weight = `weight × weight_mods`. Pick by weight with the game RNG.
3. **Scheduled events** (from `schedule` effects) fire on their day before any random pick.
4. **Landmark set pieces** are not random: they belong to the landmark (§11.1).

Events can also **chain** through flags: helping a stranded Griqua wagon sets `helped_griqua_wagon`, which unlocks a later event where the same person returns the favour.

### 8.4 Event categories (≈50 for the full route; ≈15 in the vertical slice)

| Category | Examples |
|---|---|
| Land & water | stagnant pool, dry stretch, lost track, steep descent, grass fire |
| Wagon | broken wheel / axle / disselboom, overturned wagon, lost wheel tyre |
| Livestock | oxen stray, lame ox, ox "lies down", stampede at night, theft of stock |
| Health | flux, fever, snakebite, burns, birth on the road, cold nights |
| Wildlife | lions near the herd, game herds passing, a hunt opportunity |
| People | smous, missionary, Griqua horsemen, Barolong herders, Basotho traders, a messenger with news, refugees from the Ndebele wars, British official with a letter |
| Party | servant wants to leave or asks for wages; quarrel; a child wanders off; Sunday service |
| History & news | Vegkop survivors' stories, Retief's manifesto read aloud, news of Mosega, council elections (Dutch) |
| Side-only | Dutch: council vote, leader quarrel. English: debt collector, "Are you the Governor's man?", trekkers ask you to carry letters back |

---

## 9. Hunting

### 9.1 Starting a hunt

- From the travel menu (costs half a day) or from an event ("A herd of springbok crosses the track").
- Needs a gun, powder and lead. Choose the **hunter** (any member who can shoot; skill matters) and optionally a **horse** (faster approach, carries more meat back) and **dogs**.
- The animals present come from `data/animals.json` for the current region and month.

### 9.2 The hunt screen (turn-based)

```
┌ HUNT · Highveld, open grass · Clear, wind from the east ───────────────┐
│                                                                        │
│     (side view: grass layers, an anthill, a herd of blesbok;           │
│      distance shown as a range marker under the target)                │
│                                                                        │
├────────────────────────────────────────────────────────────────────────┤
│ Target: blesbok (grazing)   Range: long   Your gun: loaded             │
│ Powder 6 lb  Lead 4 lb   Meat taken: 0 lb / can carry 200 lb           │
│ 1 Stalk closer   2 Wait   3 Aim and fire   4 Leave the hunt            │
└────────────────────────────────────────────────────────────────────────┘
```

Each turn the player picks one action; then animals act.

| Action | Effect |
|---|---|
| **Stalk closer** | range shrinks one step; chance to spook the target (wind direction, cover, tracking skill) |
| **Wait** | animals move; a better target may come close |
| **Aim and fire** | check misfire, then hit chance |
| **Reload** | flintlock takes **3 turns** (better guns fewer); can't fire meanwhile |
| **Follow the spoor** | after a wounded animal runs; tracking skill check |
| **Leave** | ends the hunt |

- **Misfire** by weather: ~2% clear, ~15% humid, ~60% rain (Gemini values, **invented**). Flint wear raises it slowly; a spare flint resets it.
- **Hit chance** = gun accuracy × range factor × hunter skill × horse/standing modifier. Each shot uses powder and lead.
- The hunt ends when the player leaves, runs out of shot, hits the carry limit, or after ~10 turns (sundown).

### 9.3 Animal tiers (from the Gemini idea)

| Tier | Animals (Highveld/Karoo, check each against Harris 1836–37) | Behaviour |
|---|---|---|
| Passive | springbok, blesbok, black wildebeest, red hartebeest, quagga, ostrich | flee when spooked; herds |
| Big game | eland, Cape buffalo *(check range)* | lots of meat; buffalo may **charge** if wounded |
| Predators | lion | may **hunt back**: the hunt turns into a short danger sequence (stand and fire, retreat to the wagon, send the dogs) |

- The **quagga** was alive and common on these plains in 1837 (extinct by 1883) **[recall, verify]**. A history note can say so.
- **Meat and carry limit:** you can bring back only what you can carry (more with a horse or if the wagon is near). Extra meat is wasted, and the game says so. Overhunting a region lowers that region's game for the rest of the run (`region_game_level`).
- **Family-friendly presentation:** the animal falls or runs off; no blood or gore. A lion "springs at the hunter" and the outcome is told in text.
- Art: W. Cornwallis Harris's 1840 animal plates (public domain) as the basis for pixel animals.
- Because the hunt is pure core logic, the trekbot can hunt with a simple policy.

---

## 10. Night: rest, laager or guard

The night stance is a **standing order** set from the travel menu, so the player doesn't answer it every evening. Events can still ask.

| Stance | Cost | Effect |
|---|---|---|
| **Rest** | none | full recovery for people and oxen; highest risk at night |
| **Form a laager** | ~1 mi of travel; small ox-stamina cost | sharply lowers predator, stock-theft and stampede events. A single family makes a small laager with thorn branches (a weaker version). |
| **Guard the herd** | morale and health cost for the guards (rotates) | lowers stock loss; the guard may spot trouble early (opens better choices) |
| **Join a group laager** | travel at the group's pace; Sabbath kept | the safest night; trade and help at drifts; Dutch side welcome, English side must be accepted (relations check, events) |

**Travelling with a trek party.** At some landmarks you can join a party of fictional trekker families for a leg. Their wagons count for laager strength and river help. The council's rules apply (pace, Sabbath, shared guard duty). Historical leaders (Potgieter, Maritz, Uys) are met as themselves in landmark scenes, not as party members.

---

## 11. Landmarks, party, health, livestock and wagon

### 11.1 Landmarks

Every landmark has a fixed **set piece** (first visit) plus repeatable actions:

| Landmark | Set piece | Actions |
|---|---|---|
| Grahamstown | The Bible gift to the Uys party (April 1837) **[sourced]**, played from your side | store (colonial), credit (English) |
| Cradock | Last colonial town; news from the frontier | store, repair (smith) |
| Colesberg area | Edge of the Colony; the Punishment Act (1836) as legal pressure, not a checkpoint | small store, rest |
| Orange River drift | River crossing (§6) | wait, cross |
| Philippolis (optional) | Griqua town; trade and relations | barter, talk |
| Thaba Nchu | Moroka and the Barolong; the Wesleyan mission; trekker gathering; Vegkop survivors (Moroka's oxen brought the stranded trek here **[sourced]**); Dutch council vote | barter, mission, council, join a trek party |
| Sand / Vet River | Highveld begins; winter frost | rest, hunt |
| Vaal River drift | River crossing; news of Mosega and the commandos; entering land Mzilikazi claimed | cross, wait |
| Mooi River | Arrival; epilogue | — |

Landmark actions: **Look around** (description + history note), **Talk** (named people; dialog), **Trade**, **Rest**, **Move on**.

### 11.2 Party members

```c
typedef struct {
    char     name[32];
    Role     role;          /* head, spouse, child, elder, servant */
    Pronouns pronouns;
    uint8_t  age_group;     /* child, adult, elder */
    uint8_t  skills[SKILL_COUNT]; /* shooting, herding, voorloper, interpreting,
                                     nursing, smithing, tracking: 0..5 */
    int16_t  health;        /* 0..100 */
    Condition conditions[4];
    int16_t  loyalty;       /* servants only, 0..100 */
    bool     alive, with_party;
} Member;
```

- Servants (Khoikhoi, formerly enslaved, Mfengu) are **full members** with names and skills. They are paid (wages in coin, livestock or goods) and can **choose to leave** if treated badly or unpaid; when one leaves, the game tells their side of it. Under **Ordinance 50 (1828)** Khoikhoi in the Colony could no longer be held by pass laws **[recall, verify]**, so leaving was a real option.
- **Health conditions** (each with a daily progression and a recovery chance, nursing skill helps):
  - *flux* (dysentery; the Oregon Trail homage, unsourced as a major killer)
  - *fever* (low on this route; higher on a future Natal/northern route; malaria is the documented killer on Tregardt's route **[sourced]**)
  - *injury* (wagon accident, fall from horse, snakebite)
  - *exposure* (Highveld frost, wet nights)
  - *exhaustion* (hard pace, poor rations)
- **Rations:** full / meagre / starving. Starting values: adults 2 lb/day full, 1.5 meagre, 1 starving; children ×0.6.
- **Morale** (party-wide, 0–100): raised by rest, Sundays, coffee and sugar, good news, music; lowered by deaths, hunger, hard pace, quarrels. Low morale lowers work (repairs, herding) and servant loyalty.

### 11.3 Oxen, livestock and the wagon

- **Oxen:** count + average stamina. A kakebeenwa needs a span of **12–16** (tunable; 01 §3). Fewer oxen = slower; below the minimum the wagon can't move and a "stranded" event starts (buy, borrow, or trade for oxen; or leave goods behind).
- **Stamina** changes daily: `+ grazing[region][month] + rest − pace_cost − load_cost − no_water_cost`. Under 30 the span slows; under 10 an ox may "lie down and not rise".
- **Loose stock** (cattle, sheep, horses): slow the trek a little, need herders, can stray or be stolen, and are food and barter currency. Horses are needed for good hunting. (Horse-sickness in summer: **[recall, verify]**. Lungsickness arrived in the 1850s, so it must **not** appear.)
- **Wagon:** condition per part (wheels, axle, disselboom, tent/canvas) 0–100. Breakage risk grows with damage, rough terrain, hard pace and overload. A spare part plus a repair action fixes it; the Blacksmith family repairs faster and can mend without a spare at a cost in time and iron.
- **Voorloper:** if the voorloper is sick, missing or dead, distance drops and wagon/route mishaps rise (Gemini idea, kept).

### 11.4 Death and graves

- Deaths happen through illness, accidents, crossings and (rarely) events. On **Easy** ("family mode") deaths are rarer and a warning always comes first.
- **Grave screen:** name, birth year, date and place of death, a typed epitaph (optional). The game adds the grave to `graves.json` in the pref path. Later runs show **graves from earlier runs** along the trail (the Oregon Trail tombstone tradition), with the epitaph.
- Violence is never shown; deaths are told in calm text.

---

## 12. Relations and endings

### 12.1 Relations

Values from −100 to +100, shown only in words on the Standing screen:

`trekker_council · british_colonial · missionaries · barolong · griqua · basotho · ndebele · smouse`, plus each servant's loyalty.

They change prices (§7.2), event weights and locked choices, who will help at a drift, and the epilogue. Starting values differ by side (plan §4).

### 12.2 Endings

| Ending | When |
|---|---|
| **Arrival** | reach the Mooi River with at least one family member |
| **Settled early** | choose to stay at Thaba Nchu or join another group (a real, non-failure ending) |
| **Turned back** | choose to return to the Colony |
| **Stranded / lost** | no oxen and no way to get more, or the whole family dies |

The **epilogue** is assembled from three parts (Gemini's "vectors"):
1. **Survival:** who arrived; oxen, livestock, wagons left.
2. **Standing:** one sentence per group whose relation is notably high or low ("The Barolong at Thaba Nchu remember the Ashworths as fair traders.").
3. **Choices:** flags from key decisions (council vote, helped a stranded wagon, paid the servants fairly, debts).

A **family score** (people × health + livestock + goods + relations bonus, multiplied by difficulty) gives a reason to replay. A closing "History behind the game" page points to the sources and says plainly what was invented.

---

## 13. Saves, settings and determinism

- **Saves:** JSON in `SDL_GetPrefPath("TheGreatTrek", "TheGreatTrek")`. 3 slots + an autosave each morning and on arriving at a landmark. Each save holds `version`, `seed`, the **RNG state**, the phase, and the full game state. Old save versions are rejected with a clear message until there is a reason to migrate them.
- **Determinism:** one PCG32 stream in the `Game` struct, passed to every system. Hunts and events use the same stream. Given seed + the list of player commands, a run replays exactly; `trekbot` and bug reports both rely on this. (A bug report can include "seed + command log".)
- **Settings** (separate `settings.json`): text speed, font size, fullscreen, volume, difficulty (Easy/Normal/Hard, chosen at new game), show odds (off by default), history notes on/off.
- **Difficulty** scales event rate, illness, breakage and starting supplies. It does not change the history text.

---

## 14. Core API sketch

The core is driven only by commands; it never waits for input.

```c
Game *game_new(uint64_t seed, Side side, const char *family_id, Difficulty d,
               const GameData *data);
void  game_free(Game *g);

GamePhase game_phase(const Game *g);

/* outfitting and stores */
bool game_store_view(const Game *g, StoreView *out);
bool game_store_buy(Game *g, const char *item_id, int qty, Report *rep);
bool game_store_sell(Game *g, const char *item_id, int qty, Report *rep);

/* travel */
void game_set_orders(Game *g, Pace p, Rations r, NightStance s, bool sabbath);
void game_travel_day(Game *g, Report *rep);   /* one tick */
void game_rest_day(Game *g, Report *rep);

/* dialog / events / landmarks */
bool game_dialog_view(const Game *g, DialogView *out);
void game_choose(Game *g, int choice, Report *rep);

/* barter, river, hunt */
bool game_barter_view(const Game *g, BarterView *out);
void game_barter_offer(Game *g, const BarterOffer *offer, Report *rep);
bool game_river_view(const Game *g, RiverView *out);
void game_river_choose(Game *g, RiverMethod m, Report *rep);
bool game_hunt_view(const Game *g, HuntView *out);
void game_hunt_act(Game *g, HuntAction a, Report *rep);

/* save */
bool game_save_json(const Game *g, char **out_json);
Game *game_load_json(const char *json, const GameData *data);
```

`GameData` is everything loaded from `data/` once at start-up; `Game` is the mutable run. The UI never changes `Game` directly.

### trekbot

Links the core only. Plays N runs per side and family with simple policies (`random`, `cautious`, `greedy`, `no_hunting`) and prints: arrival rate, days taken, deaths by cause, events seen (and never seen), money left, and relations. Balance targets are **game design goals, not history** (e.g. a cautious Normal run should usually arrive; a reckless one should often not).

### datacheck

Validates JSON schemas, every locale key (in code-referenced lists and data), event graph links, `sources` on every event, item ids in shops and effects, and that `map_xy` and `lat/lon` exist for every node.

---

## 15. Data files

| File | Holds |
|---|---|
| `data/route.json` | nodes, edges, regions, water, map positions |
| `data/regions.json` | grazing by month, event base rate, animal table id, weather table id |
| `data/weather.json` | per region × month weather weights and stickiness |
| `data/items.json` | items: weight, base price, sides, category, barter value |
| `data/wagons.json` | wagon types: capacity, min oxen, speed, sides, price |
| `data/families.json` | backstories, members (fictional), skills, perks, starting goods |
| `data/prices.json` | side × seller-group modifiers, distance modifiers, sell spreads |
| `data/shops.json` | each shop: seller group, keeper (fictional name key), stock |
| `data/barter.json` | partners: group, wants tables, fairness thresholds |
| `data/animals.json` | animals: tier, meat, speed, spook chance, danger, regions |
| `data/rivers.json` | base depth by month, crossing methods available |
| `data/illness.json` | conditions: progression, recovery, nursing effect |
| `data/endings.json` | epilogue sentence rules (relation thresholds, flags) |
| `data/events/*.json` | events (§8) |
| `data/glossary.json` | glossary terms → locale keys + DSAE links |
| `data/locale/en.json` | all player-facing text |

---

## 16. Sound and art (brief)

- **Art:** 640×360 pixel scenes per region and time of day; portraits for named people (drawn, not photographs); Harris plates as reference for animals; a pixel route map. Every asset goes in `CREDITS.md`.
- **Sound (WAV, SDL audio):** wagon creak and ox lowing on travel, whip crack at start of a day, river water, gunshot and misfire click, wind, a short concertina or hymn tune for Sundays and the epilogue (public-domain melodies only).

---

## 17. Open questions for you

1. **Distance:** my rough leg estimate (≈575 mi) is longer than the plan's 450–500. Settle in research pass 2.
2. **Group laager for the English side:** should joining be possible from the start with good standing, or only after a story event earns it?
3. **Show odds** toggle: keep it (off by default), or leave numbers out entirely?
4. **Graves from earlier runs:** fun for families sharing one computer; keep it?
5. **Difficulty names:** "Easy / Normal / Hard", or something in period voice?
6. **Money display:** should the Dutch side see rixdollars and skillings, or just whole rixdollars to keep it simple for children?
