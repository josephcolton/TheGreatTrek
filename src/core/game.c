#include "game.h"

#include "calendar.h"
#include "game_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- text helpers */

void loctext_set(LocText *t, const char *key)
{
    memset(t, 0, sizeof *t);
    snprintf(t->key, sizeof t->key, "%s", key ? key : "");
}

static TextArg *new_arg(LocText *t, const char *name)
{
    if (t->n_args >= MAX_ARGS) {
        return NULL;
    }
    TextArg *a = &t->args[t->n_args++];
    memset(a, 0, sizeof *a);
    snprintf(a->name, sizeof a->name, "%s", name);
    return a;
}

void loctext_arg_int(LocText *t, const char *name, long long v)
{
    TextArg *a = new_arg(t, name);
    if (a) {
        a->kind = ARG_INT;
        a->i = v;
    }
}

void loctext_arg_str(LocText *t, const char *name, const char *s)
{
    TextArg *a = new_arg(t, name);
    if (a) {
        a->kind = ARG_STR;
        snprintf(a->s, sizeof a->s, "%s", s ? s : "");
    }
}

void loctext_arg_person(LocText *t, const char *name, const Member *m)
{
    TextArg *a = new_arg(t, name);
    if (a) {
        a->kind = ARG_PERSON;
        a->i = m ? m->pronoun : PRONOUN_COUNT;
        snprintf(a->s, sizeof a->s, "%s", m ? m->name : "");
    }
}

void loctext_arg_key(LocText *t, const char *name, const char *key)
{
    TextArg *a = new_arg(t, name);
    if (a) {
        a->kind = ARG_KEY;
        snprintf(a->s, sizeof a->s, "%s", key ? key : "");
    }
}

void loctext_arg_date(LocText *t, const char *name, int day)
{
    TextArg *a = new_arg(t, name);
    if (a) {
        a->kind = ARG_DATE;
        a->i = day;
    }
}

void loctext_arg_money(LocText *t, const char *name, long long v)
{
    TextArg *a = new_arg(t, name);
    if (a) {
        a->kind = ARG_MONEY;
        a->i = v;
    }
}

void member_role_key(Role role, Pronoun pronoun, char *out, size_t cap)
{
    if (role == ROLE_CHILD || role == ROLE_SPOUSE || role == ROLE_SIBLING) {
        snprintf(out, cap, "role.%s.%s", role_id(role), pronoun == PRONOUN_SHE ? "she" : "he");
    } else {
        snprintf(out, cap, "role.%s", role_id(role));
    }
}

/* ---------------------------------------------------------------- lifecycle */

Game *game_new(const GameData *data, uint64_t seed, int family, Difficulty diff)
{
    if (!data || family < 0 || family >= data->family_count) {
        return NULL;
    }
    Game *g = calloc(1, sizeof *g);
    if (!g) {
        return NULL;
    }
    g->event_last_day = malloc(sizeof(int) * (size_t)(data->event_count ? data->event_count : 1));
    if (!g->event_last_day) {
        free(g);
        return NULL;
    }
    for (int i = 0; i < data->event_count; i++) {
        g->event_last_day[i] = -1;
    }
    const FamilyDef *f = &data->families[family];
    const Rules *r = &data->rules;
    g->data = data;
    g->seed = seed;
    rng_seed(&g->rng, seed, 0x7472656bu); /* "trek" */
    g->family = family;
    g->side = f->side;
    g->difficulty = diff;
    g->day = r->start_day;
    g->start_day = r->start_day;
    g->node = r->start_node;
    g->edge = -1;
    g->pace = PACE_NORMAL;
    g->rations = RATIONS_FULL;
    g->stance = NIGHT_LAAGER;
    g->sabbath = true;
    g->weather = WEATHER_CLEAR;
    g->shop = -1;
    g->wagon = f->wagon;
    for (int p = 0; p < WAGON_PART_COUNT; p++) {
        g->wagon_parts[p] = 100;
    }
    g->ox_stamina = 90;
    for (int i = 0; i < MAX_REGIONS; i++) {
        g->region_game[i] = 1.0;
    }
    g->last_hunt_day = -1;
    g->morale = 70 + f->morale_bonus;
    g->money = (int)lround(f->money * r->start_money_mul[diff]);
    for (int i = 0; i < f->start_item_count; i++) {
        g->inv[f->start_items[i].item] += f->start_items[i].qty;
    }
    for (int i = 0; i < data->group_count; i++) {
        g->relation[i] = data->groups[i].start_relation[g->side];
    }
    g->party_count = f->member_count;
    for (int i = 0; i < f->member_count; i++) {
        const MemberDef *md = &f->members[i];
        Member *m = &g->party[i];
        snprintf(m->name, sizeof m->name, "%s", md->name);
        m->role = md->role;
        m->age = md->age;
        m->pronoun = md->pronoun;
        m->birth_year = md->birth_year;
        memcpy(m->skills, md->skills, sizeof m->skills);
        snprintf(m->portrait, sizeof m->portrait, "%s", md->portrait);
        m->health = 90;
        m->loyalty = 60;
        m->alive = true;
        m->with_party = true;
    }
    LocText t;
    loctext_set(&t, "journal.start");
    loctext_arg_key(&t, "family", f->name_key);
    game_journal(g, &t);
    if (f->start_note[0]) {
        loctext_set(&t, f->start_note);
        game_journal(g, &t);
    }
    g_arrive(g, g->node);
    g_settle(g);
    return g;
}

