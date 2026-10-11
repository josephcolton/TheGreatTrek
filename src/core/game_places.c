/* Dialogs built by the core itself: landmarks, river crossings, road choices,
 * wagon breakdowns and being stranded without oxen. */
#include "calendar.h"
#include "game_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void begin_result(Game *g)
{
    g->dlg.result_count = 0;
    g->dlg.showing_result = true;
}

static void result_view(const Game *g, DialogView *v)
{
    for (int i = 0; i < g->dlg.result_count; i++) {
        v->body[v->body_count++] = g->dlg.result[i];
    }
    loctext_set(&v->choices[0].text, "choice.continue");
    v->choices[0].enabled = true;
    v->choice_count = 1;
}

/* Spends time without travelling: whole days pass, a part day costs miles on the road. */
static void spend_time(Game *g, double days)
{
    int whole = (int)floor(days);
    for (int i = 0; i < whole; i++) {
        g_pass_day(g, false, false);
    }
    double frac = days - whole;
    if (frac > 0 && g->edge >= 0) {
        g->miles_along -= frac * g->data->rules.miles[g->pace];
        if (g->miles_along < 0) {
            g->miles_along = 0;
        }
    }
}

/* ---------------------------------------------------------------- landmark */

typedef enum { LM_LOOK, LM_TRADE, LM_REST, LM_CROSS, LM_LEAVE } LandmarkAction;

static int landmark_actions(const Game *g, LandmarkAction *out)
{
    const NodeDef *n = &g->data->nodes[g->node];
    int k = 0;
    out[k++] = LM_LOOK;
    if (n->shop >= 0) {
        out[k++] = LM_TRADE;
    }
    out[k++] = LM_REST;
    out[k++] = (n->river >= 0 && !g->river_crossed[g->node]) ? LM_CROSS : LM_LEAVE;
    return k;
}

void g_landmark_view(const Game *g, DialogView *v)
{
    const NodeDef *n = &g->data->nodes[g->node];
    v->kind = DLG_LANDMARK;
    loctext_set(&v->title, n->name_key);
    snprintf(v->image, sizeof v->image, "%s", n->id);
    snprintf(v->history_key, sizeof v->history_key, "%s", n->history_key);
    if (g->dlg.showing_result) {
        result_view(g, v);
        return;
    }
    loctext_set(&v->body[v->body_count++], n->desc_key);
    LandmarkAction acts[5];
    int k = landmark_actions(g, acts);
    static const char *KEYS[] = {"landmark.look", "landmark.trade", "landmark.rest", "landmark.cross", "landmark.leave"};
    for (int i = 0; i < k; i++) {
        ChoiceView *cv = &v->choices[v->choice_count++];
        loctext_set(&cv->text, KEYS[acts[i]]);
        cv->enabled = true;
        if (acts[i] == LM_CROSS) {
            loctext_arg_key(&cv->text, "river", g->data->rivers[n->river].name_key);
        }
    }
}

void g_landmark_choose(Game *g, int choice)
{
    if (g->dlg.showing_result) {
        g->dlg.showing_result = false;
        g->dlg.result_count = 0;
        return;
    }
    LandmarkAction acts[5];
    int k = landmark_actions(g, acts);
    if (choice < 0 || choice >= k) {
        return;
    }
    const NodeDef *n = &g->data->nodes[g->node];
    switch (acts[choice]) {
    case LM_LOOK: {
        begin_result(g);
        LocText t;
        loctext_set(&t, n->history_key);
        g->dlg.result[g->dlg.result_count++] = t;
        break;
    }
    case LM_TRADE:
        g->shop = n->shop;
        g->dlg.kind = DLG_NONE;
        break;
    case LM_REST:
        g_pass_day(g, false, false);
        begin_result(g);
        loctext_set(&g->dlg.result[g->dlg.result_count++], "landmark.rested");
        break;
    case LM_CROSS:
        g_open_river(g);
        break;
    case LM_LEAVE:
        g->dlg.kind = DLG_NONE;
        g->at_node = false;
        g_depart(g);
        if (g->edge < 0 && g->dlg.kind == DLG_NONE) {
            g->at_node = true; /* nowhere to go */
        }
        break;
    }
}

