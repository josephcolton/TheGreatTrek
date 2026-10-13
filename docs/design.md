# The Great Trek: Design

How the game works, as built: the decisions behind it, every system, the data that drives it and the code that runs it. It is written so that someone who wanted to make a similar game, about this trek or another journey, could do it from this document. The history it rests on is in `research.md`, which this document cites by section.

**About the numbers.** Unless a number is marked as sourced, it is a tuning value chosen for play. Every one lives in `data/`, and each says whether it comes from a source or is invented.

---

## 1. What the game is

**The Great Trek / Die Groot Trek** is an Oregon-Trail-style survival game about the 1830s Great Trek in South Africa. A family leaves Grahamstown in April 1837 with an ox wagon, their servants and their herds, and tries to bring everyone alive to the end of the road: the Mooi River on the Highveld, the camps in Natal or Port Natal, or the Soutpansberg or Delagoa Bay in the far north.

- **Audience:** adults who grew up on Oregon Trail. Death and disease are honest; violence is told in text and kept non-graphic.
- **Length:** a full trip takes 30–60 minutes and about 100–110 days of game time on the main road.
- **Platform:** a compiled game in C17 with SDL3, one exe with no extra DLLs. Windows first; the code is portable to Linux and macOS.
- **Licence:** GPL-3.0. The game text and data use the same licence.

---

## 2. Design pillars

1. **Oregon Trail's loop, South African substance.** Travel, consume supplies, face events, decide, then arrive or die. The mechanics come from documented realities: oxen, the laager, drifts, fever and the fly, barter, flintlocks.
2. **Your side matters.** Dutch and English players see different prices, wagons, welcomes and dangers on the same road.
3. **Every event has a source,** or is clearly labelled as invented.
4. **People, not obstacles.** The Barolong, Griqua, Basotho, Ndebele, Pedi, Venda and Tsonga, the Khoikhoi and formerly enslaved servants, the missionaries and British officials are named actors with their own reasons. Servants are party members with names and skills.
5. **Honest, not graphic.** Deaths are told calmly; the player never plays through a massacre or fights for sport.
6. **Teach by being right.** The odds are hidden but follow history where sources allow, and every place, price and date can be traced to `research.md`.
7. **Text kept separate from code,** so a Dutch or Afrikaans translation needs no code changes.

---

## 3. Settled decisions

| Question | Decision |
|---|---|
| Whose story? | **One trek north.** Everyone starts at the same place, Grahamstown in April 1837, with the English settlers giving a Dutch Bible to the departing Uys party (`research.md` section 11). |
| Dutch or English? | **The side you play,** not the interface language. It changes the wagons and goods on offer, starting money and herds, prices, welcomes and some events. |
| The families | All playable families and their people are **fictional**, each with a backstory written over real history. Historical public figures (Retief, Uys, Potgieter, Moroka, Mzilikazi, Harris and others) appear as themselves. |
| The roads | Three: the main road to the **Mooi River**; **Natal**, forking at the Vet River (goal: the Bloukrans camps or Port Natal); the **Far North**, forking at the Vaal drift (goal: the Soutpansberg or Delagoa Bay). A trip ends on arrival, followed by an epilogue of what really happened there. |
| 1838 | The epilogues say plainly who did not survive, without describing how. The player never plays through the killings of 1838. |
| Interface language | English, with period Dutch words (laager, smous, voorloper, kakebeenwa, drift, sanna) explained in a glossary. |
| Money | Whole rixdollars for the Dutch side, pounds and shillings for the English side, priced as close to 1837 as sources allow. |
| Odds | Not shown (a "Show odds" setting exists, off by default), but realistic. |
| Saves | Permadeath: one save per trip, written after every step, no going back. A finished trip is scored into the high scores and its save deleted. |
| Graves | Only the current party's graves are shown, never graves from other trips. |
| The English and the laager | The English side must earn its place in the trekkers' laager; it is never offered at the start. |

---

## 4. The two sides

| | **Dutch (Voortrekker family)** | **English (settler or trader family)** |
|---|---|---|
| Why they go | Grievances against British rule, frontier-war losses, land, independence | Trade with the trekkers, hunting, a mission, a fresh start after the war |
| Starting wealth | Little money (80–250 Rds), **large herds** (up to 20 cows and 160 sheep) | **More money** (£25–£80), small herds |
| Wagons | The kakebeenwa | The kakebeenwa or a heavy transport wagon (the trader); a wagonmaker in Grahamstown trades wagons, including a light Scotch cart |
| Guns | Flintlocks (*sanna*, *roer*) | The same, or a percussion gun with caps, sold only to the English side, which cuts misfires |
| Trekkers (relation) | Welcome (+30): join the laager at the Sand River, fair prices | Wary (−10): must earn trust before they are let into the laager |
| British officials, colonial merchants, missionaries, Port Natal traders | Cool to neutral | Warmer, and cheaper to buy from |

Each group's starting relation and price modifier for each side is in `data/groups.json`. Prices are `base × side modifier × distance × relation × scarcity × history` (section 11.2).

---

## 5. Families and servants

Five families a side, each with a backstory, a party of 4–8 people and a perk (`data/families.json`; the stories are in `data/locale/en/families.json`; the history behind them is `research.md` section 12).

| Side | Family | Perk |
|---|---|---|
| Dutch | Frontier Farmer (Badenhorst) | big herds, a skilled voorloper |
| Dutch | Blacksmith (Kotzé) | repairs take half the time; mending gains doubled |
| Dutch | Hunter (du Plessis) | hunting yield ×1.4; morale −5 |
| Dutch | Predikant's Household (Marais) | morale +10; little money, few guns |
| Dutch | Widow (Lombard) | morale +5; the largest flock |
| English | 1820 Settlers (Ashworth) | money and colonial goodwill |
| English | Trader (Pascoe) | barter ×1.2; a heavy transport wagon |
| English | Hunter-Traveller (Fenwick) | hunting yield ×1.3; a small party, many horses |
| English | Mission Family (Whitlock) | illness ×0.85; much medicine, an interpreter |
| English | Discharged Soldier (Rennie) | hunting ×1.1; well armed |

