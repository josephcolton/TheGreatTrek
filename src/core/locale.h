/* Player-facing text, looked up by key. A language is a folder data/locale/<lang>/
 * with index.json ({"files": [...]}) naming flat JSON files: { "key": "text", ... }. */
#ifndef TREK_LOCALE_H
#define TREK_LOCALE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Locale Locale;

/* Parses one locale JSON object. On failure returns NULL and writes a message to err. */
Locale     *locale_load(const char *json_text, char *err, size_t err_len);
/* Loads every file listed in dir/index.json. */
Locale     *locale_load_dir(const char *dir, char *err, size_t err_len);
Locale     *locale_new(void);
/* Adds the keys from one JSON object; fails on duplicates. */
bool        locale_add(Locale *loc, const char *json_text, const char *name, char *err, size_t err_len);
void        locale_free(Locale *loc);

/* Text for key, or NULL if the key is missing. */
const char *locale_find(const Locale *loc, const char *key);
/* Text for key; a missing key returns the key itself, so gaps show up on screen. */
const char *locale_get(const Locale *loc, const char *key);
/* Side- and family-specific text: tries "key@side.family", then "key@side", then "key".
 * side and family may be NULL. */
const char *locale_get_variant(const Locale *loc, const char *key,
                               const char *side, const char *family);

size_t      locale_count(const Locale *loc);
const char *locale_key_at(const Locale *loc, size_t index);

#endif