void game_free(Game *g)
{
    if (g) {
        free(g->event_last_day);
        free(g);
    }
}

GamePhase game_phase(const Game *g)
{
    return g->phase;
}

/* ---------------------------------------------------------------- journal and notices */

void game_journal(Game *g, const LocText *t)
{
    if (g->journal_count >= MAX_JOURNAL) {
        /* Keep the start of the story and drop the oldest middle entry. */
        memmove(&g->journal[1], &g->journal[2], sizeof g->journal[0] * (MAX_JOURNAL - 2));
        g->journal_count--;
    }
    g->journal[g->journal_count].day = g->day;
    g->journal[g->journal_count].text = *t;
    g->journal_count++;
}

void g_notice(Game *g, const LocText *t)
{
    game_journal(g, t);
    if (g->notice_count < MAX_NOTICES) {
        g->notices[g->notice_count++] = *t;
    }
    g->stop_travel = true;
}

void g_notice_key(Game *g, const char *key)
{
    LocText t;
    loctext_set(&t, key);
    g_notice(g, &t);
}

void g_notice_member(Game *g, const char *key, int member)
{
    LocText t;
    loctext_set(&t, key);
    if (member >= 0) {
        loctext_arg_person(&t, "who", &g->party[member]);
    }
    g_notice(g, &t);
}

/* ---------------------------------------------------------------- state helpers */

void g_add_item(Game *g, int item, int qty)
{
    if (item < 0 || item >= g->data->item_count) {
        return;
    }
    g->inv[item] += qty;
    if (g->inv[item] < 0) {
        g->inv[item] = 0;
    }
    if (item == g->data->item_water) {
        int cap = game_water_capacity(g);
        if (g->inv[item] > cap) {
            g->inv[item] = cap;
        }
    }
}

void g_add_morale(Game *g, int delta)
{
    g->morale += delta;
    if (g->morale < 0) {
        g->morale = 0;
    }
    if (g->morale > 100) {
        g->morale = 100;
    }
}

void g_add_relation(Game *g, int group, int delta)
{
    if (group < 0 || group >= g->data->group_count) {
        return;
    }
    int v = g->relation[group] + delta;
    g->relation[group] = v < -100 ? -100 : (v > 100 ? 100 : v);
}

bool game_has_flag(const Game *g, const char *flag)
{
    for (int i = 0; i < g->flag_count; i++) {
        if (strcmp(g->flags[i], flag) == 0) {
            return true;
        }
    }
    return false;
}

void g_set_flag(Game *g, const char *flag)
{
    if (!flag || !*flag || game_has_flag(g, flag) || g->flag_count >= MAX_FLAGS) {
        return;
    }
    snprintf(g->flags[g->flag_count++], ID_LEN, "%s", flag);
}

void g_clear_flag(Game *g, const char *flag)
{
    for (int i = 0; i < g->flag_count; i++) {
        if (strcmp(g->flags[i], flag) == 0) {
            memmove(&g->flags[i], &g->flags[i + 1], sizeof g->flags[0] * (size_t)(g->flag_count - i - 1));
            g->flag_count--;
            return;
        }
    }
}

