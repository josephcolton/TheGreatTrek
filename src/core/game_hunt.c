/* Real-time hunting (04_game_design.md section 9), in the spirit of the Oregon Trail hunt:
 * wait for game to come into sight, then aim and fire with the mouse. The scene runs in fixed
 * ticks with the game's own RNG, so a hunt replays exactly from the seed and the shots.
 * A muzzle-loader fires once and is slow to load; rain makes the powder fail; shots scatter
 * less for a good shot; wounded buffalo and lions may come at the hunter.
 * Only so much meat can be carried back; meat beyond what keeps fresh is dried as biltong.
 * The same scene serves for guarding the herd at night (game_defend_start, at the end). */
#include "game_internal.h"

#include <math.h>
#include <string.h>

#define MEAT_KEEP_LB 60
#define TICK (1.0 / HUNT_TICKS_PER_SEC)

static void hunt_log_item(Game *g, const char *key, const char *animal_key, const char *item_key, int n)
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
    if (item_key) {
        loctext_arg_key(t, "item", item_key);
    }
    loctext_arg_int(t, "n", n);
}

static void hunt_log(Game *g, const char *key, const char *animal_key, int n)
{
    hunt_log_item(g, key, animal_key, NULL, n);
}

static void defend_abandon(Game *g);

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
    if (h->defend) {
        defend_abandon(g);
    }
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

/* The best shot who is well enough to go out, or failing that the best shot. */
static int pick_hunter(const Game *g)
{
    int best = -1;
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_healthy(g, i) && (best < 0 || g->party[i].skills[SKILL_SHOOTING] > g->party[best].skills[SKILL_SHOOTING])) {
            best = i;
        }
    }
    return best >= 0 ? best : g_best_skill_member(g, SKILL_SHOOTING);
}

static int shots_allowed(const Game *g)
{
    const GameData *d = g->data;
    int lb = g->inv[d->item_gunpowder] < g->inv[d->item_lead] ? g->inv[d->item_gunpowder] : g->inv[d->item_lead];
    return lb * (d->hunt.shots_per_lb > 0 ? d->hunt.shots_per_lb : 12);
}

/* ---------------------------------------------------------------- guarding the herd */

/* A night raid: the herd and the wagons stand at the bottom of the field round the fire, and
 * predators come out of the dark from either side, creep up, then rush in for a beast. A hit or
 * a shot close by sends one off (a wounded lion may turn on the guard instead); one that gets
 * through carries a beast off, unless it is hit on the way out and drops it. */
#define HERD_Y (HUNT_FIELD_H - 24.0f)
#define RUSH_DISTANCE 90.0
#define SCARE_RADIUS 60.0

static int taken_of(const HuntState *h, int item)
{
    int n = 0;
    for (int k = 0; k < h->took_count; k++) {
        n += h->took[k] == item;
    }
    return n;
}

/* A beast the raider can carry off: one of the kinds it takes, of which some are left; the
 * first kind listed is the one it usually goes for. */
static int raid_prey(Game *g, const AnimalDef *def)
{
    double w[4];
    double total = 0;
    for (int i = 0; i < def->raid_take_count; i++) {
        int item = def->raid_take[i];
        w[i] = g->inv[item] - taken_of(&g->hunt, item) > 0 ? (i == 0 ? 3.0 : 1.0) : 0.0;
        total += w[i];
    }
    if (total <= 0 || g->hunt.took_count >= HUNT_MAX_TAKES) {
        return -1;
    }
    return def->raid_take[rng_weighted(&g->rng, w, def->raid_take_count)];
}

static void flee(HuntAnimal *a)
{
    a->state = HUNT_A_FLEE;
    a->dir = a->x < HUNT_FIELD_W / 2.0f ? -1.0f : 1.0f;
}

static bool raiding(const HuntAnimal *a)
{
    return a->state == HUNT_A_WALK || a->state == HUNT_A_RUSH;
}

static void defend_spawn(Game *g)
{
    HuntState *h = &g->hunt;
    const AnimalDef *def = &g->data->animals[h->foe];
    h->spawn_ticks = seconds_to_ticks(2.0 + 4.0 * rng_unit(&g->rng));
    int group = def->herd > 1 ? rng_range(&g->rng, 1, def->herd) : 1;
    if (group > h->foes_left) {
        group = h->foes_left;
    }
    float dir = rng_chance(&g->rng, 0.5) ? 1.0f : -1.0f;
    float y = (float)(HUNT_FIELD_H * (0.15 + 0.35 * rng_unit(&g->rng)));
    int placed = 0;
    for (int i = 0; i < group; i++) {
        HuntAnimal *a = free_slot(h);
        if (!a) {
            break;
        }
        memset(a, 0, sizeof *a);
        a->kind = h->foe;
        a->dir = dir;
        a->hp = def->hp;
        a->prey = -1;
        a->y = y + (float)rng_range(&g->rng, -10, 10);
        a->x = dir > 0 ? -15.0f - (float)i * 22.0f : HUNT_FIELD_W + 15.0f + (float)i * 22.0f;
        a->tx = (float)(150 + 340 * rng_unit(&g->rng));
        a->ty = HERD_Y + (float)rng_range(&g->rng, -6, 6);
        a->state = HUNT_A_WALK;
        h->foes_left--;
        placed++;
    }
    if (placed > 0) {
        hunt_log(g, placed > 1 ? "defend.appears_pack" : "defend.appears", def->name_key, placed);
    }
}