- **Party members** have a role (head, spouse, child, sibling, elder, servant), an age group, a pronoun, a birth year, a portrait, and skills from 0 to 5: shooting, herding, voorloper, interpreting, nursing, smithing, tracking.
- **Servants are full members.** They have names, skills and a loyalty from 0 to 100. They are paid about 10 Rds / 15s a month (`research.md` section 7.3); unpaid or badly treated, loyalty falls and they can leave, and the game tells their side of it. The text reflects their different rights in 1837: a Khoikhoi servant free to leave since 1828, a formerly enslaved woman still an "apprentice" until 1838, an Mfengu herder under a harsher contract (`research.md` section 12.4).
- **The party talks.** Events pick a speaker by role or skill ("the voorloper", "whoever herds best"), so the people who did the work are at the centre of the story.
- **Names** can be kept, typed in, or re-rolled from period name lists (`data/names.json`, `research.md` section 13).

---

## 6. A session and its screens

```
Title ─▶ How hard a trek? ─▶ Side ─▶ Family ─▶ Story ─▶ Names ─▶ Opening scene (the Bible gift)
                                                                         │
                                                                         ▼
                                                                  Grahamstown store
                                                                         │
       ┌─────────────────────────────────────────────────────────────────▼─────────┐
       │ TRAVEL (one day per step; quiet days run on until something happens)      │
       │   ├─▶ Event (dialog)              ├─▶ Hunt / guard the herd at night       │
       │   ├─▶ Place ─▶ store / barter / smith / church / talk / settle / turn back │
       │   ├─▶ River crossing              ├─▶ Choosing the road at a fork          │
       │   └─▶ Grave (someone dies) ─▶ back to travel                               │
       └──────────────┬─────────────────────────────────────────────┬──────────────┘
                      ▼                                             ▼
          Arrival / settling / turning back             Stranded / everyone lost
                      ▼                                             ▼
          The family's story ─▶ the history of the place ─▶ "The trek is over" (score; title, credits or quit)
```

**Full screens:** title, your trips, high scores, settings, credits; the new-trip steps (difficulty, side, family, story, names); and the game screen, which shows whatever the core's phase says: travel, dialog, store, barter, hunt, grave or ending.

**Overlays** (opened over the game, closed with Esc): pause menu, supplies (S), party (P), map (M), journal (J), history behind this event (H), trek options, glossary (G), how to play, and Y/N confirmations.

**Which screen shows is decided by the core, not the UI.** After every command the UI asks `game_phase()` and draws the matching screen. Loading a save is just reading the state and showing its phase.

**Layout.** Everything is drawn on a 640×360 logical canvas scaled to the window (`SDL_SetRenderLogicalPresentation`). A status bar shows the date, weather and place; a scene picture fills the top; text and choices sit below. Choices take the number keys 1–9, the arrow keys and Enter, the mouse, or a gamepad; each is a full path through the game.

**Gamepad.** `src/ui/gamepad.c` turns the pad's buttons into the key presses each screen already understands, chosen by what is on screen, so the screens have no gamepad code of their own:

| Button | Does |
|---|---|
| D-pad, left stick | the arrow keys, repeating while held |
| A | Enter: choose, go on; Y to a yes/no question; fire in a hunt |
| B | Esc: back, close, leave; N to a question |
| X | supplies; sell in a store; less in barter; delete a trip or the high scores |
| Y | party; a new name; propose in barter; back to the wagons from a hunt |
| LB / RB | journal / glossary; page up and down in the journal; the other map sheet; ask in barter (LB) |
| LT | the history behind an event |
| RT | fire in a hunt |
| Back / View | the map |
| Start / Menu | the pause menu |

- **Hunting with a stick:** the left stick moves the sight on a curve (slow and fine near the middle, across the field in about a second at full tilt), and the sight slows to 40% over an animal still on its feet, so a pad can hold on a moving target. It never snaps on. The shot itself (its scatter, loading, misfires) is the same however the sight got there, so the odds and the balance bot are unchanged. A shot gives a short rumble.
- **Hints follow the device.** Every footer and help text that names keys has a `.pad` twin in the locale (`"store.footer.pad": "... [A]: buy  [X]: sell  [B]: leave"`); `[A]`, `[LB]`, `[START]` ... are filled with the names printed on the pad in use (A/B/X/Y, Cross/Circle/Square/Triangle, or the Nintendo layout; LB or L1 or L). The hints switch as soon as the pad is used, and back when a key or the mouse is.
- **Typing** (a new name, an epitaph) still needs a keyboard; a pad player can keep the names, re-roll them with Y, and leave the epitaph blank.

---

## 7. Text, dialog and translation

### 7.1 Every string is a locale key

- No player-facing text in C. Every string is a key in `data/locale/en/*.json`.
- **Lookup order,** so text can differ by side or family without code: `key@<side>.<family>` → `key@<side>` → `key`.
- **Placeholders:** `{name}` style, filled from arguments the core supplies. People: `{who}` gives a name, and `{who.he}`, `{who.him}`, `{who.his}`, `{who.hers}`, `{who.himself}` give the person's pronoun; a capital (`{who.He}`) capitalises it. The pronoun words come from `pronoun.*` keys, so a translation supplies its own. Plurals: `{n:ox|oxen}`. Money is formatted by side.
- `datacheck` checks that every key the code and data use exists, and rejects unknown placeholder forms.
- `--check-text` lays out every text the game can show and lists any that run off the screen, including each footer hint with the button names of each kind of pad.

### 7.2 The dialog model

An event is a small graph of nodes. A simple event has one node; a set piece (the Bible gift, Moroka, the Sand River laager) has several.

```
Node:    speaker, picture, portrait, text, choices (up to 6)
Choice:  text, requires (shown greyed with a reason if unmet), outcomes
Outcome: weight (+ skill bonus), text, effects, next node (or end)
```

- **Locked choices are shown, greyed, with a reason** ("Only the trekkers' own families are asked in", "Needs an interpreter"). This is how the player sees that their side matters.
- **Odds are not shown.** Choice text hints at risk in words ("risky", "slow but safe"). The "Show odds" setting reveals them.
- **The outcome is rolled and saved before it is shown** (section 19.2).
- The core hands the UI a plain `DialogView` (title, speaker, body lines, choices, picture, portrait, history key); the UI only draws it.
- **History notes:** every event can carry a "history behind this" page (H), saying plainly what is sourced and what is invented, with sources.
- **The journal** keeps a line for everything that happened, by date.

