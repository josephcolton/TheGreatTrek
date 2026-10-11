/* Barter beyond the Colony: no coin, only goods (04_game_design.md section 7.4).
 * Each partner values goods differently. A fair deal builds goodwill; a mean one costs it.
 * Gunpowder is the most valuable thing to offer and a crime to sell beyond the frontier. */
#include "game_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const BarterPartner *partner(const Game *g)
{
    return &g->data->partners[g->barter.partner];
}

static double want_value(const BarterPartner *p, int item)
{
    for (int i = 0; i < p->want_count; i++) {
        if (p->wants[i].item == item) {
            return p->wants[i].value;
        }
    }
    return 0;
}

static int step_for(const GameData *d, int item)
{
    int pack = d->items[item].pack;
    return pack > 12 ? 10 : 1;
}

static bool is_contraband(const Game *g, int item)
{
    const GameData *d = g->data;
    if (partner(g)->group == d->group_trekkers) {
        return false; /* trading powder among the trekkers themselves was not the crime */
    }
    for (int i = 0; i < d->contraband_count; i++) {
        if (d->contraband[i] == item) {
            return true;
        }
    }
    return false;
}

/* What your side of the deal is worth to them, and what theirs is worth. */
static void deal_values(const Game *g, double *give, double *take, double *need)
{
    const GameData *d = g->data;
    const BarterPartner *p = partner(g);
    double gv = 0, tv = 0;
    for (int i = 0; i < d->item_count; i++) {
        gv += g->barter.give[i] * want_value(p, i);
    }
    for (int i = 0; i < p->offer_count; i++) {
        tv += g->barter.take[p->offers[i].item] * p->offers[i].value;
    }
    /* A good interpreter and the family's trading skill make your goods count for more. */
    gv *= d->families[g->family].barter_mul * (1.0 + 0.04 * g_best_skill(g, SKILL_INTERPRETING, true));
    *give = gv;
    *take = tv;
    *need = tv * p->fairness * (1.0 - g->relation[p->group] / 400.0);
}

int game_barter_partner_here(const Game *g)
{
    for (int i = 0; i < g->data->partner_count; i++) {
        if (g->data->partners[i].node == g->node) {
            return i;
        }
    }
    return -1;
}

static void reply(Game *g, const char *key)
{
    loctext_set(&g->barter.reply, key);
}

void game_barter_start(Game *g, int p)
{
    if (p < 0 || p >= g->data->partner_count) {
        return;
    }
    memset(&g->barter, 0, sizeof g->barter);
    g->barter.active = true;
    g->barter.partner = p;
    char key[KEY_LEN + 16];
    snprintf(key, sizeof key, "%s.greeting", g->data->partners[p].name_key);
    reply(g, key);
    g->dlg.kind = DLG_NONE;
    g_settle(g);
}

bool game_barter_view(const Game *g, BarterView *out)
{
    memset(out, 0, sizeof *out);
    if (g->phase != PHASE_BARTER) {
        return false;
    }
    const GameData *d = g->data;
    const BarterPartner *p = partner(g);
    loctext_set(&out->partner, p->name_key);
    out->reply = g->barter.reply;
    /* What you can offer: anything they want. */
    for (int i = 0; i < p->want_count && out->give_count < MAX_BARTER_GOODS; i++) {
        int item = p->wants[i].item;
        BarterRow *r = &out->give[out->give_count++];
        r->item = item;
        loctext_set(&r->name, d->items[item].name_key);
        r->have = g->inv[item];
        r->amount = g->barter.give[item];
        r->step = step_for(d, item);
        /* "Highly wanted": worth noticeably more to them than in a colonial shop. */
        double shop = d->items[item].price_pence / (double)d->items[item].pack;
        r->wanted_highly = g->barter.asked && p->wants[i].value > shop * 1.4;
        if (r->amount > 0 && is_contraband(g, item)) {
            out->contraband = true;
        }
    }
    for (int i = 0; i < p->offer_count && out->take_count < MAX_BARTER_GOODS; i++) {
        int item = p->offers[i].item;
        BarterRow *r = &out->take[out->take_count++];
        r->item = item;
        loctext_set(&r->name, d->items[item].name_key);
        r->have = p->offers[i].stock - g->partner_traded[g->barter.partner][i];
        r->amount = g->barter.take[item];
        r->step = step_for(d, item);
    }
    deal_values(g, &out->give_value, &out->take_value, &out->need_value);
    return true;
}

void game_barter_adjust(Game *g, bool theirs, int row, int delta)
{
    BarterView v;
    if (!game_barter_view(g, &v)) {
        return;
    }
    if (theirs) {
        if (row < 0 || row >= v.take_count) {
            return;
        }
        BarterRow *r = &v.take[row];
        int amount = r->amount + delta * r->step;
        if (amount < 0) {
            amount = 0;
        }
        if (amount > r->have) {
            amount = r->have;
        }
        g->barter.take[r->item] = amount;
    } else {
        if (row < 0 || row >= v.give_count) {
            return;
        }
        BarterRow *r = &v.give[row];
        int amount = r->amount + delta * r->step;
        if (amount < 0) {
            amount = 0;
        }
        if (amount > r->have) {
            amount = r->have;
        }
        g->barter.give[r->item] = amount;
    }
}

void game_barter_ask(Game *g)
{
    if (g->phase != PHASE_BARTER) {
        return;
    }
    g->barter.asked = true;
    char key[KEY_LEN + 16];
    snprintf(key, sizeof key, "%s.needs", partner(g)->name_key);
    reply(g, key);
}

void game_barter_propose(Game *g)
{
    if (g->phase != PHASE_BARTER) {
        return;
    }
    const GameData *d = g->data;
    const BarterPartner *p = partner(g);
    double give, take, need;
    deal_values(g, &give, &take, &need);
    if (take <= 0 || give <= 0) {
        reply(g, "barter.reply.empty");
        return;
    }
    if (give < need) {
        reply(g, give < need * 0.7 ? "barter.reply.insult" : "barter.reply.short");
        if (give < need * 0.7) {
            g_add_relation(g, p->group, -1);
        }
        return;
    }
    bool contraband = false;
    for (int i = 0; i < d->item_count; i++) {
        if (g->barter.give[i] > 0 && is_contraband(g, i)) {
            contraband = true;
        }
        g_add_item(g, i, -g->barter.give[i]);
    }
    for (int i = 0; i < p->offer_count; i++) {
        int item = p->offers[i].item;
        int n = g->barter.take[item];
        g_add_item(g, item, n);
        g->partner_traded[g->barter.partner][i] += n;
    }
    /* Generous deals are remembered. */
    if (give > take * 1.25) {
        g_add_relation(g, p->group, 4);
        reply(g, "barter.reply.generous");
    } else {
        g_add_relation(g, p->group, 1);
        reply(g, "barter.reply.done");
    }
    if (contraband) {
        g_set_flag(g, "sold_powder");
        g_add_relation(g, data_find_group(d, "british"), -10);
        g_add_relation(g, d->group_trekkers, -4);
        reply(g, "barter.reply.powder");
    }
    LocText t;
    loctext_set(&t, contraband ? "journal.barter_powder" : "journal.barter");
    loctext_arg_key(&t, "partner", p->name_key);
    game_journal(g, &t);
    memset(g->barter.give, 0, sizeof g->barter.give);
    memset(g->barter.take, 0, sizeof g->barter.take);
}

void game_barter_leave(Game *g)
{
    if (g->phase != PHASE_BARTER) {
        return;
    }
    memset(&g->barter, 0, sizeof g->barter);
    g_settle(g);
}