static void defend_move(Game *g, HuntAnimal *a)
{
    HuntState *h = &g->hunt;
    const AnimalDef *def = &g->data->animals[a->kind];
    double v = def->speed * game_hunt_scale(a->y) * TICK;
    if (raiding(a)) {
        double dx = a->tx - a->x, dy = a->ty - a->y;
        double len = sqrt(dx * dx + dy * dy);
        if (a->state == HUNT_A_WALK && len < RUSH_DISTANCE) {
            a->state = HUNT_A_RUSH;
        }
        double step = v * (a->state == HUNT_A_RUSH ? 1.0 : 0.3);
        if (len <= step) {
            /* in among the stock */
            a->prey = raid_prey(g, def);
            if (a->prey >= 0) {
                h->took[h->took_count++] = a->prey;
                hunt_log_item(g, "defend.took", def->name_key, g->data->items[a->prey].name_key, 1);
            }
            flee(a);
            return;
        }
        a->x += (float)(dx / len * step);
        a->y += (float)(dy / len * step);
        a->dir = dx < 0 ? -1.0f : 1.0f;
    } else if (a->state == HUNT_A_FLEE) {
        a->x += (float)(a->dir * v * (a->prey >= 0 ? 0.7 : 1.1)); /* slower with a sheep in its jaws */
        a->y -= (float)(v * 0.2);
        if (a->y < 6) {
            a->y = 6;
        }
    }
    if (a->x < -60 || a->x > HUNT_FIELD_W + 60) {
        a->state = HUNT_A_NONE; /* back into the dark */
    }
}

/* A shot close by may send raiders off; a beast already running keeps running. */
static void defend_scare(Game *g, double x, double y, double mul)
{
    HuntState *h = &g->hunt;
    for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
        HuntAnimal *a = &h->animals[i];
        if (!raiding(a)) {
            continue;
        }
        double dx = a->x - x, dy = a->y - y;
        if (dx * dx + dy * dy > SCARE_RADIUS * SCARE_RADIUS) {
            continue;
        }
        const AnimalDef *def = &g->data->animals[a->kind];
        if (rng_chance(&g->rng, def->spook * mul)) {
            flee(a);
            h->driven++;
            hunt_log(g, "defend.scared", def->name_key, 0);
        }
    }
}

static void drop_prey(Game *g, HuntAnimal *a)
{
    HuntState *h = &g->hunt;
    if (a->prey < 0) {
        return;
    }
    for (int k = h->took_count - 1; k >= 0; k--) {
        if (h->took[k] == a->prey) {
            h->took[k] = h->took[--h->took_count];
            break;
        }
    }
    hunt_log_item(g, "defend.dropped", g->data->animals[a->kind].name_key, g->data->items[a->prey].name_key, 1);
    a->prey = -1;
}

static void defend_hit(Game *g, HuntAnimal *a, double x, double y)
{
    HuntState *h = &g->hunt;
    const AnimalDef *def = &g->data->animals[a->kind];
    drop_prey(g, a);
    if (--a->hp <= 0) {
        a->state = HUNT_A_DOWN;
        h->kills++;
        hunt_log(g, "defend.killed", def->name_key, 0);
        defend_scare(g, x, y, 0.5);
        return;
    }
    if (a->state != HUNT_A_CHARGE && def->charge > 0 && rng_chance(&g->rng, def->charge)) {
        charge(g, a);
        return;
    }
    if (a->state != HUNT_A_CHARGE) {
        if (a->state != HUNT_A_FLEE) {
            h->driven++;
        }
        flee(a);
        hunt_log(g, "defend.wounded", def->name_key, 0);
    }
}

/* Nobody is guarding any more: about half of those still coming get a beast. */
static void defend_abandon(Game *g)
{
    HuntState *h = &g->hunt;
    const AnimalDef *def = &g->data->animals[h->foe];
    int coming = h->foes_left;
    for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
        if (raiding(&h->animals[i])) {
            coming++;
            flee(&h->animals[i]);
        }
    }
    h->foes_left = 0;
    for (int i = 0; i < coming; i++) {
        if (rng_chance(&g->rng, 0.5)) {
            int prey = raid_prey(g, def);
            if (prey >= 0) {
                h->took[h->took_count++] = prey;
            }
        }
    }
}

