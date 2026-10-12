#include "data.h"

#include "calendar.h"
#include "util.h"

#include <cJSON.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- names */

static const char *SIDE_IDS[SIDE_COUNT] = {"dutch", "english"};
static const char *PACE_IDS[PACE_COUNT] = {"easy", "normal", "hard"};
static const char *RATION_IDS[RATIONS_COUNT] = {"full", "meagre", "starving"};
static const char *STANCE_IDS[NIGHT_COUNT] = {"rest", "laager", "guard"};
static const char *DIFF_IDS[DIFF_COUNT] = {"easy", "normal", "hard"};
static const char *WEATHER_IDS[WEATHER_COUNT] = {"clear", "hot", "windy", "cold", "frost", "rain", "storm"};
static const char *SKILL_IDS[SKILL_COUNT] = {"shooting", "herding", "voorloper", "interpreting",
                                             "nursing", "smithing", "tracking"};
static const char *ROLE_IDS[ROLE_COUNT] = {"head", "spouse", "child", "elder", "servant"};
static const char *SEASON_IDS[SEASON_COUNT] = {"summer", "autumn", "winter", "spring"};
static const char *AGE_IDS[AGE_COUNT] = {"child", "adult", "elder"};
static const char *CATEGORY_IDS[ITEMCAT_COUNT] = {"animals", "food", "water", "guns", "wagon", "trade", "medicine"};
static const char *WATER_IDS[WATER_COUNT] = {"streams", "springs", "dry"};
static const char *PART_IDS[WAGON_PART_COUNT] = {"wheels", "axle", "dissel", "tent"};

static int find_name(const char *const *names, int count, const char *s)
{
    if (!s) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], s) == 0) {
            return i;
        }
    }
    return -1;
}

const char *side_id(Side s) { return (s >= 0 && s < SIDE_COUNT) ? SIDE_IDS[s] : "?"; }
const char *pace_id(Pace p) { return (p >= 0 && p < PACE_COUNT) ? PACE_IDS[p] : "?"; }
const char *rations_id(Rations r) { return (r >= 0 && r < RATIONS_COUNT) ? RATION_IDS[r] : "?"; }
const char *stance_id(NightStance s) { return (s >= 0 && s < NIGHT_COUNT) ? STANCE_IDS[s] : "?"; }
const char *difficulty_id(Difficulty d) { return (d >= 0 && d < DIFF_COUNT) ? DIFF_IDS[d] : "?"; }
const char *weather_id(Weather w) { return (w >= 0 && w < WEATHER_COUNT) ? WEATHER_IDS[w] : "?"; }
const char *skill_id(Skill s) { return (s >= 0 && s < SKILL_COUNT) ? SKILL_IDS[s] : "?"; }
const char *role_id(Role r) { return (r >= 0 && r < ROLE_COUNT) ? ROLE_IDS[r] : "?"; }
const char *season_id(Season s) { return (s >= 0 && s < SEASON_COUNT) ? SEASON_IDS[s] : "?"; }
int side_from_id(const char *s) { return find_name(SIDE_IDS, SIDE_COUNT, s); }
int skill_from_id(const char *s) { return find_name(SKILL_IDS, SKILL_COUNT, s); }
int role_from_id(const char *s) { return find_name(ROLE_IDS, ROLE_COUNT, s); }

/* ---------------------------------------------------------------- loader context */

typedef struct {
    GameData *d;
    char *err;
    size_t err_len;
    int warnings;
    bool failed;
    const char *file;
} Loader;

static void fail(Loader *L, const char *fmt, ...)
{
    if (L->failed) {
        return;
    }
    L->failed = true;
    if (L->err && L->err_len) {
        char msg[400];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(msg, sizeof msg, fmt, ap);
        va_end(ap);
        snprintf(L->err, L->err_len, "%s: %s", L->file ? L->file : "data", msg);
    }
}

static void warn(Loader *L, const char *fmt, ...)
{
    char msg[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    fprintf(stderr, "warning: %s: %s\n", L->file ? L->file : "data", msg);
    L->warnings++;
}

static cJSON *load_json(Loader *L, const char *dir, const char *name)
{
    char *path = util_path_join(dir, name);
    char *text = util_read_file(path, NULL);
    L->file = name;
    cJSON *root = NULL;
    if (!text) {
        fail(L, "cannot read %s", path);
    } else {
        root = cJSON_Parse(text);
        if (!root) {
            const char *where = cJSON_GetErrorPtr();
            fail(L, "JSON syntax error near: %.60s", where ? where : "?");
        }
    }
    free(text);
    free(path);
    return root;
}

/* ---------------------------------------------------------------- json helpers */

static const char *jstr(const cJSON *o, const char *k)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

static double jnum(const cJSON *o, const char *k, double def)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}

static int jint(const cJSON *o, const char *k, int def)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsNumber(v) ? v->valueint : def;
}

static bool jbool(const cJSON *o, const char *k, bool def)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsBool(v) ? cJSON_IsTrue(v) : def;
}

static const cJSON *jget(const cJSON *o, const char *k)
{
    return cJSON_GetObjectItemCaseSensitive(o, k);
}

/* dst = prefix + id + suffix. Written out by hand because dst and id often live in
 * the same struct, which makes compilers warn about snprintf overlap. */
static void make_key(char *dst, size_t cap, const char *prefix, const char *id, const char *suffix)
{
    const char *parts[3] = {prefix, id, suffix};
    size_t n = 0;
    for (int p = 0; p < 3; p++) {
        for (const char *c = parts[p]; c && *c && n + 1 < cap; c++) {
            dst[n++] = *c;
        }
    }
    dst[n] = 0;
}

static void copy_str(char *dst, size_t cap, const char *src)
{
    if (!src) {
        src = "";
    }
    snprintf(dst, cap, "%s", src);
}

static void need_str(Loader *L, char *dst, size_t cap, const cJSON *o, const char *k, const char *ctx)
{
    const char *s = jstr(o, k);
    if (!s) {
        fail(L, "%s: missing string \"%s\"", ctx, k);
        dst[0] = '\0';
        return;
    }
    copy_str(dst, cap, s);
}

/* ---------------------------------------------------------------- lookups */

#define FIND_BY_ID(fn, arr, cnt)                       \
    int fn(const GameData *d, const char *id)          \
    {                                                  \
        if (!id) {                                     \
            return -1;                                 \
        }                                              \
        for (int i = 0; i < d->cnt; i++) {             \
            if (strcmp(d->arr[i].id, id) == 0) {       \
                return i;                              \
            }                                          \
        }                                              \
        return -1;                                     \
    }

FIND_BY_ID(data_find_item, items, item_count)
FIND_BY_ID(data_find_family, families, family_count)
FIND_BY_ID(data_find_node, nodes, node_count)
FIND_BY_ID(data_find_region, regions, region_count)
FIND_BY_ID(data_find_group, groups, group_count)
FIND_BY_ID(data_find_illness, illnesses, illness_count)
FIND_BY_ID(data_find_shop, shops, shop_count)
FIND_BY_ID(data_find_wagon, wagons, wagon_count)
FIND_BY_ID(data_find_event, events, event_count)
FIND_BY_ID(data_find_animal, animals, animal_count)

static int need_index(Loader *L, int idx, const char *what, const char *id, const char *ctx)
{
    if (idx < 0) {
        fail(L, "%s: unknown %s \"%s\"", ctx, what, id ? id : "(missing)");
    }
    return idx;
}

/* ---------------------------------------------------------------- rules */

