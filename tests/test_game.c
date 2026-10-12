/* Plays whole games headlessly: they must finish, be deterministic, and survive save/load. */
#include "calendar.h"
#include "data.h"
#include "game.h"
#include "game_internal.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

#ifndef TREK_DATA_DIR
#define TREK_DATA_DIR "data"
#endif

/* A simple player: first enabled choice, trade once per shop, travel. */
static Game *play(const GameData *d, uint64_t seed, int family, int stop_after_steps)
{
    Game *g = game_new(d, seed, family, DIFF_NORMAL);
    bool traded[MAX_NODES] = {false};
    for (int steps = 0; steps < 20000 && game_phase(g) != PHASE_ENDED; steps++) {
        if (stop_after_steps >= 0 && steps == stop_after_steps) {
            break;
        }
        switch (game_phase(g)) {
        case PHASE_TRAVEL:
            if (steps % 9 == 4 && game_can_hunt(g)) {
                game_hunt_start(g);
            } else {
                game_travel(g, 30);
            }
            break;
        case PHASE_STORE: game_store_leave(g); break;
        case PHASE_GRAVE: game_grave_done(g, "Rest in peace"); break;
        case PHASE_BARTER: game_barter_leave(g); break;
        case PHASE_HUNT: {
            HuntView hv;
            CHECK(game_hunt_view(g, &hv));
            /* a whole hunt: fire at the first animal in sight whenever the gun is loaded */
            for (int t = 0; t < 10000 && !hv.over; t++) {
                for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
                    const HuntAnimal *a = &hv.animals[i];
                    if (hv.loaded && a->state != HUNT_A_NONE && a->state != HUNT_A_DOWN) {
                        float b[4];
                        game_hunt_body(g, a, b);
                        game_hunt_shoot(g, b[0] + b[2] / 2, b[1] + b[3] / 2);
                        break;
                    }
                }
                game_hunt_tick(g);
                CHECK(game_hunt_view(g, &hv));
            }
            CHECK(hv.over);
            game_hunt_finish(g);
            CHECK(game_phase(g) != PHASE_HUNT);
            break;
        }
        case PHASE_DIALOG: {
            DialogView v;
            CHECK(game_dialog_view(g, &v));
            int pick = 0;
            if (v.kind == DLG_LANDMARK && !traded[g->node] && d->nodes[g->node].shop >= 0) {
                pick = 1;
                traded[g->node] = true;
            } else if (v.kind == DLG_LANDMARK) {
                pick = v.choice_count - 1;
            } else if (v.kind == DLG_STRANDED) {
                pick = v.choices[2].enabled ? 2 : 1;
            } else {
                for (int i = 0; i < v.choice_count; i++) {
                    if (v.choices[i].enabled) {
                        pick = i;
                        break;
                    }
                }
            }
            game_choose(g, pick);
            break;
        }
        default: break;
        }
    }
    return g;
}

static void test_calendar(void)
{
    CHECK(cal_day(1837, 1, 1) == 0);
    CHECK(cal_weekday(0) == 0);                     /* 1 Jan 1837 was a Sunday */
    CHECK(cal_weekday(cal_day(1837, 4, 3)) == 1);   /* Monday 3 April 1837 */
    int y, m, dd;
    cal_date(cal_day(1838, 12, 16), &y, &m, &dd);
    CHECK(y == 1838 && m == 12 && dd == 16);
    CHECK(cal_season(cal_day(1837, 7, 1)) == SEASON_WINTER);
}

/* Wagon trade-ins: the old wagon is worth wagon_trade_in of its new price. */
static void test_wagon_trade(const GameData *d)
{
    Game *g = game_new(d, 5, data_find_family(d, "settler"), DIFF_NORMAL);
    int kake = data_find_wagon(d, "kakebeenwa"), transport = data_find_wagon(d, "transport_wagon"),
        cart = data_find_wagon(d, "scotch_cart");
    CHECK(g->wagon == kake);
    double old = d->wagons[kake].price_pence * d->rules.wagon_trade_in;
    CHECK(g_wagon_trade_cost(g, transport) == d->wagons[transport].price_pence - (int)(old + 0.5));
    CHECK(g_wagon_trade_cost(g, cart) < 0); /* the cart is cheaper: money back */
    g->wagon = cart;
    CHECK(game_capacity_lb(g) == d->wagons[cart].capacity_lb);
    game_free(g);
}

