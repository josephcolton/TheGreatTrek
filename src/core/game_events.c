/* Events: checking conditions, picking an event, showing it, and applying a choice. */
#include "calendar.h"
#include "game_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- conditions */

static bool has_role(const Game *g, Role role, bool healthy)
{
    for (int i = 0; i < g->party_count; i++) {
        bool ok = healthy ? g_member_healthy(g, i) : g_member_active(g, i);
        if (ok && g->party[i].role == role) {
            return true;
        }
    }
    return false;
}

bool g_eval(const Game *g, const Cond *c)
{
    switch (c->type) {
    case COND_SIDE: return (int)g->side == c->a;
    case COND_FAMILY: return g->family == c->a;
    case COND_REGION: return g_current_region(g) == c->a;
    case COND_NODE: return g->node == c->a;
    case COND_EDGE_WATER: return g->edge >= 0 && (int)g->data->edges[g->edge].water == c->a;
    case COND_MONTH: return cal_month(g->day) == c->b;
    case COND_WEEKDAY: return cal_weekday(g->day) == c->b;
    case COND_SABBATH: return g->sabbath == (c->b != 0);
    case COND_WAGON: return g->wagon == c->a;
    case COND_WAGON_WORN: {
        bool worn = false;
        for (int p = 0; p < WAGON_PART_COUNT; p++) {
            worn = worn || g->wagon_parts[p] < 100;
        }
        return worn == (c->b != 0);
    }
    case COND_AFFORD_REPAIR: return (g->money >= game_pence_to_unit(g, g_smith_cost(g))) == (c->b != 0);
    case COND_AFFORD_WAGON: {
        int cost = g_wagon_trade_cost(g, c->a);
        return cost <= 0 || g->money >= game_pence_to_unit(g, cost);
    }
    case COND_SEASON: return (int)cal_season(g->day) == c->a;
    case COND_WEATHER: return (int)g->weather == c->a;
    case COND_FLAG: return game_has_flag(g, c->str);
    case COND_NOT_FLAG: return !game_has_flag(g, c->str);
    case COND_COUNTER_GTE: return game_counter(g, c->str) >= c->b;
    case COND_ITEM_GTE: return g->inv[c->a] >= c->b;
    case COND_ITEM_LT: return g->inv[c->a] < c->b;
    case COND_MONEY_GTE: return g->money >= c->b;
    case COND_MEMBER_ROLE: return has_role(g, (Role)c->a, false);
    case COND_MEMBER_ROLE_HEALTHY: return has_role(g, (Role)c->a, true);
    case COND_MEMBER_SKILL_GTE: return g_best_skill(g, (Skill)c->a, true) >= c->b;
    case COND_RELATION_GTE: return g->relation[c->a] >= c->b;
    case COND_RELATION_LT: return g->relation[c->a] < c->b;
    case COND_STANCE: return (int)g->stance == c->a;
    case COND_DATE_GTE: return g->day >= c->b;
    case COND_DATE_LT: return g->day < c->b;
    case COND_DAYS_GTE: return g->day - g->start_day >= c->b;
    case COND_DISTANCE_GTE: return g->miles_total >= c->b;
    case COND_CHANCE: return true; /* rolled separately so evaluation has no side effects */
    case COND_ANY:
        for (int i = 0; i < c->kid_count; i++) {
            if (g_eval(g, &c->kids[i])) {
                return true;
            }
        }
        return false;
    case COND_NOT: return c->kid_count > 0 && !g_eval(g, &c->kids[0]);
    default: return false;
    }
}

bool g_eval_all(const Game *g, const Cond *c, int n)
{
    for (int i = 0; i < n; i++) {
        if (!g_eval(g, &c[i])) {
            return false;
        }
    }
    return true;
}

/* Chance clauses are rolled when an event is picked, not while checking. */
static bool roll_chances(Game *g, const Cond *c, int n)
{
    for (int i = 0; i < n; i++) {
        if (c[i].type == COND_CHANCE && !rng_chance(&g->rng, c[i].x)) {
            return false;
        }
    }
    return true;
}

/* ---------------------------------------------------------------- picking */