static void load_rules(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "rules.json");
    if (!root) {
        return;
    }
    Rules *r = &L->d->rules;
    const cJSON *start = jget(root, "start");
    if (!cal_parse(jstr(start, "date"), &r->start_day)) {
        fail(L, "start.date must be YYYY-MM-DD");
    }
    r->start_node = need_index(L, data_find_node(L->d, jstr(start, "node")), "node", jstr(start, "node"), "start");

    const cJSON *pace = jget(root, "pace");
    for (int p = 0; p < PACE_COUNT; p++) {
        const cJSON *o = jget(pace, PACE_IDS[p]);
        r->miles[p] = jnum(o, "miles", 8);
        r->ox_cost[p] = jint(o, "ox_cost", 3);
        r->health_cost[p] = jint(o, "health_cost", 0);
    }
    const cJSON *rat = jget(root, "rations");
    for (int i = 0; i < RATIONS_COUNT; i++) {
        const cJSON *o = jget(rat, RATION_IDS[i]);
        r->food_lb[i] = jnum(o, "food_lb", 2.0);
        r->ration_health[i] = jint(o, "health", 0);
        r->ration_morale[i] = jint(o, "morale", 0);
    }
    r->child_food = jnum(root, "child_food", 0.6);
    r->water_gal_per_person = jnum(root, "water_gal_per_person", 1.0);
    const cJSON *ox = jget(root, "oxen");
    r->stamina_slow = jint(ox, "stamina_slow", 30);
    r->stamina_collapse = jint(ox, "stamina_collapse", 10);
    r->collapse_chance = jnum(ox, "collapse_chance", 0.1);
    r->rest_stamina = jint(ox, "rest_stamina", 10);
    r->no_water_ox_cost = jint(ox, "no_water_cost", 12);
    const cJSON *rest = jget(root, "rest");
    r->rest_health = jint(rest, "health", 3);
    r->rest_morale = jint(rest, "morale", 3);
    r->no_water_health = jint(root, "no_water_health", 4);
    const cJSON *laager = jget(root, "laager");
    r->laager_miles = jnum(laager, "miles", 1.0);
    r->laager_ox_cost = jint(laager, "ox_cost", 1);
    r->guard_morale = jnum(root, "guard_morale", -1);
    const cJSON *risk = jget(root, "night_risk");
    for (int s = 0; s < NIGHT_COUNT; s++) {
        r->night_risk_mul[s] = jnum(risk, STANCE_IDS[s], 1.0);
    }
    r->base_event_rate = jnum(root, "base_event_rate", 0.3);
    r->sabbath_morale = jint(root, "sabbath_morale", 3);
    r->wagon_trade_in = jnum(root, "wagon_trade_in", 0.8);
    r->breakage_per_mile = jnum(root, "breakage_per_mile", 0.001);
    const cJSON *mend = jget(root, "mend");
    r->mend_gain = jint(mend, "gain", 15);
    r->mend_gain_per_smith = jint(mend, "gain_per_smith", 8);
    r->mend_cap = jint(mend, "cap", 85);
    r->mend_iron_lb = jint(mend, "iron_lb", 3);
    r->mend_no_iron_mul = jnum(mend, "no_iron_mul", 0.5);
    r->mend_spare_below = jint(mend, "spare_below", 40);
    r->mend_tallow_lb = jint(mend, "tallow_lb", 2);
    r->mend_grease_gain = jint(mend, "grease_gain", 5);
    r->mend_tent_patch = jint(mend, "tent_patch", 15);
    r->mend_tent_cap = jint(mend, "tent_cap", 60);
    r->smith_pence_per_point = jnum(root, "smith_pence_per_point", 1.5);
    const cJSON *ill = jget(root, "illness_per_day");
    for (int s = 0; s < SEASON_COUNT; s++) {
        r->illness_per_day[s] = jnum(ill, SEASON_IDS[s], 0.002);
    }
    r->accident_per_day = jnum(root, "accident_per_day", 0.001);
    const cJSON *diff = jget(root, "difficulty");
    for (int i = 0; i < DIFF_COUNT; i++) {
        const cJSON *o = jget(diff, DIFF_IDS[i]);
        r->harm_mul[i] = jnum(o, "harm", 1.0);
        r->start_money_mul[i] = jnum(o, "start_money", 1.0);
    }
    r->warn_food_days = jnum(root, "warn_food_days", 7);
    r->warn_water_days = jnum(root, "warn_water_days", 2);
    const cJSON *wage = jget(root, "servant_wage");
    r->servant_wage = jnum(wage, "dutch", 10);
    r->servant_wage_english = jnum(wage, "english", 15);
    r->rds_per_pound = jnum(root, "rds_per_pound", 13.333);
    cJSON_Delete(root);
}

/* ---------------------------------------------------------------- items, wagons, groups */

static void load_items(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "items.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    const cJSON *it;
    cJSON_ArrayForEach(it, jget(root, "items"))
    {
        if (d->item_count >= MAX_ITEMS) {
            fail(L, "too many items");
            break;
        }
        ItemDef *item = &d->items[d->item_count];
        need_str(L, item->id, sizeof item->id, it, "id", "item");
        make_key(item->name_key, sizeof item->name_key, "item.", item->id, "");
        copy_str(item->pack_key, sizeof item->pack_key, jstr(it, "pack_key") ? jstr(it, "pack_key") : "unit.each");
        int cat = find_name(CATEGORY_IDS, ITEMCAT_COUNT, jstr(it, "category"));
        if (cat < 0) {
            fail(L, "item %s: unknown category", item->id);
            cat = 0;
        }
        item->category = (ItemCategory)cat;
        item->pack = jint(it, "pack", 1);
        if (item->pack < 1) {
            item->pack = 1;
        }
        item->weight_lb = jnum(it, "weight_lb", 0);
        item->price_pence = jint(it, "price_pence", 0);
        item->food = jnum(it, "food", 0);
        item->eat_order = jint(it, "eat_order", 5);
        item->morale = jint(it, "morale", 0);
        item->water_capacity = jint(it, "water_capacity", 0);
        item->sides = 0;
        const cJSON *sides = jget(it, "sides");
        if (cJSON_IsArray(sides)) {
            const cJSON *s;
            cJSON_ArrayForEach(s, sides)
            {
                int si = side_from_id(cJSON_IsString(s) ? s->valuestring : NULL);
                if (si >= 0) {
                    item->sides |= (uint8_t)(1u << si);
                }
            }
        } else {
            item->sides = (1u << SIDE_COUNT) - 1;
        }
        copy_str(item->source, sizeof item->source, jstr(it, "source"));
        if (!item->source[0] && item->price_pence > 0) {
            warn(L, "item %s has a price but no source", item->id);
        }
        d->item_count++;
    }
    cJSON_Delete(root);
}

static void load_wagons(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "wagons.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    const cJSON *w;
    cJSON_ArrayForEach(w, jget(root, "wagons"))
    {
        if (d->wagon_count >= MAX_WAGONS) {
            fail(L, "too many wagons");
            break;
        }
        WagonDef *wd = &d->wagons[d->wagon_count++];
        need_str(L, wd->id, sizeof wd->id, w, "id", "wagon");
        make_key(wd->name_key, sizeof wd->name_key, "wagon.", wd->id, "");
        wd->capacity_lb = jint(w, "capacity_lb", 3000);
        wd->min_oxen = jint(w, "min_oxen", 10);
        wd->full_span = jint(w, "full_span", 14);
        wd->speed = jnum(w, "speed", 1.0);
        wd->fragility = jnum(w, "fragility", 1.0);
        wd->price_pence = jint(w, "price_pence", 0);
    }
    cJSON_Delete(root);
}

static void load_groups(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "groups.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    const cJSON *g;
    cJSON_ArrayForEach(g, jget(root, "groups"))
    {
        if (d->group_count >= MAX_GROUPS) {
            fail(L, "too many groups");
            break;
        }
        GroupDef *gd = &d->groups[d->group_count++];
        need_str(L, gd->id, sizeof gd->id, g, "id", "group");
        make_key(gd->name_key, sizeof gd->name_key, "group.", gd->id, "");
        for (int s = 0; s < SIDE_COUNT; s++) {
            gd->start_relation[s] = jint(jget(g, "start"), SIDE_IDS[s], 0);
            gd->side_price_mul[s] = jnum(jget(g, "price_mul"), SIDE_IDS[s], 1.0);
        }
        gd->sell_spread = jnum(g, "sell_spread", 0.5);
    }
    cJSON_Delete(root);
}

static void load_illnesses(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "illness.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    const cJSON *o;
    cJSON_ArrayForEach(o, jget(root, "illnesses"))
    {
        if (d->illness_count >= MAX_ILLNESSES) {
            fail(L, "too many illnesses");
            break;
        }
        IllnessDef *il = &d->illnesses[d->illness_count++];
        need_str(L, il->id, sizeof il->id, o, "id", "illness");
        make_key(il->name_key, sizeof il->name_key, "illness.", il->id, "");
        make_key(il->cause_key, sizeof il->cause_key, "cause.", il->id, "");
        il->damage_per_day = jint(o, "damage", 4);
        il->recover_chance = jnum(o, "recover", 0.15);
        il->nursing_bonus = jnum(o, "nursing", 0.03);
        il->contagion = jnum(o, "contagion", 0);
        il->min_days = jint(o, "min_days", 2);
        il->weight = jnum(o, "weight", 1);
        il->child_mul = jnum(o, "child_mul", 1);
        il->accident = jbool(o, "accident", false);
    }
    cJSON_Delete(root);
}

static void load_regions(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "regions.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    const cJSON *o;
    cJSON_ArrayForEach(o, jget(root, "regions"))
    {
        if (d->region_count >= MAX_REGIONS) {
            fail(L, "too many regions");
            break;
        }
        RegionDef *r = &d->regions[d->region_count++];
        need_str(L, r->id, sizeof r->id, o, "id", "region");
        make_key(r->name_key, sizeof r->name_key, "region.", r->id, "");
        r->event_mul = jnum(o, "event_mul", 1.0);
        r->water_find = jnum(o, "water_find", 0.5);
        const cJSON *gr = jget(o, "grazing");
        for (int m = 0; m < 12; m++) {
            const cJSON *v = cJSON_GetArrayItem(gr, m);
            r->grazing[m] = cJSON_IsNumber(v) ? v->valueint : 6;
        }
        const cJSON *fever = jget(o, "fever"), *fly = jget(o, "fly");
        for (int m = 0; m < 12; m++) {
            const cJSON *v = cJSON_GetArrayItem(fever, m), *f = cJSON_GetArrayItem(fly, m);
            r->fever[m] = cJSON_IsNumber(v) ? v->valuedouble : 0;
            r->fly[m] = cJSON_IsNumber(f) ? f->valuedouble : 0;
        }
        const cJSON *we = jget(o, "weather");
        for (int s = 0; s < SEASON_COUNT; s++) {
            const cJSON *arr = jget(we, SEASON_IDS[s]);
            for (int w = 0; w < WEATHER_COUNT; w++) {
                const cJSON *v = cJSON_GetArrayItem(arr, w);
                r->weather[s][w] = cJSON_IsNumber(v) ? v->valuedouble : (w == 0 ? 1.0 : 0.0);
            }
        }
    }
    cJSON_Delete(root);
}

static void load_rivers(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "rivers.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    const cJSON *o;
    cJSON_ArrayForEach(o, jget(root, "rivers"))
    {
        if (d->river_count >= MAX_RIVERS) {
            fail(L, "too many rivers");
            break;
        }
        RiverDef *r = &d->rivers[d->river_count++];
        need_str(L, r->id, sizeof r->id, o, "id", "river");
        make_key(r->name_key, sizeof r->name_key, "river.", r->id, "");
        const cJSON *dep = jget(o, "depth");
        for (int m = 0; m < 12; m++) {
            const cJSON *v = cJSON_GetArrayItem(dep, m);
            r->depth_ft[m] = cJSON_IsNumber(v) ? v->valuedouble : 3.0;
        }
        r->spread_ft = jnum(o, "spread", 1.0);
        r->ford_max_ft = jnum(o, "ford_max", 3.5);
        r->float_min_ft = jnum(o, "float_min", 4.0);
    }
    cJSON_Delete(root);
}

