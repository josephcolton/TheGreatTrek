/* Saves: the whole Game as JSON. Things from data/ are stored by id, not by index,
 * so a save survives data files gaining new entries. */
#include "game_internal.h"
#include "version.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- write */

static cJSON *loctext_json(const LocText *t)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "k", t->key);
    if (t->n_args > 0) {
        cJSON *args = cJSON_AddArrayToObject(o, "a");
        for (int i = 0; i < t->n_args; i++) {
            const TextArg *a = &t->args[i];
            cJSON *arr = cJSON_CreateArray();
            cJSON_AddItemToArray(arr, cJSON_CreateString(a->name));
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(a->kind));
            cJSON_AddItemToArray(arr, cJSON_CreateNumber((double)a->i));
            cJSON_AddItemToArray(arr, cJSON_CreateString(a->s));
            cJSON_AddItemToArray(args, arr);
        }
    }
    return o;
}

static cJSON *int_array(const int *v, int n)
{
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(v[i]));
    }
    return arr;
}

static const char *node_id(const Game *g, int n)
{
    return n >= 0 && n < g->data->node_count ? g->data->nodes[n].id : "";
}

/* Edges have no id: store "from>to". */
static void edge_id(const Game *g, int e, char *out, size_t cap)
{
    if (e < 0 || e >= g->data->edge_count) {
        snprintf(out, cap, "%s", "");
        return;
    }
    snprintf(out, cap, "%s>%s", g->data->nodes[g->data->edges[e].from].id, g->data->nodes[g->data->edges[e].to].id);
}