static bool event_eligible(const Game *g, int idx, bool night)
{
    const EventDef *ev = &g->data->events[idx];
    if (ev->setpiece || ev->night != night) {
        return false;
    }
    if (!(ev->sides & (1u << g->side))) {
        return false;
    }
    if (ev->regions && !(ev->regions & ((uint64_t)1 << g_current_region(g)))) {
        return false;
    }
    if (ev->months && !(ev->months & (1u << (cal_month(g->day) - 1)))) {
        return false;
    }
    int last = g->event_last_day[idx];
    if (last >= 0) {
        if (ev->once || g->day - last < ev->cooldown_days) {
            return false;
        }
    }
    return g_eval_all(g, ev->requires, ev->require_count);
}

bool g_maybe_event(Game *g, bool night)
{
    const GameData *d = g->data;
    double rate = d->rules.base_event_rate * d->regions[g_current_region(g)].event_mul;
    rate *= night ? 0.4 : 0.6;
    if (!rng_chance(&g->rng, rate)) {
        return false;
    }
    double *weights = calloc((size_t)(d->event_count ? d->event_count : 1), sizeof *weights);
    if (!weights) {
        return false;
    }
    double total = 0;
    for (int i = 0; i < d->event_count; i++) {
        if (!event_eligible(g, i, night)) {
            continue;
        }
        const EventDef *ev = &d->events[i];
        double w = ev->weight;
        for (int m = 0; m < ev->mod_count; m++) {
            if (g_eval(g, &ev->mod_conds[m])) {
                w *= ev->mod_muls[m];
            }
        }
        if (ev->harmful) {
            w *= g_harm(g);
            if (night) {
                w *= d->rules.night_risk_mul[g->stance];
                /* Travelling with the united laager after the Sand River: many wagons, shared guard. */
                if (g->stance == NIGHT_LAAGER && game_has_flag(g, "joined_laager")) {
                    w *= 0.5;
                }
            }
        }
        weights[i] = w;
        total += w;
    }
    bool started = false;
    if (total > 0) {
        int pick = rng_weighted(&g->rng, weights, d->event_count);
        if (pick >= 0 && roll_chances(g, d->events[pick].requires, d->events[pick].require_count)) {
            g_start_event(g, pick);
            started = true;
        }
    }
    free(weights);
    return started;
}

static int pick_speaker(Game *g, const EventDef *ev)
{
    switch (ev->speaker_pick) {
    case SPEAKER_ROLE: return g_random_member(g, false, ev->speaker_arg);
    case SPEAKER_SKILL: return g_best_skill_member(g, (Skill)ev->speaker_arg);
    case SPEAKER_RANDOM: return g_random_member(g, false, -1);
    default: return -1;
    }
}

void g_start_event(Game *g, int event)
{
    if (event < 0 || event >= g->data->event_count) {
        return;
    }
    memset(&g->dlg, 0, sizeof g->dlg);
    g->dlg.kind = DLG_EVENT;
    g->dlg.event = event;
    g->dlg.node = 0;
    g->dlg.speaker = pick_speaker(g, &g->data->events[event]);
    g->dlg.subject = -1;
    g->dlg.next_node = -1;
    g->event_last_day[event] = g->day;
    g->stop_travel = true;
}

/* ---------------------------------------------------------------- view */

static void add_std_args(const Game *g, LocText *t)
{
    int head = g_head(g);
    if (g->dlg.speaker >= 0) {
        loctext_arg_person(t, "speaker", &g->party[g->dlg.speaker]);
    }
    if (head >= 0) {
        loctext_arg_person(t, "head", &g->party[head]);
    }
    loctext_arg_str(t, "family", g->data->families[g->family].surname);
    if (g->dlg.subject >= 0) {
        loctext_arg_person(t, "who", &g->party[g->dlg.subject]);
    }
}

static void speaker_text(const Game *g, LocText *t, const EventNode *node)
{
    if (node->speaker_key[0]) {
        loctext_set(t, node->speaker_key);
        return;
    }
    if (g->dlg.speaker >= 0) {
        const Member *m = &g->party[g->dlg.speaker];
        char role_key[KEY_LEN];
        member_role_key(m->role, m->pronoun, role_key, sizeof role_key);
        loctext_set(t, "speaker.member");
        loctext_arg_person(t, "name", m);
        loctext_arg_key(t, "role", role_key);
    }
}