/* ---------------------------------------------------------------- shops, route, families */

static void read_item_map(Loader *L, const cJSON *obj, ItemQty *out, int *count, int max, const char *ctx)
{
    const cJSON *e;
    cJSON_ArrayForEach(e, obj)
    {
        if (*count >= max) {
            fail(L, "%s: too many items", ctx);
            return;
        }
        int idx = need_index(L, data_find_item(L->d, e->string), "item", e->string, ctx);
        if (idx < 0) {
            return;
        }
        out[*count].item = idx;
        out[*count].qty = cJSON_IsNumber(e) ? e->valueint : 0;
        (*count)++;
    }
}

static void load_shops(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "shops.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    const cJSON *o;
    cJSON_ArrayForEach(o, jget(root, "shops"))
    {
        if (d->shop_count >= MAX_SHOPS) {
            fail(L, "too many shops");
            break;
        }
        ShopDef *s = &d->shops[d->shop_count++];
        need_str(L, s->id, sizeof s->id, o, "id", "shop");
        make_key(s->name_key, sizeof s->name_key, "shop.", s->id, "");
        need_str(L, s->keeper_key, sizeof s->keeper_key, o, "keeper", s->id);
        s->group = need_index(L, data_find_group(d, jstr(o, "group")), "group", jstr(o, "group"), s->id);
        read_item_map(L, jget(o, "stock"), s->stock, &s->stock_count, MAX_ITEMS, s->id);
    }
    cJSON_Delete(root);
}

static void load_route(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "route.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    static const char *KIND_IDS[] = {"town", "river", "mission", "landmark", "end"};
    const cJSON *o;
    cJSON_ArrayForEach(o, jget(root, "nodes"))
    {
        if (d->node_count >= MAX_NODES) {
            fail(L, "too many nodes");
            break;
        }
        NodeDef *n = &d->nodes[d->node_count++];
        need_str(L, n->id, sizeof n->id, o, "id", "node");
        make_key(n->name_key, sizeof n->name_key, "place.", n->id, "");
        make_key(n->desc_key, sizeof n->desc_key, "place.", n->id, ".desc");
        make_key(n->history_key, sizeof n->history_key, "history.place.", n->id, "");
        int kind = find_name(KIND_IDS, 5, jstr(o, "kind"));
        if (kind < 0) {
            fail(L, "node %s: unknown kind", n->id);
            kind = 0;
        }
        n->kind = (NodeKind)kind;
        n->lat = jnum(o, "lat", 0);
        n->lon = jnum(o, "lon", 0);
        n->shop = jstr(o, "shop") ? need_index(L, data_find_shop(d, jstr(o, "shop")), "shop", jstr(o, "shop"), n->id) : -1;
        n->river = -1;
        if (jstr(o, "river")) {
            for (int i = 0; i < d->river_count; i++) {
                if (strcmp(d->rivers[i].id, jstr(o, "river")) == 0) {
                    n->river = i;
                }
            }
            need_index(L, n->river, "river", jstr(o, "river"), n->id);
        }
        n->setpiece = -1; /* resolved after events load */
        n->price_mul = jnum(o, "price_mul", 1.0);
        const cJSON *ep;
        cJSON_ArrayForEach(ep, jget(o, "epilogue"))
        {
            if (n->epilogue_count < MAX_EPILOGUE && cJSON_IsString(ep)) {
                copy_str(n->epilogue[n->epilogue_count++], KEY_LEN, ep->valuestring);
            }
        }
    }
    const cJSON *terrain = jget(root, "terrain");
    cJSON_ArrayForEach(o, jget(root, "edges"))
    {
        if (d->edge_count >= MAX_EDGES) {
            fail(L, "too many edges");
            break;
        }
        EdgeDef *e = &d->edges[d->edge_count++];
        e->from = need_index(L, data_find_node(d, jstr(o, "from")), "node", jstr(o, "from"), "edge.from");
        e->to = need_index(L, data_find_node(d, jstr(o, "to")), "node", jstr(o, "to"), "edge.to");
        e->miles = jint(o, "miles", 10);
        e->region = need_index(L, data_find_region(d, jstr(o, "region")), "region", jstr(o, "region"), "edge");
        copy_str(e->terrain, sizeof e->terrain, jstr(o, "terrain"));
        const cJSON *t = jget(terrain, e->terrain);
        e->terrain_mul = jnum(t, "speed", 1.0);
        e->rough = jnum(t, "rough", 1.0);
        int w = find_name(WATER_IDS, WATER_COUNT, jstr(o, "water"));
        e->water = w < 0 ? WATER_STREAMS : (WaterKind)w;
        copy_str(e->label_key, sizeof e->label_key, jstr(o, "label"));
        copy_str(e->advice_key, sizeof e->advice_key, jstr(o, "advice"));
        copy_str(e->route_flag, sizeof e->route_flag, jstr(o, "route_flag"));
    }
    cJSON_Delete(root);
}

static void resolve_setpieces(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "route.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    const cJSON *o;
    int i = 0;
    cJSON_ArrayForEach(o, jget(root, "nodes"))
    {
        const char *sp = jstr(o, "setpiece");
        if (sp && i < d->node_count) {
            d->nodes[i].setpiece = need_index(L, data_find_event(d, sp), "event", sp, d->nodes[i].id);
        }
        const cJSON *act;
        cJSON_ArrayForEach(act, jget(o, "actions"))
        {
            NodeDef *n = &d->nodes[i];
            if (i >= d->node_count || n->action_count >= 3) {
                break;
            }
            const char *ev = jstr(act, "event");
            n->action_event[n->action_count] = need_index(L, data_find_event(d, ev), "event", ev, n->id);
            copy_str(n->action_label[n->action_count], KEY_LEN, jstr(act, "label"));
            n->action_count++;
        }
        i++;
    }
    cJSON_Delete(root);
}

static void map_project(const MapData *m, double lon, double lat, float *x, float *y)
{
    *x = (float)((lon - m->lon0) / (m->lon1 - m->lon0) * m->w);
    *y = (float)((lat - m->lat0) / (m->lat1 - m->lat0) * m->h);
}

static int map_points(const MapData *m, const cJSON *arr, float *xs, float *ys, int max)
{
    int n = 0;
    const cJSON *p;
    cJSON_ArrayForEach(p, arr)
    {
        const cJSON *a = cJSON_GetArrayItem(p, 0), *b = cJSON_GetArrayItem(p, 1);
        if (n < max && cJSON_IsNumber(a) && cJSON_IsNumber(b)) {
            map_project(m, a->valuedouble, b->valuedouble, &xs[n], &ys[n]);
            n++;
        }
    }
    return n;
}

static void load_map(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "map.json");
    if (!root) {
        return;
    }
    MapData *m = &L->d->map;
    const cJSON *size = jget(root, "size"), *lon = jget(root, "lon"), *lat = jget(root, "lat");
    m->w = cJSON_IsNumber(cJSON_GetArrayItem(size, 0)) ? (float)cJSON_GetArrayItem(size, 0)->valuedouble : 390;
    m->h = cJSON_IsNumber(cJSON_GetArrayItem(size, 1)) ? (float)cJSON_GetArrayItem(size, 1)->valuedouble : 280;
    m->lon0 = cJSON_IsNumber(cJSON_GetArrayItem(lon, 0)) ? cJSON_GetArrayItem(lon, 0)->valuedouble : 24;
    m->lon1 = cJSON_IsNumber(cJSON_GetArrayItem(lon, 1)) ? cJSON_GetArrayItem(lon, 1)->valuedouble : 29;
    m->lat0 = cJSON_IsNumber(cJSON_GetArrayItem(lat, 0)) ? cJSON_GetArrayItem(lat, 0)->valuedouble : -26;
    m->lat1 = cJSON_IsNumber(cJSON_GetArrayItem(lat, 1)) ? cJSON_GetArrayItem(lat, 1)->valuedouble : -34;
    if (m->lon1 == m->lon0 || m->lat1 == m->lat0) {
        fail(L, "map.json: lon and lat ranges must not be empty");
        cJSON_Delete(root);
        return;
    }
    static const char *SIDES[] = {"right", "left", "above", "below"};
    const cJSON *places = jget(root, "places");
    for (int i = 0; i < L->d->node_count; i++) {
        NodeDef *n = &L->d->nodes[i];
        map_project(m, n->lon, n->lat, &n->map_x, &n->map_y);
        const char *side = jstr(places, n->id);
        int s = side ? find_name(SIDES, 4, side) : 0;
        if (s < 0) {
            fail(L, "map.json: place %s: side must be right, left, above or below", n->id);
            s = 0;
        }
        n->map_label = s;
    }
    const cJSON *o;
    cJSON_ArrayForEach(o, jget(root, "rivers"))
    {
        if (m->river_count >= MAP_MAX_LINES) {
            fail(L, "map.json: too many rivers");
            break;
        }
        MapLine *r = &m->rivers[m->river_count++];
        need_str(L, r->key, sizeof r->key, o, "key", "map river");
        r->n = map_points(m, jget(o, "points"), r->x, r->y, MAP_MAX_POINTS);
        r->label_at = jint(o, "label_at", 0);
        if (r->label_at >= r->n) {
            r->label_at = 0;
        }
        if (r->label_at < 0) {
            r->label_at = -1; /* no name written */
        }
    }
    m->coast.n = map_points(m, jget(root, "coast"), m->coast.x, m->coast.y, MAP_MAX_POINTS);
    m->peak_count = map_points(m, jget(root, "mountains"), m->peak_x, m->peak_y, MAP_MAX_PEAKS);
    static const char *KINDS[] = {"people", "feature", "sea"};
    cJSON_ArrayForEach(o, jget(root, "labels"))
    {
        if (m->text_count >= MAP_MAX_LABELS) {
            fail(L, "map.json: too many labels");
            break;
        }
        MapText *t = &m->texts[m->text_count++];
        need_str(L, t->key, sizeof t->key, o, "key", "map label");
        int k = find_name(KINDS, 3, jstr(o, "kind"));
        t->kind = k < 0 ? MAP_TEXT_PEOPLE : (MapTextKind)k;
        const cJSON *at = jget(o, "at");
        const cJSON *a = cJSON_GetArrayItem(at, 0), *b = cJSON_GetArrayItem(at, 1);
        map_project(m, cJSON_IsNumber(a) ? a->valuedouble : 0, cJSON_IsNumber(b) ? b->valuedouble : 0, &t->x, &t->y);
    }
    cJSON_Delete(root);
}