char *game_save_json(const Game *g)
{
    const GameData *d = g->data;
    cJSON *o = cJSON_CreateObject();
    char buf[80];
    cJSON_AddNumberToObject(o, "version", TREK_SAVE_VERSION);
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)g->seed);
    cJSON_AddStringToObject(o, "seed", buf);
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)g->rng.state);
    cJSON_AddStringToObject(o, "rng_state", buf);
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)g->rng.inc);
    cJSON_AddStringToObject(o, "rng_inc", buf);
    cJSON_AddStringToObject(o, "family", d->families[g->family].id);
    cJSON_AddStringToObject(o, "difficulty", difficulty_id(g->difficulty));
    cJSON_AddNumberToObject(o, "phase", g->phase);
    cJSON_AddNumberToObject(o, "day", g->day);
    cJSON_AddNumberToObject(o, "start_day", g->start_day);
    cJSON_AddStringToObject(o, "node", node_id(g, g->node));
    edge_id(g, g->edge, buf, sizeof buf);
    cJSON_AddStringToObject(o, "edge", buf);
    cJSON_AddNumberToObject(o, "miles_along", g->miles_along);
    cJSON_AddNumberToObject(o, "miles_total", g->miles_total);
    cJSON_AddStringToObject(o, "weather", weather_id(g->weather));
    cJSON_AddBoolToObject(o, "at_node", g->at_node);
    cJSON *nodes = cJSON_AddObjectToObject(o, "nodes");
    for (int i = 0; i < d->node_count; i++) {
        if (g->visited[i] || g->setpiece_done[i] || g->river_crossed[i]) {
            cJSON *n = cJSON_AddObjectToObject(nodes, d->nodes[i].id);
            cJSON_AddBoolToObject(n, "visited", g->visited[i]);
            cJSON_AddBoolToObject(n, "setpiece", g->setpiece_done[i]);
            cJSON_AddBoolToObject(n, "crossed", g->river_crossed[i]);
        }
    }
    cJSON_AddStringToObject(o, "pace", pace_id(g->pace));
    cJSON_AddStringToObject(o, "rations", rations_id(g->rations));
    cJSON_AddStringToObject(o, "stance", stance_id(g->stance));
    cJSON_AddBoolToObject(o, "sabbath", g->sabbath);
    cJSON_AddBoolToObject(o, "keep_flock", g->keep_flock);
    cJSON_AddBoolToObject(o, "medicine_by_hand", g->medicine_by_hand);

    cJSON *party = cJSON_AddArrayToObject(o, "party");
    for (int i = 0; i < g->party_count; i++) {
        const Member *m = &g->party[i];
        cJSON *mj = cJSON_CreateObject();
        cJSON_AddStringToObject(mj, "name", m->name);
        cJSON_AddStringToObject(mj, "role", role_id(m->role));
        cJSON_AddNumberToObject(mj, "age", m->age);
        cJSON_AddNumberToObject(mj, "pronoun", m->pronoun);
        cJSON_AddNumberToObject(mj, "born", m->birth_year);
        cJSON_AddStringToObject(mj, "portrait", m->portrait);
        cJSON *sk = cJSON_AddObjectToObject(mj, "skills");
        for (int s = 0; s < SKILL_COUNT; s++) {
            if (m->skills[s]) {
                cJSON_AddNumberToObject(sk, skill_id((Skill)s), m->skills[s]);
            }
        }
        cJSON_AddNumberToObject(mj, "health", m->health);
        cJSON *conds = cJSON_AddArrayToObject(mj, "conds");
        for (int c = 0; c < m->cond_count; c++) {
            cJSON *cj = cJSON_CreateObject();
            cJSON_AddStringToObject(cj, "illness", d->illnesses[m->conds[c].illness].id);
            cJSON_AddNumberToObject(cj, "days", m->conds[c].days);
            cJSON_AddNumberToObject(cj, "severity", m->conds[c].severity);
            cJSON_AddNumberToObject(cj, "doses", m->conds[c].doses);
            cJSON_AddBoolToObject(cj, "treated", m->conds[c].treated);
            cJSON_AddItemToArray(conds, cj);
        }
        cJSON_AddNumberToObject(mj, "loyalty", m->loyalty);
        cJSON_AddBoolToObject(mj, "alive", m->alive);
        cJSON_AddBoolToObject(mj, "with_party", m->with_party);
        cJSON_AddBoolToObject(mj, "warned", m->warned);
        cJSON_AddItemToArray(party, mj);
    }
    cJSON *inv = cJSON_AddObjectToObject(o, "inventory");
    for (int i = 0; i < d->item_count; i++) {
        if (g->inv[i]) {
            cJSON_AddNumberToObject(inv, d->items[i].id, g->inv[i]);
        }
    }
    cJSON_AddNumberToObject(o, "money", g->money);
    cJSON_AddStringToObject(o, "wagon", d->wagons[g->wagon].id);
    cJSON_AddItemToObject(o, "wagon_parts", int_array(g->wagon_parts, WAGON_PART_COUNT));
    cJSON_AddNumberToObject(o, "ox_stamina", g->ox_stamina);
    cJSON_AddNumberToObject(o, "morale", g->morale);
    cJSON *rel = cJSON_AddObjectToObject(o, "relations");
    for (int i = 0; i < d->group_count; i++) {
        cJSON_AddNumberToObject(rel, d->groups[i].id, g->relation[i]);
    }
    cJSON_AddNumberToObject(o, "days_since_wages", g->days_since_wages);
    cJSON *bought = cJSON_AddObjectToObject(o, "shop_bought");
    for (int s = 0; s < d->shop_count; s++) {
        cJSON *sj = NULL;
        for (int i = 0; i < d->item_count; i++) {
            if (g->shop_bought[s][i]) {
                if (!sj) {
                    sj = cJSON_AddObjectToObject(bought, d->shops[s].id);
                }
                cJSON_AddNumberToObject(sj, d->items[i].id, g->shop_bought[s][i]);
            }
        }
    }
    cJSON *flags = cJSON_AddArrayToObject(o, "flags");
    for (int i = 0; i < g->flag_count; i++) {
        cJSON_AddItemToArray(flags, cJSON_CreateString(g->flags[i]));
    }
    cJSON *counters = cJSON_AddObjectToObject(o, "counters");
    for (int i = 0; i < g->counter_count; i++) {
        cJSON_AddNumberToObject(counters, g->counter_names[i], g->counter_values[i]);
    }
    cJSON *seen = cJSON_AddObjectToObject(o, "events_seen");
    for (int i = 0; i < d->event_count; i++) {
        if (g->event_last_day[i] >= 0) {
            cJSON_AddNumberToObject(seen, d->events[i].id, g->event_last_day[i]);
        }
    }
    cJSON *sched = cJSON_AddArrayToObject(o, "scheduled");
    for (int i = 0; i < g->scheduled_count; i++) {
        cJSON *sj = cJSON_CreateObject();
        cJSON_AddStringToObject(sj, "event", d->events[g->scheduled[i].event].id);
        cJSON_AddNumberToObject(sj, "day", g->scheduled[i].day);
        cJSON_AddItemToArray(sched, sj);
    }

    cJSON *dlg = cJSON_AddObjectToObject(o, "dialog");
    cJSON_AddNumberToObject(dlg, "kind", g->dlg.kind);
    cJSON_AddStringToObject(dlg, "event", g->dlg.kind == DLG_EVENT ? d->events[g->dlg.event].id : "");
    cJSON_AddNumberToObject(dlg, "node", g->dlg.node);
    cJSON_AddNumberToObject(dlg, "speaker", g->dlg.speaker);
    cJSON_AddNumberToObject(dlg, "subject", g->dlg.subject);
    cJSON_AddBoolToObject(dlg, "showing_result", g->dlg.showing_result);
    cJSON *res = cJSON_AddArrayToObject(dlg, "result");
    for (int i = 0; i < g->dlg.result_count; i++) {
        cJSON_AddItemToArray(res, loctext_json(&g->dlg.result[i]));
    }
    cJSON_AddNumberToObject(dlg, "next_node", g->dlg.next_node);
    cJSON_AddNumberToObject(dlg, "river_depth", g->dlg.river_depth);
    cJSON_AddNumberToObject(dlg, "part", g->dlg.part);

    cJSON *notices = cJSON_AddArrayToObject(o, "notices");
    for (int i = 0; i < g->notice_count; i++) {
        cJSON_AddItemToArray(notices, loctext_json(&g->notices[i]));
    }
    cJSON_AddStringToObject(o, "shop", g->shop >= 0 ? d->shops[g->shop].id : "");
    cJSON_AddItemToObject(o, "pending_graves", int_array(g->pending_graves, g->pending_grave_count));
    cJSON_AddBoolToObject(o, "warned_food", g->warned_food);
    cJSON_AddBoolToObject(o, "warned_water", g->warned_water);

    cJSON *graves = cJSON_AddArrayToObject(o, "graves");
    for (int i = 0; i < g->grave_count; i++) {
        const Grave *gr = &g->graves[i];
        cJSON *gj = cJSON_CreateObject();
        cJSON_AddNumberToObject(gj, "member", gr->member);
        cJSON_AddNumberToObject(gj, "day", gr->day);
        cJSON_AddStringToObject(gj, "node", node_id(g, gr->node));
        edge_id(g, gr->edge, buf, sizeof buf);
        cJSON_AddStringToObject(gj, "edge", buf);
        cJSON_AddNumberToObject(gj, "miles_along", gr->miles_along);
        cJSON_AddStringToObject(gj, "cause", gr->cause_key);
        cJSON_AddStringToObject(gj, "epitaph", gr->epitaph);
        cJSON_AddNumberToObject(gj, "illness", gr->illness);
        cJSON_AddNumberToObject(gj, "ill_days", gr->ill_days);
        cJSON_AddItemToArray(graves, gj);
    }
    cJSON *journal = cJSON_AddArrayToObject(o, "journal");
    for (int i = 0; i < g->journal_count; i++) {
        cJSON *jj = loctext_json(&g->journal[i].text);
        cJSON_AddNumberToObject(jj, "d", g->journal[i].day);
        cJSON_AddItemToArray(journal, jj);
    }
    cJSON_AddNumberToObject(o, "ending", g->ending);
    cJSON_AddNumberToObject(o, "deaths", g->deaths);

    cJSON_AddNumberToObject(o, "last_hunt_day", g->last_hunt_day);
    cJSON *traded = cJSON_AddObjectToObject(o, "partner_traded");
    for (int p = 0; p < d->partner_count; p++) {
        cJSON *pj = NULL;
        for (int i = 0; i < d->partners[p].offer_count; i++) {
            if (g->partner_traded[p][i]) {
                if (!pj) {
                    pj = cJSON_AddObjectToObject(traded, d->partners[p].id);
                }
                cJSON_AddNumberToObject(pj, d->items[d->partners[p].offers[i].item].id, g->partner_traded[p][i]);
            }
        }
    }
    if (g->barter.active) {
        cJSON *bj = cJSON_AddObjectToObject(o, "barter");
        cJSON_AddStringToObject(bj, "partner", d->partners[g->barter.partner].id);
        cJSON_AddBoolToObject(bj, "asked", g->barter.asked);
        cJSON_AddItemToObject(bj, "reply", loctext_json(&g->barter.reply));
        cJSON *give = cJSON_AddObjectToObject(bj, "give");
        cJSON *take = cJSON_AddObjectToObject(bj, "take");
        for (int i = 0; i < d->item_count; i++) {
            if (g->barter.give[i]) {
                cJSON_AddNumberToObject(give, d->items[i].id, g->barter.give[i]);
            }
            if (g->barter.take[i]) {
                cJSON_AddNumberToObject(take, d->items[i].id, g->barter.take[i]);
            }
        }
    }
    cJSON *game_level = cJSON_AddObjectToObject(o, "region_game");
    for (int i = 0; i < d->region_count; i++) {
        cJSON_AddNumberToObject(game_level, d->regions[i].id, g->region_game[i]);
    }
    const HuntState *h = &g->hunt;
    if (h->active) {
        cJSON *hj = cJSON_AddObjectToObject(o, "hunt");
        /* The animals in the field are not saved: a hunt picks up again with the veld empty. */
        cJSON_AddNumberToObject(hj, "hunter", h->hunter);
        cJSON_AddNumberToObject(hj, "ticks_left", h->ticks_left);
        cJSON_AddNumberToObject(hj, "reload_ticks", h->reload_ticks);
        cJSON_AddNumberToObject(hj, "meat", h->meat);
        cJSON_AddNumberToObject(hj, "carry", h->carry);
        cJSON_AddNumberToObject(hj, "shots", h->shots);
        cJSON_AddNumberToObject(hj, "shots_max", h->shots_max);
        cJSON_AddNumberToObject(hj, "kills", h->kills);
        cJSON_AddBoolToObject(hj, "over", h->over);
        if (h->defend) {
            cJSON_AddStringToObject(hj, "defend", d->animals[h->foe].id);
            cJSON_AddNumberToObject(hj, "foes_left", h->foes_left);
            cJSON_AddNumberToObject(hj, "driven", h->driven);
            cJSON *took = cJSON_AddArrayToObject(hj, "took");
            for (int i = 0; i < h->took_count; i++) {
                cJSON_AddItemToArray(took, cJSON_CreateString(d->items[h->took[i]].id));
            }
        }
        cJSON *log = cJSON_AddArrayToObject(hj, "log");
        for (int i = 0; i < h->log_count; i++) {
            cJSON_AddItemToArray(log, loctext_json(&h->log[i]));
        }
    }

    char *text = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return text;
}

