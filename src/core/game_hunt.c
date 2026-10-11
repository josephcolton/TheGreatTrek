/* Turn-based flintlock hunting (04_game_design.md section 9).
 * One shot, then a slow reload; misfires depend on the weather; wounded animals run,
 * and some turn on the hunter. Meat beyond what keeps fresh is dried as biltong. */
#include "game_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MEAT_KEEP_LB 60

static void hunt_log(Game *g, const char *key, const char *animal_key, int n)
{
    HuntState *h = &g->hunt;
    if (h->log_count == HUNT_LOG) {
        memmove(&h->log[0], &h->log[1], sizeof h->log[0] * (HUNT_LOG - 1));
        h->log_count--;
    }
    LocText *t = &h->log[h->log_count++];
    loctext_set(t, key);
    if (h->hunter >= 0) {
        loctext_arg_str(t, "who", g->party[h->hunter].name);
    }
    if (animal_key) {
        loctext_arg_key(t, "animal", animal_key);
    }
    loctext_arg_int(t, "n", n);
}

static const AnimalDef *cur_animal(const Game *g)
{
    return g->hunt.animal >= 0 ? &g->data->animals[g->hunt.animal] : NULL;
}

static int pick_animal(Game *g)
{
    const GameData *d = g->data;
    int region = g_current_region(g);
    double w[MAX_ANIMALS];
    double total = 0;
    for (int i = 0; i < d->animal_count; i++) {
        w[i] = d->animals[i].region_weight[region];
        if (d->animals[i].tier != TIER_PREDATOR) {
            w[i] *= g->region_game[region];
        }
        total += w[i];
    }
    /* Hunted-out country often shows nothing at all. */
    if (total <= 0 || !rng_chance(&g->rng, 0.35 + 0.6 * g->region_game[region])) {
        return -1;
    }
    return rng_weighted(&g->rng, w, d->animal_count);
}

bool game_can_hunt(const Game *g)
{
    const GameData *d = g->data;
    if (g->phase != PHASE_TRAVEL || g->edge < 0 || g->last_hunt_day == g->day) {
        return false;
    }
    if (g->inv[d->item_gunpowder] < 1 || g->inv[d->item_lead] < 1) {
        return false;
    }
    return g_best_skill_member(g, SKILL_SHOOTING) >= 0;
}

void game_hunt_start(Game *g)
{
    if (!game_can_hunt(g)) {
        return;
    }
    const GameData *d = g->data;
    HuntState *h = &g->hunt;
    memset(h, 0, sizeof *h);
    h->active = true;
    h->hunter = -1;
    g->last_hunt_day = g->day;
    int best = -1;
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_healthy(g, i) && (best < 0 || g->party[i].skills[SKILL_SHOOTING] > g->party[best].skills[SKILL_SHOOTING])) {
            best = i;
        }
    }
    h->hunter = best >= 0 ? best : g_best_skill_member(g, SKILL_SHOOTING);
    h->carry = d->hunt.carry_on_foot + (g->inv[d->item_horses] > 0 ? d->hunt.carry_per_horse : 0);
    h->loaded = true;
    h->animal = pick_animal(g);
    h->range = rng_range(&g->rng, 0, 1);
    /* A hunt takes half a day off the road. */
    g->miles_along -= 0.5 * d->rules.miles[g->pace];
    if (g->miles_along < 0) {
        g->miles_along = 0;
    }
    const AnimalDef *a = cur_animal(g);
    hunt_log(g, a ? "hunt.start" : "hunt.start_empty", a ? a->name_key : NULL, 0);
    g_settle(g);
}

bool game_hunt_view(const Game *g, HuntView *out)
{
    memset(out, 0, sizeof *out);
    if (g->phase != PHASE_HUNT) {
        return false;
    }
    const GameData *d = g->data;
    const HuntState *h = &g->hunt;
    const AnimalDef *a = cur_animal(g);
    loctext_set(&out->hunter, "hunt.hunter");
    if (h->hunter >= 0) {
        loctext_arg_str(&out->hunter, "who", g->party[h->hunter].name);
    }
    if (a) {
        loctext_set(&out->animal, a->name_key);
        snprintf(out->animal_id, sizeof out->animal_id, "%s", a->id);
        out->tier = a->tier;
    }
    out->range = h->range;
    out->loaded = h->loaded;
    out->reload_left = h->reload_left;
    out->wounded = h->wounded;
    out->turns_left = d->hunt.max_turns - h->turn;
    out->meat = h->meat;
    out->carry = h->carry;
    out->powder = g->inv[d->item_gunpowder];
    out->lead = g->inv[d->item_lead];
    out->over = h->over;
    out->log_count = h->log_count;
    memcpy(out->log, h->log, sizeof out->log);
    if (h->over) {
        out->can[HUNT_LEAVE] = true;
        return true;
    }
    out->can[HUNT_STALK] = a && h->range < 3 && !(h->wounded && h->range == 0);
    out->can[HUNT_WAIT] = true;
    out->can[HUNT_FIRE] = a && h->loaded;
    out->can[HUNT_RELOAD] = !h->loaded;
    out->can[HUNT_FOLLOW] = a && h->wounded && h->range == 0;
    out->can[HUNT_LEAVE] = true;
    return true;
}