static void load_families(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "families.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    static const char *PRONOUN_IDS[] = {"he", "she"};
    const cJSON *o;
    cJSON_ArrayForEach(o, jget(root, "families"))
    {
        if (d->family_count >= MAX_FAMILIES) {
            fail(L, "too many families");
            break;
        }
        FamilyDef *f = &d->families[d->family_count++];
        need_str(L, f->id, sizeof f->id, o, "id", "family");
        int side = side_from_id(jstr(o, "side"));
        if (side < 0) {
            fail(L, "family %s: unknown side", f->id);
            side = 0;
        }
        f->side = (Side)side;
        make_key(f->name_key, sizeof f->name_key, "family.", f->id, "");
        make_key(f->summary_key, sizeof f->summary_key, "family.", f->id, ".summary");
        make_key(f->story_key, sizeof f->story_key, "story.", f->id, "");
        copy_str(f->surname, sizeof f->surname, jstr(o, "surname"));
        f->money = jint(o, "money", 0);
        f->wagon = need_index(L, data_find_wagon(d, jstr(o, "wagon")), "wagon", jstr(o, "wagon"), f->id);
        const cJSON *perks = jget(o, "perks");
        f->repair_mul = jnum(perks, "repair", 1.0);
        f->hunt_mul = jnum(perks, "hunt", 1.0);
        f->morale_bonus = jint(perks, "morale", 0);
        f->barter_mul = jnum(perks, "barter", 1.0);
        f->illness_mul = jnum(perks, "illness", 1.0);
        const cJSON *m;
        cJSON_ArrayForEach(m, jget(o, "members"))
        {
            if (f->member_count >= MAX_MEMBERS) {
                fail(L, "family %s: too many members", f->id);
                break;
            }
            MemberDef *md = &f->members[f->member_count++];
            copy_str(md->name, sizeof md->name, jstr(m, "name"));
            int role = role_from_id(jstr(m, "role"));
            int age = find_name(AGE_IDS, AGE_COUNT, jstr(m, "age"));
            int pro = find_name(PRONOUN_IDS, 2, jstr(m, "pronoun"));
            if (role < 0 || age < 0 || pro < 0) {
                fail(L, "family %s member %s: bad role, age or pronoun", f->id, md->name);
            }
            md->role = role < 0 ? ROLE_CHILD : (Role)role;
            md->age = age < 0 ? AGE_ADULT : (AgeGroup)age;
            md->pronoun = pro < 0 ? PRONOUN_HE : (Pronoun)pro;
            md->birth_year = jint(m, "born", 1800);
            copy_str(md->portrait, sizeof md->portrait, jstr(m, "portrait"));
            const cJSON *sk;
            cJSON_ArrayForEach(sk, jget(m, "skills"))
            {
                int si = skill_from_id(sk->string);
                if (si < 0) {
                    fail(L, "family %s: unknown skill %s", f->id, sk->string);
                } else {
                    md->skills[si] = (uint8_t)(cJSON_IsNumber(sk) ? sk->valueint : 0);
                }
            }
        }
        read_item_map(L, jget(o, "items"), f->start_items, &f->start_item_count, MAX_START_ITEMS, f->id);
    }
    cJSON_Delete(root);
}

static void load_shocks(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "price_shocks.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    const cJSON *o;
    cJSON_ArrayForEach(o, jget(root, "shocks"))
    {
        if (d->shock_count >= MAX_SHOCKS) {
            fail(L, "too many price shocks");
            break;
        }
        PriceShock *s = &d->shocks[d->shock_count++];
        need_str(L, s->id, sizeof s->id, o, "id", "shock");
        if (!cal_parse(jstr(o, "from"), &s->from_day) || !cal_parse(jstr(o, "to"), &s->to_day)) {
            fail(L, "shock %s: bad dates", s->id);
        }
        const cJSON *n;
        cJSON_ArrayForEach(n, jget(o, "nodes"))
        {
            if (s->node_count < MAX_NODES) {
                s->nodes[s->node_count++] = need_index(L, data_find_node(d, cJSON_IsString(n) ? n->valuestring : NULL),
                                                       "node", cJSON_IsString(n) ? n->valuestring : NULL, s->id);
            }
        }
        s->item = jstr(o, "item") ? need_index(L, data_find_item(d, jstr(o, "item")), "item", jstr(o, "item"), s->id) : -1;
        int cat = find_name(CATEGORY_IDS, ITEMCAT_COUNT, jstr(o, "category"));
        s->category = cat < 0 ? ITEMCAT_COUNT : (ItemCategory)cat;
        s->mul_start = jnum(o, "mul_start", 1.0);
        s->mul_end = jnum(o, "mul_end", 1.0);
        copy_str(s->excuse_key, sizeof s->excuse_key, jstr(o, "excuse"));
        copy_str(s->history_key, sizeof s->history_key, jstr(o, "history"));
        if (!cJSON_IsArray(jget(o, "sources")) || cJSON_GetArraySize(jget(o, "sources")) == 0) {
            warn(L, "price shock %s has no sources", s->id);
        }
    }
    cJSON_Delete(root);
}

/* ---------------------------------------------------------------- animals */

static void load_animals(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "animals.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    HuntRules *h = &d->hunt;
    const cJSON *mis = jget(root, "misfire");
    for (int w = 0; w < WEATHER_COUNT; w++) {
        h->misfire[w] = jnum(mis, WEATHER_IDS[w], 0.05);
    }
    h->seconds = jnum(root, "seconds", 45);
    h->reload_seconds = jnum(root, "reload_seconds", 3.5);
    h->prime_seconds = jnum(root, "prime_seconds", 1.0);
    h->spread = jnum(root, "spread", 9);
    const cJSON *spawn = jget(root, "spawn_seconds");
    const cJSON *s0 = cJSON_GetArrayItem(spawn, 0), *s1 = cJSON_GetArrayItem(spawn, 1);
    h->spawn_min = cJSON_IsNumber(s0) ? s0->valuedouble : 2;
    h->spawn_max = cJSON_IsNumber(s1) ? s1->valuedouble : 6;
    h->max_animals = jint(root, "max_animals", 8);
    if (h->max_animals > 10) {
        h->max_animals = 10; /* HUNT_MAX_ANIMALS */
    }
    h->game_per_kill = jnum(root, "game_per_kill", 0.04);
    h->carry_on_foot = jint(root, "carry_on_foot", 120);
    h->carry_per_horse = jint(root, "carry_per_horse", 150);
    h->shots_per_lb = jint(root, "shots_per_lb", 12);
    h->biltong_yield = jnum(root, "biltong_yield", 0.45);
    h->percussion_misfire_mul = jnum(root, "percussion_misfire_mul", 1.0);
    h->defend_seconds = jnum(root, "defend_seconds", 40);
    static const char *TIERS[] = {"passive", "big", "predator"};
    const cJSON *o;
    cJSON_ArrayForEach(o, jget(root, "animals"))
    {
        if (d->animal_count >= MAX_ANIMALS) {
            fail(L, "too many animals");
            break;
        }
        AnimalDef *a = &d->animals[d->animal_count++];
        need_str(L, a->id, sizeof a->id, o, "id", "animal");
        make_key(a->name_key, sizeof a->name_key, "animal.", a->id, "");
        int tier = find_name(TIERS, 3, jstr(o, "tier"));
        a->tier = tier < 0 ? TIER_PASSIVE : (AnimalTier)tier;
        a->meat = jint(o, "meat", 50);
        a->spook = jnum(o, "spook", 0.3);
        a->charge = jnum(o, "charge", 0.0);
        a->speed = jnum(o, "speed", 100);
        a->hp = jint(o, "hp", 1);
        a->herd = jint(o, "herd", 1);
        const cJSON *body = jget(o, "body");
        const cJSON *b0 = cJSON_GetArrayItem(body, 0), *b1 = cJSON_GetArrayItem(body, 1), *b2 = cJSON_GetArrayItem(body, 2);
        a->body_w = cJSON_IsNumber(b0) ? b0->valuedouble : 18;
        a->body_h = cJSON_IsNumber(b1) ? b1->valuedouble : 9;
        a->legs = cJSON_IsNumber(b2) ? b2->valuedouble : 8;
        const cJSON *r;
        cJSON_ArrayForEach(r, jget(o, "regions"))
        {
            int ri = need_index(L, data_find_region(d, r->string), "region", r->string, a->id);
            if (ri >= 0 && cJSON_IsNumber(r)) {
                a->region_weight[ri] = r->valuedouble;
            }
        }
        const cJSON *raid = jget(o, "raid");
        if (raid) {
            const cJSON *count = jget(raid, "count");
            const cJSON *c0 = cJSON_GetArrayItem(count, 0), *c1 = cJSON_GetArrayItem(count, 1);
            a->raid_min = cJSON_IsNumber(c0) ? c0->valueint : 1;
            a->raid_max = cJSON_IsNumber(c1) ? c1->valueint : a->raid_min;
            if (a->raid_max > 12) {
                a->raid_max = 12;
            }
            const cJSON *t;
            cJSON_ArrayForEach(t, jget(raid, "takes"))
            {
                if (a->raid_take_count < 4 && cJSON_IsString(t)) {
                    a->raid_take[a->raid_take_count++] = need_index(L, data_find_item(d, t->valuestring), "item", t->valuestring, a->id);
                }
            }
        }
    }
    cJSON_Delete(root);
}

