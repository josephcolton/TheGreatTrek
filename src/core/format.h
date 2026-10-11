/* Turning a LocText (key + arguments) into a display string.
 * Placeholders: {name}; plural choice {name:one|many}; money args format by side. */
#ifndef TREK_FORMAT_H
#define TREK_FORMAT_H

#include "game.h"
#include "locale.h"

#include <stddef.h>

/* side_id_str and family may be NULL; they pick key@side.family variants. */
void fmt_text(const Locale *loc, const LocText *t, const char *side, const char *family, char *out, size_t cap);
/* Formats a raw template string with t's arguments (used for nested keys). */
void fmt_template(const Locale *loc, const char *tmpl, const LocText *t, const char *side, const char *family,
                  char *out, size_t cap);
/* "45 Rds" or "£3 8s". amount is in the side's unit. */
void fmt_money(const Locale *loc, Side side, long long amount, char *out, size_t cap);
/* "Monday 3 April 1837". */
void fmt_date(const Locale *loc, int day, char *out, size_t cap);
/* Convenience for the current game: side and family taken from g. */
void fmt_game(const Locale *loc, const Game *g, const LocText *t, char *out, size_t cap);

#endif
