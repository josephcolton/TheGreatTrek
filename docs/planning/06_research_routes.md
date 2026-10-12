# Research Pass 3: the Natal and Far North routes

Status: **researched and built (2026-10-11).** Section 9 says how the game uses it and what trekbot shows. This covers the two routes the user asked for: **Natal** over the Drakensberg, and the **Far North** to the Soutpansberg and Delagoa Bay. Both are new forks off the existing road.

Labels as in `02_research_resources.md`: **[sourced]**, **[conflict]**, **[recall, verify]**, **[estimate]**. Links are in §8.

---

## 1. What this changes in the design

1. **Two new roads, each with two goals the player picks.** The ending options are kept:
   - **Natal:** the camps on the Bloukrans and Bushman's rivers (where most trekkers stopped, Nov 1837 – Feb 1838), or on to **Port Natal**.
   - **Far North:** the **Soutpansberg** (where Tregardt's party lived for over a year), or on over the mountains to **Delagoa Bay**.
2. **The game ends on arrival, then an epilogue** says what happened to the people who really went there. It says plainly who did not survive; it does not describe how.
3. **Both forks fit our calendar.** A party leaving Grahamstown in April 1837 reaches:
   - the Vet River in late June, which is when the trekkers there chose Natal (6 June 1837);
   - the far north in the spring.
   Without changing anything, the Far North player meets the **lowveld in the rainy season**, as Tregardt did. That is the deadliest time.
4. **Malaria and tsetse ("the fly") become real dangers, tied to place and season**, not general bad luck.
   - The lowveld in summer is deadly. The highveld is not.
   - People on the road warn about this. A player who listens can wait for the dry season, as the Tsonga traders did.
5. **Warnings in towns and camps** come from people who really knew. Each one has a source (§5).

---

## 2. Where the forks go

```
... Thaba Nchu ─ Vet R. ─┬─ Sand R. ─ Vaal drift ─┬─ Mooi River                          (existing ending)
                         │                         └─ Magaliesberg ─ Strydpoort ─ Soutpansberg ─┬─ (goal 1: stay)
                         │                                                                      └─ Olifants R. ─ Drakensberg descent ─ Lowveld ─ Lebombo ─ Komati ─ Delagoa Bay (goal 2)
                         └─ Kerkenberg ─ Drakensberg descent ─ Bloukrans / Bushman's R. camps ─┬─ (goal 1: stay)
                                                                                                └─ Port Natal (goal 2)
```

- **Natal leaves at the Vet River.** That is where the gathered trekkers adopted the nine Vet River articles on **6 June 1837** (140 men present) and chose **Port Natal and Natal** as their goal **[sourced: LitNet]**. It is a better fork than Thaba Nchu, where the plan first put it. Potgieter was against going to Natal and stayed on the Highveld **[sourced: Wikipedia, Potgieter]**, so the council scene can show both sides.
- **The Far North leaves at the Vaal drift.** Potgieter rode north from the trek at the Sand River to the Soutpansberg in mid-1836 to find Tregardt, so the road north from the Sand and Vaal was known **[sourced: Preller's introduction to the diary]**.

### 2.1 Distances [estimate]

Straight lines between approximate modern places, × 1.25 for the ground, as in `05` §2. The points are rough (±10 mi), so treat these as first numbers for `route.json`.

| Leg | Straight | Road (×1.25) | Notes |
|---|---|---|---|
| Vet River → Kerkenberg | 123 mi | ~155 mi | Kerkenberg is 32 km south of Harrismith, Oliviershoek Pass area |
| Kerkenberg → Bloukrans camps | 61 mi | ~75 mi | includes the descent |
| Bloukrans → Port Natal | 92 mi | ~115 mi | |
| Vaal drift → Magaliesberg | 96 mi | ~120 mi | |
| Magaliesberg → Strydpoort | 125 mi | ~155 mi | Strydpoort = "the poort of Sekwati" in the diary |
| Strydpoort → Soutpansberg | 96 mi | ~120 mi | |
| Soutpansberg → Chuniespoort → Olifants R. | 108 mi | ~135 mi | Tregardt went west and south first, round the mountains |
| Olifants R. → top of the escarpment | 39 mi | ~50 mi | |
| Escarpment descent | 32 mi | ~40 mi | **took 2 months 18 days** (§3.2) |
| Foot of the escarpment → Lebombo | 72 mi | ~90 mi | lowveld: fly and fever |
| Lebombo → Komati → Delagoa Bay | 103 mi | ~130 mi | |

Totals:
- **Natal:** ~345 mi from the Vet River to Port Natal, ~230 mi to the camps.
- **Far North:** ~395 mi from the Vaal to the Soutpansberg, and ~440 mi more to Delagoa Bay.
- Tregardt took **8 months** from the Soutpansberg to the Bay (Aug 1837 – 13 Apr 1838) **[sourced]**. Most of that was the descent and the weak oxen.

---

## 3. The Far North (Tregardt and Van Rensburg)

Main source: Gustav Preller's introduction to the 1938 edition of Tregardt's diary (DBNL, pp. LXXVII–XCV), which draws on the diary, Carolus Trichardt's later account, and Francis Owen's letters. Facts below are from it unless marked.

### 3.1 Before our start date

- **Two small parties went north in 1835–36.** Tregardt had 9 families. Van Rensburg's party joined them for a while **[sourced]**.
  - They split over Van Rensburg's use of ammunition hunting ivory **[sourced: Wikipedia]**. Another account says Van Rensburg went ahead to scout a route to the coast **[conflict]**.
- **Van Rensburg's party, 49 people, was killed** near the Limpopo / on the way to Inhambane in mid-1836 **[sourced; conflict on the details]**.
  - The month is given as June or July.
  - The attackers are named as Soshangane's (Manukosi's) Gaza force, or as Tsonga.
  - Two children are said to have been spared.
  - The epilogue should say "killed", name no detail, and note the uncertainty in the History note.
- **Potgieter rode north to visit Tregardt** at the Soutpansberg in mid-1836. He went past the Limpopo, about 80 miles, and turned back on 17 Aug 1836. They agreed Tregardt would wait for the others. **They never met again**: Vegkop and the war with the Ndebele kept Potgieter in the south.
- **Tregardt waited at the Soutpansberg for over a year**, with houses, a smithy, gardens, a water furrow and a school.
  - The school was **21 children** under the old teacher **Daniel Pfeffer** **[sourced]**.
  - This is the "stay" ending: where the Far North party really stopped.

### 3.2 The road to Delagoa Bay, 1837–38

- **Trade with the coast:** **Tsonga traders (the diary's "Magwambas") came up from Delagoa Bay in the winter**, across the lowveld, with trade goods **[sourced]**. They knew which season was safe. A trader, Waai-Waai, offered in May 1837 to fetch goods from the Bay.
- **The Portuguese commandant sent two soldiers** to guide them (Aug 1837). Tregardt hesitated, then went, because:
  - the Ndebele were close to the south;
  - the Van Rensburgs had been killed to the east;
  - the trekkers' powder was down to one keg and they were melting tin and copper for bullets **[sourced]**.
  Carolus later said: *"we go through to Delagoa Bay, and the place was death! … I said to my father, let us make a good fort and stay here."* **[sourced]**
- **Left on 23 Aug 1837.** They went west and south first to avoid **the fly**: local people warned that the direct road east would bring them into it, as it had the Pretorius party (§3.3). They passed Chuniespoort and reached the **Olifants River on 2 Oct 1837**.
- **Sekwati (Pedi) warned that the mountains could not be crossed with wagons.** Tregardt didn't believe him and went anyway **[sourced]**. That gives us a warning event with a real choice.
- **The escarpment descent:** **8 Nov 1837 to 25 Jan 1838**, two months and eighteen days, in heavy rain and mist **[sourced]**.
  - The men nearly gave up and talked of leaving the wagons and walking. The women objected, and Tregardt wrote on 9 Nov that they were glad to hear a road could be made.
  - **On steep slopes they took off the back wheels and tied them on a sledge behind**, protected the axle, and tied the rear oxen's tails to the front axle so they would not fall. Going up took **18 oxen to a wagon** **[sourced, Carolus Trichardt]**.
  - **Jan Pretorius took his wagons apart and carried them down piece by piece** **[sourced]**.
  - Note for the Gemini error list: our earlier ban on "vertical-cliff wagon dismantling" still stands for **Natal**. This is a different, documented case on a steep slope, not a cliff.
- **The lowveld in Feb–Mar 1838, "the unhealthiest months of the year"** **[sourced]**.
  - Cattle died of the "bos-siekte" (bush sickness, the fly): **479 cattle on 23 Mar**, still many on 26 Mar, then most dead within weeks of arrival **[sourced]**. In 1836 "fly-disease" struck the pioneers' herds; this is the first record of tsetse in the north of South Africa **[sourced: Kappmeier 1998]**.
  - Local people stole cattle and attacked at night. The wagons laagered every night again.
  - A Portuguese officer met them at the Komati. The ivory and heavy goods went down the river by boat.
- **Arrived at Lourenço Marques, 3 pm on 13 Apr 1838, with 7 wagons** (2 left behind).
  - The place was a small earth fort on a sand ridge, beside a permanent swamp.
  - They fired a salute; no one answered. Then **they had to give up their guns** to the fort, because the Portuguese were afraid of them. The commandant who had sent for them had died; Captain António Gamitto received them kindly **[sourced]**.
- **Fever:**
  - **Four days after arrival** Tregardt's wife Martha, three children and Pfeffer fell ill. Pfeffer died on 21 Apr, and others followed through April.
  - **Martha died on 1 May 1838**, and the diary stops.
  - Carolus's wife and one of their sons died soon after.
  - The Rev. Francis Owen passed through on the *Comet* in May 1838 and wrote that **nobody at the Bay knew any medicine**, and that the Portuguese were very kind **[sourced]**.
  - **Tregardt died on 25 Oct 1838.**
- **Rescue:** news reached Pietermaritzburg in Feb 1839. The *Mazeppa* sailed for the Bay on 25 June 1839 and landed the survivors at **Port Natal on 19 July 1839**: **26 people, most of them children** (25 plus Carolus, who was away exploring the coast) **[sourced]**.
  - Wikipedia says 26 of 52. Older summaries say "about 27 of 53 died" **[conflict, small]**. Use "about half".

### 3.3 The warning nobody heeded (sourced, and good for the game)

Late in 1836, **Jan Pretorius, Hendrik Botha, Gert Scheepers and Izaak Albach** left Tregardt with their families to look for the Van Rensburgs to the east.
- **Tregardt "had warned them seriously not to go."**
- The season was wrong. **Fever and the fly** struck, and the oxen died until they could go neither forward nor back.
- Scheepers's wife secretly sent a girl back with a letter asking for help. Tregardt sent Venda helpers and spans of oxen.
- **Gert Scheepers died of the fever** on the way back, and later one of his children did too.
- They rejoined on 1 Mar 1837.

This is the model for **warnings in the game**: someone who knows tells you plainly, and you choose.

### 3.4 People of the Far North (named actors)

| People | Leader (1836–38) | Role in the story | Label |
|---|---|---|---|
| **Venda** at the Soutpansberg | **Ramabulana**; his brother Ramavhoya (the diary's "Ramabooya") | Gave Tregardt land; Tregardt helped him against his brother. There was a night attack on the small laager after the others left. | [sourced; dates conflict] |
| **Pedi** at Strydpoort | **Sekwati** | Warned that the mountains were impassable for wagons | [sourced] |
| **Tsonga traders** ("Magwambas") | (traders, not a chief) | Winter trade to the Bay; guides; knew the fly and the seasons | [sourced] |
| **Gaza** | **Soshangane** (Manukosi) | The power to the east; Van Rensburg's killers in most accounts | [sourced; conflict] |
| **Ndebele** | **Mzilikazi** | South-west of the road until the Nov 1837 Marico campaign drove them north | [sourced, `05`] |
| **Portuguese** at Lourenço Marques | Capt. **António Gamitto** | Small, weak garrison; took the trekkers' guns, then helped them | [sourced] |

Per the content rules, none of these is a generic "tribe" encounter. Each has its own reasons, and most of them gave warnings, guides, trade or help.

---

## 4. Natal

### 4.1 The road

- **Kerkenberg:** Retief camped with **54 wagons** on **5 Oct 1837**. The trekkers stayed from mid-October to mid-November and **sent parties to find ways down the escarpment** **[sourced]**.
- **Retief rode ahead** on 6 Oct 1837 with about 15 men and 4 wagons, down what is now **Retief's Pass**. He was **welcomed at Port Natal by the traders on 19 Oct 1837** **[sourced]**.
- **2 Nov 1837:** Bantjes rode back to Kerkenberg with word that the trekkers could come down into Natal **[sourced]**.
- The main body went down in **November 1837**. They spread out in many small camps along the **Bloukrans and Bushman's (Mtshezi) rivers**, near present Estcourt and Weenen, to wait while Retief went to see the Zulu king Dingane **[sourced]**. This is where most trekkers stopped, so it is **goal 1**.
- **How the descent was done:** no first-hand method was found in this pass **[recall, verify]**. Likely methods are locked wheels, drag-shoes and trees dragged behind, and men on ropes. Only the Tregardt method (§3.2) is sourced. Don't invent detail: describe it as "long and dangerous" until a source turns up.
- **Port Natal in 1837 (goal 2):**
  - English traders: about **50 settlers in 1835** **[sourced]**; 1837 numbers unknown.
  - **Allen Gardiner's** mission on the hill above the bay, from 1835. In 1837 he was given authority over the traders, which they refused **[sourced]**.
  - The **American mission** (Adams, Grout, Champion), from 1836 **[sourced]**.
  - This is where an English family would feel at home.

### 4.2 What Grahamstown already knew

- **The Kommissietrek of 1834:** Piet Uys led **21 people** to scout Natal. They visited Port Natal and returned to Uitenhage in Feb 1835 with **favourable reports** that helped start the Trek **[sourced]**. Whether they met Dingane is uncertain **[conflict]**.
- So in Grahamstown in April 1837 the talk is of **good, well-watered land in Natal**. This is the hopeful rumour, set against later warnings.

### 4.3 For the epilogue (what happened after the player arrives)

The player's trip ends at the camps or at Port Natal, late 1837. The epilogue tells, without detail:
- **6 Feb 1838:** Retief and about **100** of his party were killed at Dingane's capital, after the land deed was signed **[sourced]**.
- **17 Feb 1838:** the camps on the Bloukrans and Bushman's rivers were attacked at night.
  - Killed: **282 trekkers** (41 men, 56 women, 185 children) and **about 250 Khoikhoi and Basotho** who travelled with them **[sourced; Pretorius gives other figures]**.
  - Some camps beat off the attack. The town later founded there was named **Weenen, "weeping"** **[sourced]**.
- **April 1838:**
  - Piet Uys and his son Dirkie were killed at **Italeni** **[sourced]**.
  - The Port Natal traders' force under Robert Biggar was defeated at the Tugela on **17 Apr 1838**; few came back. Most people at Port Natal escaped on the ship *Comet* **[sourced; numbers conflict]**.
  - The same *Comet* then came to Delagoa Bay and found Tregardt's party dying. This links the two endings.
- **16 Dec 1838:** Blood River.
- **1838–43:** the Republic of Natalia.
- **1843:** Britain annexed Natal. Most trekkers then left again for the Highveld **[sourced, `02`]**.
- **The Far North survivors** were landed at Port Natal in July 1839. This links the two roads.

The epilogue must give **both sides**, and note how the events have been remembered and argued over:
- Dingane's reasons (land, guns, the trekkers' victory over the Ndebele) as well as the trekkers' grief;
- the 250 servants killed, not only the trekkers.

---

## 5. Warnings on the road (sourced, for events)

| Where | Who warns | What they say | Source |
|---|---|---|---|
| Grahamstown, Cradock | settlers, the *Graham's Town Journal* | Natal is green and well watered (the Kommissietrek). Nobody knows much of the far north. | §4.2 |
| Thaba Nchu, Vet River | Potgieter's people | The north is open, but far. Tregardt is waiting at the Soutpansberg. Potgieter himself is against Natal. | §2, §3.1 |
| Vet River council | the council | The vote for Natal (6 June 1837). | §2 |
| Vaal drift, Soutpansberg | anyone who knows of the Pretorius party | Don't go down into the low country in the wrong season: the fever and the fly | §3.3 |
| Soutpansberg | Tsonga traders | They cross the lowveld **in winter** only | §3.2 |
| Soutpansberg | news from the east | The Van Rensburg party is not coming back | §3.1 |
| Strydpoort | Sekwati | The mountains are impassable for wagons | §3.2 |
| Kerkenberg | scouts | The way down is steep; parties are still looking for a road | §4.1 |

---

## 6. Malaria and the fly in the game

- **1837 understanding:** people called it **"the fever"** (*koors*) and blamed the low country, swamps and the rainy season. Nobody linked it to mosquitoes until 1897. The game's text uses the period view and the History note gives the modern one **[sourced: general history; Owen's "a consequence of the climate"]**.
- **Quinine** was isolated in 1820 and in wide use elsewhere by the 1840s **[sourced: Utrecht abstract]**. **No source shows the trekkers had it**, and Owen says nobody at the Bay knew medicine. Don't offer quinine in stores. The medicine chest may ease a fever but not cure it **[estimate, mark invented]**.
- **Risk by place and season [estimate, to be marked as such]:**
  - Highveld: none.
  - Soutpansberg: low (the Scheepers child caught it in the lowveld).
  - Lowveld and Delagoa Bay, Nov–Apr: high.
  - Lowveld and Delagoa Bay, May–Oct: low.
  - The Tregardt party is the anchor: roughly half the people who reached the Bay in April died within a year.
- **The fly** kills oxen, not people, in the lowveld bush. Cattle sicken over weeks. This is why a party can get stranded, as the Pretorius party did.
- **Horse-sickness** is already in the game (`05` §7) and fits the north as well.

---

## 7. Open questions and gaps

- **The Natal descent method:** needs a source (Voortrekker Museum, Bezuidenhout's Pass, Retief's Pass).
- **Exact route of the main Natal descent:** several passes were used; which ones, and when.
- **Fuller (1932), *Louis Trichardt's Trek across the Drakensberg*** (Van Riebeeck Society 13) has maps of the whole Far North route. It wasn't found online in this pass.
- **The Kommissietrek report** itself: we have only secondary summaries.
- **Venda dates:** sources disagree on when Ramabulana gave the land (1836 or 1837).

---

## 8. Sources added in this pass

- Preller (ed.), *Dagboek van Louis Trichardt 1836–1838* (1938), introduction pp. LXXVII–XCV (DBNL): https://www.dbnl.org/tekst/tric015dagb01_01/
- Wikipedia, Louis Tregardt: https://en.wikipedia.org/wiki/Louis_Tregardt
- Wikipedia, Lang Hans van Rensburg: https://en.wikipedia.org/wiki/Lang_Hans_van_Rensburg
- HIPSA, *Louis Trigardt's trek across the Drakensberg 1837–1838*: https://hipsa.org.za/publication/louis-trigardts-trek-across-the-drakensberg-1837-1838/
- Wikipedia, Soutpansberg: https://en.wikipedia.org/wiki/Soutpansberg
- Wikipedia, Soshangane: https://en.wikipedia.org/wiki/Soshangane
- Wikipedia, Fort of Lourenço Marques: https://en.wikipedia.org/wiki/Fort_Nossa_Senhora_da_Concei%C3%A7%C3%A3o_de_Louren%C3%A7o_Marques
- Kappmeier, Nevill & Bagnall (1998), tsetse in South Africa (Univ. of Pretoria): https://repository.up.ac.za/bitstream/handle/2263/20372/26kappmeier1998.pdf
- Utrecht, quinine history abstract: https://dspace.library.uu.nl/handle/1874/308426
- Wikipedia, Piet Retief: https://en.wikipedia.org/wiki/Piet_Retief
- Wikipedia, Kerkenberg: https://en.wikipedia.org/wiki/Kerkenberg
- LitNet, the nine Vet River articles of 1837: https://www.litnet.co.za/we-the-reformed-members-in-the-nine-vet-river-articles-of-1837/
- Wikipedia, Hendrik Potgieter: https://en.wikipedia.org/wiki/Hendrik_Potgieter
- Wikipedia, Weenen massacre: https://en.wikipedia.org/wiki/Weenen_massacre
- Wikipedia, Colony of Natal: https://en.wikipedia.org/wiki/Colony_of_Natal
- SAHO, Durban timeline: https://sahistory.org.za/article/durban-timeline-1497-1990
- Wikipedia, Piet Uys (Kommissietrek): https://en.wikipedia.org/wiki/Piet_Uys
- Wikipedia, Biggar family: https://en.wikipedia.org/wiki/Biggar_family
- Wikipedia, Dick King: https://en.wikipedia.org/wiki/Dick_King

---

## 9. In the game (built 2026-10-11)

- **Route** (`route.json`): the forks are at the Vet River and the Vaal drift as in §2. Bloukrans and the Soutpansberg are goals: their set piece asks whether to stop (the trip ends there) or go on. Port Natal and Delagoa Bay end the trip on arrival. Each goal has a history page after the ending (`epilogue` keys in `routes.json` locale).
- **At the Soutpansberg** the player can also **wait for the dry season** (until 20 April, with grain and sheep traded from the Venda and grown in a garden, as Tregardt's people did), then go on. This follows the Tsonga traders' practice (§3.2).
- **Malaria** and **the fly** by region and month (`regions.json`, §6). Rates are estimates.
- **Warnings** (§5): the fork advice at the Vet River and the Vaal, the council at the Vet River, Sekwati's people at Strydpoort and the Olifants, and the talk at the Soutpansberg (the Tsonga traders, the Pretorius party, the Van Rensburgs).
- **trekbot** (`--goal`, 200 runs a family, Normal; the bot carries more food, powder and beads on the Far North, as a player would after the warnings):

| Goal | Arrive | Deaths per trip (of about 8) | Main killers |
|---|---|---|---|
| Mooi River | 100% | ~0.1 | flux, hardship |
| Bloukrans camps | 100% | ~0.2 | flux, hardship |
| Port Natal | 100% | ~0.2 | flux, hardship |
| Soutpansberg | 86–100% | 0.2–1.4 | flux; the big predikant family runs short of food |
| Delagoa Bay in the rains | 0–88% by family | ~2.7–8 | **malaria**, hunger, stranded by the fly |
| Delagoa Bay after waiting for the dry season | 0–87% by family | ~0.4–7 | hunger; malaria much less |

  The Delagoa Bay road is meant to be the deadly one, as it was: about half of Tregardt's party died, most of them of malaria. Families with good shots and herds (the frontier farmer, the hunter) can make it; a large family with no hunter usually cannot. The warnings say so.