/* Hunts a whole afternoon, firing at the nearest animal whenever the gun is loaded. */
static int hunt_once(Game *g, int *kills)
{
    game_hunt_start(g);
    CHECK(game_phase(g) == PHASE_HUNT);
    HuntView v;
    bool tried_unloaded = false;
    for (int t = 0; t < 10000 && game_hunt_view(g, &v) && !v.over; t++) {
        const HuntAnimal *near = NULL;
        for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
            const HuntAnimal *a = &v.animals[i];
            if (a->state != HUNT_A_NONE && a->state != HUNT_A_DOWN && a->x > 0 && a->x < HUNT_FIELD_W && (!near || a->y > near->y)) {
                near = a;
            }
        }
        if (near) {
            float b[4];
            game_hunt_body(g, near, b);
            if (v.loaded) {
                CHECK(game_hunt_shoot(g, b[0] + b[2] / 2, b[1] + b[3] / 2));
            } else if (!tried_unloaded) {
                CHECK(!game_hunt_shoot(g, b[0] + b[2] / 2, b[1] + b[3] / 2)); /* still loading */
                tried_unloaded = true;
            }
        }
        game_hunt_tick(g);
    }
    CHECK(v.over);
    *kills += g->hunt.kills;
    int meat = g->hunt.meat;
    game_hunt_finish(g);
    CHECK(game_phase(g) != PHASE_HUNT);
    return meat;
}

static void test_hunt(const GameData *d)
{
    int meat = 0, kills = 0, hunts = 0;
    for (uint64_t seed = 1; seed <= 6; seed++) {
        for (int steps = 20; steps < 400; steps += 7) {
            Game *g = play(d, seed, data_find_family(d, "jagter"), steps);
            if (game_phase(g) == PHASE_TRAVEL && g->edge >= 0) {
                g->last_hunt_day = -1;
                if (game_can_hunt(g)) {
                    /* the same hunt with the same shots comes out the same */
                    Game *twin = play(d, seed, data_find_family(d, "jagter"), steps);
                    twin->last_hunt_day = -1;
                    int k2 = 0, k1 = 0;
                    int m1 = hunt_once(g, &k1);
                    CHECK(hunt_once(twin, &k2) == m1);
                    CHECK(k1 == k2);
                    game_free(twin);
                    meat += m1;
                    kills += k1;
                    hunts++;
                    game_free(g);
                    break;
                }
            }
            game_free(g);
        }
    }
    CHECK(hunts >= 3);
    CHECK(kills > 0);
    CHECK(meat > 0);
}

/* Stands guard over the herd against one predator. With `shoot`, fires at whatever is nearest the
 * herd; without, just watches. Returns the head of stock lost. */
static int guard_once(Game *g, int animal, bool shoot, bool round_trip)
{
    const GameData *d = g->data;
    int stock0 = g->inv[d->item_sheep] + g->inv[d->item_cows] + g->inv[d->item_oxen];
    g->dlg.kind = DLG_NONE; /* as if the event's result page has just been read */
    g->notice_count = 0;
    game_defend_start(g, animal);
    g_settle(g);
    CHECK(game_phase(g) == PHASE_HUNT);
    HuntView v;
    for (int t = 0; t < 10000 && game_hunt_view(g, &v) && !v.over; t++) {
        if (round_trip && t == 200) {
            /* a save mid-watch keeps the stock already lost and the raiders still to come */
            char *json = game_save_json(g);
            char err[128];
            Game *back = game_load_json(d, json, err, sizeof err);
            free(json);
            CHECK(back != NULL);
            if (back) {
                CHECK(back->hunt.defend && back->hunt.foe == animal);
                CHECK(back->hunt.took_count == g->hunt.took_count);
                CHECK(back->hunt.foes_left == g->hunt.foes_left);
                game_free(back);
            }
        }
        const HuntAnimal *near = NULL;
        for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
            const HuntAnimal *a = &v.animals[i];
            bool live = a->state == HUNT_A_WALK || a->state == HUNT_A_RUSH || a->state == HUNT_A_CHARGE ||
                        (a->state == HUNT_A_FLEE && a->prey >= 0);
            if (live && a->x > 0 && a->x < HUNT_FIELD_W && (!near || a->y > near->y)) {
                near = a;
            }
        }
        if (shoot && near && v.loaded) {
            float b[4];
            game_hunt_body(g, near, b);
            game_hunt_shoot(g, b[0] + b[2] / 2, b[1] + b[3] / 2);
        }
        game_hunt_tick(g);
    }
    CHECK(v.over);
    int lost = g->hunt.took_count;
    game_hunt_finish(g);
    CHECK(game_phase(g) != PHASE_HUNT);
    int stock1 = g->inv[d->item_sheep] + g->inv[d->item_cows] + g->inv[d->item_oxen];
    CHECK(stock0 - stock1 == lost);
    CHECK(g->inv[d->item_sheep] >= 0 && g->inv[d->item_cows] >= 0 && g->inv[d->item_oxen] >= 0);
    return lost;
}