/* ---------------------------------------------------------------- barter */

static void load_barter(Loader *L, const char *dir)
{
    cJSON *root = load_json(L, dir, "barter.json");
    if (!root) {
        return;
    }
    GameData *d = L->d;
    const cJSON *o;
    cJSON_ArrayForEach(o, jget(root, "contraband"))
    {
        const char *id = cJSON_IsString(o) ? o->valuestring : NULL;
        int item = need_index(L, data_find_item(d, id), "item", id, "contraband");
        if (item >= 0 && d->contraband_count < MAX_ITEMS) {
            d->contraband[d->contraband_count++] = item;
        }
    }
    cJSON_ArrayForEach(o, jget(root, "partners"))
    {
        if (d->partner_count >= MAX_PARTNERS) {
            fail(L, "too many barter partners");
            break;
        }
        BarterPartner *p = &d->partners[d->partner_count++];
        need_str(L, p->id, sizeof p->id, o, "id", "partner");
        make_key(p->name_key, sizeof p->name_key, "barter.", p->id, "");
        p->group = need_index(L, data_find_group(d, jstr(o, "group")), "group", jstr(o, "group"), p->id);
        p->node = need_index(L, data_find_node(d, jstr(o, "node")), "node", jstr(o, "node"), p->id);
        p->fairness = jnum(o, "fairness", 1.0);
        const cJSON *w;
        cJSON_ArrayForEach(w, jget(o, "wants"))
        {
            if (p->want_count >= MAX_BARTER_GOODS) {
                break;
            }
            BarterGood *g = &p->wants[p->want_count];
            g->item = need_index(L, data_find_item(d, w->string), "item", w->string, p->id);
            g->value = cJSON_IsNumber(w) ? w->valuedouble : 0;
            p->want_count++;
        }
        cJSON_ArrayForEach(w, jget(o, "offers"))
        {
            if (p->offer_count >= MAX_BARTER_GOODS) {
                break;
            }
            BarterGood *g = &p->offers[p->offer_count];
            g->item = need_index(L, data_find_item(d, w->string), "item", w->string, p->id);
            const cJSON *qty = cJSON_GetArrayItem(w, 0), *val = cJSON_GetArrayItem(w, 1);
            g->stock = cJSON_IsNumber(qty) ? qty->valueint : 0;
            g->value = cJSON_IsNumber(val) ? val->valuedouble : 0;
            p->offer_count++;
        }
    }
    cJSON_Delete(root);
}

/* ---------------------------------------------------------------- events */

static bool parse_cond(Loader *L, const cJSON *o, Cond *c, const char *ctx);

static bool parse_cond_list(Loader *L, const cJSON *arr, Cond **out, int *count, const char *ctx)
{
    *out = NULL;
    *count = 0;
    if (!arr) {
        return true;
    }
    if (!cJSON_IsArray(arr)) {
        fail(L, "%s: conditions must be a list", ctx);
        return false;
    }
    int n = cJSON_GetArraySize(arr);
    if (n == 0) {
        return true;
    }
    *out = calloc((size_t)n, sizeof **out);
    const cJSON *e;
    cJSON_ArrayForEach(e, arr)
    {
        if (!parse_cond(L, e, &(*out)[*count], ctx)) {
            return false;
        }
        (*count)++;
    }
    return true;
}

static bool pair_arg(const cJSON *v, const char **name, int *num)
{
    if (!cJSON_IsArray(v) || cJSON_GetArraySize(v) != 2) {
        return false;
    }
    const cJSON *a = cJSON_GetArrayItem(v, 0), *b = cJSON_GetArrayItem(v, 1);
    if (!cJSON_IsString(a) || !cJSON_IsNumber(b)) {
        return false;
    }
    *name = a->valuestring;
    *num = b->valueint;
    return true;
}

static bool parse_cond(Loader *L, const cJSON *o, Cond *c, const char *ctx)
{
    GameData *d = L->d;
    const cJSON *v = o ? o->child : NULL;
    if (!cJSON_IsObject(o) || !v || v->next) {
        fail(L, "%s: each condition must be an object with exactly one key", ctx);
        return false;
    }
    const char *k = v->string;
    const char *s = cJSON_IsString(v) ? v->valuestring : NULL;
    const char *pname = NULL;
    int pnum = 0;
    memset(c, 0, sizeof *c);
    if (strcmp(k, "side") == 0) {
        c->type = COND_SIDE;
        c->a = side_from_id(s);
        if (c->a < 0) { fail(L, "%s: unknown side %s", ctx, s ? s : "?"); }
    } else if (strcmp(k, "family") == 0) {
        c->type = COND_FAMILY;
        c->a = need_index(L, data_find_family(d, s), "family", s, ctx);
    } else if (strcmp(k, "region") == 0) {
        c->type = COND_REGION;
        c->a = need_index(L, data_find_region(d, s), "region", s, ctx);
    } else if (strcmp(k, "node") == 0) {
        c->type = COND_NODE;
        c->a = need_index(L, data_find_node(d, s), "node", s, ctx);
    } else if (strcmp(k, "edge_water") == 0) {
        c->type = COND_EDGE_WATER;
        c->a = find_name(WATER_IDS, WATER_COUNT, s);
        if (c->a < 0) { fail(L, "%s: unknown water kind", ctx); }
    } else if (strcmp(k, "season") == 0) {
        c->type = COND_SEASON;
        c->a = find_name(SEASON_IDS, SEASON_COUNT, s);
        if (c->a < 0) { fail(L, "%s: unknown season", ctx); }
    } else if (strcmp(k, "weather") == 0) {
        c->type = COND_WEATHER;
        c->a = find_name(WEATHER_IDS, WEATHER_COUNT, s);
        if (c->a < 0) { fail(L, "%s: unknown weather", ctx); }
    } else if (strcmp(k, "flag") == 0 || strcmp(k, "not_flag") == 0) {
        c->type = k[0] == 'f' ? COND_FLAG : COND_NOT_FLAG;
        if (!s) { fail(L, "%s: flag needs a name", ctx); }
        copy_str(c->str, sizeof c->str, s);
    } else if (strcmp(k, "counter_gte") == 0) {
        c->type = COND_COUNTER_GTE;
        if (!pair_arg(v, &pname, &pnum)) { fail(L, "%s: counter_gte needs [name, n]", ctx); return false; }
        copy_str(c->str, sizeof c->str, pname);
        c->b = pnum;
    } else if (strcmp(k, "item_gte") == 0 || strcmp(k, "item_lt") == 0) {
        c->type = k[5] == 'g' ? COND_ITEM_GTE : COND_ITEM_LT;
        if (!pair_arg(v, &pname, &pnum)) { fail(L, "%s: %s needs [item, n]", ctx, k); return false; }
        c->a = need_index(L, data_find_item(d, pname), "item", pname, ctx);
        c->b = pnum;
    } else if (strcmp(k, "money_gte") == 0) {
        c->type = COND_MONEY_GTE;
        c->b = v->valueint;
    } else if (strcmp(k, "member_role") == 0 || strcmp(k, "member_role_healthy") == 0) {
        c->type = strlen(k) > 11 ? COND_MEMBER_ROLE_HEALTHY : COND_MEMBER_ROLE;
        c->a = role_from_id(s);
        if (c->a < 0) { fail(L, "%s: unknown role", ctx); }
    } else if (strcmp(k, "member_skill_gte") == 0) {
        c->type = COND_MEMBER_SKILL_GTE;
        if (!pair_arg(v, &pname, &pnum)) { fail(L, "%s: member_skill_gte needs [skill, n]", ctx); return false; }
        c->a = skill_from_id(pname);
        if (c->a < 0) { fail(L, "%s: unknown skill %s", ctx, pname); }
        c->b = pnum;
    } else if (strcmp(k, "relation_gte") == 0 || strcmp(k, "relation_lt") == 0) {
        c->type = k[9] == 'g' ? COND_RELATION_GTE : COND_RELATION_LT;
        if (!pair_arg(v, &pname, &pnum)) { fail(L, "%s: %s needs [group, n]", ctx, k); return false; }
        c->a = need_index(L, data_find_group(d, pname), "group", pname, ctx);
        c->b = pnum;
    } else if (strcmp(k, "stance") == 0) {
        c->type = COND_STANCE;
        c->a = find_name(STANCE_IDS, NIGHT_COUNT, s);
        if (c->a < 0) { fail(L, "%s: unknown stance", ctx); }
    } else if (strcmp(k, "date_gte") == 0 || strcmp(k, "date_lt") == 0) {
        c->type = k[5] == 'g' ? COND_DATE_GTE : COND_DATE_LT;
        if (!cal_parse(s, &c->b)) { fail(L, "%s: bad date", ctx); }
    } else if (strcmp(k, "month") == 0) {
        c->type = COND_MONTH;
        c->b = v->valueint;
    } else if (strcmp(k, "weekday") == 0) {
        c->type = COND_WEEKDAY; /* 0 = Sunday */
        c->b = v->valueint;
        if (c->b < 0 || c->b > 6) { fail(L, "%s: weekday must be 0 (Sunday) to 6", ctx); }
    } else if (strcmp(k, "sabbath") == 0) {
        c->type = COND_SABBATH; /* true: the party keeps Sunday rest */
        if (!cJSON_IsBool(v)) { fail(L, "%s: sabbath needs true or false", ctx); }
        c->b = cJSON_IsTrue(v);
    } else if (strcmp(k, "wagon_worn") == 0 || strcmp(k, "afford_repair") == 0) {
        c->type = k[0] == 'w' ? COND_WAGON_WORN : COND_AFFORD_REPAIR; /* true: some part needs work / money for the smith */
        if (!cJSON_IsBool(v)) { fail(L, "%s: %s needs true or false", ctx, k); }
        c->b = cJSON_IsTrue(v);
    } else if (strcmp(k, "wagon") == 0 || strcmp(k, "afford_wagon") == 0) {
        c->type = k[0] == 'w' ? COND_WAGON : COND_AFFORD_WAGON;
        c->a = need_index(L, data_find_wagon(d, s), "wagon", s, ctx);
    } else if (strcmp(k, "days_gte") == 0) {
        c->type = COND_DAYS_GTE;
        c->b = v->valueint;
    } else if (strcmp(k, "distance_gte") == 0) {
        c->type = COND_DISTANCE_GTE;
        c->b = v->valueint;
    } else if (strcmp(k, "chance") == 0) {
        c->type = COND_CHANCE;
        c->x = v->valuedouble;
    } else if (strcmp(k, "any") == 0) {
        c->type = COND_ANY;
        return parse_cond_list(L, v, &c->kids, &c->kid_count, ctx);
    } else if (strcmp(k, "not") == 0) {
        c->type = COND_NOT;
        c->kids = calloc(1, sizeof *c->kids);
        c->kid_count = 1;
        return parse_cond(L, v, c->kids, ctx);
    } else {
        fail(L, "%s: unknown condition \"%s\"", ctx, k);
        return false;
    }
    return !L->failed;
}