/* ---------------------------------------------------------------- river */

static double roll_depth(Game *g, const RiverDef *r)
{
    int m = cal_month(g->day) - 1;
    double depth = r->depth_ft[m] + (rng_unit(&g->rng) * 2.0 - 1.0) * r->spread_ft;
    if (g->weather == WEATHER_RAIN || g->weather == WEATHER_STORM) {
        depth += 0.8;
    }
    return depth < 0.5 ? 0.5 : depth;
}

void g_open_river(Game *g)
{
    const NodeDef *n = &g->data->nodes[g->node];
    memset(&g->dlg, 0, sizeof g->dlg);
    g->dlg.kind = DLG_RIVER;
    g->dlg.speaker = -1;
    g->dlg.subject = -1;
    g->dlg.river_depth = roll_depth(g, &g->data->rivers[n->river]);
}

static const char *depth_key(double depth, const RiverDef *r)
{
    if (depth < 2.0) {
        return "river.depth.shallow";
    }
    if (depth <= r->ford_max_ft) {
        return "river.depth.waist";
    }
    if (depth <= r->ford_max_ft + 1.5) {
        return "river.depth.deep";
    }
    return "river.depth.flood";
}

typedef enum { RV_FORD, RV_FLOAT, RV_CARRY, RV_WAIT } RiverMethod;

static bool river_allowed(const Game *g, RiverMethod m, const char **lock)
{
    const RiverDef *r = &g->data->rivers[g->data->nodes[g->node].river];
    double d = g->dlg.river_depth;
    switch (m) {
    case RV_FORD:
        *lock = "lock.too_deep";
        return d <= r->ford_max_ft;
    case RV_FLOAT:
        if (d < r->float_min_ft) {
            *lock = "lock.too_shallow";
            return false;
        }
        *lock = "lock.need_tallow";
        return g->inv[g->data->item_tallow] >= 10;
    case RV_CARRY:
        *lock = "lock.too_deep";
        return d <= r->ford_max_ft + 1.0;
    default:
        return true;
    }
}

void g_river_view(const Game *g, DialogView *v)
{
    const NodeDef *n = &g->data->nodes[g->node];
    const RiverDef *r = &g->data->rivers[n->river];
    v->kind = DLG_RIVER;
    loctext_set(&v->title, r->name_key);
    snprintf(v->image, sizeof v->image, "%s", r->id);
    snprintf(v->history_key, sizeof v->history_key, "history.river.%s", r->id);
    if (g->dlg.showing_result) {
        result_view(g, v);
        return;
    }
    LocText *t = &v->body[v->body_count++];
    loctext_set(t, depth_key(g->dlg.river_depth, r));
    loctext_arg_key(t, "river", r->name_key);
    loctext_arg_int(t, "ft", (long long)lround(g->dlg.river_depth));
    static const char *KEYS[] = {"river.ford", "river.float", "river.carry", "river.wait"};
    for (int m = RV_FORD; m <= RV_WAIT; m++) {
        ChoiceView *cv = &v->choices[v->choice_count++];
        loctext_set(&cv->text, KEYS[m]);
        const char *lock = NULL;
        cv->enabled = river_allowed(g, (RiverMethod)m, &lock);
        if (!cv->enabled && lock) {
            loctext_set(&cv->locked, lock);
        }
    }
}

static void lose_goods(Game *g)
{
    const GameData *d = g->data;
    double w[MAX_ITEMS];
    for (int i = 0; i < d->item_count; i++) {
        bool loose = d->items[i].category != ITEMCAT_ANIMALS && i != d->item_water && g->inv[i] > 0;
        w[i] = loose ? d->items[i].weight_lb * g->inv[i] : 0;
    }
    int item = rng_weighted(&g->rng, w, d->item_count);
    if (item < 0) {
        return;
    }
    int lost = (int)lround(g->inv[item] * (0.1 + 0.2 * rng_unit(&g->rng)));
    if (lost < 1) {
        lost = 1;
    }
    g_add_item(g, item, -lost);
    LocText t;
    char key[KEY_LEN];
    snprintf(key, sizeof key, "%s%s", d->items[item].name_key, lost == 1 ? ".one" : "");
    loctext_set(&t, "result.lost");
    loctext_arg_int(&t, "n", lost);
    loctext_arg_key(&t, "item", key);
    g_add_result(g, &t);
}