---

## 8. Time, calendar and weather

- **One step is one day.** The date is a day count from 1 Jan 1837 (a Sunday); weekdays and months are computed. The trip starts on Monday 3 April 1837.
- **Seasons** (southern hemisphere) and **weather** per region and season: clear, hot, windy, cold, frost, rain, storm (`regions.json`). Weather changes travel, oxen, illness, misfires and river depth.
- **Sundays:** a trek option, "Rest on Sundays", on by default. It costs a day a week and lifts morale; the trekkers expect it. How strictly parties kept it varied (`research.md` section 10.1).
- **The day, in a fixed order** (for determinism): orders → weather → travel (distance from pace, oxen, terrain, weather, wagon, voorloper and load) → food and water → oxen stamina and grazing → illness, recovery and contagion → morale and loyalty → a daytime event check → the night stance and a night event check → deaths, stranding, arrival.
- **Travel on** runs days until something needs the player: an event, a place, a river, a death, or a **warning** (food under 7 days, water under 2 days). Any key stops the wagon.
- **Other actions:** rest a day; mend the wagon (a day); hunt (half a day, once a day).

---

## 9. The route and the map

### 9.1 The route graph

The route is a graph in `data/route.json`, not a line: 25 places and the roads between them. Each **place** has a kind (town, river, mission, landmark, journey's end), a real latitude and longitude, its shop, river, set piece, extra actions, a price modifier, a history page and, for a journey's end, an epilogue. Each **road** has miles, a region, terrain (speed and roughness), water (streams, springs or dry), and for a fork a label, a line of advice and a route flag.

```
                 ┌─ Cradock ─ Colesberg ─ Orange drift ─┬───────────── Thaba Nchu ─ Vet R. ─┬─ Sand R. ─ Vaal drift ─┬─ Mooi River
 Grahamstown ────┤      (western road)                  └─ Philippolis ┘                   │                        └─ Magaliesberg ─ … ─ Soutpansberg ─ … ─ Delagoa Bay
                 └─ Tarka ─ Stormberg ─ Buffelsvlei ─ Caledon drift ─ Thaba Nchu            └─ Kerkenberg ─ Bloukrans ─ Port Natal
                        (eastern road)
```

- **Distances** are straight lines between the real places × 1.25 for the ground (`research.md` sections 5 and 15.2): about 650 miles on the main road. **Calibrated on history:** normal pace gives ~6–7 miles a day with Sundays and stops, as the Uys party managed in 1837 (`research.md` section 5.2), so the trip reaches the Highveld in its winter.
- **Forks:** arriving at a place with more than one road out opens a road choice. Each option shows its distance and a line of advice from someone who knows it, drawn from the real warnings of 1837 (`research.md` section 18).
  - **Grahamstown:** the western road by Cradock and Colesberg (towns, stores, the dry Karoo) or the eastern road by the Tarka and the Stormberg (shorter, mountain cold, few stores; the Orange at Buffelsvlei, where Retief's party crossed).
  - **The Orange drift:** straight to Thaba Nchu, or by Philippolis (Griqua trade and news).
  - **The Vet River:** on north, or east to Natal (the Vet River vote of 6 June 1837).
  - **The Vaal drift:** to the Mooi River, or the Far North (Tregardt's road).
- **Journey's ends that are also stops:** at Bloukrans and the Soutpansberg the set piece asks whether to stop there (the trip ends) or go on. At the Soutpansberg the party can also **wait for the dry season**, as the Tsonga traders did.
- **Known hold-ups** (the Drakensberg descents) carry `delay_days`, so the game can say how long a road really takes.

### 9.2 The map

- **The trail strip** on the travel screen: the places as a line with the wagon moving along it, and the miles to the next.
- **The map** (M, also from the ending): a parchment map on two sheets, the Colony to the Vaal, and Natal and the Far North. Places sit at their real latitude and longitude, projected by `data/map.json`, stretched east to west so close places stay readable (the map says so). Rivers, the coast, the sea and the mountain ranges are drawn from simplified courses. Each kind of place has its own mark; places not yet near show only a faded mark. Roads travelled are red, known roads ink, unseen roads dotted. Each grave of this party is a small cross.
- **Peoples are labelled softly across areas,** with no hard borders: land claims were contested, and the map shouldn't draw lines that didn't exist.

---

## 10. River crossings

The Orange, Caledon, Vet and Vaal are crossings (`data/rivers.json`). Arriving at a drift, the party reads the river: its depth comes from a typical depth for the month plus a random spread, and is described in words (low and clear, waist deep, running deep, in flood).

| Choice | Effect |
|---|---|
| Drive the wagon straight across | fast; the risk rises steeply with depth; impossible above the ford limit |
| Caulk the wagon with tallow and float it over | needs tallow and enough water; slower, risk of tipping |
| Carry the goods over first, then the empty wagon | a day; protects the goods |
| Wait a day for the river to drop | a day of food; the depth is read again |

Troubles: goods lost to the water, oxen swept off, the wagon damaged, or a person swept away, saved or not by a named helper.

---

## 11. Money, stores and barter

### 11.1 Currency

Each side sees prices only in its own money, in whole units: **rixdollars** for the Dutch side, **pounds and shillings** for the English side (no pence). Base prices in `data/items.json` are in pence, exact to the source, with a `source` line each; the shop converts the final price to the side's unit and rounds it. From 1825, 1 Rd = 1s 6d, so £1 ≈ 13⅓ Rds (`research.md` section 4). Meal and grain are sold by the **muid** (about 180 lb), the unit of the time.

### 11.2 Prices

```
price = base_price[item]
      × side_modifier[seller_group][side]
      × place price modifier (distance)
      × (1 − relation with the seller's group / 400)
      × scarcity (the price rises as the shop's stock is bought out)
      × the strongest price shock for this item, place and date
```

Selling pays the price × the group's sell spread (0.4–0.6). Starting prices for Grahamstown come from the Graham's Town market register, Harris and Backhouse (`research.md` section 7.4); goods with no period price found are estimates and say so.

### 11.3 Prices moved by history

`data/price_shocks.json` holds price changes from real events: a date window, the places and goods, a multiplier that fades over time, the line a seller uses to explain it, and a history page. Built: the frontier war's livestock prices around Grahamstown, oxen scarce after Vegkop, and grain dear on the frontier (`research.md` section 8). **Sellers use the news:** when a shock applies, the shopkeeper says why ("Since the war, an ox costs what a horse did"), and H explains the real event.

### 11.4 The store

- Items are grouped: animals, food, water, guns and shot, wagon and parts, trade goods, medicine. Each side's catalogue is filtered (`sides`).
- The store shows money, the wagon's load and capacity, and under the food about how many days each journey's end is from here.
- **The keeper gives one line of advice** about the worst gap in your supplies (no spare wheel, too few oxen, no water vats before the Karoo, not enough food to reach the next place): the Oregon Trail "Matt's General Store" lesson.
- **Smiths and wagonmakers:** the Grahamstown wagonmaker and the smiths at Cradock and Colesberg quote a price for putting every part right and do it in half a day; the wagonmaker also trades wagons (the old one is worth 80% of its new price). There are no smiths north of the Orange.

### 11.5 Barter beyond the Orange

North of the Orange coin is worth little and **trade goods are money** (`research.md` section 7.6). Seven named partners, each at a place: the Griqua at Philippolis, the Barolong at Thaba Nchu, the Basotho at the Caledon, the trekkers at the Sand River, the Pedi at Strydpoort, the Venda at the Soutpansberg and the Tsonga traders in the lowveld (`data/barter.json`).

```
┌ TRADING WITH THE BAROLONG AT THABA NCHU ─────────────────────────────────┐
│  You offer                         │  They offer                         │
│  ▸ Tobacco      4 lb               │  ▸ Oxen          2                  │
│    Beads        2 strings          │    Sorghum       100 lb             │
│  Balance: ███████░░░  "They think this is a little short."               │
└──────────────────────────────────────────────────────────────────────────┘
```

- Each partner has a **wants table** (what each good is worth to them) and a stock of offers. A balance meter compares the two sides; they accept when what you give is worth at least their **fairness** share of what you take.
- "Ask what they need" is free and shows what they want most. The trader family barters better.
- **Gunpowder is contraband** beyond the frontier: the most valuable thing to trade and a crime under colonial law, as Harris found. The barter screen says so when it is in the offer.

---

## 12. Events

About 100 events in `data/events/*.json`, with their text in `data/locale/en/`.

### 12.1 The format

A real one, from `data/events/road.json`:

```json
{
  "id": "karoo_stagnant_water", "regions": ["karoo"], "weight": 12, "cooldown_days": 15, "harmful": true,
  "requires": [ { "edge_water": "dry" } ],
  "weight_mods": [ { "if": { "weather": "hot" }, "mul": 1.5 } ],
  "speaker": { "pick": "skill", "skill": "voorloper" },
  "history": "history.karoo_water",
  "sources": ["invented. Dysentery as a major killer is plausible but not sourced (research.md section 10.1)."],
  "text": "event.karoo_stagnant_water.body",
  "choices": [
    { "text": "event.karoo_stagnant_water.drink",
      "outcomes": [
        { "weight": 65, "text": "event.karoo_stagnant_water.drink.ok", "effects": [ { "item": "water", "add": 40 } ] },
        { "weight": 35, "text": "event.karoo_stagnant_water.drink.sick",
          "effects": [ { "item": "water", "add": 40 }, { "illness": "flux", "who": "random", "count": 2 } ] } ] },
    { "text": "event.karoo_stagnant_water.boil",
      "outcomes": [ { "text": "event.karoo_stagnant_water.boil.ok", "effects": [ { "item": "water", "add": 40 }, { "time_days": 0.5 } ] } ] },
    { "text": "event.karoo_stagnant_water.ask", "requires": [ { "member_skill_gte": ["voorloper", 2] } ], "locked": "lock.need_voorloper",
      "outcomes": [
        { "weight": 60, "skill": "voorloper", "per_point": 8, "text": "event.karoo_stagnant_water.ask.spring",
          "effects": [ { "item": "water", "add": 60 }, { "morale": 3 } ] },
        { "weight": 40, "text": "event.karoo_stagnant_water.ask.none", "effects": [ { "time_days": 0.5 } ] } ] }
  ]
}
```

Other fields: `sides` and `months` (any if left out), `"time": "night"`, `once`, `setpiece` (played only by a place). Multi-node events add a `nodes` list; an outcome's `next` names the next node. A node can name someone met on the road (`speaker_key`, `portrait`). **Every event must have `sources`**, citing `research.md` or another source, or saying `invented`.

### 12.2 Conditions and effects: a fixed vocabulary

The vocabulary is fixed and checked; there is no scripting language. `parse_cond` and `parse_effect` in `src/core/data.c` are the vocabulary as built, and `datacheck` rejects anything else.

| Conditions (`requires`, all must hold; `any` = or, `not` = negate) | Effects |
|---|---|
| `side`, `family` | `item` add or remove (food, powder, lead, parts, trade goods, animals) |
| `region`, `node`, `edge_water`, `month`, `season`, `weather`, `date_gte`, `date_lt` | `money` |
| `flag`, `not_flag`, `counter_gte` | `ox_stamina`, `wagon_damage` (part, amount) |
| `item_gte`, `item_lt`, `money_gte` | `health`, `illness` (who: random, all, role, speaker, head, child) |
| `member_role`, `member_role_healthy`, `member_skill_gte` | `morale`, `loyalty`, `relation` (group, amount) |
| `relation_gte`, `relation_lt` (group, value) | `flag`, `clear_flag`, `counter` |
| `stance` (tonight's night stance) | `time_days`, `distance` (miles lost or gained) |
| `days_gte`, `distance_gte`, `chance` | `schedule` (an event in N days: chains and follow-ups) |
| `weekday` (0 = Sunday), `sabbath` | `death` (rare, always with its own text), `leave` (a servant goes) |
| `wagon`, `afford_wagon`, `wagon_worn`, `afford_repair` | `journal`, `ending`, `wagon` (trade it in), `smith` (quote or repair) |
| | `defend` (guard the herd against a predator), `wait_until` (a date: wait out the rains) |

### 12.3 How events are picked

1. **Scheduled events** (from `schedule` effects) fire on their day first.
2. **Does anything happen?** A base rate (about one event every four days) × the region's event rate × the night stance for night events.
3. **Which event?** Every event whose sides, regions, months and conditions match, and which isn't on cooldown or used up (`once`), is weighted by `weight × weight_mods`. The game's random stream picks one.
4. **Set pieces** are not random: each belongs to a place and plays on the first arrival.
5. **Chains** run through flags: a choice sets a flag, and a later event requires it.

### 12.4 What the events are about

| Kind | Examples |
|---|---|
| Land and water | a stagnant pool, a dry stretch, a mirage, a lost track, quicksand |
| Wagon | a broken wheel, axle or disselboom, an upset wagon, a torn tent |
| Livestock | oxen stray, an ox lies down, horses go home, a storm stampede, horse-sickness |
| Health | the flux, fever, a chest illness, snakebite, cold nights, a birth on the road |
| Wildlife and the night | lions, hyenas and wild dogs at the herd (guard it, section 14.2), game passing |
| People | a smous, the Griqua, Barolong herders, Basotho traders, missionaries, messengers with news, trekker families |
| The party | a servant asks for wages or leaves; a Sunday service; a child wanders off |
| History and news | Vegkop survivors, Retief's manifesto, Mosega, the Vet River council, Tregardt's people, the Van Rensburgs |
| Side-only | Dutch: the council, the laager. English: "Are you the Governor's man?", earning the trekkers' trust |
| Places | church in a town or a laager, the smith, settling at Thaba Nchu, turning back at the Orange |

Many come straight from the travellers on the same road (`research.md` section 14).

---

## 13. Realistic odds and difficulty

The game is meant to teach, so **chances match history where history gives a number**, and are honest guesses where it doesn't.

- **Rates live in data,** not scattered through events: illness per day by season (`rules.json`), each illness's damage, recovery, contagion and the extra risk to children (`illness.json`), malaria and the fly by region and month (`regions.json`), breakage per mile, night risk by stance. Every rate carries a `basis`: a source with the working, or `invented` with a reason.
- **Converting a documented rate to a daily chance:** `p_day = 1 − (1 − p_period)^(1 / days_in_period)`, so a whole trip matches the documented figure (`research.md` section 9).
- **Difficulty** is the first step of a new trip and fixed for it:
  - **Normal** uses the history-based rates as they are.
  - **Easy** halves the harm: harmful events come half as often, and injuries, breakages, losses and illness damage are halved. It gives a quarter more money and warns before anyone dies. The difficulty screen says Easy is kinder than history was.
  - **Hard** uses the same rates as Normal, with a quarter less money and no supply warnings.
- **The balance bot checks history.** `trekbot` plays hundreds of trips per family and reports arrivals, days, deaths by cause and events seen. On Normal, on the main road, a family usually loses no one or one person in ~100 days. The Delagoa Bay road is meant to be the deadly one, as it was:

| Goal | Arrive | Deaths per trip (of about 8) | Main killers |
|---|---|---|---|
| Mooi River | 100% | ~0.1 | flux, hardship |
| Bloukrans camps | 100% | ~0.2 | flux, hardship |
| Port Natal | 100% | ~0.2 | flux, hardship |
| Soutpansberg | 86–100% | 0.2–1.4 | flux; a big family with no hunter runs short of food |
| Delagoa Bay in the rains | 0–88% by family | ~2.7–8 | **malaria**, hunger, stranded by the fly |
| Delagoa Bay after waiting for the dry season | 0–87% by family | ~0.4–7 | hunger; much less malaria |

---

## 14. Hunting and guarding the herd

### 14.1 The hunt

Hunting plays like the Oregon Trail hunt: watch the game come into sight, aim and click to fire.

- **Starting:** from the travel menu (half a day, once a day) or an event. It needs a gun, powder and lead, and a member who can shoot; the best shot goes. The game present comes from `data/animals.json` by region.
- **The field** is a side view: game near the horizon is small and far, near game big. Animals come in from either side, singly or in herds, walk, graze or run.
- **Firing:** left click (or Space; the arrow keys move the sight). The ball lands near the sight, not on it: the scatter shrinks with shooting skill and the family's hunting perk. The nearest animal under the ball is hit.
- **The flintlock:** loading takes a few seconds (far less than the real 20 seconds or more; shortened for play); a misfire needs the pan primed again. Misfires by weather: about 2% when clear, 60% in rain **[invented]**; a percussion gun with caps cuts that to 15% of it. The bang spooks nearby game.
- **Big game** takes more than one hit (eland 2, buffalo 3). A wounded buffalo may charge; a lion may stalk closer and spring. Shoot it first, or the hunter is hurt or killed, told in text.
- **Daylight** lasts 45 seconds; Enter goes back any time.
- **Meat:** only what can be carried comes back (more with horses), and the summary says how much was left. Each shot uses powder and lead. Hunting a region out lowers its game for the rest of the trip.
- **Animals** (Harris's road, 1836–37): springbok, blesbok, black wildebeest (gnu), red hartebeest, quagga, ostrich, kudu, eland, buffalo, lion. The quagga was on this road in the 1830s (`research.md` section 10.1). No blood: an animal falls or runs off.
- **Determinism:** the hunt runs in the core in fixed ticks (30 a second) on the game's random stream, so the same seed and the same shots give the same hunt.

### 14.2 Guarding the herd at night

Night raids by hyenas, lions or wild dogs offer **"Take a gun out to the herd and stand guard"**, which opens the hunting scene at night.

- The camp, the wagons, the fire and the stock are along the bottom. Predators come out of the dark from either side; far off, only their eyes show in the firelight.
- Each creeps toward a beast, then rushes in. One that gets through carries a beast off (hyenas and wild dogs take sheep, lions oxen). **If it is hit on the way out, it drops it.**
- A lion takes two hits, and a wounded lion may turn on the guard. A shot close by may scare raiders off.
- Raid sizes (hyenas 4–7, lions 2–3, wild dogs 6–9) and the 40-second watch are in `animals.json` **[invented]**. Going back to the fire leaves the herd: about half of those still coming get a beast.

---

## 15. Night: rest, laager or guard

The night stance is a trek option, so the player doesn't answer it every evening.

| Stance | Cost | Night risk |
|---|---|---|
| **Rest** | none; full recovery | ×1.0 |
| **Form a laager** | half a mile of the day's travel, a little ox stamina | ×0.35 (Vegkop, 1836: two trekkers died behind the wagons) |
| **Guard the herd** | a little morale | ×0.6 |

**Joining the trekkers' laager:** at the Sand River the united laager takes the Dutch family in. The English family is asked in only if it has earned the trekkers' trust (relation 20 or more), through choices on the road such as helping a trekker family, sharing, and answering honestly when asked whose side they are on. Once joined, the laager is safer still, and some events follow from it.

---

## 16. Places, party, health, livestock and the wagon

### 16.1 Places

Every place has a **set piece** on the first arrival, plus actions (`route.json`):

| Place | Set piece | Actions |
|---|---|---|
| Grahamstown | the Bible gift to the Uys party, from your side | store, talk, wagonmaker, church |
| Cradock, Colesberg | the last colonial towns; news; at Colesberg, the edge of the Colony and the Punishment Act as legal pressure, not a checkpoint | store, smith, church |
| Orange drift, Buffelsvlei | the Groot Rivier: a crossing; at Buffelsvlei, news of Retief's party | cross, turn back to the Colony |
| Philippolis | the Griqua town | traders, barter |
| Stormberg | the pass: cold nights and snow in winter | |
| Thaba Nchu | Moroka and the Barolong; the Wesleyan mission; Vegkop's survivors | mission store, barter, talk, church, settle here |
| Vet River | the council and the vote for Natal; the fork | cross |
| Sand River | the united laager | join (section 15), the laager's store, barter, church |
| Vaal drift | news of Mosega and the commandos; the fork | cross |
| Kerkenberg, the escarpment | the Drakensberg descents (wheels off, branches as a brake; weeks of road-making) | the laager's store at Kerkenberg |
| Magaliesberg, Strydpoort, the Olifants | the Ndebele country; Sekwati's warning | barter at Strydpoort |
| Soutpansberg | Tregardt's people, the Venda, the Tsonga traders | barter, talk, wait for the dry season, stay or go on |
| Lowveld, Komati | the fly; the Portuguese at the Komati | barter with the Tsonga |
| Bloukrans | the camps along the rivers | stay or go on to Port Natal |
| Journey's ends | arrival; the trip ends and the epilogue follows | |

Towns each have their own picture (section 20.1).

### 16.2 Health

- Each member has **health** (0–100) and up to three conditions at once. Illnesses (`illness.json`): the flux (dysentery), fever, a chest illness, exposure, injury, snakebite, and **malaria**. Each does damage a day, has a daily chance of recovery helped by the party's nursing, can pass to others, and may hit children harder.
- **Malaria is never a random illness:** it is caught from the country, by the region's fever rate for the month, high in the lowveld in the rains (`research.md` section 19). The party calls it "the fever"; a death and a grave name it as malaria.
- **The fly** (nagana) kills a share of the oxen, cattle and horses a day in the lowveld bush, so a party can be stranded, as the Pretorius party was.
- **Medicine** is given a dose a day while an illness lasts: to anyone who falls ill, or by hand from the party screen. It eases but doesn't cure; there is no quinine (`research.md` section 19).
- **Rations:** full, meagre or starving (2, 1.5 or 1 lb a day for an adult; children 0.6 of that). Hunger costs health and morale.
- **Food** is eaten in order: fresh meat first, then the stores. The flock can be eaten (a sheep whenever the meat runs out, as the trekkers did) or kept.
- **Morale** (0–100) rises with rest, Sundays, coffee and sugar and good news, and falls with deaths, hunger, a hard pace and quarrels. Low morale lowers servants' loyalty.

### 16.3 Oxen, herds and the wagon

- **Oxen:** a count and a shared stamina (0–100). Each wagon needs a minimum span and goes best with a full one (kakebeenwa 10 and 14; transport wagon 14 and 18; Scotch cart 4 and 6). Stamina rises with grazing (by region and month) and rest, and falls with a hard pace, a heavy load and no water. Below 30 the span slows; below 10 an ox may lie down and not rise. Below the minimum the wagon can't move and the party is stranded.
- **Loose stock** (cows, sheep, horses) are food and barter goods, can stray or be taken, and horses carry meat back from a hunt.
- **The wagon** has four parts (wheels, axle, disselboom, tent), each 0–100%. Rough roads, a hard pace, overloading, river crossings and some events wear them; storms tear the tent. When a wheel, axle or disselboom breaks, the wagon stops: fit a spare, mend it with iron (better with a good smith), or rig something.
- **Mending on the road** (a day): a badly worn part gets the spare if there is one; others gain with the best smith's skill and bar iron, but never past 85% (field mending never makes a part new). Tallow greases the wheels and axle; canvas renews the tent.
- **The voorloper:** without a healthy one, the wagon goes slower and breaks more.

---

## 17. Death and graves

- Deaths come from illness, accidents, crossings, the night and (rarely) events, at the rates in section 13.
- **The grave screen:** name, birth year, date and place of death, and a typed epitaph. Beside the stone: who they were, what they died of, and how it came about, so a sudden death is never a mystery.
- **Long waits** (weeks of road-making, waiting out the rains) end with a summary: days passed, illnesses, recoveries, deaths and animals lost.
- **Only this party's graves are shown:** as crosses on the map, and by the trail if the road passes the spot again. Graves from other trips never appear.
- Violence is never shown; deaths are told in calm text.

---

## 18. Relations, endings and the score

### 18.1 Relations

Each group has a relation from −100 to +100 (`data/groups.json`): the trekkers, the British, colonial merchants, missionaries, the Barolong, Griqua, Basotho, Ndebele, smouse, Pedi, Venda, Tsonga, the Portuguese and the Port Natal traders. Starting values differ by side. Relations change prices, unlock or lock choices (the English and the laager), and appear in the epilogue. They are never shown as numbers.

### 18.2 Endings

| Ending | When |
|---|---|
| **Arrived** | reached a journey's end with someone still in the party: the Mooi River, Port Natal or Delagoa Bay, or chose to stay at the Bloukrans camps or the Soutpansberg, where most of the real parties stopped |
| **Settled** | chose to stop short, at Thaba Nchu with Moroka's people |
| **Turned back** | went back to the Colony from the Orange |
| **Stranded** | no oxen to move the wagon, and no way to get more |
| **Lost** | everyone has died or gone |

### 18.3 The end of a trek

The ending has three pages:
1. **The family's story:** who came through, what the herds and wagon came to, a line for each group whose relation is notably good or bad ("The Barolong at Thaba Nchu remember the Ashworths as fair traders"), and the key choices.
2. **The history:** what really happened at that place afterwards (Retief, Weenen and Blood River for Natal; the fever at Delagoa Bay; the founding of Potchefstroom), told plainly and without detail, from both sides.
3. **"The trek is over":** the score, and back to the title, the credits, or quit. There is no "save and quit" once a trek is over.

### 18.4 The score

Mostly the people brought through (250 + their health each), then the road covered (1 a mile), so a longer trek counts for more but never more than the lives it costs. Herds, money and good relations add a little. Settling short counts ¾; stranded, turned back or lost counts ½; Easy ×½, Hard ×1½. **High scores** keep the ten best (`scores.json` in the save folder) and can all be deleted after a Y/N question.

---

## 19. Menus, saves and settings

### 19.1 Main menu

Continue a trip · New trip · High scores · Settings · Credits and sources · Quit. Continue opens **Your trips**, newest first; D deletes one after a Y/N question.

### 19.2 Saves and permadeath

- **One save per trip:** `trips/<trip_id>.json` in `SDL_GetPrefPath("TheGreatTrek", "TheGreatTrek")`. No slots, no "save as", no loading an earlier point. Several trips can exist at once, one per player.
- **The game saves after every step** that changes the trip: each day, choice, crossing, purchase and hunt. The file always holds *now*.
- **The outcome is saved before it is shown,** and the random state is part of the save, so quitting and coming back gives the same result.
- **Crash safety:** the save is written to a `.tmp` file and renamed over the real one, so a crash costs at most one step.
- **Finished trips** are scored into the high scores and their saves deleted, so the trip list holds only trips that can go on.
- Old save versions are refused with a clear message.

### 19.3 Settings

`settings.json`, separate from the trips: show the odds (off by default), full screen (also F11), travel speed (slow, normal, fast), sound effects volume and music volume (off to 100% in steps of 10, on a squared curve so low settings are really quiet), and Reset to defaults. The last difficulty chosen is remembered and offered first.

---

## 20. Art, sound and music

### 20.1 Art

- **Pixel art on a 640×360 canvas.** Sprites are text files in `assets/sprites/` (a palette of one character per colour, then rows of pixels; `.` is transparent), loaded by `src/ui/sprites.c`. They can be edited in any text editor.
- **Sprites:** wagons (kakebeenwa, transport wagon, Scotch cart, with turning wheels), red and black oxen in yoked pairs with walking legs, the voorloper; Cape Dutch and settler houses, churches, Karoo houses, Griqua clay houses, reed-mat houses, round houses, kraals; thorn trees, acacias, bushes, aloes, willows, koppies; the camp and the fire; twelve animals (walk, graze, fallen).
- **Scenes are drawn in layers in code** (`src/ui/scene.c`): sky by time and weather, hills, the plants of each region (Karoo mesas, frontier thorn and aloes, Highveld grass), the road, the wagon and span. Near things slide past faster than far ones. Night tints everything blue except the fire. Each town has its own picture, drawn to suggest the place, not to record its 1837 buildings.
- **The title screen** is the highveld with acacias and a wagon and span crossing it.
- **Portraits:** every family member has a head-and-shoulders portrait and a small face, dressed by side and role (kappies and broad hats for the trekkers, top hats and poke bonnets for the settlers, headcloths and old hats for the servants). People met on the road (Moroka, an old trekker) have their own. Moroka's is an impression, not a likeness.
- **The icon** is an ox wagon on Karoo brown (`tools/make_icon.py`).

### 20.2 Sound effects

Synthesised at start-up in `src/ui/sound.c`, so there are no audio files to license: a click, a gunshot, a misfire, a whip crack, water, an ox lowing, a bell.

### 20.3 Music

Four public-domain tunes in a 1990s tracker style (`research.md` section 20), synthesised by `src/ui/music.c` from note lists in `data/music.json`: the melody a 25% pulse wave with vibrato, the inner parts soft square waves, the bass a triangle wave, with a short echo. A tune is rendered the first time it is needed and loops with a short rest; tunes fade into each other.

| Tune | Where |
|---|---|
| Old Hundredth (Genevan Psalter, c. 1551) | title, church, the end of a trek |
| Tallis' Canon (c. 1567) | nights in camp |
| St Anne (Croft, 1708) | at a grave |
| O Waly Waly (traditional) | towns, stores, the mission |

The road itself stays quiet. The tunes come from Open Hymnal ABC files in `tools/music/`, turned into `music.json` by `tools/abc_to_music.py`.

---

## 21. Architecture and code

### 21.1 Stack

- **C17, CMake ≥ 3.24.** CMake fetches **SDL3**, **SDL3_ttf** (with its own FreeType) and **SDL3_image** with `FetchContent`, pinned to release tags and built static, so the game is one exe. System SDL3 packages can be used instead.
- **Data files are JSON,** read with a vendored **cJSON** (MIT) in `third_party/`.
- **Font:** VT323 (SIL OFL), a pixel-friendly font.
- **Warnings on** (`-Wall -Wextra` / `/W4`); no compiler extensions in the core.

### 21.2 The core never touches SDL

```
src/core/   the simulation, pure C: data loading, calendar, travel, events, stores, barter,
            hunting, places, saves, locale lookup, text formatting. No SDL headers.
src/ui/     the SDL front end: screens, drawing and text layout, scenes, sprites, sound,
            music, trips and high scores, settings.
tools/      trekbot and datacheck link only the core; Python scripts make the icon and the music.
tests/      CTest unit tests of the core.
```

Keeping SDL out of the core means the balance bot and the tests run without a window, and the simulation could have another front end.

### 21.3 Determinism

- **One PCG32 stream** in the `Game` struct, passed explicitly to every system. **Never `rand()`,** never hidden global random state.
- A trip replays exactly from its seed and the player's commands. Hunts run in fixed ticks on the same stream.
- The random state is saved with the trip.

### 21.4 The core API

The core is driven only by commands; it never waits for input. After each command the UI asks for the phase and a view to draw.

```c
Game *game_new(const GameData *data, uint64_t seed, int family, Difficulty diff);
GamePhase game_phase(const Game *g);            /* dialog, store, travel, grave, ended, hunt, barter */

void game_set_orders(Game *g, Pace, Rations, NightStance, bool sabbath);
int  game_travel(Game *g, int max_days);         /* days until something needs the player */
void game_rest_day(Game *g);
void game_mend_wagon(Game *g);

bool game_dialog_view(const Game *g, DialogView *out);
void game_choose(Game *g, int choice);

bool game_store_view(const Game *g, StoreView *out);
bool game_store_buy(Game *g, int item, int packs);

void game_hunt_start(Game *g);
void game_hunt_tick(Game *g);                    /* 1/30 s */
bool game_hunt_shoot(Game *g, double x, double y);

void game_barter_start(Game *g, int partner);
void game_barter_adjust(Game *g, bool theirs, int row, int delta);
void game_barter_propose(Game *g);

void game_grave_done(Game *g, const char *epitaph);
int  game_epilogue(const Game *g, LocText *out, int max);
int  game_score(const Game *g);

char *game_save_json(const Game *g);
Game *game_load_json(const GameData *data, const char *json, char *err, size_t err_len);
```

`GameData` is everything loaded from `data/` once at start-up; `Game` is the mutable trip. The UI never changes `Game` directly. The core turns a `Game` into JSON and back; the UI does the file writing.

### 21.5 Data files

| File | Holds |
|---|---|
| `route.json` | places, roads, regions, water, forks, set pieces, epilogues, latitude and longitude |
| `map.json` | the two map sheets: projections, rivers, coast, mountains, labels |
| `regions.json` | grazing by month, weather by season, event rate, water, fever and fly by month |
| `rivers.json` | depth by month, spread, ford and float limits |
| `items.json` | items: category, pack, weight, price in pence with its source, food value, sides |
| `wagons.json` | wagons: capacity, spans, speed, fragility, price |
| `families.json` | families: members, skills, portraits, money, wagon, goods, perks |
| `groups.json` | groups: starting relation and price modifier by side, sell spread |
| `shops.json` | each shop: group, keeper, stock |
| `price_shocks.json` | history-driven price changes, with sellers' excuses |
| `barter.json` | partners: place, wants, offers, fairness; contraband goods |
| `animals.json` | animals: tier, meat, speed, hits, herd, spook and charge chances, regions, night raids; hunting rules |
| `illness.json` | illnesses: damage, recovery, nursing, contagion, children, cause of death |
| `rules.json` | tuning: pace, rations, oxen, rest, laager, night risk, event rate, breakage, mending, illness, difficulty, warnings, wages |
| `names.json` | period names by side and for servants |
| `music.json` | tunes as note lists, and which scene plays which |
| `events/*.json` | events (section 12) |
| `locale/en/*.json` | all player-facing text |

### 21.6 Tools and testing

- **datacheck** validates every data file: every locale key, every gamepad hint (a keyboard twin, known buttons, a name for each button on each kind of pad), every event link and condition, `sources` on every event (the loader refuses an event without them), the music, and reports how many prices come from a period source and how many are estimates.
- **trekbot** plays N trips per family headless with simple policies and prints arrivals, days, deaths by cause, events seen and never seen, money and relations (`--goal` picks a road; `--dump` saves a trip at a chosen moment, such as a river or the ending, to open in the game).
- **Unit tests** (CTest) cover the RNG, locale lookup and the game rules.
- **The game itself is testable without a person:** `--keys` replays key presses, mouse clicks and gamepad buttons and stick (`pad:a`, `stick:0.6,0`), `--seed` fixes a new trip's seed, `--pref` keeps saves and settings out of the user's folder, `--screenshot` saves a PNG and exits, `--quit` exits after the keys, `--check-text` checks every text fits, and `--music-wav` writes a tune to a WAV.
- **Packaging:** `tools/package.sh` builds a zip and, with Inno Setup, a Windows installer into Program Files.

---

## 22. Design references

- **The Oregon Trail (MECC, 1985).** R. Philip Bouchard's *You Have Died of Dysentery: The Creation of Oregon Trail* explains how pace, rations, river crossings, hunting and the tombstone were chosen. https://thevideogamelibrary.org/book/you-have-died-of-dysentery-the-creation-of-oregon-trail · https://news.uga.edu/creating-80s-oregon-trail/
- **When Rivers Were Trails** (Elizabeth LaPensée, Indian Land Tenure Foundation, 2019): an Oregon-Trail-style game told from an Anishinaabe perspective by 20–30+ Indigenous writers; the model for showing the people a trek moves through as full characters. https://indianlandtenure.itch.io/when-rivers-were-trails · https://www.theartnewspaper.com/2020/08/13/from-pixelated-to-post-colonial-iconic-1980s-oregon-trail-video-game-gets-an-indigenous-reboot
- **Epic Games, "Representing history: the many faces of The Oregon Trail":** https://store.epicgames.com/news/representing-history-the-many-faces-of-the-oregon-trail

---

## 23. Making a game like this

What worked, for anyone building a journey game on real history:

1. **Pick one journey, one year and one starting place,** and tie the calendar to a documented party. Calibrate the daily distance on how long they really took; the seasons then fall where history put them.
2. **Research before writing.** Keep one research document with labelled facts ([sourced], [conflict], [estimate], [invented]) and links. Write events only from it, and make every event cite it.
3. **Let the player's side change the world, not the interface.** Prices, welcomes and locked choices teach more than a speech about who was who.
4. **Write the people you meet as actors with reasons.** Name them; give them warnings, trade and help as well as danger.
5. **Hide the odds but make them real.** Convert documented rates to daily chances, mark the guesses, and run a bot to check whole trips come out believable.
6. **Make every death explicable.** Say who they were, what killed them and how it came about.
7. **Keep the simulation pure and deterministic,** apart from the UI, with one explicit random stream. It makes a balance bot, unit tests, replays and permadeath saves easy.
8. **Put every number and every word in data files,** with a checker that refuses anything missing a source or a translation key.
9. **Make the front end scriptable** (key replay, screenshots, isolated save folders), so it can be tested without a person.
10. **End each road with what really happened next,** told plainly and from both sides.
