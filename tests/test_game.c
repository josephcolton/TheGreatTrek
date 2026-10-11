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
    test_mend(d);
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