static void defend_tick(Game *g)
{
    HuntState *h = &g->hunt;
    if (h->foes_left > 0 && --h->spawn_ticks <= 0) {
        defend_spawn(g);
    }
    for (int i = 0; i < HUNT_MAX_ANIMALS && !h->over; i++) {
        HuntAnimal *a = &h->animals[i];
        if (a->state == HUNT_A_CHARGE) {
            move_animal(g, a);
        } else if (a->state != HUNT_A_NONE && a->state != HUNT_A_DOWN) {
            defend_move(g, a);
        }
    }
    if (h->over) {
        return;
    }
    if (h->foes_left == 0 && live_animals(h) == 0) {
        h->over = true;
        hunt_log(g, "defend.quiet", g->data->animals[h->foe].name_key, 0);
    } else if (--h->ticks_left <= 0) {
        /* first light: whatever is still out there slinks off */
        h->ticks_left = 0;
        h->over = true;
        hunt_log(g, "defend.dawn", g->data->animals[h->foe].name_key, 0);
    }
}

void game_defend_start(Game *g, int animal)
{
    const GameData *d = g->data;
    if (animal < 0 || animal >= d->animal_count || d->animals[animal].raid_max <= 0) {
        return;
    }
    HuntState *h = &g->hunt;
    memset(h, 0, sizeof *h);
    h->hunter = pick_hunter(g);
    if (h->hunter < 0) {
        return;
    }
    const AnimalDef *def = &d->animals[animal];
    h->active = true;
    h->defend = true;
    h->foe = animal;
    h->foes_left = rng_range(&g->rng, def->raid_min, def->raid_max);
    h->ticks_left = seconds_to_ticks(d->hunt.defend_seconds);
    h->shots_max = shots_allowed(g);
    h->shot_age = 1000;
    h->spawn_ticks = seconds_to_ticks(2.0);
    hunt_log(g, "defend.start", def->name_key, 0);
    /* game_choose settles the phase once the result page is read */
}

static void defend_summary(const Game *g, HuntView *out)
{
    const GameData *d = g->data;
    const HuntState *h = &g->hunt;
    const Member *who = h->hunter >= 0 ? &g->party[h->hunter] : NULL;
    loctext_set(&out->summary[0], h->kills + h->driven > 0 ? "defend.summary.result" : "defend.summary.nothing");
    loctext_arg_person(&out->summary[0], "who", who);
    loctext_arg_key(&out->summary[0], "animal", d->animals[h->foe].name_key);
    loctext_arg_int(&out->summary[0], "killed", h->kills);
    loctext_arg_int(&out->summary[0], "driven", h->driven);
    if (h->took_count == 0) {
        loctext_set(&out->summary[1], "defend.summary.safe");
        return;
    }
    int line = 1;
    for (int item = 0; item < d->item_count && line < 4; item++) {
        int n = taken_of(h, item);
        if (n > 0) {
            loctext_set(&out->summary[line], "defend.summary.lost");
            loctext_arg_int(&out->summary[line], "n", n);
            loctext_arg_key(&out->summary[line], "item", d->items[item].name_key);
            line++;
        }
    }
}

static void defend_finish(Game *g)
{
    const GameData *d = g->data;
    HuntState *h = &g->hunt;
    for (int k = 0; k < h->took_count; k++) {
        g_add_item(g, h->took[k], -1);
    }
    int per = d->hunt.shots_per_lb > 0 ? d->hunt.shots_per_lb : 12;
    int lb = (h->shots + per - 1) / per;
    g_add_item(g, d->item_gunpowder, -lb);
    g_add_item(g, d->item_lead, -lb);
    LocText t;
    loctext_set(&t, h->took_count > 0 ? "journal.defend" : "journal.defend_safe");
    if (h->hunter >= 0) {
        loctext_arg_person(&t, "who", &g->party[h->hunter]);
    }
    loctext_arg_key(&t, "animal", d->animals[h->foe].name_key);
    loctext_arg_int(&t, "n", h->took_count);
    game_journal(g, &t);
    g_add_morale(g, h->took_count == 0 ? 3 : -1);
    memset(h, 0, sizeof *h);
    g_settle(g);
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
    h->hunter = pick_hunter(g);
    h->carry = d->hunt.carry_on_foot + (g->inv[d->item_horses] > 0 ? d->hunt.carry_per_horse : 0);
    h->ticks_left = seconds_to_ticks(d->hunt.seconds);
    h->shots_max = shots_allowed(g);
    h->shot_age = 1000;
    h->spawn_ticks = seconds_to_ticks(1.0);
    /* A hunt takes half a day off the road. */
    g_move_along(g, -0.5 * d->rules.miles[g->pace]);
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
    if (h->defend) {
        defend_tick(g);
        return;
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
        if (h->defend) {
            defend_scare(g, hx, hy, 1.0);
        } else {
            startle(g, x);
        }
        return true;
    }
    const AnimalDef *def = &d->animals[hit->kind];
    if (h->defend) {
        defend_hit(g, hit, hx, hy);
        return true;
    }
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
    if (h->defend) {
        hunt_log(g, "defend.give_up", g->data->animals[h->foe].name_key, 0);
        defend_abandon(g);
        return;
    }
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
    out->defend = h->defend;
    out->lost = h->took_count;
    if (h->over && h->defend) {
        defend_summary(g, out);
    } else if (h->over) {
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
    if (h->defend) {
        defend_finish(g);
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