void g_event_view(const Game *g, DialogView *v)
{
    const EventDef *ev = &g->data->events[g->dlg.event];
    const EventNode *node = &ev->nodes[g->dlg.node];
    v->kind = DLG_EVENT;
    snprintf(v->image, sizeof v->image, "%s", node->image);
    if (node->portrait[0]) {
        snprintf(v->portrait, sizeof v->portrait, "%s", node->portrait);
    } else if (!node->speaker_key[0] && g->dlg.speaker >= 0) {
        snprintf(v->portrait, sizeof v->portrait, "%s", g->party[g->dlg.speaker].portrait);
    }
    snprintf(v->history_key, sizeof v->history_key, "%s", ev->history_key);
    speaker_text(g, &v->speaker, node);
    if (g->dlg.showing_result) {
        for (int i = 0; i < g->dlg.result_count && i < MAX_RESULT_LINES; i++) {
            v->body[v->body_count++] = g->dlg.result[i];
        }
        loctext_set(&v->choices[0].text, "choice.continue");
        v->choices[0].enabled = true;
        v->choice_count = 1;
        return;
    }
    loctext_set(&v->body[0], node->text_key);
    add_std_args(g, &v->body[0]);
    v->body_count = 1;
    for (int c = 0; c < node->choice_count; c++) {
        const Choice *ch = &node->choices[c];
        ChoiceView *cv = &v->choices[v->choice_count++];
        loctext_set(&cv->text, ch->text_key);
        add_std_args(g, &cv->text);
        cv->enabled = g_eval_all(g, ch->requires, ch->require_count);
        if (!cv->enabled) {
            loctext_set(&cv->locked, ch->locked_key[0] ? ch->locked_key : "lock.generic");
        }
    }
}

/* ---------------------------------------------------------------- effects */

void g_add_result(Game *g, const LocText *t)
{
    if (g->dlg.result_count < MAX_RESULT_LINES) {
        g->dlg.result[g->dlg.result_count++] = *t;
    }
    game_journal(g, t);
}

void g_add_result_key(Game *g, const char *key)
{
    LocText t;
    loctext_set(&t, key);
    g_add_result(g, &t);
}

static int select_members(Game *g, const Effect *e, int *out)
{
    int n = 0;
    switch (e->who) {
    case WHO_ALL:
        for (int i = 0; i < g->party_count; i++) {
            if (g_member_active(g, i)) {
                out[n++] = i;
            }
        }
        break;
    case WHO_SPEAKER:
        if (g_member_active(g, g->dlg.speaker)) {
            out[n++] = g->dlg.speaker;
        }
        break;
    case WHO_HEAD: {
        int h = g_head(g);
        if (h >= 0) {
            out[n++] = h;
        }
        break;
    }
    default: {
        int count = e->count < 1 ? 1 : e->count;
        for (int k = 0; k < count; k++) {
            int m = g_random_member(g, e->who == WHO_CHILD, e->who == WHO_ROLE ? (int)e->role : -1);
            if (m < 0) {
                break;
            }
            bool dup = false;
            for (int j = 0; j < n; j++) {
                dup |= out[j] == m;
            }
            if (!dup) {
                out[n++] = m;
            }
        }
        break;
    }
    }
    return n;
}

static void item_result(Game *g, int item, int delta)
{
    const ItemDef *it = &g->data->items[item];
    LocText t;
    int n = delta > 0 ? delta : -delta;
    char key[KEY_LEN];
    snprintf(key, sizeof key, "%s%s", it->name_key, n == 1 ? ".one" : "");
    loctext_set(&t, delta > 0 ? "result.gained" : "result.lost");
    loctext_arg_int(&t, "n", n);
    loctext_arg_key(&t, "item", key);
    g_add_result(g, &t);
}

