/* Real-time hunting (04_game_design.md section 9), in the spirit of the Oregon Trail hunt:
 * wait for game to come into sight, then aim and fire with the mouse. The scene runs in fixed
 * ticks with the game's own RNG, so a hunt replays exactly from the seed and the shots.
 * A muzzle-loader fires once and is slow to load; rain makes the powder fail; shots scatter
 * less for a good shot; wounded buffalo and lions may come at the hunter.
 * Only so much meat can be carried back; meat beyond what keeps fresh is dried as biltong. */
#include "game_internal.h"

#include <math.h>
#include <string.h>

#define MEAT_KEEP_LB 60
#define TICK (1.0 / HUNT_TICKS_PER_SEC)

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
        loctext_arg_person(t, "who", &g->party[h->hunter]);
    }
    if (animal_key) {
        loctext_arg_key(t, "animal", animal_key);
    }
    loctext_arg_int(t, "n", n);
}

double game_hunt_scale(double y)
{
    if (y < 0) {
        y = 0;
    }
    return 0.9 + 1.1 * y / HUNT_FIELD_H;
}

void game_hunt_body(const Game *g, const HuntAnimal *a, float out[4])
{
    const AnimalDef *def = &g->data->animals[a->kind];
    float s = (float)game_hunt_scale(a->y);
    float w = (float)def->body_w * s, h = (float)(def->body_h + def->legs) * s;
    float head = 5 * s; /* head and neck stick out in front and up */
    out[0] = a->x - w / 2 - (a->dir < 0 ? head : 0);
    out[1] = a->y - h - head;
    out[2] = w + head;
    out[3] = h + head;
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

static int seconds_to_ticks(double s)
{
    return (int)lround(s * HUNT_TICKS_PER_SEC);
}

static void next_spawn(Game *g)
{
    const HuntRules *r = &g->data->hunt;
    double game = g->region_game[g_current_region(g)];
    double s = r->spawn_min + rng_unit(&g->rng) * (r->spawn_max - r->spawn_min);
    g->hunt.spawn_ticks = seconds_to_ticks(s / (game > 0.3 ? game : 0.3));
}

static int live_animals(const HuntState *h)
{
    int n = 0;
    for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
        if (h->animals[i].state != HUNT_A_NONE && h->animals[i].state != HUNT_A_DOWN) {
            n++;
        }
    }
    return n;
}

static HuntAnimal *free_slot(HuntState *h)
{
    for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
        if (h->animals[i].state == HUNT_A_NONE) {
            return &h->animals[i];
        }
    }
    return NULL;
}

/* Game comes into sight from one side, singly or as a small herd. */
static void spawn(Game *g)
{
    HuntState *h = &g->hunt;
    const GameData *d = g->data;
    next_spawn(g);
    if (live_animals(h) >= d->hunt.max_animals) {
        return;
    }
    int kind = pick_animal(g);
    if (kind < 0) {
        return;
    }
    const AnimalDef *def = &d->animals[kind];
    float dir = rng_chance(&g->rng, 0.5) ? 1.0f : -1.0f;
    bool predator = def->tier == TIER_PREDATOR;
    float y = (float)(HUNT_FIELD_H * (predator ? 0.15 + 0.2 * rng_unit(&g->rng) : 0.15 + 0.7 * rng_unit(&g->rng)));
    int count = def->herd > 1 ? rng_range(&g->rng, 1, def->herd) : 1;
    bool running = !predator && rng_chance(&g->rng, 0.35);
    int placed = 0;
    for (int i = 0; i < count; i++) {
        HuntAnimal *a = free_slot(h);
        if (!a) {
            break;
        }
        memset(a, 0, sizeof *a);
        a->kind = kind;
        a->dir = dir;
        a->hp = def->hp;
        a->y = y + (float)rng_range(&g->rng, -12, 12);
        if (a->y < 8) {
            a->y = 8;
        }
        if (a->y > HUNT_FIELD_H - 20) {
            a->y = HUNT_FIELD_H - 20;
        }
        float gap = (float)(def->body_w * game_hunt_scale(a->y) * 1.5) + rng_range(&g->rng, 0, 10);
        a->x = dir > 0 ? -20.0f - i * gap : HUNT_FIELD_W + 20.0f + i * gap;
        a->state = running ? HUNT_A_RUN : HUNT_A_WALK;
        a->timer = seconds_to_ticks(1.5 + 3 * rng_unit(&g->rng));
        placed++;
    }
    if (placed > 1) {
        hunt_log(g, "hunt.appears_herd", def->name_key, placed);
    } else if (placed == 1) {
        hunt_log(g, predator ? "hunt.appears_lion" : "hunt.appears", def->name_key, 1);
    }
}