static void test_defend(const GameData *d)
{
    static const char *FOES[] = {"hyena", "wild_dog", "lion"};
    int lost_watching = 0, lost_shooting = 0;
    for (uint64_t seed = 1; seed <= 4; seed++) {
        for (size_t f = 0; f < 3; f++) {
            int animal = data_find_animal(d, FOES[f]);
            CHECK(animal >= 0 && d->animals[animal].raid_max > 0);
            int lost[2];
            for (int shoot = 0; shoot < 2; shoot++) {
                Game *g = play(d, seed, data_find_family(d, "jagter"), 30);
                g->inv[d->item_sheep] = 40;
                g->inv[d->item_cows] = 8;
                g->inv[d->item_oxen] = 14;
                g->inv[d->item_gunpowder] = 6;
                g->inv[d->item_lead] = 6;
                lost[shoot] = guard_once(g, animal, shoot, seed == 1 && shoot);
                game_free(g);
            }
            lost_watching += lost[0];
            lost_shooting += lost[1];
        }
    }
    CHECK(lost_watching > 0);
    CHECK(lost_shooting < lost_watching); /* standing guard with the gun saves stock */

    /* the same watch with the same shots comes out the same */
    int animal = data_find_animal(d, "hyena");
    Game *a = play(d, 5, data_find_family(d, "jagter"), 30), *b = play(d, 5, data_find_family(d, "jagter"), 30);
    CHECK(guard_once(a, animal, true, false) == guard_once(b, animal, true, false));
    CHECK(a->inv[d->item_sheep] == b->inv[d->item_sheep]);
    game_free(a);
    game_free(b);
}

/* The Far North: fever is caught in the low country in the rains and hardly at all in the dry
 * season; the fly kills cattle there; the ending at Delagoa Bay has a history page. */
static int fever_cases(const GameData *d, int month)
{
    int lowveld = -1;
    for (int e = 0; e < d->edge_count; e++) {
        if (strcmp(d->regions[d->edges[e].region].id, "lowveld") == 0) {
            lowveld = e;
        }
    }
    CHECK(lowveld >= 0 && d->illness_fever >= 0);
    int cases = 0;
    for (uint64_t seed = 1; seed <= 6; seed++) {
        Game *g = play(d, seed, data_find_family(d, "grensboer"), 30);
        g->edge = lowveld;
        g->node = d->edges[lowveld].from;
        g->day = cal_day(1838, month, 1);
        for (int day = 0; day < 20; day++) {
            g_pass_day(g, false, false);
            for (int i = 0; i < g->party_count; i++) {
                for (int c = 0; c < g->party[i].cond_count; c++) {
                    if (g->party[i].conds[c].illness == d->illness_fever && g->party[i].conds[c].days == 0) {
                        cases++;
                    }
                }
            }
        }
        game_free(g);
    }
    return cases;
}

