/* trekbot: plays many headless games to check that runs finish and to tune balance.
 * Usage: trekbot [--runs N] [--seed S] [--family ID|all] [--policy random|cautious|reckless]
 *                [--difficulty easy|normal|hard] [--data DIR] [--verbose] [--savecheck]
 *                [--dump FILE --stop-at grave|river|store|route|ending|barter|hunt|landmark]  (save one game at that moment)
 * Prints arrival rate, days taken, deaths and causes, and events never seen. */
#include "calendar.h"
#include "data.h"
#include "format.h"
#include "game.h"
#include "locale.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { POLICY_RANDOM, POLICY_CAUTIOUS, POLICY_RECKLESS } Policy;

typedef struct {
    int runs, arrived, ended[8];
    long long days, deaths, survivors;
    int min_days, max_days;
    int *event_seen;
    int cause_counts[32];
    char cause_keys[32][KEY_LEN];
    int cause_n;
} Stats;

static void count_cause(Stats *s, const char *key)
{
    for (int i = 0; i < s->cause_n; i++) {
        if (strcmp(s->cause_keys[i], key) == 0) {
            s->cause_counts[i]++;
            return;
        }
    }
    if (s->cause_n < 32) {
        snprintf(s->cause_keys[s->cause_n], KEY_LEN, "%s", key);
        s->cause_counts[s->cause_n++] = 1;
    }
}

static void outfit(Game *g, Policy policy)
{
    const GameData *d = g->data;
    const WagonDef *w = &d->wagons[g->wagon];
    int oxen_want = policy == POLICY_RECKLESS ? w->min_oxen : w->full_span + 2;
    int item;
    if (g->inv[d->item_oxen] < oxen_want) {
        game_store_buy(g, d->item_oxen, oxen_want - g->inv[d->item_oxen]);
    }
    if (policy != POLICY_RECKLESS) {
        if ((item = data_find_item(d, "spare_wheel")) >= 0 && g->inv[item] < 1) {
            game_store_buy(g, item, 1);
        }
        if ((item = data_find_item(d, "spare_axle")) >= 0 && g->inv[item] < 1) {
            game_store_buy(g, item, 1);
        }
        if (g->inv[d->item_vat] < 4) {
            game_store_buy(g, d->item_vat, 4 - g->inv[d->item_vat]);
        }
        if ((item = data_find_item(d, "medicine")) >= 0 && g->inv[item] < 8) {
            game_store_buy(g, item, 8 - g->inv[item]);
        }
        if ((item = data_find_item(d, "tallow")) >= 0 && g->inv[item] < 20) {
            game_store_buy(g, item, 1);
        }
    }
    if ((item = data_find_item(d, "meal")) >= 0 && game_food_days(g) < 110) {
        game_store_buy(g, item, 2);
    }
}

static int pick_choice(Game *g, const DialogView *v, Policy policy, TrekRng *bot_rng)
{
    int enabled[8], n = 0;
    for (int i = 0; i < v->choice_count; i++) {
        if (v->choices[i].enabled) {
            enabled[n++] = i;
        }
    }
    if (n == 0) {
        return 0;
    }
    if (v->kind == DLG_LANDMARK) {
        /* Trade once at each shop (handled via a flag-like check), then move on. */
        return enabled[n - 1];
    }
    if (v->kind == DLG_RIVER && policy == POLICY_CAUTIOUS) {
        /* cautious: carry goods if possible, else ford, else float, else wait */
        if (v->choices[2].enabled) {
            return 2;
        }
        if (v->choices[0].enabled) {
            return 0;
        }
        if (v->choices[1].enabled) {
            return 1;
        }
        return 3;
    }
    if (v->kind == DLG_ROUTE) {
        return enabled[(g->seed / 2) % (uint64_t)n]; /* half the runs take each road */
    }
    if (v->kind == DLG_STRANDED) {
        if (v->choices[2].enabled) {
            return 2;
        }
        return g->day - g->start_day > 300 ? 1 : 0;
    }
    (void)g;
    if (policy == POLICY_CAUTIOUS || policy == POLICY_RECKLESS) {
        return enabled[0];
    }
    return enabled[rng_below(bot_rng, (uint32_t)n)];
}

static bool visited_shop[MAX_NODES];
static bool bartered[MAX_NODES];
static const char *dump_path, *stop_at;