/* ---------------------------------------------------------------- read */

typedef struct {
    char *err;
    size_t err_len;
    bool failed;
} SaveReader;

static void rfail(SaveReader *R, const char *msg, const char *detail)
{
    if (!R->failed && R->err && R->err_len) {
        snprintf(R->err, R->err_len, "%s%s%s", msg, detail ? ": " : "", detail ? detail : "");
    }
    R->failed = true;
}

static const char *sget(const cJSON *o, const char *k)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsString(v) ? v->valuestring : "";
}

static double nget(const cJSON *o, const char *k, double def)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}

static bool bget(const cJSON *o, const char *k)
{
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, k));
}

static void read_loctext(const cJSON *o, LocText *t)
{
    loctext_set(t, sget(o, "k"));
    const cJSON *a;
    cJSON_ArrayForEach(a, cJSON_GetObjectItemCaseSensitive(o, "a"))
    {
        if (t->n_args >= MAX_ARGS) {
            break;
        }
        TextArg *ta = &t->args[t->n_args++];
        const cJSON *name = cJSON_GetArrayItem(a, 0), *kind = cJSON_GetArrayItem(a, 1);
        const cJSON *iv = cJSON_GetArrayItem(a, 2), *sv = cJSON_GetArrayItem(a, 3);
        snprintf(ta->name, sizeof ta->name, "%s", cJSON_IsString(name) ? name->valuestring : "");
        ta->kind = cJSON_IsNumber(kind) ? (ArgKind)kind->valueint : ARG_STR;
        ta->i = cJSON_IsNumber(iv) ? (long long)iv->valuedouble : 0;
        snprintf(ta->s, sizeof ta->s, "%s", cJSON_IsString(sv) ? sv->valuestring : "");
    }
}