int game_counter(const Game *g, const char *name)
{
    for (int i = 0; i < g->counter_count; i++) {
        if (strcmp(g->counter_names[i], name) == 0) {
            return g->counter_values[i];
        }
    }
    return 0;
}

void g_add_counter(Game *g, const char *name, int delta)
{
    for (int i = 0; i < g->counter_count; i++) {
        if (strcmp(g->counter_names[i], name) == 0) {
            g->counter_values[i] += delta;
            return;
        }
    }
    if (g->counter_count < MAX_COUNTERS) {
        snprintf(g->counter_names[g->counter_count], ID_LEN, "%s", name);
        g->counter_values[g->counter_count++] = delta;
    }
}

bool g_member_active(const Game *g, int m)
{
    return m >= 0 && m < g->party_count && g->party[m].alive && g->party[m].with_party;
}

bool g_member_healthy(const Game *g, int m)
{
    return g_member_active(g, m) && g->party[m].cond_count == 0 && g->party[m].health >= 40;
}

int g_best_skill(const Game *g, Skill s, bool healthy_only)
{
    int best = 0;
    for (int i = 0; i < g->party_count; i++) {
        bool ok = healthy_only ? g_member_healthy(g, i) : g_member_active(g, i);
        if (ok && g->party[i].skills[s] > best) {
            best = g->party[i].skills[s];
        }
    }
    return best;
}

int g_best_skill_member(const Game *g, Skill s)
{
    int best = -1, best_v = 0;
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i) && g->party[i].skills[s] > best_v) {
            best = i;
            best_v = g->party[i].skills[s];
        }
    }
    return best;
}

int g_random_member(Game *g, bool children_only, int role)
{
    int pool[MAX_PARTY], n = 0;
    for (int i = 0; i < g->party_count; i++) {
        if (!g_member_active(g, i)) {
            continue;
        }
        if (children_only && g->party[i].age != AGE_CHILD) {
            continue;
        }
        if (role >= 0 && (int)g->party[i].role != role) {
            continue;
        }
        pool[n++] = i;
    }
    return n ? pool[rng_below(&g->rng, (uint32_t)n)] : -1;
}

int g_head(const Game *g)
{
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i) && g->party[i].role == ROLE_HEAD) {
            return i;
        }
    }
    /* The head has died: the eldest adult family member leads. */
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i) && g->party[i].role != ROLE_SERVANT && g->party[i].age != AGE_CHILD) {
            return i;
        }
    }
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i)) {
            return i;
        }
    }
    return -1;
}

void g_move_along(Game *g, double miles)
{
    if (g->edge < 0) {
        return;
    }
    double before = g->miles_along;
    g->miles_along += miles;
    if (g->miles_along < 0) {
        g->miles_along = 0;
    }
    /* the total counts road covered, so time lost on the road comes off it too */
    g->miles_total += (int)lround(g->miles_along) - (int)lround(before);
    if (g->miles_total < 0) {
        g->miles_total = 0;
    }
}

int g_current_region(const Game *g)
{
    if (g->edge >= 0) {
        return g->data->edges[g->edge].region;
    }
    /* At a node: use the region of an edge that reaches it. */
    for (int i = 0; i < g->data->edge_count; i++) {
        if (g->data->edges[i].to == g->node || g->data->edges[i].from == g->node) {
            return g->data->edges[i].region;
        }
    }
    return 0;
}

double g_harm(const Game *g)
{
    return g->data->rules.harm_mul[g->difficulty];
}

int game_pence_to_unit(const Game *g, int pence)
{
    if (pence <= 0) {
        return 0;
    }
    double unit = g->side == SIDE_DUTCH ? 18.0 : 12.0; /* 1 Rd = 1s 6d = 18d; 1s = 12d */
    int v = (int)lround(pence / unit);
    return v < 1 ? 1 : v;
}

int g_wagon_trade_cost(const Game *g, int wagon)
{
    const GameData *d = g->data;
    double old_value = d->wagons[g->wagon].price_pence * d->rules.wagon_trade_in;
    return d->wagons[wagon].price_pence - (int)lround(old_value);
}