/* For testing the front end: write the save when the game reaches a chosen moment. */
static bool maybe_dump(Game *g)
{
    if (!dump_path || !stop_at) {
        return false;
    }
    bool hit = false;
    DialogView v;
    if (strcmp(stop_at, "grave") == 0) {
        hit = game_phase(g) == PHASE_GRAVE;
    } else if (strcmp(stop_at, "ending") == 0) {
        hit = game_phase(g) == PHASE_ENDED;
    } else if (strcmp(stop_at, "barter") == 0) {
        hit = game_phase(g) == PHASE_BARTER;
    } else if (strcmp(stop_at, "hunt") == 0) {
        hit = game_phase(g) == PHASE_HUNT;
    } else if (strcmp(stop_at, "store") == 0) {
        hit = game_phase(g) == PHASE_STORE;
    } else if (strcmp(stop_at, "river") == 0) {
        hit = game_phase(g) == PHASE_DIALOG && game_dialog_view(g, &v) && v.kind == DLG_RIVER;
    } else if (strcmp(stop_at, "landmark") == 0) {
        hit = game_phase(g) == PHASE_DIALOG && game_dialog_view(g, &v) && v.kind == DLG_LANDMARK;
    } else if (strcmp(stop_at, "route") == 0) {
        hit = game_phase(g) == PHASE_DIALOG && game_dialog_view(g, &v) && v.kind == DLG_ROUTE && g->day > g->start_day;
    }
    if (hit) {
        char *json = game_save_json(g);
        util_write_file(dump_path, json, strlen(json));
        free(json);
        dump_path = NULL;
    }
    return hit;
}

/* Plays a whole hunt like a fair human shot: waits a moment after taking aim, aims at the body
 * with some error, shoots anything coming at it first, and goes home once there is enough meat. */
static void bot_hunt(Game *g, TrekRng *bot_rng)
{
    int aim = 0;
    for (int t = 0; t < 100000 && game_phase(g) == PHASE_HUNT && !g->hunt.over; t++) {
        game_hunt_tick(g);
        HuntView hv;
        if (!game_hunt_view(g, &hv) || hv.over) {
            break;
        }
        if (hv.meat >= hv.carry) {
            game_hunt_quit(g);
            break;
        }
        const HuntAnimal *target = NULL;
        for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
            const HuntAnimal *a = &hv.animals[i];
            if (a->state == HUNT_A_NONE || a->state == HUNT_A_DOWN || a->x < 10 || a->x > HUNT_FIELD_W - 10) {
                continue;
            }
            if (!target || (a->state == HUNT_A_CHARGE) > (target->state == HUNT_A_CHARGE) ||
                ((a->state == HUNT_A_CHARGE) == (target->state == HUNT_A_CHARGE) && a->y > target->y)) {
                target = a;
            }
        }
        if (!hv.loaded || !target) {
            aim = 0;
            continue;
        }
        if (++aim < 15) {
            continue;
        }
        aim = 0;
        float box[4];
        game_hunt_body(g, target, box);
        double x = box[0] + box[2] / 2 + rng_range(bot_rng, -6, 6);
        double y = box[1] + box[3] * 0.55 + rng_range(bot_rng, -4, 4);
        game_hunt_shoot(g, x, y);
    }
    game_hunt_finish(g);
}