static void hunter_hurt(Game *g, const AnimalDef *a)
{
    HuntState *h = &g->hunt;
    int harm = (int)lround(rng_range(&g->rng, 15, 40) * g_harm(g));
    g->party[h->hunter].health -= harm;
    hunt_log(g, "hunt.charged", a->name_key, harm);
    if (g->party[h->hunter].health <= 0) {
        g_kill(g, h->hunter, "cause.hunting");
        hunt_log(g, "hunt.hunter_died", a->name_key, 0);
        h->over = true;
        return;
    }
    if (g->party[h->hunter].cond_count == 0) {
        int injury = data_find_illness(g->data, "injury");
        g_add_illness(g, h->hunter, injury);
    }
    h->over = true; /* nobody goes on hunting after that */
}

static void fire(Game *g)
{
    const GameData *d = g->data;
    HuntState *h = &g->hunt;
    const AnimalDef *a = cur_animal(g);
    h->shots++;
    h->loaded = false;
    h->reload_left = d->hunt.reload_turns;
    if (rng_chance(&g->rng, d->hunt.misfire[g->weather])) {
        h->reload_left = 1; /* only the pan to prime again */
        hunt_log(g, "hunt.misfire", a->name_key, 0);
        if (rng_chance(&g->rng, a->spook)) {
            hunt_log(g, "hunt.ran", a->name_key, 0);
            h->animal = -1;
            h->wounded = false;
        }
        return;
    }
    int skill = g->party[h->hunter].skills[SKILL_SHOOTING];
    double p = d->hunt.hit_by_range[h->range] * a->size * (0.55 + 0.12 * skill) * d->families[g->family].hunt_mul;
    if (p > 0.95) {
        p = 0.95;
    }
    if (!rng_chance(&g->rng, p)) {
        hunt_log(g, "hunt.miss", a->name_key, 0);
        if (rng_chance(&g->rng, 0.6)) {
            hunt_log(g, "hunt.ran", a->name_key, 0);
            h->animal = -1;
            h->wounded = false;
        }
        return;
    }
    double kill = 0.55 + 0.1 * h->range + (h->wounded ? 0.2 : 0);
    if (rng_chance(&g->rng, kill)) {
        h->kills++;
        int region = g_current_region(g);
        g->region_game[region] -= 0.08;
        if (g->region_game[region] < 0.1) {
            g->region_game[region] = 0.1;
        }
        if (a->meat > 0) {
            int got = a->meat;
            if (got > h->carry - h->meat) {
                got = h->carry - h->meat;
            }
            h->meat += got;
            hunt_log(g, got < a->meat ? "hunt.kill_too_much" : "hunt.kill", a->name_key, got);
        } else {
            hunt_log(g, "hunt.kill_predator", a->name_key, 0);
        }
        h->animal = -1;
        h->wounded = false;
        if (h->meat >= h->carry) {
            hunt_log(g, "hunt.full", NULL, h->meat);
            h->over = true;
        }
        return;
    }
    h->wounded = true;
    hunt_log(g, "hunt.wounded", a->name_key, 0);
    if (a->charge > 0 && rng_chance(&g->rng, a->charge)) {
        hunter_hurt(g, a);
        return;
    }
    if (rng_chance(&g->rng, a->flee_wounded)) {
        hunt_log(g, "hunt.wounded_ran", a->name_key, 0);
        h->range = 0;
    }
}

