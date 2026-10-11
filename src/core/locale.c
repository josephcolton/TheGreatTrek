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
    size_t cap;
};

static int entry_cmp(const void *a, const void *b)
{
    return strcmp(((const LocaleEntry *)a)->key, ((const LocaleEntry *)b)->key);
}

Locale *locale_new(void)
{
    return calloc(1, sizeof(Locale));
}

bool locale_add(Locale *loc, const char *json_text, const char *name, char *err, size_t err_len)
{
    if (!name) {
        name = "locale";
    }
    cJSON *root = cJSON_Parse(json_text);
    if (!root || !cJSON_IsObject(root)) {
        if (err && err_len) {
            const char *where = cJSON_GetErrorPtr();
            snprintf(err, err_len, "%s: not a JSON object (near: %.40s)", name, where ? where : "?");
        }
        cJSON_Delete(root);
        return false;
    }
    size_t n = (size_t)cJSON_GetArraySize(root);
    if (loc->count + n > loc->cap) {
        size_t cap = (loc->count + n) * 2 + 16;
        LocaleEntry *grown = realloc(loc->entries, cap * sizeof *grown);
        if (!grown) {
            cJSON_Delete(root);
            return false;
        }
        loc->entries = grown;
        loc->cap = cap;
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, root)
    {
        if (item->string && item->string[0] == '_') {
            continue; /* "_about" and other notes for translators */
        }
        if (!cJSON_IsString(item)) {
            if (err && err_len) {
                snprintf(err, err_len, "%s: value of \"%s\" is not a string", name, item->string);
            }
            cJSON_Delete(root);
            return false;
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
                snprintf(err, err_len, "%s: duplicate key \"%s\"", name, loc->entries[i].key);
            }
            return false;
        }
    }
    return true;
}

Locale *locale_load(const char *json_text, char *err, size_t err_len)
{
    Locale *loc = locale_new();
    if (!loc) {
        return NULL;
    }
    if (!locale_add(loc, json_text, "locale", err, err_len)) {
        locale_free(loc);
        return NULL;
    }
    return loc;
}

Locale *locale_load_dir(const char *dir, char *err, size_t err_len)
{
    char *index_path = util_path_join(dir, "index.json");
    char *index_text = util_read_file(index_path, NULL);
    free(index_path);
    if (!index_text) {
        if (err && err_len) {
            snprintf(err, err_len, "cannot read %s/index.json", dir);
        }
        return NULL;
    }
    cJSON *index = cJSON_Parse(index_text);
    free(index_text);
    Locale *loc = locale_new();
    bool ok = index && loc;
    if (!index && err && err_len) {
        snprintf(err, err_len, "%s/index.json is not valid JSON", dir);
    }
    const cJSON *f;
    cJSON_ArrayForEach(f, cJSON_GetObjectItemCaseSensitive(index, "files"))
    {
        if (!ok || !cJSON_IsString(f)) {
            continue;
        }
        char *path = util_path_join(dir, f->valuestring);
        char *text = util_read_file(path, NULL);
        if (!text) {
            if (err && err_len) {
                snprintf(err, err_len, "cannot read %s", path);
            }
            ok = false;
        } else {
            ok = locale_add(loc, text, f->valuestring, err, err_len);
        }
        free(text);
        free(path);
    }
    cJSON_Delete(index);
    if (!ok) {
        locale_free(loc);
        return NULL;
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
    if (!loc || !key || loc->count == 0) {
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
