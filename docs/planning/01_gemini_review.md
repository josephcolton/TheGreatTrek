# Review of the Gemini Design Files

The `Gemini/` folder will be deleted. This file keeps what is worth keeping from it, flags what is wrong or unverified, and notes the design problems it leaves open.

Files reviewed (Oct 2026): `great_trek_game_vision.md`, `great_trek_game_design1.md`, `great_trek_game_design2.md`, `family_connections.md`, `hunting.md`, `shopping.md`, `master_document.txt`.

---

## 1. What the Gemini files contain

| File | Content | Verdict |
|---|---|---|
| `great_trek_game_vision.md` | Historical overview, 5-phase structure, Laager and Voorloper mechanics | Good skeleton. Several facts need correcting (see §3). |
| `great_trek_game_design1.md` | 4 classes (Blacksmith, Hunter, Sheep Farmer, Predikant) and 4 event tables | Good event format. Prices are in £. |
| `great_trek_game_design2.md` | 3 classes with hidden debuffs and 4 events (incl. Smous) | Same events, reworked. Prices are in Rixdollars, so it conflicts with design1. |
| `family_connections.md` | English 1820 Settler "Freighter/Mason" class, Cradock border raid, a rescued-child story | Based on a real family from a later period. **Not used**: the game's families are fictional. |
| `hunting.md` | Turn-based flintlock hunting, 3 animal tiers, misfire by weather, night raids | Strong mechanic. Keep it. |
| `shopping.md` | Store catalog, Smous, mission stations, barter with Sotho/Griqua/Barolong | Good structure. Fix the prices and locations. |
| `master_document.txt` | Combined spec written as a **system prompt for an AI to run the game**, with a JSON state schema, factions, prologue, endings | The JSON state and endings matrix are useful. The "AI narrator runs the game" architecture is a separate decision (see `03_development_plan.md`). |

---

## 2. Ideas worth keeping

### Core mechanics
- **Laager stance at night.** Costs time and ox stamina, but sharply cuts the risk of predator or raid events. This is the game's signature mechanic and an equivalent of Oregon Trail's "pace".
- **Voorloper.** A party member (historically often a boy or an African/Khoikhoi servant) walks ahead leading the front oxen. If that person is sick or dead, axle damage and navigation mishaps go up.
- **Oxen are life.** Losing the span strands you. Track ox count and ox stamina separately.
- **Cash turns into barter.** Coin works in the Colony. North of the Orange River, gunpowder, lead, tobacco, beads, iron and cattle become the currency.
- **Flintlock hunting.** One shot, a long reload, and misfire chance that depends on weather (about 2% clear, 15% humid, 60% rain). Animals come in tiers: passive (springbok, blesbok, wildebeest), defensive megafauna (eland, Cape buffalo), and predators (lion). Predators hunt back.
- **Night predator raid** when no laager is formed. Responses: fire blind, send dogs, or advance with a torch.
- **Weighted-choice events.** Each choice has 1–3 outcomes with probabilities and stat effects. This format maps directly onto data files.
- **Class modifiers:** Blacksmith (repairs), Hunter (meat yield, ammo), Wealthy Farmer (cash and herd, but family more prone to illness), Predikant (morale, healing). Hidden debuffs (design2) are a good idea.
- **Endings built from vectors.** The epilogue is assembled from material survival + relationships/legacy + personal choices.

### Event seeds already written (worth porting)
1. Stagnant water in the Karoo: drink / boil / push on. Dysentery risk. Includes the ox "red water" variant.
2. Lions at night: laager / fire / hunt / bonfires.
3. Broken axle on a descent: repair / dismantle / abandon wagon.
4. Encounter with a Zulu force. **Needs a full rethink** (see §4).
5. Wandering smous: barter / pay coin / pass.
6. Flooded Orange River: wait (eats rations) or float the wagons.
7. Thaba Nchu gathering: trade, resupply, elect a leader (morale).
8. Drakensberg descent: the climax.
9. Border raid, Cradock district (English settler).
10. Stranded child. **Dropped** (see §4).

### State model (from `master_document.txt`, simplified)
```
game:      faction, class, milestone, distance, day, date, pace, rations, weather
resources: money, oxen, ox_stamina, cattle, sheep, horses, flour, biltong, rusks,
           coffee, gunpowder, lead, tallow, spare_parts, tobacco, trade_goods
party[]:   name, role, health, status_effects[]
standing:  trekker_council, british_colonial, per-polity relations (Barolong, Griqua, Zulu kingdom, Ndebele, Port Natal traders, ...)
flags:     story choices (e.g. laager_mastery, met_uys_bible, ...)
```

---

## 3. Factual problems to fix