int g_smith_cost(const Game *g)
{
    int points = 0;
    for (int p = 0; p < WAGON_PART_COUNT; p++) {
        points += 100 - g->wagon_parts[p];
    }
    const NodeDef *n = &g->data->nodes[g->node];
    double mul = n->price_mul > 0 ? n->price_mul : 1.0;
    return (int)lround(points * g->data->rules.smith_pence_per_point * mul);
}

void g_add_money_pence(Game *g, int pence)
{
    int units = game_pence_to_unit(g, pence < 0 ? -pence : pence);
    g->money += pence < 0 ? -units : units;
    if (g->money < 0) {
        g->money = 0;
    }
}

void g_kill(Game *g, int member, const char *cause_key)
{
    if (!g_member_active(g, member)) {
        return;
    }
    Member *m = &g->party[member];
    /* the illness that killed them, if it was one, and how long they had it */
    int illness = -1, ill_days = 0;
    for (int c = 0; c < m->cond_count && cause_key; c++) {
        if (strcmp(g->data->illnesses[m->conds[c].illness].cause_key, cause_key) == 0) {
            illness = m->conds[c].illness;
            ill_days = m->conds[c].days;
        }
    }
    m->alive = false;
    m->health = 0;
    m->cond_count = 0;
    g->deaths++;
    if (g->grave_count < MAX_GRAVES) {
        Grave *gr = &g->graves[g->grave_count];
        memset(gr, 0, sizeof *gr);
        gr->member = member;
        gr->day = g->day;
        gr->node = g->node;
        gr->edge = g->edge;
        gr->miles_along = (int)g->miles_along;
        snprintf(gr->cause_key, sizeof gr->cause_key, "%s", cause_key ? cause_key : "cause.unknown");
        gr->illness = illness;
        gr->ill_days = ill_days;
        if (g->pending_grave_count < MAX_GRAVES) {
            g->pending_graves[g->pending_grave_count++] = g->grave_count;
        }
        g->grave_count++;
    }
    LocText t;
    loctext_set(&t, "notice.death");
    loctext_arg_person(&t, "who", m);
    loctext_arg_key(&t, "cause", cause_key ? cause_key : "cause.unknown");
    game_journal(g, &t);
    g_add_morale(g, -15);
    g->stop_travel = true;
}

void g_member_leaves(Game *g, int member)
{
    if (!g_member_active(g, member)) {
        return;
    }
    g->party[member].with_party = false;
    g_notice_member(g, g->party[member].role == ROLE_SERVANT ? "notice.servant_left" : "notice.member_left", member);
    g_add_morale(g, -5);
}

int game_alive_count(const Game *g)
{
    int n = 0;
    for (int i = 0; i < g->party_count; i++) {
        if (g->party[i].alive) {
            n++;
        }
    }
    return n;
}

int game_member_count_with_party(const Game *g)
{
    int n = 0;
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i)) {
            n++;
        }
    }
    return n;
}

/* ---------------------------------------------------------------- loads and food */

int game_load_lb(const Game *g)
{
    double lb = 0;
    for (int i = 0; i < g->data->item_count; i++) {
        lb += g->inv[i] * g->data->items[i].weight_lb;
    }
    return (int)lb;
}

int game_capacity_lb(const Game *g)
{
    return g->data->wagons[g->wagon].capacity_lb;
}

int game_food_lb(const Game *g)
{
    double lb = 0;
    for (int i = 0; i < g->data->item_count; i++) {
        lb += g->inv[i] * g->data->items[i].food;
    }
    return (int)lb;
}

static double daily_food_need(const Game *g)
{
    const Rules *r = &g->data->rules;
    double need = 0;
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i)) {
            need += r->food_lb[g->rations] * (g->party[i].age == AGE_CHILD ? r->child_food : 1.0);
        }
    }
    return need;
}

double game_food_need(const Game *g)
{
    return daily_food_need(g);
}

double game_food_days(const Game *g)
{
    double need = daily_food_need(g);
    /* Sheep count as food too: about 35 lb of meat each. */
    double food = game_food_lb(g) + g->inv[g->data->item_sheep] * 35.0;
    return need > 0 ? food / need : 999;
}