static void test_far_north(const GameData *d)
{
    int wet = fever_cases(d, 2), dry = fever_cases(d, 7);
    CHECK(wet > 0);
    CHECK(wet > dry * 3); /* the rains are the fever season */

    /* the fly: cattle die in the low country in the rains */
    Game *g = play(d, 2, data_find_family(d, "grensboer"), 30);
    for (int e = 0; e < d->edge_count; e++) {
        if (strcmp(d->regions[d->edges[e].region].id, "lowveld") == 0) {
            g->edge = e;
        }
    }
    g->day = cal_day(1838, 2, 1);
    g->inv[d->item_cows] = 40;
    for (int day = 0; day < 30; day++) {
        g_pass_day(g, false, false);
    }
    CHECK(g->inv[d->item_cows] < 40);
    CHECK(game_has_flag(g, "fly_seen"));

    /* the history page belongs to the place the trip ended */
    LocText lines[MAX_EPILOGUE];
    CHECK(game_history_epilogue(g, lines, MAX_EPILOGUE) == 0); /* still on the road */
    g->node = data_find_node(d, "delagoa_bay");
    g->ending = ENDING_ARRIVED;
    CHECK(game_history_epilogue(g, lines, MAX_EPILOGUE) >= 4);
    g->ending = ENDING_STRANDED;
    CHECK(game_history_epilogue(g, lines, MAX_EPILOGUE) == 0);
    game_free(g);

    /* every goal place has a history page */
    static const char *GOALS[] = {"mooi_river", "bloukrans", "port_natal", "soutpansberg", "delagoa_bay"};
    for (size_t i = 0; i < sizeof GOALS / sizeof GOALS[0]; i++) {
        int n = data_find_node(d, GOALS[i]);
        CHECK(n >= 0 && d->nodes[n].epilogue_count > 0);
    }
}

/* Eating the flock takes a sheep every few days; keeping it eats the stores and spares the sheep
 * while there is food in the wagon. The journal says once a week how many went to the pot. */
static void test_flock(const GameData *d)
{
    int left[2];
    for (int keep = 0; keep < 2; keep++) {
        Game *g = play(d, 4, data_find_family(d, "grensboer"), 30);
        g->inv[d->item_sheep] = 50;
        g->inv[data_find_item(d, "meal")] = 2000;
        game_set_keep_flock(g, keep);
        int journal0 = g->journal_count;
        for (int day = 0; day < 21; day++) {
            g_pass_day(g, false, false);
        }
        left[keep] = g->inv[d->item_sheep];
        if (!keep) {
            bool noted = false;
            for (int j = journal0; j < g->journal_count; j++) {
                noted = noted || strncmp(g->journal[j].text.key, "journal.sheep_eaten", 19) == 0;
            }
            CHECK(noted);
        }
        game_free(g);
    }
    CHECK(left[0] < 45);       /* eating the flock: several sheep in three weeks */
    CHECK(left[1] == 50);      /* keeping it, with plenty of meal: none */
}

/* Medicine goes only to the sick who are not kept back from it, and the journal says so. */
static void test_medicine(const GameData *d)
{
    int used[2];
    for (int give = 0; give < 2; give++) {
        Game *g = play(d, 6, data_find_family(d, "grensboer"), 30);
        int med = d->item_medicine;
        g->inv[med] = 20;
        for (int i = 0; i < g->party_count; i++) {
            g->party[i].cond_count = 0;
            game_set_medicine(g, i, give);
        }
        g->party[1].conds[0] = (MemberCond){.illness = data_find_illness(d, "flux"), .days = 5, .severity = 60};
        g->party[1].cond_count = 1;
        int journal0 = g->journal_count;
        for (int day = 0; day < 4; day++) {
            g_pass_day(g, false, false);
        }
        used[give] = 20 - g->inv[med];
        if (give) {
            bool noted = false;
            for (int j = journal0; j < g->journal_count; j++) {
                noted = noted || strcmp(g->journal[j].text.key, "journal.medicine") == 0;
            }
            CHECK(noted);
        }
        game_free(g);
    }
    CHECK(used[0] == 0); /* kept back: none used, even with a sick person */
    CHECK(used[1] > 0);

    /* the trek option: by hand, only those chosen get it; a choice for one person stands */
    Game *g = play(d, 6, data_find_family(d, "grensboer"), 30);
    game_set_medicine_by_hand(g, true);
    CHECK(!game_gives_medicine(g, 1));
    game_set_medicine(g, 1, true);
    CHECK(game_gives_medicine(g, 1));
    CHECK(!game_gives_medicine(g, 2));
    game_set_medicine_by_hand(g, false);
    CHECK(game_gives_medicine(g, 2));
    game_set_medicine(g, 2, false);
    CHECK(!game_gives_medicine(g, 2));
    game_free(g);
}

