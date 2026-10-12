/* Shops: prices from base x side x distance x relation x scarcity x history. */
#include "game_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int shop_stock_of(const ShopDef *s, int item)
{
    for (int i = 0; i < s->stock_count; i++) {
        if (s->stock[i].item == item) {
            return s->stock[i].qty;
        }
    }
    return -1; /* not traded here */
}

/* Strongest price shock for this item here today; sets *shock to its index. */
static double shock_mul(const Game *g, int item, int *shock)
{
    const GameData *d = g->data;
    double best = 1.0;
    *shock = -1;
    for (int i = 0; i < d->shock_count; i++) {
        const PriceShock *s = &d->shocks[i];
        if (g->day < s->from_day || g->day > s->to_day) {
            continue;
        }
        bool here = false;
        for (int n = 0; n < s->node_count; n++) {
            here |= s->nodes[n] == g->node;
        }
        if (!here) {
            continue;
        }
        bool applies = s->item >= 0 ? s->item == item : d->items[item].category == s->category;
        if (!applies) {
            continue;
        }
        double span = (double)(s->to_day - s->from_day);
        double t = span > 0 ? (g->day - s->from_day) / span : 0;
        double mul = s->mul_start + (s->mul_end - s->mul_start) * t;
        if (fabs(mul - 1.0) > fabs(best - 1.0)) {
            best = mul;
            *shock = i;
        }
    }
    return best;
}

int game_price(const Game *g, int item, bool selling)
{
    if (g->shop < 0) {
        return 0;
    }
    const GameData *d = g->data;
    const ShopDef *s = &d->shops[g->shop];
    const ItemDef *it = &d->items[item];
    int stock = shop_stock_of(s, item);
    if (stock < 0 || it->price_pence <= 0) {
        return 0;
    }
    const GroupDef *grp = &d->groups[s->group];
    double pence = it->price_pence;
    pence *= grp->side_price_mul[g->side];
    pence *= d->nodes[g->node].price_mul;
    /* Good relations bring the price down a little; bad ones push it up. */
    pence *= 1.0 - g->relation[s->group] / 400.0;
    if (stock > 0) {
        double bought = g->shop_bought[g->shop][item];
        pence *= 1.0 + 0.5 * (bought / stock);
    }
    int shock;
    pence *= shock_mul(g, item, &shock);
    if (selling) {
        pence *= grp->sell_spread * d->families[g->family].barter_mul;
    }
    return game_pence_to_unit(g, (int)lround(pence));
}

static void advice(const Game *g, LocText *t)
{
    const GameData *d = g->data;
    bool spare = g->inv[d->item_spare[WAGON_PART_WHEELS]] > 0 || g->inv[d->item_spare[WAGON_PART_AXLE]] > 0;
    /* food enough for the nearest journey's end and a fortnight over */
    int near_end = -1;
    for (int n = 0; n < d->node_count; n++) {
        int days = d->nodes[n].epilogue_count > 0 ? game_days_to(g, n) : -1;
        if (days > 0 && (near_end < 0 || days < near_end)) {
            near_end = days;
        }
    }
    if (near_end < 0) {
        near_end = 46; /* no end ahead to measure by: two months' food in all */
    }
    bool dry_ahead = false;
    for (int i = 0; i < d->edge_count; i++) {
        if (d->edges[i].water == WATER_DRY && !g->visited[d->edges[i].to]) {
            dry_ahead = true;
        }
    }
    if (g->inv[d->item_oxen] < d->wagons[g->wagon].full_span) {
        loctext_set(t, "advice.oxen");
    } else if (!spare) {
        loctext_set(t, "advice.spare");
    } else if (dry_ahead && g->inv[d->item_vat] < 3) {
        loctext_set(t, "advice.water");
    } else if (game_food_days(g) < near_end + 14) {
        loctext_set(t, "advice.food");
    } else if (g->inv[d->item_medicine] < 4) {
        loctext_set(t, "advice.medicine");
    } else {
        loctext_set(t, "advice.ok");
    }
}

bool game_store_view(const Game *g, StoreView *out)
{
    memset(out, 0, sizeof *out);
    if (g->phase != PHASE_STORE || g->shop < 0) {
        return false;
    }
    const GameData *d = g->data;
    const ShopDef *s = &d->shops[g->shop];
    loctext_set(&out->shop_name, s->name_key);
    loctext_set(&out->keeper, s->keeper_key);
    advice(g, &out->advice);
    int worst_shock = -1;
    double worst = 1.0;
    for (int i = 0; i < s->stock_count; i++) {
        int item = s->stock[i].item;
        const ItemDef *it = &d->items[item];
        if (!(it->sides & (1u << g->side))) {
            continue;
        }
        StoreRow *row = &out->rows[out->row_count++];
        row->item = item;
        loctext_set(&row->name, it->name_key);
        loctext_set(&row->pack, it->pack_key);
        row->price = game_price(g, item, false);
        row->sell_price = game_price(g, item, true);
        row->stock = s->stock[i].qty - g->shop_bought[g->shop][item];
        if (row->stock < 0) {
            row->stock = 0;
        }
        row->have = g->inv[item];
        row->pack_size = it->pack;
        int sh;
        double m = shock_mul(g, item, &sh);
        if (sh >= 0 && m > worst) {
            worst = m;
            worst_shock = sh;
        }
    }
    if (worst_shock >= 0 && d->shocks[worst_shock].excuse_key[0]) {
        loctext_set(&out->excuse, d->shocks[worst_shock].excuse_key);
    }
    out->money = g->money;
    out->load_lb = game_load_lb(g);
    out->capacity_lb = game_capacity_lb(g);
    return true;
}

bool game_store_buy(Game *g, int item, int packs)
{
    if (g->phase != PHASE_STORE || packs <= 0 || item < 0 || item >= g->data->item_count) {
        return false;
    }
    const ShopDef *s = &g->data->shops[g->shop];
    int stock = shop_stock_of(s, item);
    if (stock < 0 || !(g->data->items[item].sides & (1u << g->side))) {
        return false;
    }
    for (int k = 0; k < packs; k++) {
        if (g->shop_bought[g->shop][item] >= stock) {
            return k > 0;
        }
        int price = game_price(g, item, false);
        if (price <= 0 || g->money < price) {
            return k > 0;
        }
        g->money -= price;
        g->shop_bought[g->shop][item]++;
        if (item == g->data->item_water) {
            continue;
        }
        g->inv[item] += g->data->items[item].pack;
    }
    return true;
}

bool game_store_sell(Game *g, int item, int packs)
{
    if (g->phase != PHASE_STORE || packs <= 0 || item < 0 || item >= g->data->item_count) {
        return false;
    }
    const ShopDef *s = &g->data->shops[g->shop];
    if (shop_stock_of(s, item) < 0) {
        return false;
    }
    int pack = g->data->items[item].pack;
    for (int k = 0; k < packs; k++) {
        if (g->inv[item] < pack) {
            return k > 0;
        }
        int price = game_price(g, item, true);
        g->inv[item] -= pack;
        g->money += price;
        if (g->shop_bought[g->shop][item] > 0) {
            g->shop_bought[g->shop][item]--;
        }
    }
    return true;
}

void game_store_leave(Game *g)
{
    if (g->phase != PHASE_STORE) {
        return;
    }
    g->shop = -1;
    LocText t;
    loctext_set(&t, "journal.traded");
    loctext_arg_key(&t, "place", g->data->nodes[g->node].name_key);
    game_journal(g, &t);
    g_settle(g);
}