int game_water_capacity(const Game *g)
{
    const GameData *d = g->data;
    return g->inv[d->item_vat] * d->items[d->item_vat].water_capacity;
}

/* ---------------------------------------------------------------- route */

int game_next_node(const Game *g)
{
    return g->edge >= 0 ? g->data->edges[g->edge].to : -1;
}

int game_miles_to_next(const Game *g)
{
    if (g->edge < 0) {
        return 0;
    }
    double left = g->data->edges[g->edge].miles - g->miles_along;
    return left < 0 ? 0 : (int)ceil(left);
}

void g_arrive(Game *g, int node)
{
    if (g->edge >= 0) {
        /* the last day's miles may run past the place; the road is only so long */
        g->miles_total += g->data->edges[g->edge].miles - (int)lround(g->miles_along);
    }
    g->node = node;
    g->edge = -1;
    g->miles_along = 0;
    g->at_node = true;
    g->visited[node] = true;
    g->stop_travel = true;
    LocText t;
    loctext_set(&t, "journal.arrived");
    loctext_arg_key(&t, "place", g->data->nodes[node].name_key);
    game_journal(g, &t);
    const NodeDef *n = &g->data->nodes[node];
    if (n->setpiece >= 0 && !g->setpiece_done[node]) {
        g->setpiece_done[node] = true;
        g_start_event(g, n->setpiece);
    }
}

void g_start_edge(Game *g, int edge)
{
    const EdgeDef *e = &g->data->edges[edge];
    g->edge = edge;
    g->miles_along = 0;
    g->at_node = false;
    g->dlg.kind = DLG_NONE;
    if (e->route_flag[0]) {
        g_set_flag(g, e->route_flag);
    }
    LocText t;
    loctext_set(&t, "journal.left");
    loctext_arg_key(&t, "place", g->data->nodes[g->node].name_key);
    loctext_arg_key(&t, "dest", g->data->nodes[e->to].name_key);
    game_journal(g, &t);
}

void g_depart(Game *g)
{
    int outs[MAX_EDGES], n = 0;
    for (int i = 0; i < g->data->edge_count; i++) {
        if (g->data->edges[i].from == g->node) {
            outs[n++] = i;
        }
    }
    if (n == 0) {
        return; /* end of the road */
    }
    if (n == 1) {
        g_start_edge(g, outs[0]);
        return;
    }
    memset(&g->dlg, 0, sizeof g->dlg);
    g->dlg.kind = DLG_ROUTE;
    g->dlg.speaker = -1;
    g->dlg.subject = -1;
}

/* ---------------------------------------------------------------- orders */

void game_set_orders(Game *g, Pace pace, Rations rations, NightStance stance, bool sabbath)
{
    g->pace = pace;
    g->rations = rations;
    g->stance = stance;
    g->sabbath = sabbath;
}

void game_set_keep_flock(Game *g, bool keep)
{
    g->keep_flock = keep;
}

void game_set_medicine(Game *g, int member, bool give)
{
    if (member >= 0 && member < g->party_count) {
        g->party[member].no_medicine = !give;
    }
}

/* ---------------------------------------------------------------- settle: what to show next */

static bool everyone_gone(const Game *g)
{
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i) && g->party[i].role != ROLE_SERVANT) {
            return false;
        }
    }
    return true;
}

static void open_notice(Game *g)
{
    memset(&g->dlg, 0, sizeof g->dlg);
    g->dlg.kind = DLG_NOTICE;
    g->dlg.speaker = -1;
    g->dlg.subject = -1;
}