static void river_trouble(Game *g, bool cargo_safe)
{
    const GameData *d = g->data;
    double w[4] = {cargo_safe ? 0 : 4, 3, 2, 1};
    switch (rng_weighted(&g->rng, w, 4)) {
    case 0:
        g_add_result_key(g, "river.trouble.goods");
        lose_goods(g);
        lose_goods(g);
        break;
    case 1: {
        int n = rng_range(&g->rng, 1, 2);
        if (n > g->inv[d->item_oxen]) {
            n = g->inv[d->item_oxen];
        }
        g_add_result_key(g, "river.trouble.oxen");
        if (n > 0) {
            g_add_item(g, d->item_oxen, -n);
            LocText t;
            loctext_set(&t, "result.lost");
            loctext_arg_int(&t, "n", n);
            loctext_arg_key(&t, "item", n == 1 ? "item.oxen.one" : "item.oxen");
            g_add_result(g, &t);
        }
        break;
    }
    case 2: {
        int part = rng_chance(&g->rng, 0.5) ? WAGON_PART_WHEELS : WAGON_PART_AXLE;
        g->wagon_parts[part] -= rng_range(&g->rng, 20, 50);
        if (g->wagon_parts[part] < 1) {
            g->wagon_parts[part] = 1;
        }
        g_add_result_key(g, "river.trouble.wagon");
        break;
    }
    default: {
        int m = g_random_member(g, false, -1);
        if (m < 0) {
            break;
        }
        g->dlg.subject = m;
        LocText t;
        loctext_set(&t, "river.trouble.person");
        loctext_arg_str(&t, "who", g->party[m].name);
        int rescuer = g_best_skill_member(g, SKILL_HERDING);
        loctext_arg_str(&t, "helper", rescuer >= 0 && rescuer != m ? g->party[rescuer].name : g->party[m].name);
        g_add_result(g, &t);
        g->party[m].health -= (int)lround(rng_range(&g->rng, 20, 45) * g_harm(g));
        if (g->party[m].health <= 0) {
            g_kill(g, m, "cause.drowned");
            LocText dt;
            loctext_set(&dt, "result.died");
            loctext_arg_str(&dt, "who", g->party[m].name);
            g_add_result(g, &dt);
        }
        break;
    }
    }
}

void g_river_choose(Game *g, int choice)
{
    if (g->dlg.showing_result) {
        bool crossed = g->river_crossed[g->node];
        g->dlg.showing_result = false;
        g->dlg.result_count = 0;
        if (crossed) {
            g->dlg.kind = DLG_NONE;
            g->at_node = false;
            g_depart(g);
        } else {
            g->dlg.river_depth = roll_depth(g, &g->data->rivers[g->data->nodes[g->node].river]);
        }
        return;
    }
    if (choice < RV_FORD || choice > RV_WAIT) {
        return;
    }
    const char *lock = NULL;
    if (!river_allowed(g, (RiverMethod)choice, &lock)) {
        return;
    }
    const RiverDef *r = &g->data->rivers[g->data->nodes[g->node].river];
    double depth = g->dlg.river_depth;
    begin_result(g);
    if (choice == RV_WAIT) {
        g_pass_day(g, false, false);
        g_add_result_key(g, "river.waited");
        return;
    }
    double risk;
    bool cargo_safe = false;
    if (choice == RV_FLOAT) {
        g_add_item(g, g->data->item_tallow, -10);
        risk = 0.15 + 0.05 * (depth - r->float_min_ft);
    } else {
        double x = depth / r->ford_max_ft;
        risk = 0.04 + 0.5 * x * x * x;
        if (choice == RV_CARRY) {
            cargo_safe = true;
            spend_time(g, 1.0);
        }
    }
    /* Strong herders and a good voorloper make a crossing safer. */
    risk *= 1.0 - 0.05 * g_best_skill(g, SKILL_HERDING, true) - 0.04 * g_best_skill(g, SKILL_VOORLOPER, true);
    risk *= g_harm(g);
    if (risk > 0.9) {
        risk = 0.9;
    }
    if (rng_chance(&g->rng, risk)) {
        river_trouble(g, cargo_safe);
    } else {
        g_add_result_key(g, "river.safe");
    }
    g->river_crossed[g->node] = true;
    LocText t;
    loctext_set(&t, "journal.crossed");
    loctext_arg_key(&t, "river", r->name_key);
    game_journal(g, &t);
}