static void apply_effect(Game *g, const Effect *e)
{
    const GameData *d = g->data;
    int members[MAX_PARTY];
    int n;
    switch (e->type) {
    case EFF_ITEM: {
        int before = g->inv[e->a];
        g_add_item(g, e->a, e->amount);
        int delta = g->inv[e->a] - before;
        if (delta != 0 && e->a != d->item_water) {
            item_result(g, e->a, delta);
        }
        break;
    }
    case EFF_MONEY: {
        int before = g->money;
        g_add_money_pence(g, e->amount);
        int delta = g->money - before;
        if (delta != 0) {
            LocText t;
            loctext_set(&t, delta > 0 ? "result.money_gained" : "result.money_lost");
            loctext_arg_money(&t, "n", delta > 0 ? delta : -delta);
            g_add_result(g, &t);
        }
        break;
    }
    case EFF_OX_STAMINA:
        g->ox_stamina += e->amount;
        g->ox_stamina = g->ox_stamina < 0 ? 0 : (g->ox_stamina > 100 ? 100 : g->ox_stamina);
        break;
    case EFF_WAGON_DAMAGE:
        g->wagon_parts[e->a] -= (int)lround(e->amount * g_harm(g));
        if (g->wagon_parts[e->a] < 0) {
            g->wagon_parts[e->a] = 0;
        }
        break;
    case EFF_HEALTH:
        n = select_members(g, e, members);
        for (int i = 0; i < n; i++) {
            int amount = e->amount < 0 ? (int)lround(e->amount * g_harm(g)) : e->amount;
            g->party[members[i]].health += amount;
            if (g->party[members[i]].health > 100) {
                g->party[members[i]].health = 100;
            }
            g->dlg.subject = members[i];
            if (g->party[members[i]].health <= 0) {
                g_kill(g, members[i], "cause.accident");
                LocText t;
                loctext_set(&t, "result.died");
                loctext_arg_person(&t, "who", &g->party[members[i]]);
                g_add_result(g, &t);
            }
        }
        break;
    case EFF_ILLNESS:
        n = select_members(g, e, members);
        for (int i = 0; i < n; i++) {
            g_add_illness(g, members[i], e->a);
            g->dlg.subject = members[i];
            LocText t;
            loctext_set(&t, d->illnesses[e->a].accident ? "notice.accident" : "notice.fell_ill");
            loctext_arg_person(&t, "who", &g->party[members[i]]);
            loctext_arg_key(&t, "illness", d->illnesses[e->a].name_key);
            g_add_result(g, &t);
        }
        break;
    case EFF_MORALE: g_add_morale(g, e->amount); break;
    case EFF_LOYALTY:
        n = select_members(g, e, members);
        for (int i = 0; i < n; i++) {
            int v = g->party[members[i]].loyalty + e->amount;
            g->party[members[i]].loyalty = v < 0 ? 0 : (v > 100 ? 100 : v);
        }
        break;
    case EFF_RELATION: g_add_relation(g, e->a, e->amount); break;
    case EFF_FLAG: g_set_flag(g, e->str); break;
    case EFF_CLEAR_FLAG: g_clear_flag(g, e->str); break;
    case EFF_COUNTER: g_add_counter(g, e->str, e->amount); break;
    case EFF_TIME_DAYS: {
        int whole = (int)floor(e->famount);
        for (int i = 0; i < whole; i++) {
            g_pass_day(g, false, false);
        }
        double frac = e->famount - whole;
        if (frac > 0 && g->edge >= 0) {
            g->miles_along -= frac * d->rules.miles[g->pace];
            if (g->miles_along < 0) {
                g->miles_along = 0;
            }
        }
        break;
    }
    case EFF_DISTANCE:
        if (g->edge >= 0) {
            g->miles_along += e->amount;
            if (g->miles_along < 0) {
                g->miles_along = 0;
            }
        }
        break;
    case EFF_SCHEDULE:
        if (g->scheduled_count < MAX_SCHEDULED && e->a >= 0) {
            g->scheduled[g->scheduled_count].event = e->a;
            g->scheduled[g->scheduled_count].day = g->day + e->amount;
            g->scheduled_count++;
        }
        break;
    case EFF_DEATH:
        n = select_members(g, e, members);
        for (int i = 0; i < n; i++) {
            g->dlg.subject = members[i];
            g_kill(g, members[i], "cause.accident");
        }
        break;
    case EFF_LEAVE:
        n = select_members(g, e, members);
        for (int i = 0; i < n; i++) {
            g->dlg.subject = members[i];
            g->party[members[i]].with_party = false;
            g_add_morale(g, -5);
        }
        break;
    case EFF_JOURNAL: {
        LocText t;
        loctext_set(&t, e->str);
        game_journal(g, &t);
        break;
    }
    case EFF_ENDING:
        g->ending = (EndingKind)e->amount;
        break;
    case EFF_DEFEND: game_defend_start(g, e->a); break;
    case EFF_SMITH_QUOTE:
    case EFF_SMITH_REPAIR: {
        int cost = g_smith_cost(g);
        LocText t;
        loctext_set(&t, e->type == EFF_SMITH_QUOTE ? "notice.smith_quote" : "notice.smith_paid");
        loctext_arg_money(&t, "money", game_pence_to_unit(g, cost));
        g_add_result(g, &t);
        if (e->type == EFF_SMITH_REPAIR) {
            g_add_money_pence(g, -cost);
            for (int p = 0; p < WAGON_PART_COUNT; p++) {
                g->wagon_parts[p] = 100;
            }
        }
        break;
    }
    case EFF_WAGON: {
        int cost = g_wagon_trade_cost(g, e->a);
        if (cost != 0) {
            LocText t;
            loctext_set(&t, cost > 0 ? "notice.wagon_paid" : "notice.wagon_got");
            loctext_arg_money(&t, "money", game_pence_to_unit(g, cost > 0 ? cost : -cost));
            g_add_result(g, &t);
        }
        g_add_money_pence(g, -cost);
        g->wagon = e->a;
        for (int p = 0; p < WAGON_PART_COUNT; p++) {
            g->wagon_parts[p] = 100;
        }
        break;
    }
    default: break;
    }
}