void g_settle(Game *g)
{
    if (g->ending == ENDING_NONE && everyone_gone(g) && !(g->dlg.kind != DLG_NONE && g->dlg.showing_result)) {
        g->ending = ENDING_LOST;
        g->dlg.kind = DLG_NONE;
    }
    if (g->ending != ENDING_NONE && !(g->dlg.kind != DLG_NONE && g->dlg.showing_result)) {
        g->dlg.kind = DLG_NONE;
        g->phase = g->pending_grave_count > 0 ? PHASE_GRAVE : PHASE_ENDED;
        return;
    }
    if (g->dlg.kind != DLG_NONE) {
        g->phase = PHASE_DIALOG;
        return;
    }
    if (g->hunt.active) {
        g->phase = PHASE_HUNT;
        return;
    }
    if (g->barter.active) {
        g->phase = PHASE_BARTER;
        return;
    }
    if (g->pending_grave_count > 0) {
        g->phase = PHASE_GRAVE;
        return;
    }
    if (g->notice_count > 0) {
        open_notice(g);
        g->phase = PHASE_DIALOG;
        return;
    }
    if (everyone_gone(g)) {
        g->ending = ENDING_LOST;
        g->phase = PHASE_ENDED;
        return;
    }
    if (g->shop >= 0) {
        g->phase = PHASE_STORE;
        return;
    }
    if (g->at_node) {
        if (g->data->nodes[g->node].kind == NODE_END) {
            g->ending = ENDING_ARRIVED;
            LocText t;
            loctext_set(&t, "journal.arrived_end");
            game_journal(g, &t);
            g->phase = PHASE_ENDED;
            return;
        }
        memset(&g->dlg, 0, sizeof g->dlg);
        g->dlg.kind = DLG_LANDMARK;
        g->dlg.speaker = -1;
        g->dlg.subject = -1;
        g->phase = PHASE_DIALOG;
        return;
    }
    g->phase = PHASE_TRAVEL;
}

/* ---------------------------------------------------------------- dialogs */

static void notice_view(const Game *g, DialogView *v)
{
    v->kind = DLG_NOTICE;
    loctext_set(&v->title, "title.notice");
    if (g->notice_count > 0) {
        v->body[0] = g->notices[0];
        v->body_count = 1;
    }
    loctext_set(&v->choices[0].text, "choice.continue");
    v->choices[0].enabled = true;
    v->choice_count = 1;
}

bool game_dialog_view(const Game *g, DialogView *out)
{
    memset(out, 0, sizeof *out);
    if (g->phase != PHASE_DIALOG) {
        return false;
    }
    switch (g->dlg.kind) {
    case DLG_EVENT: g_event_view(g, out); break;
    case DLG_LANDMARK: g_landmark_view(g, out); break;
    case DLG_RIVER: g_river_view(g, out); break;
    case DLG_ROUTE: g_route_view(g, out); break;
    case DLG_NOTICE: notice_view(g, out); break;
    case DLG_BREAKDOWN: g_breakdown_view(g, out); break;
    case DLG_STRANDED: g_stranded_view(g, out); break;
    default: return false;
    }
    return true;
}

void game_choose(Game *g, int choice)
{
    if (g->phase != PHASE_DIALOG) {
        return;
    }
    switch (g->dlg.kind) {
    case DLG_EVENT: g_event_choose(g, choice); break;
    case DLG_LANDMARK: g_landmark_choose(g, choice); break;
    case DLG_RIVER: g_river_choose(g, choice); break;
    case DLG_ROUTE: g_route_choose(g, choice); break;
    case DLG_BREAKDOWN: g_breakdown_choose(g, choice); break;
    case DLG_STRANDED: g_stranded_choose(g, choice); break;
    case DLG_NOTICE:
        if (g->notice_count > 0) {
            memmove(&g->notices[0], &g->notices[1], sizeof g->notices[0] * (size_t)(g->notice_count - 1));
            g->notice_count--;
        }
        g->dlg.kind = DLG_NONE;
        break;
    default: break;
    }
    g_settle(g);
}

/* ---------------------------------------------------------------- graves */

const Grave *game_pending_grave(const Game *g)
{
    if (g->pending_grave_count <= 0) {
        return NULL;
    }
    return &g->graves[g->pending_graves[0]];
}

void game_grave_done(Game *g, const char *epitaph)
{
    if (g->pending_grave_count <= 0) {
        return;
    }
    Grave *gr = &g->graves[g->pending_graves[0]];
    snprintf(gr->epitaph, sizeof gr->epitaph, "%s", epitaph ? epitaph : "");
    memmove(&g->pending_graves[0], &g->pending_graves[1], sizeof g->pending_graves[0] * (size_t)(g->pending_grave_count - 1));
    g->pending_grave_count--;
    g_settle(g);
}

/* ---------------------------------------------------------------- epilogue */