/* ---------------------------------------------------------------- road choice */

static int out_edges(const Game *g, int *out)
{
    int n = 0;
    for (int i = 0; i < g->data->edge_count && n < 6; i++) {
        if (g->data->edges[i].from == g->node) {
            out[n++] = i;
        }
    }
    return n;
}

void g_route_view(const Game *g, DialogView *v)
{
    v->kind = DLG_ROUTE;
    loctext_set(&v->title, "route.title");
    loctext_set(&v->body[v->body_count++], "route.body");
    int edges[6];
    int n = out_edges(g, edges);
    for (int i = 0; i < n && v->body_count < MAX_RESULT_LINES; i++) {
        const EdgeDef *e = &g->data->edges[edges[i]];
        if (e->advice_key[0]) {
            LocText *t = &v->body[v->body_count++];
            loctext_set(t, e->advice_key);
            loctext_arg_int(t, "miles", e->miles);
        }
    }
    for (int i = 0; i < n; i++) {
        const EdgeDef *e = &g->data->edges[edges[i]];
        ChoiceView *cv = &v->choices[v->choice_count++];
        loctext_set(&cv->text, e->label_key[0] ? e->label_key : "route.default");
        loctext_arg_int(&cv->text, "miles", e->miles);
        loctext_arg_key(&cv->text, "dest", g->data->nodes[e->to].name_key);
        cv->enabled = true;
    }
}

void g_route_choose(Game *g, int choice)
{
    int edges[6];
    int n = out_edges(g, edges);
    if (choice < 0 || choice >= n) {
        return;
    }
    g_start_edge(g, edges[choice]);
}

/* ---------------------------------------------------------------- breakdown */

static const char *PART_KEYS[WAGON_PART_COUNT] = {"part.wheels", "part.axle", "part.dissel", "part.tent"};

static bool breakdown_allowed(const Game *g, int choice)
{
    switch (choice) {
    case 0: return g->inv[g->data->item_spare[g->dlg.part]] > 0;
    case 1: return g->inv[g->data->item_iron] >= 10 || g_best_skill(g, SKILL_SMITHING, true) >= 3;
    default: return true;
    }
}

void g_breakdown_view(const Game *g, DialogView *v)
{
    v->kind = DLG_BREAKDOWN;
    loctext_set(&v->title, "breakdown.title");
    if (g->dlg.showing_result) {
        result_view(g, v);
        return;
    }
    LocText *t = &v->body[v->body_count++];
    loctext_set(t, "breakdown.body");
    loctext_arg_key(t, "part", PART_KEYS[g->dlg.part]);
    static const char *KEYS[] = {"breakdown.spare", "breakdown.mend", "breakdown.rig"};
    static const char *LOCKS[] = {"lock.no_spare", "lock.no_iron", ""};
    for (int c = 0; c < 3; c++) {
        ChoiceView *cv = &v->choices[v->choice_count++];
        loctext_set(&cv->text, KEYS[c]);
        loctext_arg_key(&cv->text, "part", PART_KEYS[g->dlg.part]);
        cv->enabled = breakdown_allowed(g, c);
        if (!cv->enabled) {
            loctext_set(&cv->locked, LOCKS[c]);
        }
    }
}