static int find_edge(const Game *g, const char *id)
{
    for (int e = 0; e < g->data->edge_count; e++) {
        char buf[80];
        edge_id(g, e, buf, sizeof buf);
        if (strcmp(buf, id) == 0) {
            return e;
        }
    }
    return -1;
}

static int find_id(const char *const *ids, int n, const char *s)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(ids[i], s) == 0) {
            return i;
        }
    }
    return -1;
}

Game *game_load_json(const GameData *data, const char *json, char *err, size_t err_len)
{
    SaveReader R = {err, err_len, false};
    cJSON *o = cJSON_Parse(json);
    if (!o) {
        rfail(&R, "save is not valid JSON", NULL);
        return NULL;
    }
    if ((int)nget(o, "version", 0) != TREK_SAVE_VERSION) {
        rfail(&R, "save is from a different version of the game", NULL);
        cJSON_Delete(o);
        return NULL;
    }
    int family = data_find_family(data, sget(o, "family"));
    static const char *DIFFS[] = {"easy", "normal", "hard"};
    int diff = find_id(DIFFS, DIFF_COUNT, sget(o, "difficulty"));
    if (family < 0 || diff < 0) {
        rfail(&R, "save names an unknown family or difficulty", sget(o, "family"));
        cJSON_Delete(o);
        return NULL;
    }
    unsigned long long seed = strtoull(sget(o, "seed"), NULL, 10);
    Game *g = game_new(data, seed, family, (Difficulty)diff);
    if (!g) {
        rfail(&R, "out of memory", NULL);
        cJSON_Delete(o);
        return NULL;
    }
    /* game_new wrote a start journal entry and opened the first scene; start clean. */
    g->journal_count = 0;
    memset(&g->dlg, 0, sizeof g->dlg);
    memset(g->visited, 0, sizeof g->visited);
    memset(g->setpiece_done, 0, sizeof g->setpiece_done);
    for (int i = 0; i < data->event_count; i++) {
        g->event_last_day[i] = -1;
    }

    g->rng.state = strtoull(sget(o, "rng_state"), NULL, 10);
    g->rng.inc = strtoull(sget(o, "rng_inc"), NULL, 10);
    g->phase = (GamePhase)(int)nget(o, "phase", PHASE_TRAVEL);
    g->day = (int)nget(o, "day", g->day);
    g->start_day = (int)nget(o, "start_day", g->start_day);
    g->node = data_find_node(data, sget(o, "node"));
    if (g->node < 0) {
        rfail(&R, "unknown place in save", sget(o, "node"));
    }
    g->edge = sget(o, "edge")[0] ? find_edge(g, sget(o, "edge")) : -1;
    g->miles_along = nget(o, "miles_along", 0);
    g->miles_total = (int)nget(o, "miles_total", 0);
    static const char *WEATHERS[] = {"clear", "hot", "windy", "cold", "frost", "rain", "storm"};
    int w = find_id(WEATHERS, WEATHER_COUNT, sget(o, "weather"));
    g->weather = w < 0 ? WEATHER_CLEAR : (Weather)w;
    g->at_node = bget(o, "at_node");
    const cJSON *n;
    cJSON_ArrayForEach(n, cJSON_GetObjectItemCaseSensitive(o, "nodes"))
    {
        int ni = data_find_node(data, n->string);
        if (ni >= 0) {
            g->visited[ni] = bget(n, "visited");
            g->setpiece_done[ni] = bget(n, "setpiece");
            g->river_crossed[ni] = bget(n, "crossed");
        }
    }
    static const char *PACES[] = {"easy", "normal", "hard"};
    static const char *RATIONS[] = {"full", "meagre", "starving"};
    static const char *STANCES[] = {"rest", "laager", "guard"};
    int p = find_id(PACES, PACE_COUNT, sget(o, "pace"));
    int r = find_id(RATIONS, RATIONS_COUNT, sget(o, "rations"));
    int s = find_id(STANCES, NIGHT_COUNT, sget(o, "stance"));
    g->pace = p < 0 ? PACE_NORMAL : (Pace)p;
    g->rations = r < 0 ? RATIONS_FULL : (Rations)r;
    g->stance = s < 0 ? NIGHT_LAAGER : (NightStance)s;
    g->sabbath = bget(o, "sabbath");
    g->keep_flock = bget(o, "keep_flock"); /* older saves: eat the flock, as before */
    g->medicine_by_hand = bget(o, "medicine_by_hand");

    g->party_count = 0;
    const cJSON *mj;
    cJSON_ArrayForEach(mj, cJSON_GetObjectItemCaseSensitive(o, "party"))
    {
        if (g->party_count >= MAX_PARTY) {
            break;
        }
        Member *m = &g->party[g->party_count++];
        memset(m, 0, sizeof *m);
        snprintf(m->name, sizeof m->name, "%s", sget(mj, "name"));
        int role = role_from_id(sget(mj, "role"));
        m->role = role < 0 ? ROLE_CHILD : (Role)role;
        m->age = (AgeGroup)(int)nget(mj, "age", AGE_ADULT);
        m->pronoun = (Pronoun)(int)nget(mj, "pronoun", PRONOUN_HE);
        m->birth_year = (int)nget(mj, "born", 1800);
        snprintf(m->portrait, sizeof m->portrait, "%s", sget(mj, "portrait"));
        if (!m->portrait[0] && g->party_count <= data->families[family].member_count) {
            /* saves from before portraits: the party is in the family's order */
            snprintf(m->portrait, sizeof m->portrait, "%s", data->families[family].members[g->party_count - 1].portrait);
        }
        const cJSON *sk;
        cJSON_ArrayForEach(sk, cJSON_GetObjectItemCaseSensitive(mj, "skills"))
        {
            int si = skill_from_id(sk->string);
            if (si >= 0 && cJSON_IsNumber(sk)) {
                m->skills[si] = (uint8_t)sk->valueint;
            }
        }
        m->health = (int)nget(mj, "health", 50);
        const cJSON *cj;
        cJSON_ArrayForEach(cj, cJSON_GetObjectItemCaseSensitive(mj, "conds"))
        {
            int il = data_find_illness(data, sget(cj, "illness"));
            if (il >= 0 && m->cond_count < MAX_MEMBER_CONDS) {
                m->conds[m->cond_count].illness = il;
                m->conds[m->cond_count].days = (int)nget(cj, "days", 0);
                m->conds[m->cond_count].severity = (int)nget(cj, "severity", 100);
                m->conds[m->cond_count].doses = (int)nget(cj, "doses", 0);
                m->conds[m->cond_count].treated = bget(cj, "treated");
                m->cond_count++;
            }
        }
        m->loyalty = (int)nget(mj, "loyalty", 50);
        m->alive = bget(mj, "alive");
        m->with_party = bget(mj, "with_party");
        m->warned = bget(mj, "warned");
    }
    memset(g->inv, 0, sizeof g->inv);
    const cJSON *it;
    cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(o, "inventory"))
    {
        int ii = data_find_item(data, it->string);
        if (ii >= 0 && cJSON_IsNumber(it)) {
            g->inv[ii] = it->valueint;
        }
    }
    g->money = (int)nget(o, "money", 0);
    int wagon = data_find_wagon(data, sget(o, "wagon"));
    if (wagon >= 0) {
        g->wagon = wagon;
    }
    const cJSON *parts = cJSON_GetObjectItemCaseSensitive(o, "wagon_parts");
    for (int i = 0; i < WAGON_PART_COUNT; i++) {
        const cJSON *v = cJSON_GetArrayItem(parts, i);
        g->wagon_parts[i] = cJSON_IsNumber(v) ? v->valueint : 100;
    }
    g->ox_stamina = (int)nget(o, "ox_stamina", 50);
    g->morale = (int)nget(o, "morale", 50);
    const cJSON *rel;
    cJSON_ArrayForEach(rel, cJSON_GetObjectItemCaseSensitive(o, "relations"))
    {
        int gi = data_find_group(data, rel->string);
        if (gi >= 0 && cJSON_IsNumber(rel)) {
            g->relation[gi] = rel->valueint;
        }
    }
    g->days_since_wages = (int)nget(o, "days_since_wages", 0);
    const cJSON *sj;
    cJSON_ArrayForEach(sj, cJSON_GetObjectItemCaseSensitive(o, "shop_bought"))
    {
        int si = data_find_shop(data, sj->string);
        const cJSON *bj;
        cJSON_ArrayForEach(bj, sj)
        {
            int ii = data_find_item(data, bj->string);
            if (si >= 0 && ii >= 0 && cJSON_IsNumber(bj)) {
                g->shop_bought[si][ii] = bj->valueint;
            }
        }
    }
    g->flag_count = 0;
    const cJSON *f;
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "flags"))
    {
        if (cJSON_IsString(f)) {
            g_set_flag(g, f->valuestring);
        }
    }
    g->counter_count = 0;
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "counters"))
    {
        if (cJSON_IsNumber(f)) {
            g_add_counter(g, f->string, f->valueint);
        }
    }
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "events_seen"))
    {
        int ei = data_find_event(data, f->string);
        if (ei >= 0 && cJSON_IsNumber(f)) {
            g->event_last_day[ei] = f->valueint;
        }
    }
    g->scheduled_count = 0;
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "scheduled"))
    {
        int ei = data_find_event(data, sget(f, "event"));
        if (ei >= 0 && g->scheduled_count < MAX_SCHEDULED) {
            g->scheduled[g->scheduled_count].event = ei;
            g->scheduled[g->scheduled_count].day = (int)nget(f, "day", 0);
            g->scheduled_count++;
        }
    }

    const cJSON *dlg = cJSON_GetObjectItemCaseSensitive(o, "dialog");
    g->dlg.kind = (DialogKind)(int)nget(dlg, "kind", DLG_NONE);
    if (g->dlg.kind == DLG_EVENT) {
        g->dlg.event = data_find_event(data, sget(dlg, "event"));
        if (g->dlg.event < 0) {
            rfail(&R, "save refers to an event that no longer exists", sget(dlg, "event"));
        }
    }
    g->dlg.node = (int)nget(dlg, "node", 0);
    g->dlg.speaker = (int)nget(dlg, "speaker", -1);
    g->dlg.subject = (int)nget(dlg, "subject", -1);
    g->dlg.showing_result = bget(dlg, "showing_result");
    g->dlg.result_count = 0;
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(dlg, "result"))
    {
        if (g->dlg.result_count < MAX_RESULT_LINES) {
            read_loctext(f, &g->dlg.result[g->dlg.result_count++]);
        }
    }
    g->dlg.next_node = (int)nget(dlg, "next_node", -1);
    g->dlg.river_depth = nget(dlg, "river_depth", 0);
    g->dlg.part = (int)nget(dlg, "part", 0);

    g->notice_count = 0;
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "notices"))
    {
        if (g->notice_count < MAX_NOTICES) {
            read_loctext(f, &g->notices[g->notice_count++]);
        }
    }
    g->shop = sget(o, "shop")[0] ? data_find_shop(data, sget(o, "shop")) : -1;
    g->pending_grave_count = 0;
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "pending_graves"))
    {
        if (cJSON_IsNumber(f) && g->pending_grave_count < MAX_GRAVES) {
            g->pending_graves[g->pending_grave_count++] = f->valueint;
        }
    }
    g->warned_food = bget(o, "warned_food");
    g->warned_water = bget(o, "warned_water");
    g->grave_count = 0;
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "graves"))
    {
        if (g->grave_count >= MAX_GRAVES) {
            break;
        }
        Grave *gr = &g->graves[g->grave_count++];
        memset(gr, 0, sizeof *gr);
        gr->member = (int)nget(f, "member", 0);
        gr->day = (int)nget(f, "day", 0);
        gr->node = data_find_node(data, sget(f, "node"));
        gr->edge = sget(f, "edge")[0] ? find_edge(g, sget(f, "edge")) : -1;
        gr->miles_along = (int)nget(f, "miles_along", 0);
        snprintf(gr->cause_key, sizeof gr->cause_key, "%s", sget(f, "cause"));
        snprintf(gr->epitaph, sizeof gr->epitaph, "%s", sget(f, "epitaph"));
        gr->illness = (int)nget(f, "illness", -1); /* older saves: not known */
        gr->ill_days = (int)nget(f, "ill_days", 0);
        if (gr->illness >= data->illness_count) {
            gr->illness = -1;
        }
    }
    g->journal_count = 0;
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "journal"))
    {
        if (g->journal_count < MAX_JOURNAL) {
            JournalEntry *je = &g->journal[g->journal_count++];
            read_loctext(f, &je->text);
            je->day = (int)nget(f, "d", 0);
        }
    }
    g->ending = (EndingKind)(int)nget(o, "ending", ENDING_NONE);
    g->deaths = (int)nget(o, "deaths", 0);
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "region_game"))
    {
        int ri = data_find_region(data, f->string);
        if (ri >= 0 && cJSON_IsNumber(f)) {
            g->region_game[ri] = f->valuedouble;
        }
    }
    g->last_hunt_day = (int)nget(o, "last_hunt_day", -1);
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(o, "partner_traded"))
    {
        int pi = -1;
        for (int p = 0; p < data->partner_count; p++) {
            if (strcmp(data->partners[p].id, f->string) == 0) {
                pi = p;
            }
        }
        const cJSON *it2;
        cJSON_ArrayForEach(it2, f)
        {
            int item = data_find_item(data, it2->string);
            for (int i = 0; pi >= 0 && i < data->partners[pi].offer_count; i++) {
                if (data->partners[pi].offers[i].item == item && cJSON_IsNumber(it2)) {
                    g->partner_traded[pi][i] = it2->valueint;
                }
            }
        }
    }
    memset(&g->barter, 0, sizeof g->barter);
    const cJSON *bj = cJSON_GetObjectItemCaseSensitive(o, "barter");
    if (bj) {
        g->barter.partner = -1;
        for (int p = 0; p < data->partner_count; p++) {
            if (strcmp(data->partners[p].id, sget(bj, "partner")) == 0) {
                g->barter.partner = p;
            }
        }
        if (g->barter.partner >= 0) {
            g->barter.active = true;
            g->barter.asked = bget(bj, "asked");
            read_loctext(cJSON_GetObjectItemCaseSensitive(bj, "reply"), &g->barter.reply);
            const cJSON *gi;
            cJSON_ArrayForEach(gi, cJSON_GetObjectItemCaseSensitive(bj, "give"))
            {
                int item = data_find_item(data, gi->string);
                if (item >= 0 && cJSON_IsNumber(gi)) {
                    g->barter.give[item] = gi->valueint;
                }
            }
            cJSON_ArrayForEach(gi, cJSON_GetObjectItemCaseSensitive(bj, "take"))
            {
                int item = data_find_item(data, gi->string);
                if (item >= 0 && cJSON_IsNumber(gi)) {
                    g->barter.take[item] = gi->valueint;
                }
            }
        }
    }
    memset(&g->hunt, 0, sizeof g->hunt);
    const cJSON *hj = cJSON_GetObjectItemCaseSensitive(o, "hunt");
    if (hj) {
        HuntState *h = &g->hunt;
        h->active = true;
        h->hunter = (int)nget(hj, "hunter", 0);
        h->ticks_left = (int)nget(hj, "ticks_left", 0);
        h->reload_ticks = (int)nget(hj, "reload_ticks", 0);
        h->meat = (int)nget(hj, "meat", 0);
        h->carry = (int)nget(hj, "carry", 0);
        h->shots = (int)nget(hj, "shots", 0);
        h->shots_max = (int)nget(hj, "shots_max", 0);
        h->kills = (int)nget(hj, "kills", 0);
        h->over = bget(hj, "over") || h->ticks_left <= 0;
        h->spawn_ticks = HUNT_TICKS_PER_SEC;
        h->shot_age = 1000;
        const cJSON *foe = cJSON_GetObjectItemCaseSensitive(hj, "defend");
        if (cJSON_IsString(foe)) {
            /* guarding the herd: the raiders in the field are lost, as in a hunt, but not those still to come */
            h->defend = true;
            h->foe = data_find_animal(g->data, foe->valuestring);
            if (h->foe < 0) {
                h->foe = 0;
                h->over = true;
            }
            h->foes_left = (int)nget(hj, "foes_left", 0);
            h->driven = (int)nget(hj, "driven", 0);
            cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(hj, "took"))
            {
                int item = cJSON_IsString(f) ? data_find_item(g->data, f->valuestring) : -1;
                if (item >= 0 && h->took_count < HUNT_MAX_TAKES) {
                    h->took[h->took_count++] = item;
                }
            }
        }
        cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(hj, "log"))
        {
            if (h->log_count < HUNT_LOG) {
                read_loctext(f, &h->log[h->log_count++]);
            }
        }
    }
    cJSON_Delete(o);
    if (R.failed) {
        game_free(g);
        return NULL;
    }
    return g;
}
