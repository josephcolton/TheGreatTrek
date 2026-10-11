#include "locale.h"

#include "util.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *key;
    char *text;
} LocaleEntry;

struct Locale {
    LocaleEntry *entries; /* sorted by key */
    size_t count;
};

static int entry_cmp(const void *a, const void *b)
{
    return strcmp(((const LocaleEntry *)a)->key, ((const LocaleEntry *)b)->key);
}

Locale *locale_load(const char *json_text, char *err, size_t err_len)
{
    cJSON *root = cJSON_Parse(json_text);
    if (!root || !cJSON_IsObject(root)) {
        if (err && err_len) {
            const char *where = cJSON_GetErrorPtr();
            snprintf(err, err_len, "locale: not a JSON object (near: %.40s)", where ? where : "?");
        }
        cJSON_Delete(root);
        return NULL;
    }
    Locale *loc = calloc(1, sizeof *loc);
    size_t n = (size_t)cJSON_GetArraySize(root);
    if (loc) {
        loc->entries = calloc(n ? n : 1, sizeof *loc->entries);
    }
    if (!loc || !loc->entries) {
        cJSON_Delete(root);
        locale_free(loc);
        return NULL;
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, root)
    {
        if (!cJSON_IsString(item)) {
            if (err && err_len) {
                snprintf(err, err_len, "locale: value of \"%s\" is not a string", item->string);
            }
            cJSON_Delete(root);
            locale_free(loc);
            return NULL;
        }
        loc->entries[loc->count].key = util_strdup(item->string);
        loc->entries[loc->count].text = util_strdup(item->valuestring);
        loc->count++;
    }
    cJSON_Delete(root);
    qsort(loc->entries, loc->count, sizeof *loc->entries, entry_cmp);
    for (size_t i = 1; i < loc->count; i++) {
        if (strcmp(loc->entries[i - 1].key, loc->entries[i].key) == 0) {
            if (err && err_len) {
                snprintf(err, err_len, "locale: duplicate key \"%s\"", loc->entries[i].key);
            }
            locale_free(loc);
            return NULL;
        }
    }
    return loc;
}

void locale_free(Locale *loc)
{
    if (!loc) {
        return;
    }
    for (size_t i = 0; i < loc->count; i++) {
        free(loc->entries[i].key);
        free(loc->entries[i].text);
    }
    free(loc->entries);
    free(loc);
}

const char *locale_find(const Locale *loc, const char *key)
{
    if (!loc || !key) {
        return NULL;
    }
    LocaleEntry probe = {(char *)key, NULL};
    const LocaleEntry *hit = bsearch(&probe, loc->entries, loc->count, sizeof *loc->entries, entry_cmp);
    return hit ? hit->text : NULL;
}

const char *locale_get(const Locale *loc, const char *key)
{
    const char *text = locale_find(loc, key);
    return text ? text : key;
}

const char *locale_get_variant(const Locale *loc, const char *key,
                               const char *side, const char *family)
{
    char buf[256];
    if (side && family) {
        snprintf(buf, sizeof buf, "%s@%s.%s", key, side, family);
        const char *text = locale_find(loc, buf);
        if (text) {
            return text;
        }
    }
    if (side) {
        snprintf(buf, sizeof buf, "%s@%s", key, side);
        const char *text = locale_find(loc, buf);
        if (text) {
            return text;
        }
    }
    return locale_get(loc, key);
}

size_t locale_count(const Locale *loc)
{
    return loc ? loc->count : 0;
}

const char *locale_key_at(const Locale *loc, size_t index)
{
    return (loc && index < loc->count) ? loc->entries[index].key : NULL;
}