static void hunter_hurt(Game *g, const AnimalDef *a)
{
    HuntState *h = &g->hunt;
    int harm = (int)lround(rng_range(&g->rng, 15, 40) * g_harm(g));
    g->party[h->hunter].health -= harm;
    hunt_log(g, "hunt.charged", a->name_key, harm);
    h->over = true; /* nobody goes on hunting after that */
    if (g->party[h->hunter].health <= 0) {
        g_kill(g, h->hunter, "cause.hunting");
        hunt_log(g, "hunt.hunter_died", a->name_key, 0);
        return;
    }
    if (g->party[h->hunter].cond_count == 0) {
        g_add_illness(g, h->hunter, data_find_illness(g->data, "injury"));
    }
}

static void charge(Game *g, HuntAnimal *a)
{
    a->state = HUNT_A_CHARGE;
    hunt_log(g, "hunt.charges", g->data->animals[a->kind].name_key, 0);
}

static void move_animal(Game *g, HuntAnimal *a)
{
    const AnimalDef *def = &g->data->animals[a->kind];
    double s = game_hunt_scale(a->y);
    double v = def->speed * s * TICK;
    switch (a->state) {
    case HUNT_A_WALK:
        a->x += (float)(a->dir * v * 0.25);
        if (def->tier == TIER_PREDATOR) {
            /* a lion works its way closer; then it either comes on or thinks better of it */
            a->y += (float)(v * 0.08);
            if (a->y > HUNT_FIELD_H * 0.55) {
                if (rng_chance(&g->rng, def->charge)) {
                    charge(g, a);
                } else {
                    a->state = HUNT_A_RUN;
                    hunt_log(g, "hunt.lion_leaves", def->name_key, 0);
                }
            }
            break;
        }
        if (--a->timer <= 0) {
            bool graze = rng_chance(&g->rng, 0.5);
            a->state = graze ? HUNT_A_GRAZE : HUNT_A_WALK;
            a->timer = seconds_to_ticks(graze ? 1 + 3 * rng_unit(&g->rng) : 1 + 2 * rng_unit(&g->rng));
        }
        break;
    case HUNT_A_GRAZE:
        if (--a->timer <= 0) {
            a->state = HUNT_A_WALK;
            a->timer = seconds_to_ticks(1 + 3 * rng_unit(&g->rng));
        }
        break;
    case HUNT_A_RUN: a->x += (float)(a->dir * v); break;
    case HUNT_A_CHARGE: {
        /* straight at the hunter, who stands at the bottom middle of the field */
        double dx = HUNT_FIELD_W / 2.0 - a->x, dy = HUNT_FIELD_H + 10 - a->y;
        double len = sqrt(dx * dx + dy * dy);
        double step = def->speed * 1.4 * TICK * s;
        if (len <= step || a->y >= HUNT_FIELD_H) {
            a->state = HUNT_A_NONE;
            hunter_hurt(g, def);
            return;
        }
        a->x += (float)(dx / len * step);
        a->y += (float)(dy / len * step);
        a->dir = dx < 0 ? -1.0f : 1.0f;
        break;
    }
    default: break;
    }
    if (a->state != HUNT_A_DOWN && (a->x < -80 - 60 * def->herd || a->x > HUNT_FIELD_W + 80 + 60 * def->herd)) {
        a->state = HUNT_A_NONE; /* gone over the rise */
    }
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
    h->ticks_left = seconds_to_ticks(d->hunt.seconds);
    int lb = g->inv[d->item_gunpowder] < g->inv[d->item_lead] ? g->inv[d->item_gunpowder] : g->inv[d->item_lead];
    h->shots_max = lb * (d->hunt.shots_per_lb > 0 ? d->hunt.shots_per_lb : 12);
    h->shot_age = 1000;
    h->spawn_ticks = seconds_to_ticks(1.0);
    /* A hunt takes half a day off the road. */
    g->miles_along -= 0.5 * d->rules.miles[g->pace];
    if (g->miles_along < 0) {
        g->miles_along = 0;
    }
    hunt_log(g, "hunt.start", NULL, 0);
    g_settle(g);
}

