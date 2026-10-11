/* Plays whole games headlessly: they must finish, be deterministic, and survive save/load. */
#include "calendar.h"
#include "data.h"
#include "game.h"
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
            game_hunt_act(g, hv.can[HUNT_FIRE] ? HUNT_FIRE : (hv.can[HUNT_RELOAD] ? HUNT_RELOAD : HUNT_LEAVE));
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