/* ---------------------------------------------------------------- odds */

int game_choice_odds(const Game *g, int choice, double *out, int max)
{
    if (g->phase != PHASE_DIALOG || g->dlg.kind != DLG_EVENT || g->dlg.showing_result) {
        return 0;
    }
    const EventNode *node = &g->data->events[g->dlg.event].nodes[g->dlg.node];
    if (choice < 0 || choice >= node->choice_count) {
        return 0;
    }
    const Choice *ch = &node->choices[choice];
    double total = 0;
    int n = ch->outcome_count < max ? ch->outcome_count : max;
    for (int i = 0; i < n; i++) {
        const Outcome *o = &ch->outcomes[i];
        out[i] = o->weight + (o->skill >= 0 ? o->per_point * g_best_skill(g, (Skill)o->skill, true) : 0);
        total += out[i];
    }
    for (int i = 0; i < n; i++) {
        out[i] = total > 0 ? out[i] / total : 0;
    }
    return n;
}

/* ---------------------------------------------------------------- choosing */

void g_event_choose(Game *g, int choice)
{
    const EventDef *ev = &g->data->events[g->dlg.event];
    if (g->dlg.showing_result) {
        g->dlg.showing_result = false;
        g->dlg.result_count = 0;
        if (g->dlg.next_node >= 0) {
            g->dlg.node = g->dlg.next_node;
            g->dlg.next_node = -1;
        } else {
            g->dlg.kind = DLG_NONE;
        }
        return;
    }
    const EventNode *node = &ev->nodes[g->dlg.node];
    if (choice < 0 || choice >= node->choice_count) {
        return;
    }
    const Choice *ch = &node->choices[choice];
    if (!g_eval_all(g, ch->requires, ch->require_count)) {
        return;
    }
    double w[16];
    int n = ch->outcome_count < 16 ? ch->outcome_count : 16;
    for (int i = 0; i < n; i++) {
        const Outcome *o = &ch->outcomes[i];
        w[i] = o->weight;
        if (o->skill >= 0) {
            w[i] += o->per_point * g_best_skill(g, (Skill)o->skill, true);
        }
    }
    int pick = rng_weighted(&g->rng, w, n);
    if (pick < 0) {
        pick = 0;
    }
    const Outcome *o = &ch->outcomes[pick];
    g->dlg.result_count = 0;
    g->dlg.subject = -1;
    /* Effects first, so the outcome text can name who was affected. */
    LocText lines_before[MAX_RESULT_LINES];
    int effect_lines;
    for (int i = 0; i < o->effect_count; i++) {
        apply_effect(g, &o->effects[i]);
    }
    effect_lines = g->dlg.result_count;
    memcpy(lines_before, g->dlg.result, sizeof(LocText) * (size_t)effect_lines);
    g->dlg.result_count = 0;
    if (o->text_key[0]) {
        LocText t;
        loctext_set(&t, o->text_key);
        add_std_args(g, &t);
        g_add_result(g, &t);
    }
    for (int i = 0; i < effect_lines && g->dlg.result_count < MAX_RESULT_LINES; i++) {
        g->dlg.result[g->dlg.result_count++] = lines_before[i];
    }
    g->dlg.next_node = o->next;
    if (g->dlg.result_count > 0) {
        g->dlg.showing_result = true;
    } else if (o->next >= 0) {
        g->dlg.node = o->next;
    } else {
        g->dlg.kind = DLG_NONE;
    }
}