void game_hunt_tick(Game *g)
{
    HuntState *h = &g->hunt;
    if (g->phase != PHASE_HUNT || h->over) {
        return;
    }
    if (h->reload_ticks > 0) {
        h->reload_ticks--;
    }
    if (h->shot_age < 1000) {
        h->shot_age++;
    }
    if (--h->spawn_ticks <= 0) {
        spawn(g);
    }
    for (int i = 0; i < HUNT_MAX_ANIMALS && !h->over; i++) {
        if (h->animals[i].state != HUNT_A_NONE) {
            move_animal(g, &h->animals[i]);
        }
    }
    if (!h->over && --h->ticks_left <= 0) {
        h->ticks_left = 0;
        hunt_log(g, "hunt.sundown", NULL, h->meat);
        h->over = true;
    }
}

/* The bang sends game running; the nearer the shot, the surer. */
static void startle(Game *g, double x)
{
    HuntState *h = &g->hunt;
    for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
        HuntAnimal *a = &h->animals[i];
        if (a->state != HUNT_A_WALK && a->state != HUNT_A_GRAZE) {
            continue;
        }
        const AnimalDef *def = &g->data->animals[a->kind];
        if (def->tier == TIER_PREDATOR) {
            continue;
        }
        if (rng_chance(&g->rng, def->spook)) {
            a->state = HUNT_A_RUN;
            a->dir = a->x < x ? -1.0f : 1.0f;
        }
    }
}

bool game_hunt_shoot(Game *g, double x, double y)
{
    const GameData *d = g->data;
    HuntState *h = &g->hunt;
    if (g->phase != PHASE_HUNT || h->over || h->reload_ticks > 0) {
        return false;
    }
    if (h->shots >= h->shots_max) {
        if (h->log_count == 0 || strcmp(h->log[h->log_count - 1].key, "hunt.out_of_shot") != 0) {
            hunt_log(g, "hunt.out_of_shot", NULL, 0);
        }
        return false;
    }
    h->shots++;
    h->shot_x = (float)x;
    h->shot_y = (float)y;
    h->shot_age = 0;
    h->shot_misfire = false;
    double misfire = d->hunt.misfire[g->weather];
    if (g->inv[d->item_percussion] > 0 && g->inv[d->item_caps] > 0) {
        /* A copper cap fires the charge without an open pan, so rain matters far less. */
        misfire *= d->hunt.percussion_misfire_mul;
        g_add_item(g, d->item_caps, -1);
    }
    if (rng_chance(&g->rng, misfire)) {
        h->shot_misfire = true;
        h->reload_ticks = seconds_to_ticks(d->hunt.prime_seconds);
        hunt_log(g, "hunt.misfire", NULL, 0);
        return true;
    }
    int skill = g->party[h->hunter].skills[SKILL_SHOOTING];
    h->reload_ticks = seconds_to_ticks(d->hunt.reload_seconds * (1.15 - 0.06 * skill));
    /* The ball does not go quite where it was aimed; a good shot scatters less. */
    double spread = d->hunt.spread * (1.3 - 0.12 * skill) / d->families[g->family].hunt_mul;
    double ang = rng_unit(&g->rng) * 6.283185307179586, rad = spread * sqrt(rng_unit(&g->rng));
    float hx = (float)(x + cos(ang) * rad), hy = (float)(y + sin(ang) * rad);
    HuntAnimal *hit = NULL;
    for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
        HuntAnimal *a = &h->animals[i];
        if (a->state == HUNT_A_NONE || a->state == HUNT_A_DOWN) {
            continue;
        }
        float b[4];
        game_hunt_body(g, a, b);
        if (hx >= b[0] && hx < b[0] + b[2] && hy >= b[1] && hy < b[1] + b[3] && (!hit || a->y > hit->y)) {
            hit = a; /* the nearest animal in the line of fire takes the ball */
        }
    }
    if (!hit) {
        hunt_log(g, "hunt.miss", NULL, 0);
        startle(g, x);
        return true;
    }
    const AnimalDef *def = &d->animals[hit->kind];
    if (--hit->hp <= 0) {
        hit->state = HUNT_A_DOWN;
        h->kills++;
        int region = g_current_region(g);
        g->region_game[region] -= d->hunt.game_per_kill;
        if (g->region_game[region] < 0.1) {
            g->region_game[region] = 0.1;
        }
        h->meat += def->meat;
        hunt_log(g, def->meat > 0 ? "hunt.kill" : "hunt.kill_predator", def->name_key, def->meat);
        startle(g, x);
        return true;
    }
    hunt_log(g, "hunt.wounded", def->name_key, 0);
    if (hit->state != HUNT_A_CHARGE && def->charge > 0 && rng_chance(&g->rng, def->charge)) {
        charge(g, hit);
    } else if (hit->state != HUNT_A_CHARGE) {
        hit->state = HUNT_A_RUN;
    }
    startle(g, x);
    return true;
}