static void parse_who(Loader *L, const cJSON *o, Effect *e, const char *ctx)
{
    const char *who = jstr(o, "who");
    e->who = WHO_RANDOM;
    e->count = jint(o, "count", 1);
    if (!who || strcmp(who, "random") == 0) {
        e->who = WHO_RANDOM;
    } else if (strcmp(who, "all") == 0) {
        e->who = WHO_ALL;
    } else if (strcmp(who, "speaker") == 0) {
        e->who = WHO_SPEAKER;
    } else if (strcmp(who, "head") == 0) {
        e->who = WHO_HEAD;
    } else if (strcmp(who, "child") == 0) {
        e->who = WHO_CHILD;
    } else if (strncmp(who, "role:", 5) == 0) {
        e->who = WHO_ROLE;
        int r = role_from_id(who + 5);
        if (r < 0) {
            fail(L, "%s: unknown role in who", ctx);
        }
        e->role = r < 0 ? ROLE_CHILD : (Role)r;
    } else {
        fail(L, "%s: unknown who \"%s\"", ctx, who);
    }
}

static bool parse_effect(Loader *L, const cJSON *o, Effect *e, const char *ctx)
{
    GameData *d = L->d;
    memset(e, 0, sizeof *e);
    if (!cJSON_IsObject(o) || !o->child) {
        fail(L, "%s: effect must be an object", ctx);
        return false;
    }
    const char *k = o->child->string;
    const cJSON *v = o->child;
    const char *s = cJSON_IsString(v) ? v->valuestring : NULL;
    if (strcmp(k, "item") == 0) {
        e->type = EFF_ITEM;
        e->a = need_index(L, data_find_item(d, s), "item", s, ctx);
        e->amount = jint(o, "add", 0);
    } else if (strcmp(k, "money_pence") == 0) {
        e->type = EFF_MONEY;
        e->amount = v->valueint;
    } else if (strcmp(k, "ox_stamina") == 0) {
        e->type = EFF_OX_STAMINA;
        e->amount = v->valueint;
    } else if (strcmp(k, "wagon_damage") == 0) {
        e->type = EFF_WAGON_DAMAGE;
        e->a = find_name(PART_IDS, WAGON_PART_COUNT, s);
        if (e->a < 0) { fail(L, "%s: unknown wagon part", ctx); }
        e->amount = jint(o, "amount", 30);
    } else if (strcmp(k, "health") == 0) {
        e->type = EFF_HEALTH;
        e->amount = v->valueint;
        parse_who(L, o, e, ctx);
    } else if (strcmp(k, "illness") == 0) {
        e->type = EFF_ILLNESS;
        e->a = need_index(L, data_find_illness(d, s), "illness", s, ctx);
        parse_who(L, o, e, ctx);
    } else if (strcmp(k, "morale") == 0) {
        e->type = EFF_MORALE;
        e->amount = v->valueint;
    } else if (strcmp(k, "loyalty") == 0) {
        e->type = EFF_LOYALTY;
        e->amount = v->valueint;
        parse_who(L, o, e, ctx);
    } else if (strcmp(k, "relation") == 0) {
        e->type = EFF_RELATION;
        e->a = need_index(L, data_find_group(d, s), "group", s, ctx);
        e->amount = jint(o, "add", 0);
    } else if (strcmp(k, "flag") == 0 || strcmp(k, "clear_flag") == 0) {
        e->type = k[0] == 'f' ? EFF_FLAG : EFF_CLEAR_FLAG;
        copy_str(e->str, sizeof e->str, s);
    } else if (strcmp(k, "counter") == 0) {
        e->type = EFF_COUNTER;
        copy_str(e->str, sizeof e->str, s);
        e->amount = jint(o, "add", 1);
    } else if (strcmp(k, "time_days") == 0) {
        e->type = EFF_TIME_DAYS;
        e->famount = v->valuedouble;
    } else if (strcmp(k, "distance") == 0) {
        e->type = EFF_DISTANCE;
        e->amount = v->valueint;
    } else if (strcmp(k, "schedule") == 0) {
        e->type = EFF_SCHEDULE;
        copy_str(e->str, sizeof e->str, s); /* resolved after all events load */
        e->amount = jint(o, "in_days", 1);
    } else if (strcmp(k, "death") == 0) {
        e->type = EFF_DEATH;
        parse_who(L, o, e, ctx);
    } else if (strcmp(k, "leave") == 0) {
        e->type = EFF_LEAVE;
        parse_who(L, o, e, ctx);
    } else if (strcmp(k, "journal") == 0) {
        e->type = EFF_JOURNAL;
        copy_str(e->str, sizeof e->str, s);
    } else if (strcmp(k, "smith") == 0) {
        /* "repair": a town smith puts every part right, for money; "quote": he says what it would cost */
        if (strcmp(s, "repair") == 0) {
            e->type = EFF_SMITH_REPAIR;
        } else if (strcmp(s, "quote") == 0) {
            e->type = EFF_SMITH_QUOTE;
        } else {
            fail(L, "%s: smith must be repair or quote", ctx);
        }
    } else if (strcmp(k, "defend") == 0) {
        e->type = EFF_DEFEND; /* the player guards the herd against this animal (game_hunt.c) */
        e->a = need_index(L, data_find_animal(d, s), "animal", s, ctx);
        if (e->a >= 0 && d->animals[e->a].raid_max <= 0) {
            fail(L, "%s: animal \"%s\" has no raid in animals.json", ctx, s);
        }
    } else if (strcmp(k, "wait_until") == 0) {
        /* "MM-DD": the party stays where it is until that date comes round */
        e->type = EFF_WAIT_UNTIL;
        if (!s || sscanf(s, "%d-%d", &e->a, &e->amount) != 2 || e->a < 1 || e->a > 12 || e->amount < 1 || e->amount > 31) {
            fail(L, "%s: wait_until must be \"MM-DD\"", ctx);
        }
    } else if (strcmp(k, "wagon") == 0) {
        e->type = EFF_WAGON; /* trade the wagon in for this one; its parts are new */
        e->a = need_index(L, data_find_wagon(d, s), "wagon", s, ctx);
    } else if (strcmp(k, "ending") == 0) {
        static const char *ENDINGS[] = {"none", "arrived", "settled", "turned_back", "stranded", "lost"};
        e->type = EFF_ENDING;
        e->amount = find_name(ENDINGS, 6, s);
        if (e->amount <= 0) {
            fail(L, "%s: unknown ending \"%s\"", ctx, s ? s : "?");
        }
    } else {
        fail(L, "%s: unknown effect \"%s\"", ctx, k);
        return false;
    }
    return !L->failed;
}