static void test_mend(const GameData *d)
{
    Game *g = NULL;
    for (int steps = 20; steps < 400 && !g; steps += 7) {
        g = play(d, 3, data_find_family(d, "grensboer"), steps);
        if (game_phase(g) != PHASE_TRAVEL || g->edge < 0) {
            game_free(g);
            g = NULL;
        }
    }
    CHECK(g != NULL);
    if (!g) {
        return;
    }
    for (int p = 0; p < WAGON_PART_COUNT; p++) {
        g->wagon_parts[p] = 100;
    }
    CHECK(!game_can_mend(g));
    CHECK(g_smith_cost(g) == 0);
    int spare = d->item_spare[WAGON_PART_WHEELS], canvas = d->item_spare[WAGON_PART_TENT];
    g->wagon_parts[WAGON_PART_WHEELS] = 30; /* badly worn: the spare goes on */
    g->wagon_parts[WAGON_PART_AXLE] = 60;   /* worn: mended with iron and greased */
    g->wagon_parts[WAGON_PART_TENT] = 20;   /* torn: new canvas */
    g->inv[spare] = 1;
    g->inv[canvas] = 1;
    g->inv[d->item_iron] = 10;
    g->inv[d->item_tallow] = 10;
    CHECK(g_smith_cost(g) > 0);
    CHECK(game_can_mend(g));
    int day = g->day;
    game_mend_wagon(g);
    CHECK(g->day == day + 1);
    CHECK(g->inv[spare] == 0);
    CHECK(g->inv[canvas] == 0);
    CHECK(g->inv[d->item_iron] == 10 - d->rules.mend_iron_lb);
    CHECK(g->inv[d->item_tallow] == 10 - d->rules.mend_tallow_lb);
    CHECK(g->notice_count >= 3); /* spare, canvas, and what was done */
    game_free(g);
}

/* Every member has a portrait from the family; it survives a save, and a save from before
 * portraits gets them back from the family. */
static void test_portraits(const GameData *d)
{
    for (int f = 0; f < d->family_count; f++) {
        Game *g = play(d, 5, f, 30);
        const FamilyDef *fam = &d->families[f];
        for (int i = 0; i < g->party_count; i++) {
            CHECK(g->party[i].portrait[0] != '\0');
            CHECK(strcmp(g->party[i].portrait, fam->members[i].portrait) == 0);
        }
        char *json = game_save_json(g);
        /* blank every portrait id, as an old save would have none */
        for (int i = 0; i < fam->member_count; i++) {
            char want[48];
            snprintf(want, sizeof want, "\"%s\"", fam->members[i].portrait);
            for (char *at = strstr(json, want); at; at = strstr(at, want)) {
                size_t n = strlen(want);
                memmove(at + 2, at + n, strlen(at + n) + 1);
                at[0] = '"';
                at[1] = '"';
            }
        }
        char err[256];
        Game *h = game_load_json(d, json, err, sizeof err);
        CHECK(h != NULL);
        if (h) {
            for (int i = 0; i < h->party_count; i++) {
                CHECK(strcmp(h->party[i].portrait, fam->members[i].portrait) == 0);
            }
            game_free(h);
        }
        free(json);
        game_free(g);
    }
}

int main(void)
{
    test_calendar();
    char err[512];
    GameData *d = data_load(TREK_DATA_DIR, err, sizeof err, NULL);
    CHECK(d != NULL);
    if (!d) {
        fprintf(stderr, "%s\n", err);
        return TEST_REPORT();
    }
    test_wagon_trade(d);
    test_hunt(d);
    test_defend(d);
    test_far_north(d);
    test_flock(d);
    test_medicine(d);
    test_mend(d);
    test_portraits(d);
    for (int f = 0; f < d->family_count; f++) {
        for (uint64_t seed = 1; seed <= 3; seed++) {
            Game *a = play(d, seed, f, -1);
            Game *b = play(d, seed, f, -1);
            CHECK(game_phase(a) == PHASE_ENDED);
            /* Same seed and same choices: identical games. */
            char *sa = game_save_json(a);
            char *sb = game_save_json(b);
            CHECK(strcmp(sa, sb) == 0);
            free(sa);
            free(sb);
            game_free(a);
            game_free(b);
        }
    }
    /* Save part way, reload, and the save reproduces itself exactly. */
    Game *g = play(d, 42, 0, 40);
    char *s1 = game_save_json(g);
    Game *h = game_load_json(d, s1, err, sizeof err);
    CHECK(h != NULL);
    if (h) {
        char *s2 = game_save_json(h);
        CHECK(strcmp(s1, s2) == 0);
        free(s2);
        game_free(h);
    }
    free(s1);
    game_free(g);
    data_free(d);
    return TEST_REPORT();
}