void game_hunt_quit(Game *g)
{
    HuntState *h = &g->hunt;
    if (g->phase != PHASE_HUNT || h->over) {
        return;
    }
    h->over = true;
    hunt_log(g, "hunt.give_up", NULL, h->meat);
}

bool game_hunt_view(const Game *g, HuntView *out)
{
    memset(out, 0, sizeof *out);
    if (g->phase != PHASE_HUNT) {
        return false;
    }
    const GameData *d = g->data;
    const HuntState *h = &g->hunt;
    loctext_set(&out->hunter, "hunt.hunter");
    if (h->hunter >= 0) {
        loctext_arg_person(&out->hunter, "who", &g->party[h->hunter]);
    }
    out->over = h->over;
    out->loaded = h->reload_ticks == 0 && h->shots < h->shots_max;
    int full = seconds_to_ticks(d->hunt.reload_seconds);
    out->reload = h->reload_ticks <= 0 ? 1.0 : 1.0 - (double)h->reload_ticks / (full > 0 ? full : 1);
    if (out->reload < 0) {
        out->reload = 0;
    }
    int day = seconds_to_ticks(d->hunt.seconds);
    out->daylight = day > 0 ? (double)h->ticks_left / day : 0;
    out->meat = h->meat;
    out->carry = h->carry;
    out->shots_left = h->shots_max - h->shots;
    memcpy(out->animals, h->animals, sizeof out->animals);
    out->shot_x = h->shot_x;
    out->shot_y = h->shot_y;
    out->shot_age = h->shot_age;
    out->shot_misfire = h->shot_misfire;
    out->log_count = h->log_count;
    memcpy(out->log, h->log, sizeof out->log);
    if (h->over) {
        const Member *who = h->hunter >= 0 ? &g->party[h->hunter] : NULL;
        if (h->meat <= 0) {
            loctext_set(&out->summary[0], "hunt.summary.none");
            loctext_arg_person(&out->summary[0], "who", who);
        } else {
            loctext_set(&out->summary[0], "hunt.summary.shot");
            loctext_arg_person(&out->summary[0], "who", who);
            loctext_arg_int(&out->summary[0], "meat", h->meat);
            loctext_set(&out->summary[1], h->meat > h->carry ? "hunt.summary.too_much" : "hunt.summary.all");
            loctext_arg_int(&out->summary[1], "carry", h->carry);
        }
    }
    return true;
}

void game_hunt_finish(Game *g)
{
    const GameData *d = g->data;
    HuntState *h = &g->hunt;
    if (g->phase != PHASE_HUNT || !h->over) {
        return;
    }
    int brought = h->meat < h->carry ? h->meat : h->carry;
    int fresh_room = MEAT_KEEP_LB - g->inv[d->item_meat];
    if (fresh_room < 0) {
        fresh_room = 0;
    }
    int fresh = brought < fresh_room ? brought : fresh_room;
    int dried = (int)lround((brought - fresh) * d->hunt.biltong_yield);
    g->inv[d->item_meat] += fresh;
    g->inv[d->item_biltong] += dried;
    int per = d->hunt.shots_per_lb > 0 ? d->hunt.shots_per_lb : 12;
    int lb = (h->shots + per - 1) / per;
    g_add_item(g, d->item_gunpowder, -lb);
    g_add_item(g, d->item_lead, -lb);
    LocText t;
    loctext_set(&t, brought > 0 ? "journal.hunt" : "journal.hunt_empty");
    if (h->hunter >= 0) {
        loctext_arg_person(&t, "who", &g->party[h->hunter]);
    }
    loctext_arg_int(&t, "fresh", fresh);
    loctext_arg_int(&t, "dried", dried);
    game_journal(g, &t);
    if (brought > 0) {
        g_add_morale(g, 3);
    }
    memset(h, 0, sizeof *h);
    g_settle(g);
}
