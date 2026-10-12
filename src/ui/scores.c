/* High scores in <pref>/scores.json (see scores.h). */
#include "scores.h"

#include "util.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *scores_path(const char *pref_path)
{
    return util_path_join(pref_path, "scores.json");
}

static int read_all(const char *pref_path, ScoreEntry *out, int max)
{
    char *path = scores_path(pref_path);
    char *text = util_read_file(path, NULL);
    free(path);
    if (!text) {
        return 0;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    int n = 0;
    const cJSON *o;
    cJSON_ArrayForEach(o, root)
    {
        if (n >= max) {
            break;
        }
        ScoreEntry *e = &out[n++];
        memset(e, 0, sizeof *e);
        const cJSON *v;
#define STR(field, name)                                                                                              \
    if (cJSON_IsString(v = cJSON_GetObjectItemCaseSensitive(o, name))) {                                              \
        snprintf(e->field, sizeof e->field, "%s", v->valuestring);                                                    \
    }
#define NUM(field, name)                                                                                              \
    if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(o, name))) {                                              \
        e->field = v->valueint;                                                                                       \
    }
        STR(trip_id, "trip")
        STR(family_id, "family")
        STR(surname, "surname")
        STR(node, "node")
        NUM(day, "day")
        NUM(alive, "alive")
        NUM(party, "party")
        NUM(miles, "miles")
        NUM(score, "score")
        if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(o, "difficulty"))) {
            e->difficulty = (Difficulty)v->valueint;
        }
        if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(o, "ending"))) {
            e->ending = (EndingKind)v->valueint;
        }
#undef STR
#undef NUM
    }
    cJSON_Delete(root);
    return n;
}

static void write_all(const char *pref_path, const ScoreEntry *in, int n)
{
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        const ScoreEntry *e = &in[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "trip", e->trip_id);
        cJSON_AddStringToObject(o, "family", e->family_id);
        cJSON_AddStringToObject(o, "surname", e->surname);
        cJSON_AddNumberToObject(o, "difficulty", e->difficulty);
        cJSON_AddNumberToObject(o, "ending", e->ending);
        cJSON_AddStringToObject(o, "node", e->node);
        cJSON_AddNumberToObject(o, "day", e->day);
        cJSON_AddNumberToObject(o, "alive", e->alive);
        cJSON_AddNumberToObject(o, "party", e->party);
        cJSON_AddNumberToObject(o, "miles", e->miles);
        cJSON_AddNumberToObject(o, "score", e->score);
        cJSON_AddItemToArray(root, o);
    }
    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    char *path = scores_path(pref_path);
    util_write_file(path, text, strlen(text));
    free(path);
    free(text);
}

static void entry_from_game(ScoreEntry *e, const char *trip_id, const Game *g)
{
    const GameData *d = g->data;
    memset(e, 0, sizeof *e);
    snprintf(e->trip_id, sizeof e->trip_id, "%s", trip_id);
    snprintf(e->family_id, sizeof e->family_id, "%s", d->families[g->family].id);
    snprintf(e->surname, sizeof e->surname, "%s", d->families[g->family].surname);
    snprintf(e->node, sizeof e->node, "%s", d->nodes[g->node].id);
    e->difficulty = g->difficulty;
    e->ending = g->ending;
    e->day = g->day;
    e->alive = game_member_count_with_party(g);
    e->party = g->party_count;
    e->miles = g->miles_total;
    e->score = game_score(g);
}

/* Adds or replaces the entry for its trip; returns the new count. */
static int put(ScoreEntry *all, int n, int max, const ScoreEntry *e)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(all[i].trip_id, e->trip_id) == 0) {
            all[i] = *e;
            return n;
        }
    }
    if (n < max) {
        all[n++] = *e;
    }
    return n;
}

void scores_record(const char *pref_path, const char *trip_id, const Game *g)
{
    if (!trip_id || !trip_id[0] || g->ending == ENDING_NONE) {
        return;
    }
    static ScoreEntry all[MAX_SCORES];
    int n = read_all(pref_path, all, MAX_SCORES);
    ScoreEntry e;
    entry_from_game(&e, trip_id, g);
    for (int i = 0; i < n; i++) {
        if (strcmp(all[i].trip_id, trip_id) == 0 && all[i].score == e.score && all[i].ending == e.ending) {
            return; /* already there, unchanged */
        }
    }
    n = put(all, n, MAX_SCORES, &e);
    write_all(pref_path, all, n);
}

static int by_score(const void *a, const void *b)
{
    const ScoreEntry *x = a, *y = b;
    return (y->score > x->score) - (y->score < x->score);
}

int scores_load(const char *pref_path, const TripList *trips, const GameData *data, ScoreEntry *out, int max)
{
    int n = read_all(pref_path, out, max);
    bool added = false;
    for (int t = 0; trips && t < trips->count; t++) {
        const TripInfo *ti = &trips->trips[t];
        bool known = false;
        for (int i = 0; i < n; i++) {
            known = known || strcmp(out[i].trip_id, ti->id) == 0;
        }
        if (known || ti->ending == ENDING_NONE) {
            continue;
        }
        char err[128];
        Game *g = trips_read(pref_path, ti->id, data, err, sizeof err);
        if (g) {
            ScoreEntry e;
            entry_from_game(&e, ti->id, g);
            n = put(out, n, max, &e);
            added = true;
            game_free(g);
        }
    }
    if (added) {
        write_all(pref_path, out, n);
    }
    qsort(out, (size_t)n, sizeof out[0], by_score);
    return n;
}