static int play(const GameData *d, const Locale *loc, uint64_t seed, int family, Policy policy, Difficulty diff,
                bool verbose, bool savecheck, Stats *st)
{
    Game *g = game_new(d, seed, family, diff);
    if (!g) {
        return -1;
    }
    TrekRng bot_rng;
    rng_seed(&bot_rng, seed, 99);
    memset(visited_shop, 0, sizeof visited_shop);
    memset(bartered, 0, sizeof bartered);
    if (policy == POLICY_RECKLESS) {
        game_set_orders(g, PACE_HARD, RATIONS_MEAGRE, NIGHT_REST, false);
    } else if (policy == POLICY_CAUTIOUS) {
        game_set_orders(g, PACE_NORMAL, RATIONS_FULL, NIGHT_LAAGER, true);
    }
    int steps = 0;
    while (game_phase(g) != PHASE_ENDED && steps < 20000) {
        steps++;
        if (maybe_dump(g)) {
            break;
        }
        if (savecheck && steps % 37 == 0) {
            /* Round-trip through a save: the game must carry on identically. */
            char *a = game_save_json(g);
            char err[256];
            Game *h = game_load_json(d, a, err, sizeof err);
            if (!h) {
                fprintf(stderr, "seed %llu: reload failed: %s\n", (unsigned long long)seed, err);
                free(a);
                game_free(g);
                return -1;
            }
            char *b = game_save_json(h);
            if (strcmp(a, b) != 0) {
                fprintf(stderr, "seed %llu: save round trip differs at step %d\n", (unsigned long long)seed, steps);
                free(a);
                free(b);
                game_free(h);
                game_free(g);
                return -1;
            }
            free(a);
            free(b);
            game_free(g);
            g = h;
        }
        switch (game_phase(g)) {
        case PHASE_TRAVEL:
            if (game_food_days(g) < 3 && g->rations == RATIONS_FULL) {
                game_set_orders(g, g->pace, RATIONS_MEAGRE, g->stance, g->sabbath);
            }
            /* Hunt when food runs short, and hunting families hunt every week or so. */
            if (game_can_hunt(g) && (game_food_days(g) < 30 ||
                                     (d->families[family].hunt_mul > 1.0 && (g->day - g->start_day) % 7 == 3))) {
                game_hunt_start(g);
                break;
            }
            game_travel(g, policy == POLICY_RECKLESS ? 30 : 7);
            break;
        case PHASE_BARTER: {
            /* Offer beads and tobacco for oxen if the span is short, then leave. */
            BarterView bv;
            game_barter_view(g, &bv);
            if (g->inv[d->item_oxen] < d->wagons[g->wagon].full_span) {
                for (int r = 0; r < bv.take_count; r++) {
                    if (bv.take[r].item == d->item_oxen) {
                        game_barter_adjust(g, true, r, 1);
                    }
                }
                for (int r = 0; r < bv.give_count; r++) {
                    for (int k = 0; k < 20; k++) {
                        game_barter_view(g, &bv);
                        if (bv.give_value >= bv.need_value || bv.give[r].item == d->item_gunpowder) {
                            break;
                        }
                        game_barter_adjust(g, false, r, 1);
                    }
                }
                game_barter_propose(g);
            }
            game_barter_leave(g);
            break;
        }
        case PHASE_HUNT: bot_hunt(g, &bot_rng); break;
        case PHASE_STORE:
            outfit(g, policy);
            game_store_leave(g);
            visited_shop[g->node] = true;
            break;
        case PHASE_GRAVE: {
            const Grave *gr = game_pending_grave(g);
            if (gr) {
                count_cause(st, gr->cause_key);
            }
            game_grave_done(g, "");
            break;
        }
        case PHASE_DIALOG: {
            DialogView v;
            game_dialog_view(g, &v);
            int c;
            if (v.kind == DLG_LANDMARK && !visited_shop[g->node] && g->data->nodes[g->node].shop >= 0 &&
                v.choice_count > 1) {
                c = 1; /* Trade */
                visited_shop[g->node] = true;
            } else if (v.kind == DLG_LANDMARK && game_barter_partner_here(g) >= 0 && !bartered[g->node]) {
                bartered[g->node] = true;
                c = d->nodes[g->node].shop >= 0 ? 2 : 1; /* Barter comes after Trade */
            } else {
                c = pick_choice(g, &v, policy, &bot_rng);
            }
            if (verbose && loc) {
                char buf[600];
                for (int i = 0; i < v.body_count; i++) {
                    fmt_game(loc, g, &v.body[i], buf, sizeof buf);
                    printf("    | %s\n", buf);
                }
                if (c < v.choice_count) {
                    fmt_game(loc, g, &v.choices[c].text, buf, sizeof buf);
                    printf("    > %s\n", buf);
                }
            }
            game_choose(g, c);
            break;
        }
        default: break;
        }
    }
    maybe_dump(g);
    int days = g->day - g->start_day;
    st->runs++;
    st->ended[g->ending]++;
    if (g->ending == ENDING_ARRIVED) {
        st->arrived++;
        st->days += days;
        if (days < st->min_days || st->min_days == 0) {
            st->min_days = days;
        }
        if (days > st->max_days) {
            st->max_days = days;
        }
    }
    st->deaths += g->deaths;
    st->survivors += game_alive_count(g);
    for (int i = 0; i < d->event_count; i++) {
        if (g->event_last_day[i] >= 0) {
            st->event_seen[i]++;
        }
    }
    if (verbose) {
        char date[64];
        if (loc) {
            fmt_date(loc, g->day, date, sizeof date);
        } else {
            cal_format_iso(g->day, date, sizeof date);
        }
        printf("seed %llu: ending %d after %d days (%s), %d miles, %d deaths, %d oxen, score %d\n",
               (unsigned long long)seed, g->ending, days, date, g->miles_total, g->deaths,
               g->inv[d->item_oxen], game_score(g));
    }
    int result = steps >= 20000 ? -1 : 0;
    if (result < 0) {
        fprintf(stderr, "seed %llu: game did not finish (stuck in phase %d, dialog %d, node %s, day %d, oxen %d)\n",
                (unsigned long long)seed, game_phase(g), g->dlg.kind, d->nodes[g->node].id, g->day - g->start_day,
                g->inv[d->item_oxen]);
    }
    game_free(g);
    return result;
}

