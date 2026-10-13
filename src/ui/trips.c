#include "trips.h"

#include "util.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trips_dir(const char *pref, char *out, size_t cap)
{
    snprintf(out, cap, "%strips", pref);
}

static void trip_path(const char *pref, const char *id, const char *ext, char *out, size_t cap)
{
    snprintf(out, cap, "%strips/%s%s", pref, id, ext);
}

bool trips_write(const char *pref_path, const char *trip_id, const Game *g)
{
    char dir[1024], path[1024], tmp[1024];
    trips_dir(pref_path, dir, sizeof dir);
    SDL_CreateDirectory(dir);
    trip_path(pref_path, trip_id, ".json", path, sizeof path);
    trip_path(pref_path, trip_id, ".json.tmp", tmp, sizeof tmp);
    char *json = game_save_json(g);
    if (!json) {
        return false;
    }
    bool ok = util_write_file(tmp, json, strlen(json)) == 0;
    free(json);
    if (ok) {
        ok = SDL_RenamePath(tmp, path);
        if (!ok) {
            /* Some systems will not rename over an existing file: remove it first. */
            SDL_RemovePath(path);
            ok = SDL_RenamePath(tmp, path);
        }
    }
    if (!ok) {
        SDL_Log("Could not save trip %s: %s", trip_id, SDL_GetError());
    }
    return ok;
}

Game *trips_read(const char *pref_path, const char *trip_id, const GameData *data, char *err, size_t err_len)
{
    char path[1024];
    /* A leftover .tmp means the game stopped between writing it and renaming it. If it is
     * complete it is the newest save; if the crash was mid-write it will not parse. */
    trip_path(pref_path, trip_id, ".json.tmp", path, sizeof path);
    char *json = util_read_file(path, NULL);
    Game *g = json ? game_load_json(data, json, err, err_len) : NULL;
    free(json);
    if (g) {
        return g;
    }
    trip_path(pref_path, trip_id, ".json", path, sizeof path);
    json = util_read_file(path, NULL);
    if (!json) {
        snprintf(err, err_len, "cannot read the save for this trip");
        return NULL;
    }
    g = game_load_json(data, json, err, err_len);
    free(json);
    return g;
}

bool trips_delete(const char *pref_path, const char *trip_id)
{
    char path[1024];
    trip_path(pref_path, trip_id, ".json.tmp", path, sizeof path);
    SDL_RemovePath(path);
    trip_path(pref_path, trip_id, ".json", path, sizeof path);
    return SDL_RemovePath(path);
}

void trips_new_id(char *out, size_t cap, uint64_t seed)
{
    SDL_Time now = 0;
    SDL_GetCurrentTime(&now);
    SDL_DateTime dt;
    SDL_TimeToDateTime(now, &dt, true);
    snprintf(out, cap, "trip-%04d%02d%02d-%02d%02d%02d-%04x", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second,
             (unsigned)(seed & 0xffff));
}

typedef struct {
    const char *pref;
    TripList *list;
} ScanCtx;

static void read_info(const char *pref, const char *id, TripInfo *t)
{
    char path[1024];
    trip_path(pref, id, ".json", path, sizeof path);
    memset(t, 0, sizeof *t);
    snprintf(t->id, sizeof t->id, "%s", id);
    SDL_PathInfo info;
    if (SDL_GetPathInfo(path, &info)) {
        t->modified = info.modify_time;
    }
    char *json = util_read_file(path, NULL);
    cJSON *o = json ? cJSON_Parse(json) : NULL;
    free(json);
    if (!o) {
        return;
    }
    const cJSON *v;
    if (cJSON_IsString(v = cJSON_GetObjectItemCaseSensitive(o, "family"))) {
        snprintf(t->family_id, sizeof t->family_id, "%s", v->valuestring);
    }
    if (cJSON_IsString(v = cJSON_GetObjectItemCaseSensitive(o, "difficulty"))) {
        snprintf(t->difficulty, sizeof t->difficulty, "%s", v->valuestring);
    }
    if (cJSON_IsString(v = cJSON_GetObjectItemCaseSensitive(o, "node"))) {
        snprintf(t->node, sizeof t->node, "%s", v->valuestring);
    }
    if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(o, "day"))) {
        t->day = v->valueint;
    }
    if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(o, "ending"))) {
        t->ending = v->valueint;
    }
    t->phase = PHASE_TRAVEL;
    if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(o, "phase"))) {
        t->phase = v->valueint;
    }
    const cJSON *m;
    cJSON_ArrayForEach(m, cJSON_GetObjectItemCaseSensitive(o, "party"))
    {
        t->party++;
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(m, "alive")) &&
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(m, "with_party"))) {
            t->alive++;
        }
    }
    cJSON_Delete(o);
}

static SDL_EnumerationResult SDLCALL scan_cb(void *userdata, const char *dirname, const char *fname)
{
    (void)dirname;
    ScanCtx *ctx = userdata;
    size_t n = strlen(fname);
    if (n > 5 && strcmp(fname + n - 5, ".json") == 0 && ctx->list->count < MAX_TRIPS) {
        char id[64];
        snprintf(id, sizeof id, "%.*s", (int)(n - 5), fname);
        read_info(ctx->pref, id, &ctx->list->trips[ctx->list->count++]);
    }
    return SDL_ENUM_CONTINUE;
}

static int newest_first(const void *a, const void *b)
{
    SDL_Time ta = ((const TripInfo *)a)->modified, tb = ((const TripInfo *)b)->modified;
    return ta < tb ? 1 : (ta > tb ? -1 : 0);
}

void trips_scan(const char *pref_path, TripList *list)
{
    memset(list, 0, sizeof *list);
    char dir[1024];
    trips_dir(pref_path, dir, sizeof dir);
    ScanCtx ctx = {pref_path, list};
    SDL_EnumerateDirectory(dir, scan_cb, &ctx);
    qsort(list->trips, (size_t)list->count, sizeof list->trips[0], newest_first);
}