| Gemini claim | Problem | Fix |
|---|---|---|
| Currency: £150 starting cash (design1), 600 Rds (design2), "£1 = 1.5 Rds" (master), "£1 ≈ 13 Rds" (shopping) | These contradict each other. From 1825 one rixdollar was officially worth 1s 6d, so **£1 ≈ 13⅓ Rds**. Sterling was the official currency by the 1830s, but people still reckoned in rixdollars. | Use one internal unit (pence or Rds) and display it as £/s/d or Rds per faction. |
| Musket called *baviansboud* | I couldn't verify this term. Period terms are **sanna** (the Voortrekker flintlock), **roer** (a heavy long-barrelled hunting gun), **snaphaan** (flintlock), and **sterloop** (a popular sanna type with a star mark near the muzzle). | Use *sanna* / *roer*. |
| *Grens-Smit*, *Baanbreker-Jagter* | Dutch and Afrikaans spellings are mixed. "Smid" is correct in both; "jager" is Dutch and "jagter" is Afrikaans. | Decide on Dutch vs Afrikaans (decided: period Dutch words in English text; see `03_development_plan.md` §1), then make the spelling consistent. |
| Drakensberg: "dismantle wagons, carry pieces over vertical cliffs" | Exaggerated. Retief's people camped at **Kerkenberg** (Oct–Nov 1837) and descended by **Retief's Pass** and nearby routes. A reported method was to remove the rear wheels and lash heavy branches underneath as a drag brake. Dismantling did happen on some routes, but not "vertical cliffs". | Make the descent a set-piece with historically grounded options: remove wheels + branch drag, chain-lock the wheels, double the spans, unload and carry. |
| Kuruman as a trading stop | Kuruman (Moffat's LMS mission) is far west of the main trek routes. | Use **Thaba Nchu** (Barolong under Moroka, Wesleyan mission), **Philippolis** (Griqua), or Platberg. |
| Griqua guide translating with a Zulu patrol | Implausible. The Griqua lived around the Orange River, far from Zululand. | In Natal, interpreters came from Port Natal: English traders and missionaries. Thomas Halstead was Retief's interpreter and was killed with him (verify). |
| British patrols "confiscating weapons" at the border | No source found. The British largely did not physically stop the trek. The Cape of Good Hope Punishment Act (1836) claimed legal jurisdiction north to 25°S. | Use British authority as legal and political pressure, not border checkpoints. Mark as unverified. |
| "70–75% success rate" | No source. This looks invented. | Drop it. Use documented episodes instead (e.g. Tregardt: about 27 of 53 died, mostly of malaria). |
| 12–16 oxen per wagon | Plausible and commonly cited, but I didn't find a Great-Trek-specific source. A source on later dray wagons gives 16–18. | Keep 12–16 as a tunable game value. |
| Dysentery as a major killer | Plausible but not directly sourced. **Malaria** (Tregardt, Delagoa Bay 1838) and **tsetse/nagana** (cattle) are well documented. | Keep the dysentery line for its Oregon Trail homage, but lead with malaria, fever and nagana. |
| Natal end-point and the estate ending | These come from a real Eastern Cape family's history, not from Natal or the Trek. | Dropped. The ending is on the Highveld. |

---

## 4. Design problems the Gemini files leave unresolved

1. **Two different stories are merged into one.**
   - *The Great Trek* happened in 1835–1840s, mostly among Dutch-speaking frontier families, from the Eastern Cape to the Highveld and Natal.
   - *The 1820 Settler family story* runs from the 1820s to the 1860s in the English-speaking Eastern Cape: the frontier wars, the 1856–57 Xhosa cattle-killing famine, missionaries arriving in the 1850s, and emigration overseas. Most 1820 Settlers did **not** join the Trek.
   - The master document mixes these together: a 1856 famine event at a Drakensberg milestone, 1850s missionaries in Natal, and so on. **Decided:** a single trek north, with Dutch or English as the side you play, and only fictional families (see `03_development_plan.md` §1).
2. **African peoples written only as obstacles, guides or traders.** This is the exact criticism made of Oregon Trail (see *When Rivers Were Trails* in the resources file). The Zulu "patrol" event turns Retief–Dingane, Bloukrans/Weenen and Blood River/Ncome, which are specific and contested events, into a random encounter. Those events need scripted, carefully framed chapters and named polities and people (Moroka, Mzilikazi, Dingane, Mpande, Moshoeshoe), not "tribe" encounters.
3. **The people who actually did much of the work are missing.** Large numbers of Khoikhoi, formerly enslaved, and Black servants (*agterryers*, herders, voorlopers) travelled with the trekkers. One estimate puts them roughly equal in number to the Boers. They should appear as party members with names.
4. **The rescued-child story.** It was based on a real person's life, which the Gemini version turned into a "+25% foraging perk" and a "Covenant of Mercy" ending. **Dropped.** In general, don't turn real people's lives into perks.
5. **Religion as a stat buff.** "Accept baptism → 100% immune to despair" and "drive the missionaries out → small morale gain" turn faith choices into min-max decisions. Narrative consequences (relationships, new story branches, a different ending) would probably work better than raw buffs.
6. **The AI-narrator architecture.** `master_document.txt` is written as a prompt for an AI to *be* the game engine. That's fun to prototype, but an AI narrator can't guarantee consistent numbers, a reproducible balance, or classroom-safe historical framing. A coded engine with written content is the safer base (see plan).