int game_epilogue(const Game *g, LocText *out, int max)
{
    const GameData *d = g->data;
    int n = 0;
#define ADD(key) (n < max ? (loctext_set(&out[n], key), &out[n++]) : NULL)
    static const char *ENDING_KEYS[] = {"ending.none", "ending.arrived", "ending.settled", "ending.turned_back",
                                        "ending.stranded", "ending.lost"};
    LocText *t = ADD(ENDING_KEYS[g->ending]);
    if (t) {
        loctext_arg_str(t, "family", d->families[g->family].surname);
        loctext_arg_date(t, "date", g->day);
        loctext_arg_int(t, "days", g->day - g->start_day);
        loctext_arg_int(t, "miles", g->miles_total);
        loctext_arg_key(t, "place", d->nodes[g->node].name_key);
    }
    int with = game_member_count_with_party(g);
    if ((t = ADD("ending.survivors")) != NULL) {
        loctext_arg_int(t, "n", with);
        loctext_arg_int(t, "total", g->party_count);
    }
    for (int i = 0; i < g->grave_count; i++) {
        const Grave *gr = &g->graves[i];
        if ((t = ADD("ending.grave")) != NULL) {
            loctext_arg_person(t, "who", &g->party[gr->member]);
            loctext_arg_date(t, "date", gr->day);
            loctext_arg_key(t, "place", d->nodes[gr->node].name_key);
            loctext_arg_key(t, "cause", gr->cause_key);
        }
    }
    for (int i = 0; i < g->party_count; i++) {
        if (g->party[i].alive && !g->party[i].with_party && (t = ADD("ending.left")) != NULL) {
            loctext_arg_person(t, "who", &g->party[i]);
        }
    }
    for (int i = 0; i < d->group_count; i++) {
        if (g->relation[i] >= 30 && (t = ADD("ending.relation.good")) != NULL) {
            loctext_arg_key(t, "group", d->groups[i].name_key);
        } else if (g->relation[i] <= -20 && (t = ADD("ending.relation.bad")) != NULL) {
            loctext_arg_key(t, "group", d->groups[i].name_key);
        }
    }
    static const char *FLAGS[] = {"joined_laager", "helped_trekkers", "met_moroka", "heard_mosega", "saw_bible_gift"};
    for (size_t i = 0; i < sizeof FLAGS / sizeof FLAGS[0]; i++) {
        if (game_has_flag(g, FLAGS[i])) {
            char key[KEY_LEN];
            snprintf(key, sizeof key, "ending.flag.%s", FLAGS[i]);
            ADD(key);
        }
    }
    if ((t = ADD("ending.score")) != NULL) {
        int people, road, goods, total;
        game_score_parts(g, &people, &road, &goods, &total);
        loctext_arg_int(t, "n", total);
        loctext_arg_int(t, "people", people);
        loctext_arg_int(t, "road", road);
        loctext_arg_int(t, "goods", goods);
    }
#undef ADD
    return n;
}

int game_days_to(const Game *g, int node)
{
    const GameData *d = g->data;
    if (node < 0 || node >= d->node_count) {
        return -1;
    }
    /* shortest road, counted in days: each road at the pace's miles a day for its terrain and the
     * wagon, with about three days in ten lost to Sundays, weather, rests and stops */
    double best[MAX_NODES];
    bool done[MAX_NODES] = {false};
    for (int i = 0; i < d->node_count; i++) {
        best[i] = 1e9;
    }
    int from = g->edge >= 0 ? d->edges[g->edge].to : g->node;
    double per_day = d->rules.miles[g->pace] * d->wagons[g->wagon].speed * 0.7;
    if (per_day <= 0) {
        return -1;
    }
    best[from] = g->edge >= 0 ? (d->edges[g->edge].miles - g->miles_along) / per_day : 0;
    for (int round = 0; round < d->node_count; round++) {
        int u = -1;
        for (int i = 0; i < d->node_count; i++) {
            if (!done[i] && best[i] < 1e9 && (u < 0 || best[i] < best[u])) {
                u = i;
            }
        }
        if (u < 0) {
            break;
        }
        done[u] = true;
        for (int e = 0; e < d->edge_count; e++) {
            const EdgeDef *ed = &d->edges[e];
            if (ed->from != u) {
                continue;
            }
            double days = best[u] + ed->miles / (per_day * ed->terrain_mul) + d->nodes[ed->to].delay_days;
            if (days < best[ed->to]) {
                best[ed->to] = days;
            }
        }
    }
    return best[node] >= 1e9 ? -1 : (int)lround(best[node]);
}