static void finish(Game *g)
{
    const GameData *d = g->data;
    HuntState *h = &g->hunt;
    int fresh_room = MEAT_KEEP_LB - g->inv[d->item_meat];
    if (fresh_room < 0) {
        fresh_room = 0;
    }
    int fresh = h->meat < fresh_room ? h->meat : fresh_room;
    int dried = (int)lround((h->meat - fresh) * d->hunt.biltong_yield);
    g->inv[d->item_meat] += fresh;
    g->inv[d->item_biltong] += dried;
    int per = d->hunt.shots_per_lb > 0 ? d->hunt.shots_per_lb : 12;
    int lb = (h->shots + per - 1) / per;
    g_add_item(g, d->item_gunpowder, -lb);
    g_add_item(g, d->item_lead, -lb);
    LocText t;
    loctext_set(&t, h->meat > 0 ? "journal.hunt" : "journal.hunt_empty");
    if (h->hunter >= 0) {
        loctext_arg_str(&t, "who", g->party[h->hunter].name);
    }
    loctext_arg_int(&t, "fresh", fresh);
    loctext_arg_int(&t, "dried", dried);
    game_journal(g, &t);
    if (h->meat > 0) {
        g_add_morale(g, 3);
    }
    memset(h, 0, sizeof *h);
    h->animal = -1;
    g_settle(g);
}

void game_hunt_act(Game *g, HuntAction act)
{
    if (g->phase != PHASE_HUNT) {
        return;
    }
    HuntView v;
    game_hunt_view(g, &v);
    if (act < 0 || act >= HUNT_ACTION_COUNT || !v.can[act]) {
        return;
    }
    HuntState *h = &g->hunt;
    if (h->over) {
        finish(g);
        return;
    }
    const AnimalDef *a = cur_animal(g);
    int tracking = g_best_skill(g, SKILL_TRACKING, true);
    switch (act) {
    case HUNT_STALK: {
        double spook = a->spook * (1.0 - 0.08 * tracking) + (g->weather == WEATHER_WINDY ? 0.1 : 0.0);
        if (rng_chance(&g->rng, spook)) {
            hunt_log(g, "hunt.spooked", a->name_key, 0);
            h->animal = -1;
            h->wounded = false;
        } else {
            h->range++;
            hunt_log(g, "hunt.closer", a->name_key, 0);
        }
        break;
    }
    case HUNT_WAIT:
        if (!a) {
            h->animal = pick_animal(g);
            h->range = rng_range(&g->rng, 0, 1);
            a = cur_animal(g);
            hunt_log(g, a ? "hunt.appears" : "hunt.nothing", a ? a->name_key : NULL, 0);
        } else if (rng_chance(&g->rng, 0.3) && h->range < 3) {
            h->range++;
            hunt_log(g, "hunt.drifts_closer", a->name_key, 0);
        } else if (rng_chance(&g->rng, 0.25)) {
            if (h->range == 0) {
                hunt_log(g, "hunt.ran", a->name_key, 0);
                h->animal = -1;
                h->wounded = false;
            } else {
                h->range--;
                hunt_log(g, "hunt.drifts_away", a->name_key, 0);
            }
        } else {
            hunt_log(g, "hunt.waits", a->name_key, 0);
        }
        break;
    case HUNT_FIRE: fire(g); break;
    case HUNT_RELOAD:
        if (--h->reload_left <= 0) {
            h->loaded = true;
            h->reload_left = 0;
            hunt_log(g, "hunt.loaded", NULL, 0);
        } else {
            hunt_log(g, "hunt.reloading", NULL, h->reload_left);
        }
        break;
    case HUNT_FOLLOW:
        if (rng_chance(&g->rng, 0.4 + 0.1 * tracking)) {
            h->range = 2;
            hunt_log(g, "hunt.followed", a->name_key, 0);
        } else {
            hunt_log(g, "hunt.lost_spoor", a->name_key, 0);
            h->animal = -1;
            h->wounded = false;
        }
        break;
    case HUNT_LEAVE:
        h->over = true;
        hunt_log(g, "hunt.give_up", NULL, h->meat);
        return;
    default: break;
    }
    /* A lion close by may come for the hunter whatever he is doing. */
    a = cur_animal(g);
    if (!h->over && a && a->tier == TIER_PREDATOR && h->range >= 2 && act != HUNT_FIRE && rng_chance(&g->rng, 0.25)) {
        hunter_hurt(g, a);
    }
    h->turn++;
    if (!h->over && h->turn >= g->data->hunt.max_turns) {
        hunt_log(g, "hunt.sundown", NULL, h->meat);
        h->over = true;
    }
}