void g_breakdown_choose(Game *g, int choice)
{
    if (g->dlg.showing_result) {
        g->dlg.kind = DLG_NONE;
        return;
    }
    if (choice < 0 || choice > 2 || !breakdown_allowed(g, choice)) {
        return;
    }
    double repair = g->data->families[g->family].repair_mul;
    int part = g->dlg.part;
    begin_result(g);
    LocText t;
    switch (choice) {
    case 0:
        g_add_item(g, g->data->item_spare[part], -1);
        g->wagon_parts[part] = 100;
        spend_time(g, 0.5 * repair);
        loctext_set(&t, "breakdown.spare.done");
        break;
    case 1:
        if (g_best_skill(g, SKILL_SMITHING, true) < 3 || g->inv[g->data->item_iron] >= 10) {
            g_add_item(g, g->data->item_iron, -10);
        }
        g->wagon_parts[part] = g_best_skill(g, SKILL_SMITHING, true) >= 3 ? 70 : 45;
        spend_time(g, 1.0 * repair);
        loctext_set(&t, "breakdown.mend.done");
        break;
    default:
        g->wagon_parts[part] = 25;
        spend_time(g, 2.0 * repair);
        loctext_set(&t, "breakdown.rig.done");
        break;
    }
    loctext_arg_key(&t, "part", PART_KEYS[part]);
    g_add_result(g, &t);
}

/* ---------------------------------------------------------------- stranded */

void g_stranded_view(const Game *g, DialogView *v)
{
    v->kind = DLG_STRANDED;
    loctext_set(&v->title, "stranded.title");
    if (g->dlg.showing_result) {
        result_view(g, v);
        return;
    }
    LocText *t = &v->body[v->body_count++];
    loctext_set(t, "stranded.body");
    loctext_arg_int(t, "n", g_oxen(g));
    loctext_set(&v->choices[0].text, "stranded.wait");
    v->choices[0].enabled = true;
    loctext_set(&v->choices[1].text, "stranded.give_up");
    v->choices[1].enabled = true;
    loctext_set(&v->choices[2].text, "stranded.yoke");
    v->choices[2].enabled = g->inv[g->data->item_cows] > 0;
    if (!v->choices[2].enabled) {
        loctext_set(&v->choices[2].locked, "lock.no_cattle");
    }
    v->choice_count = 3;
}

void g_stranded_choose(Game *g, int choice)
{
    if (g->dlg.showing_result) {
        g->dlg.kind = DLG_NONE;
        g->dlg.showing_result = false;
        g_check_stranded(g);
        return;
    }
    const GameData *d = g->data;
    if (choice == 2) {
        /* Backhouse (Dec 1838) put "bullocks unaccustomed to the yoke" between trained oxen. */
        int cows = d->item_cows;
        int want = d->wagons[g->wagon].min_oxen - g_oxen(g);
        int n = want < g->inv[cows] ? want : g->inv[cows];
        if (n <= 0) {
            return;
        }
        g_add_item(g, cows, -n);
        g_add_item(g, d->item_oxen, n);
        g->ox_stamina -= 10;
        if (g->ox_stamina < 0) {
            g->ox_stamina = 0;
        }
        begin_result(g);
        spend_time(g, 1.0);
        LocText t;
        loctext_set(&t, "stranded.yoked");
        loctext_arg_int(&t, "n", n);
        g_add_result(g, &t);
        return;
    }
    if (choice == 1) {
        g->dlg.kind = DLG_NONE;
        g->ending = ENDING_STRANDED;
        LocText t;
        loctext_set(&t, "journal.gave_up");
        game_journal(g, &t);
        return;
    }
    begin_result(g);
    spend_time(g, 3.0);
    double help = 0.35 + g->relation[d->group_trekkers] / 200.0;
    if (rng_chance(&g->rng, help)) {
        int want = d->wagons[g->wagon].min_oxen - g_oxen(g);
        int price = game_pence_to_unit(g, d->items[d->item_oxen].price_pence) * 2;
        int can = price > 0 ? g->money / price : 0;
        if (g->relation[d->group_trekkers] >= 40) {
            can = want; /* friends lend oxen */
            price = 0;
        }
        int got = want < can ? want : can;
        if (got > 0) {
            g->money -= got * price;
            g_add_item(g, d->item_oxen, got);
            LocText t;
            loctext_set(&t, price > 0 ? "stranded.bought" : "stranded.lent");
            loctext_arg_int(&t, "n", got);
            loctext_arg_money(&t, "cost", (long long)got * price);
            g_add_result(g, &t);
            return;
        }
        g_add_result_key(g, "stranded.no_money");
        return;
    }
    g_add_result_key(g, "stranded.nobody");
}