int game_grave_story(const Game *g, const Grave *gr, LocText *out, int max)
{
    const GameData *d = g->data;
    const Member *m = &g->party[gr->member];
    int n = 0;
    int y, mo, md;
    cal_date(gr->day, &y, &mo, &md);
    if (n < max) {
        char role[32];
        member_role_key(m->role, m->pronoun, role, sizeof role);
        loctext_set(&out[n], m->role == ROLE_SERVANT ? "grave.who.servant" : "grave.who");
        loctext_arg_person(&out[n], "who", m);
        loctext_arg_key(&out[n], "role", role);
        loctext_arg_str(&out[n], "family", d->families[g->family].surname);
        loctext_arg_int(&out[n], "age", y - m->birth_year);
        n++;
    }
    if (n < max) {
        char cause[40]; /* a TextArg holds 40 characters; cause keys are short */
        snprintf(cause, sizeof cause, "%.39s", gr->cause_key);
        loctext_set(&out[n], "grave.cause");
        loctext_arg_person(&out[n], "who", m);
        loctext_arg_key(&out[n], "cause", cause);
        n++;
    }
    if (n < max) {
        /* grave.why.<the cause without "cause.">: how it came about */
        const char *id = strncmp(gr->cause_key, "cause.", 6) == 0 ? gr->cause_key + 6 : "unknown";
        char key[KEY_LEN];
        snprintf(key, sizeof key, "grave.why.%s", id);
        loctext_set(&out[n], key);
        loctext_arg_person(&out[n], "who", m);
        loctext_arg_date(&out[n], "date", gr->day - (gr->illness >= 0 ? gr->ill_days : 0));
        loctext_arg_int(&out[n], "days", gr->ill_days);
        loctext_arg_key(&out[n], "place", d->nodes[gr->node].name_key);
        n++;
    }
    return n;
}

int game_history_epilogue(const Game *g, LocText *out, int max)
{
    if (g->ending != ENDING_ARRIVED && g->ending != ENDING_SETTLED) {
        return 0;
    }
    const NodeDef *nd = &g->data->nodes[g->node];
    int n = 0;
    for (int k = 0; k < nd->epilogue_count && n < max; k++) {
        loctext_set(&out[n++], nd->epilogue[k]);
    }
    return n;
}

/* ---------------------------------------------------------------- score */

/* The score is mostly the people brought through (250 and their health each), then the road
 * covered (1 a mile), so a longer trek counts for more but never more than the lives it costs;
 * herds, money and good relations add a little. Settling short of a journey's end counts three
 * quarters, a trek that got nowhere (stranded, turned back, everyone lost) half, and harder
 * difficulties count more. */
void game_score_parts(const Game *g, int *people, int *road, int *goods, int *total)
{
    const GameData *d = g->data;
    int p = 0, r = 0, s = 0;
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i)) {
            p += 250 + g->party[i].health;
        }
    }
    r = g->miles_total;
    s += g->inv[d->item_oxen] * 3 + g->inv[d->item_cows] + g->inv[d->item_horses] * 2 + g->inv[d->item_sheep] / 4;
    s += g->money / (g->side == SIDE_DUTCH ? 20 : 30);
    for (int i = 0; i < d->group_count; i++) {
        if (g->relation[i] > 0) {
            s += g->relation[i] / 2;
        }
    }
    /* settling short of a journey's end counts three quarters; not getting anywhere, half */
    double mul = g->ending == ENDING_ARRIVED ? 1.0 : g->ending == ENDING_SETTLED ? 0.75 : 0.5;
    static const double DIFF_MUL[DIFF_COUNT] = {0.5, 1.0, 1.5};
    mul *= DIFF_MUL[g->difficulty];
    *people = (int)lround(p * mul);
    *road = (int)lround(r * mul);
    *goods = (int)lround(s * mul);
    *total = *people + *road + *goods;
}

int game_score(const Game *g)
{
    int people, road, goods, total;
    game_score_parts(g, &people, &road, &goods, &total);
    return total;
}