static void parse_choice(Loader *L, const cJSON *o, Choice *ch, const cJSON *node_ids, const char *ctx)
{
    need_str(L, ch->text_key, sizeof ch->text_key, o, "text", ctx);
    copy_str(ch->locked_key, sizeof ch->locked_key, jstr(o, "locked"));
    parse_cond_list(L, jget(o, "requires"), &ch->requires, &ch->require_count, ctx);
    const cJSON *outs = jget(o, "outcomes");
    int n = cJSON_IsArray(outs) ? cJSON_GetArraySize(outs) : 0;
    ch->outcomes = calloc((size_t)(n ? n : 1), sizeof *ch->outcomes);
    if (n == 0) {
        /* A choice with no outcomes simply ends the event. */
        ch->outcomes[0].weight = 1;
        ch->outcomes[0].skill = -1;
        ch->outcomes[0].next = -1;
        ch->outcome_count = 1;
        return;
    }
    const cJSON *oc;
    cJSON_ArrayForEach(oc, outs)
    {
        Outcome *out = &ch->outcomes[ch->outcome_count++];
        out->weight = jnum(oc, "weight", 1);
        out->skill = jstr(oc, "skill") ? skill_from_id(jstr(oc, "skill")) : -1;
        out->per_point = jnum(oc, "per_point", 0);
        copy_str(out->text_key, sizeof out->text_key, jstr(oc, "text"));
        out->next = -1;
        const char *next = jstr(oc, "next");
        if (next) {
            int i = 0;
            const cJSON *nid;
            cJSON_ArrayForEach(nid, node_ids)
            {
                if (cJSON_IsString(nid) && strcmp(nid->valuestring, next) == 0) {
                    out->next = i;
                }
                i++;
            }
            if (out->next < 0) {
                fail(L, "%s: outcome goes to unknown node \"%s\"", ctx, next);
            }
        }
        const cJSON *effs = jget(oc, "effects");
        int ne = cJSON_IsArray(effs) ? cJSON_GetArraySize(effs) : 0;
        if (ne > 0) {
            out->effects = calloc((size_t)ne, sizeof *out->effects);
            const cJSON *ef;
            cJSON_ArrayForEach(ef, effs)
            {
                if (!parse_effect(L, ef, &out->effects[out->effect_count], ctx)) {
                    return;
                }
                out->effect_count++;
            }
        }
    }
}

static void parse_node(Loader *L, const cJSON *o, EventNode *n, const cJSON *node_ids, const char *ctx)
{
    copy_str(n->id, sizeof n->id, jstr(o, "id") ? jstr(o, "id") : "start");
    copy_str(n->speaker_key, sizeof n->speaker_key, jstr(o, "speaker_key"));
    need_str(L, n->text_key, sizeof n->text_key, o, "text", ctx);
    copy_str(n->image, sizeof n->image, jstr(o, "image"));
    copy_str(n->portrait, sizeof n->portrait, jstr(o, "portrait"));
    const cJSON *ch;
    cJSON_ArrayForEach(ch, jget(o, "choices"))
    {
        if (n->choice_count >= MAX_EVENT_CHOICES) {
            fail(L, "%s: at most %d choices per node", ctx, MAX_EVENT_CHOICES);
            return;
        }
        parse_choice(L, ch, &n->choices[n->choice_count++], node_ids, ctx);
    }
    if (n->choice_count == 0) {
        fail(L, "%s: node \"%s\" has no choices", ctx, n->id);
    }
}

static void parse_event(Loader *L, const cJSON *o, EventDef *ev)
{
    GameData *d = L->d;
    need_str(L, ev->id, sizeof ev->id, o, "id", "event");
    const char *ctx = ev->id;
    ev->sides = 0;
    const cJSON *a;
    const cJSON *sides = jget(o, "sides");
    if (cJSON_IsArray(sides)) {
        cJSON_ArrayForEach(a, sides)
        {
            int si = side_from_id(cJSON_IsString(a) ? a->valuestring : NULL);
            if (si < 0) {
                fail(L, "%s: unknown side", ctx);
            } else {
                ev->sides |= (uint8_t)(1u << si);
            }
        }
    } else {
        ev->sides = (1u << SIDE_COUNT) - 1;
    }
    cJSON_ArrayForEach(a, jget(o, "regions"))
    {
        int r = need_index(L, data_find_region(d, cJSON_IsString(a) ? a->valuestring : NULL), "region",
                           cJSON_IsString(a) ? a->valuestring : NULL, ctx);
        if (r >= 0) {
            ev->regions |= (uint64_t)1 << r;
        }
    }
    cJSON_ArrayForEach(a, jget(o, "months"))
    {
        if (cJSON_IsNumber(a) && a->valueint >= 1 && a->valueint <= 12) {
            ev->months |= (uint16_t)(1u << (a->valueint - 1));
        }
    }
    const char *time = jstr(o, "time");
    ev->night = time && strcmp(time, "night") == 0;
    ev->setpiece = jbool(o, "setpiece", false);
    ev->weight = jnum(o, "weight", 10);
    ev->once = jbool(o, "once", ev->setpiece);
    ev->cooldown_days = jint(o, "cooldown_days", 20);
    ev->harmful = jbool(o, "harmful", false);
    copy_str(ev->history_key, sizeof ev->history_key, jstr(o, "history"));
    parse_cond_list(L, jget(o, "requires"), &ev->requires, &ev->require_count, ctx);

    const cJSON *mods = jget(o, "weight_mods");
    int nm = cJSON_IsArray(mods) ? cJSON_GetArraySize(mods) : 0;
    if (nm > 0) {
        ev->mod_conds = calloc((size_t)nm, sizeof *ev->mod_conds);
        ev->mod_muls = calloc((size_t)nm, sizeof *ev->mod_muls);
        cJSON_ArrayForEach(a, mods)
        {
            if (!parse_cond(L, jget(a, "if"), &ev->mod_conds[ev->mod_count], ctx)) {
                return;
            }
            ev->mod_muls[ev->mod_count++] = jnum(a, "mul", 1.0);
        }
    }

    const cJSON *sp = jget(o, "speaker");
    ev->speaker_pick = SPEAKER_NONE;
    if (sp) {
        const char *pick = jstr(sp, "pick");
        if (pick && strcmp(pick, "role") == 0) {
            ev->speaker_pick = SPEAKER_ROLE;
            ev->speaker_arg = role_from_id(jstr(sp, "role"));
            if (ev->speaker_arg < 0) { fail(L, "%s: unknown speaker role", ctx); }
        } else if (pick && strcmp(pick, "skill") == 0) {
            ev->speaker_pick = SPEAKER_SKILL;
            ev->speaker_arg = skill_from_id(jstr(sp, "skill"));
            if (ev->speaker_arg < 0) { fail(L, "%s: unknown speaker skill", ctx); }
        } else if (pick && strcmp(pick, "random") == 0) {
            ev->speaker_pick = SPEAKER_RANDOM;
        } else {
            fail(L, "%s: speaker.pick must be role, skill or random", ctx);
        }
    }

    const cJSON *srcs = jget(o, "sources");
    if (!cJSON_IsArray(srcs) || cJSON_GetArraySize(srcs) == 0) {
        fail(L, "%s: every event needs a non-empty \"sources\" list (or [\"invented\"])", ctx);
        return;
    }

    /* Either a "nodes" list, or a single node written inline. */
    const cJSON *nodes = jget(o, "nodes");
    cJSON *ids = cJSON_CreateArray();
    if (cJSON_IsArray(nodes)) {
        cJSON_ArrayForEach(a, nodes)
        {
            cJSON_AddItemToArray(ids, cJSON_CreateString(jstr(a, "id") ? jstr(a, "id") : ""));
        }
        ev->node_count = cJSON_GetArraySize(nodes);
        ev->nodes = calloc((size_t)(ev->node_count ? ev->node_count : 1), sizeof *ev->nodes);
        int i = 0;
        cJSON_ArrayForEach(a, nodes)
        {
            parse_node(L, a, &ev->nodes[i++], ids, ctx);
        }
        if (ev->node_count == 0) {
            fail(L, "%s: empty nodes list", ctx);
        }
    } else {
        cJSON_AddItemToArray(ids, cJSON_CreateString("start"));
        ev->node_count = 1;
        ev->nodes = calloc(1, sizeof *ev->nodes);
        parse_node(L, o, &ev->nodes[0], ids, ctx);
    }
    cJSON_Delete(ids);
}

static void resolve_schedules(Loader *L)
{
    GameData *d = L->d;
    for (int i = 0; i < d->event_count; i++) {
        EventDef *ev = &d->events[i];
        for (int n = 0; n < ev->node_count; n++) {
            for (int c = 0; c < ev->nodes[n].choice_count; c++) {
                Choice *ch = &ev->nodes[n].choices[c];
                for (int o = 0; o < ch->outcome_count; o++) {
                    for (int e = 0; e < ch->outcomes[o].effect_count; e++) {
                        Effect *ef = &ch->outcomes[o].effects[e];
                        if (ef->type == EFF_SCHEDULE) {
                            ef->a = need_index(L, data_find_event(d, ef->str), "event", ef->str, ev->id);
                        }
                    }
                }
            }
        }
    }
}

static void load_events(Loader *L, const char *dir)
{
    cJSON *index = load_json(L, dir, "events/index.json");
    if (!index) {
        return;
    }
    const cJSON *f;
    cJSON_ArrayForEach(f, jget(index, "files"))
    {
        if (!cJSON_IsString(f)) {
            continue;
        }
        char name[128];
        snprintf(name, sizeof name, "events/%s", f->valuestring);
        cJSON *root = load_json(L, dir, name);
        if (!root) {
            break;
        }
        const cJSON *evs = jget(root, "events");
        int n = cJSON_GetArraySize(evs);
        EventDef *grown = realloc(L->d->events, (size_t)(L->d->event_count + n) * sizeof *grown);
        if (!grown) {
            fail(L, "out of memory");
            cJSON_Delete(root);
            break;
        }
        L->d->events = grown;
        memset(&L->d->events[L->d->event_count], 0, (size_t)n * sizeof *grown);
        const cJSON *e;
        cJSON_ArrayForEach(e, evs)
        {
            EventDef *ev = &L->d->events[L->d->event_count++];
            parse_event(L, e, ev);
            if (data_find_event(L->d, ev->id) != L->d->event_count - 1) {
                fail(L, "duplicate event id \"%s\"", ev->id);
            }
            if (L->failed) {
                break;
            }
        }
        cJSON_Delete(root);
        if (L->failed) {
            break;
        }
    }
    cJSON_Delete(index);
    if (!L->failed) {
        L->file = "events";
        resolve_schedules(L);
    }
}