int main(int argc, char **argv)
{
    int runs = 50;
    uint64_t seed = 1;
    const char *family_id = "all";
    const char *data_dir = "data";
    Policy policy = POLICY_CAUTIOUS;
    Difficulty diff = DIFF_NORMAL;
    bool verbose = false, savecheck = false;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *next = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(a, "--runs") == 0 && next) {
            runs = atoi(argv[++i]);
        } else if (strcmp(a, "--seed") == 0 && next) {
            seed = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(a, "--family") == 0 && next) {
            family_id = argv[++i];
        } else if (strcmp(a, "--data") == 0 && next) {
            data_dir = argv[++i];
        } else if (strcmp(a, "--policy") == 0 && next) {
            const char *p = argv[++i];
            policy = strcmp(p, "random") == 0 ? POLICY_RANDOM : strcmp(p, "reckless") == 0 ? POLICY_RECKLESS : POLICY_CAUTIOUS;
        } else if (strcmp(a, "--difficulty") == 0 && next) {
            const char *p = argv[++i];
            diff = strcmp(p, "easy") == 0 ? DIFF_EASY : strcmp(p, "hard") == 0 ? DIFF_HARD : DIFF_NORMAL;
        } else if (strcmp(a, "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(a, "--savecheck") == 0) {
            savecheck = true;
        } else if (strcmp(a, "--dump") == 0 && next) {
            dump_path = argv[++i];
        } else if (strcmp(a, "--stop-at") == 0 && next) {
            stop_at = argv[++i];
        } else {
            fprintf(stderr, "unknown option %s\n", a);
            return 2;
        }
    }
    char err[512];
    GameData *d = data_load(data_dir, err, sizeof err, NULL);
    if (!d) {
        fprintf(stderr, "error: %s\n", err);
        return 1;
    }
    char *loc_dir = util_path_join(data_dir, "locale/en");
    Locale *loc = locale_load_dir(loc_dir, err, sizeof err);
    free(loc_dir);

    int failures = 0;
    int *seen_any = calloc((size_t)d->event_count, sizeof(int));
    for (int f = 0; f < d->family_count; f++) {
        if (strcmp(family_id, "all") != 0 && strcmp(family_id, d->families[f].id) != 0) {
            continue;
        }
        Stats st;
        memset(&st, 0, sizeof st);
        st.event_seen = calloc((size_t)d->event_count, sizeof(int));
        for (int r = 0; r < runs; r++) {
            if (play(d, loc, seed + (uint64_t)r, f, policy, diff, verbose, savecheck, &st) < 0) {
                failures++;
            }
        }
        printf("%-10s runs %3d  arrived %3d%%  days %3lld avg (%d-%d)  deaths/run %.2f  survivors/run %.1f",
               d->families[f].id, st.runs, st.runs ? 100 * st.arrived / st.runs : 0,
               st.arrived ? st.days / st.arrived : 0, st.min_days, st.max_days,
               st.runs ? (double)st.deaths / st.runs : 0, st.runs ? (double)st.survivors / st.runs : 0);
        printf("  endings: stranded %d lost %d\n", st.ended[ENDING_STRANDED], st.ended[ENDING_LOST]);
        if (st.cause_n) {
            printf("           causes:");
            for (int i = 0; i < st.cause_n; i++) {
                printf(" %s=%d", st.cause_keys[i], st.cause_counts[i]);
            }
            printf("\n");
        }
        for (int i = 0; i < d->event_count; i++) {
            seen_any[i] += st.event_seen[i];
        }
        free(st.event_seen);
    }
    for (int i = 0; i < d->event_count; i++) {
        if (!seen_any[i] && !d->events[i].setpiece) {
            printf("never seen: %s\n", d->events[i].id);
        }
    }
    free(seen_any);
    locale_free(loc);
    data_free(d);
    return failures ? 1 : 0;
}