/* ---------------------------------------------------------------- top level */

static void resolve_known(Loader *L)
{
    L->d->illness_fever = data_find_illness(L->d, "malaria");
    GameData *d = L->d;
    L->file = "items.json";
    d->item_oxen = need_index(L, data_find_item(d, "oxen"), "item", "oxen", "required items");
    d->item_water = need_index(L, data_find_item(d, "water"), "item", "water", "required items");
    d->item_vat = need_index(L, data_find_item(d, "water_vat"), "item", "water_vat", "required items");
    d->item_meat = need_index(L, data_find_item(d, "meat"), "item", "meat", "required items");
    d->item_medicine = need_index(L, data_find_item(d, "medicine"), "item", "medicine", "required items");
    d->item_sheep = need_index(L, data_find_item(d, "sheep"), "item", "sheep", "required items");
    d->item_tallow = need_index(L, data_find_item(d, "tallow"), "item", "tallow", "required items");
    d->item_iron = need_index(L, data_find_item(d, "iron"), "item", "iron", "required items");
    d->item_cows = need_index(L, data_find_item(d, "cows"), "item", "cows", "required items");
    d->item_biltong = need_index(L, data_find_item(d, "biltong"), "item", "biltong", "required items");
    d->item_gunpowder = need_index(L, data_find_item(d, "gunpowder"), "item", "gunpowder", "required items");
    d->item_lead = need_index(L, data_find_item(d, "lead"), "item", "lead", "required items");
    d->item_horses = need_index(L, data_find_item(d, "horses"), "item", "horses", "required items");
    d->item_percussion = need_index(L, data_find_item(d, "percussion_gun"), "item", "percussion_gun", "required items");
    d->item_caps = need_index(L, data_find_item(d, "caps"), "item", "caps", "required items");
    static const char *SPARES[WAGON_PART_COUNT] = {"spare_wheel", "spare_axle", "spare_dissel", "canvas"};
    for (int p = 0; p < WAGON_PART_COUNT; p++) {
        d->item_spare[p] = need_index(L, data_find_item(d, SPARES[p]), "item", SPARES[p], "required items");
    }
    d->group_trekkers = need_index(L, data_find_group(d, "trekkers"), "group", "trekkers", "required groups");
}

GameData *data_load(const char *data_dir, char *err, size_t err_len, int *warnings)
{
    GameData *d = calloc(1, sizeof *d);
    if (!d) {
        return NULL;
    }
    Loader L = {d, err, err_len, 0, false, NULL};
    if (err && err_len) {
        err[0] = '\0';
    }
    /* Order matters: later files refer to ids from earlier ones. */
    load_items(&L, data_dir);
    if (!L.failed) load_wagons(&L, data_dir);
    if (!L.failed) load_groups(&L, data_dir);
    if (!L.failed) load_illnesses(&L, data_dir);
    if (!L.failed) load_regions(&L, data_dir);
    if (!L.failed) load_rivers(&L, data_dir);
    if (!L.failed) load_shops(&L, data_dir);
    if (!L.failed) load_route(&L, data_dir);
    if (!L.failed) load_map(&L, data_dir);
    if (!L.failed) load_families(&L, data_dir);
    if (!L.failed) load_rules(&L, data_dir);
    if (!L.failed) load_shocks(&L, data_dir);
    if (!L.failed) load_animals(&L, data_dir);
    if (!L.failed) load_barter(&L, data_dir);
    if (!L.failed) load_events(&L, data_dir);
    if (!L.failed) resolve_setpieces(&L, data_dir);
    if (!L.failed) resolve_known(&L);
    if (warnings) {
        *warnings = L.warnings;
    }
    if (L.failed) {
        data_free(d);
        return NULL;
    }
    return d;
}

static void free_conds(Cond *c, int n)
{
    for (int i = 0; i < n; i++) {
        free_conds(c[i].kids, c[i].kid_count);
    }
    free(c);
}

void data_free(GameData *d)
{
    if (!d) {
        return;
    }
    for (int i = 0; i < d->event_count; i++) {
        EventDef *ev = &d->events[i];
        free_conds(ev->requires, ev->require_count);
        free_conds(ev->mod_conds, ev->mod_count);
        free(ev->mod_muls);
        for (int n = 0; n < ev->node_count; n++) {
            for (int c = 0; c < ev->nodes[n].choice_count; c++) {
                Choice *ch = &ev->nodes[n].choices[c];
                free_conds(ch->requires, ch->require_count);
                for (int o = 0; o < ch->outcome_count; o++) {
                    free(ch->outcomes[o].effects);
                }
                free(ch->outcomes);
            }
        }
        free(ev->nodes);
    }
    free(d->events);
    free(d);
}

void data_each_key(const GameData *d, void (*fn)(const char *key, const char *where, void *ctx), void *ctx)
{
    char buf[KEY_LEN + 16];
    for (int i = 0; i < d->item_count; i++) {
        fn(d->items[i].name_key, d->items[i].id, ctx);
        fn(d->items[i].pack_key, d->items[i].id, ctx);
    }
    for (int i = 0; i < d->wagon_count; i++) {
        fn(d->wagons[i].name_key, d->wagons[i].id, ctx);
    }
    for (int i = 0; i < d->group_count; i++) {
        fn(d->groups[i].name_key, d->groups[i].id, ctx);
    }
    for (int i = 0; i < d->illness_count; i++) {
        fn(d->illnesses[i].name_key, d->illnesses[i].id, ctx);
        fn(d->illnesses[i].cause_key, d->illnesses[i].id, ctx);
    }
    for (int i = 0; i < d->region_count; i++) {
        fn(d->regions[i].name_key, d->regions[i].id, ctx);
    }
    for (int i = 0; i < d->node_count; i++) {
        for (int k = 0; k < d->nodes[i].epilogue_count; k++) {
            fn(d->nodes[i].epilogue[k], d->nodes[i].id, ctx);
        }
    }
    for (int i = 0; i < d->river_count; i++) {
        fn(d->rivers[i].name_key, d->rivers[i].id, ctx);
    }
    for (int i = 0; i < d->shop_count; i++) {
        fn(d->shops[i].name_key, d->shops[i].id, ctx);
        fn(d->shops[i].keeper_key, d->shops[i].id, ctx);
    }
    for (int i = 0; i < d->map.river_count; i++) {
        fn(d->map.rivers[i].key, "map.json", ctx);
    }
    for (int i = 0; i < d->map.text_count; i++) {
        fn(d->map.texts[i].key, "map.json", ctx);
    }
    for (int i = 0; i < d->node_count; i++) {
        fn(d->nodes[i].name_key, d->nodes[i].id, ctx);
        fn(d->nodes[i].desc_key, d->nodes[i].id, ctx);
        for (int a = 0; a < d->nodes[i].action_count; a++) {
            fn(d->nodes[i].action_label[a], d->nodes[i].id, ctx);
        }
    }
    for (int i = 0; i < d->edge_count; i++) {
        if (d->edges[i].label_key[0]) {
            fn(d->edges[i].label_key, "edge", ctx);
        }
        if (d->edges[i].advice_key[0]) {
            fn(d->edges[i].advice_key, "edge", ctx);
        }
    }
    for (int i = 0; i < d->family_count; i++) {
        const FamilyDef *f = &d->families[i];
        fn(f->name_key, f->id, ctx);
        fn(f->summary_key, f->id, ctx);
        for (int p = 1; p <= 3; p++) {
            snprintf(buf, sizeof buf, "%s.%d", f->story_key, p);
            fn(buf, f->id, ctx);
        }
    }
    for (int i = 0; i < d->animal_count; i++) {
        fn(d->animals[i].name_key, d->animals[i].id, ctx);
    }
    for (int i = 0; i < d->partner_count; i++) {
        fn(d->partners[i].name_key, d->partners[i].id, ctx);
        snprintf(buf, sizeof buf, "%s.greeting", d->partners[i].name_key);
        fn(buf, d->partners[i].id, ctx);
        snprintf(buf, sizeof buf, "%s.needs", d->partners[i].name_key);
        fn(buf, d->partners[i].id, ctx);
    }
    for (int i = 0; i < d->shock_count; i++) {
        if (d->shocks[i].excuse_key[0]) {
            fn(d->shocks[i].excuse_key, d->shocks[i].id, ctx);
        }
        if (d->shocks[i].history_key[0]) {
            fn(d->shocks[i].history_key, d->shocks[i].id, ctx);
        }
    }
    for (int i = 0; i < d->event_count; i++) {
        const EventDef *ev = &d->events[i];
        if (ev->history_key[0]) {
            fn(ev->history_key, ev->id, ctx);
        }
        for (int n = 0; n < ev->node_count; n++) {
            const EventNode *node = &ev->nodes[n];
            fn(node->text_key, ev->id, ctx);
            if (node->speaker_key[0]) {
                fn(node->speaker_key, ev->id, ctx);
            }
            for (int c = 0; c < node->choice_count; c++) {
                const Choice *ch = &node->choices[c];
                fn(ch->text_key, ev->id, ctx);
                if (ch->locked_key[0]) {
                    fn(ch->locked_key, ev->id, ctx);
                }
                for (int o = 0; o < ch->outcome_count; o++) {
                    if (ch->outcomes[o].text_key[0]) {
                        fn(ch->outcomes[o].text_key, ev->id, ctx);
                    }
                    for (int e = 0; e < ch->outcomes[o].effect_count; e++) {
                        if (ch->outcomes[o].effects[e].type == EFF_JOURNAL) {
                            fn(ch->outcomes[o].effects[e].str, ev->id, ctx);
                        }
                    }
                }
            }
        }
    }
}
