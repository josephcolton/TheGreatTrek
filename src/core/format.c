#include "format.h"

#include "calendar.h"

#include <stdio.h>
#include <string.h>

static void append(char *out, size_t cap, size_t *len, const char *s, size_t n)
{
    if (*len + 1 >= cap) {
        return;
    }
    if (n > cap - 1 - *len) {
        n = cap - 1 - *len;
    }
    memcpy(out + *len, s, n);
    *len += n;
    out[*len] = '\0';
}

void fmt_money(const Locale *loc, Side side, long long amount, char *out, size_t cap)
{
    LocText t;
    loctext_set(&t, "");
    if (side == SIDE_DUTCH) {
        loctext_arg_int(&t, "n", amount);
        fmt_template(loc, locale_get(loc, "money.rds"), &t, NULL, NULL, out, cap);
        return;
    }
    long long pounds = amount / 20, shillings = amount % 20;
    loctext_arg_int(&t, "l", pounds);
    loctext_arg_int(&t, "s", shillings);
    const char *key = pounds > 0 ? (shillings > 0 ? "money.pounds_shillings" : "money.pounds") : "money.shillings";
    fmt_template(loc, locale_get(loc, key), &t, NULL, NULL, out, cap);
}

/* {who.his}: the pronoun of a person argument, from the locale (pronoun.she.his = "her"), so a
 * translation can supply its own. A capital first letter ({who.His}) capitalises the word. Plain
 * names from older saves have no pronoun and use pronoun.they. */
static void pronoun_word(const Locale *loc, const TextArg *a, const char *form, size_t form_len, char *out,
                         size_t cap)
{
    static const char *IDS[] = {"he", "she"};
    const char *who = (a->kind == ARG_PERSON && a->i >= 0 && a->i < 2) ? IDS[a->i] : "they";
    char key[KEY_LEN];
    snprintf(key, sizeof key, "pronoun.%s.%.*s", who, (int)form_len, form);
    if (form_len > 0 && form[0] >= 'A' && form[0] <= 'Z') {
        char *k = key + strlen("pronoun.") + strlen(who) + 1;
        *k = (char)(*k - 'A' + 'a');
    }
    snprintf(out, cap, "%s", locale_get(loc, key));
    if (form_len > 0 && form[0] >= 'A' && form[0] <= 'Z' && out[0] >= 'a' && out[0] <= 'z') {
        out[0] = (char)(out[0] - 'a' + 'A');
    }
}

static const TextArg *find_arg(const LocText *t, const char *name, size_t len)
{
    for (int i = 0; t && i < t->n_args; i++) {
        if (strlen(t->args[i].name) == len && strncmp(t->args[i].name, name, len) == 0) {
            return &t->args[i];
        }
    }
    return NULL;
}

void fmt_template(const Locale *loc, const char *tmpl, const LocText *t, const char *side, const char *family,
                  char *out, size_t cap)
{
    size_t len = 0;
    if (cap == 0) {
        return;
    }
    out[0] = '\0';
    Side s = (side && strcmp(side, "english") == 0) ? SIDE_ENGLISH : SIDE_DUTCH;
    for (const char *p = tmpl; p && *p;) {
        if (*p != '{') {
            const char *next = strchr(p, '{');
            size_t n = next ? (size_t)(next - p) : strlen(p);
            append(out, cap, &len, p, n);
            p += n;
            continue;
        }
        const char *close = strchr(p, '}');
        if (!close) {
            append(out, cap, &len, p, strlen(p));
            break;
        }
        const char *name = p + 1;
        const char *colon = memchr(name, ':', (size_t)(close - name));
        const char *dot = colon ? NULL : memchr(name, '.', (size_t)(close - name));
        size_t name_len = (size_t)((colon ? colon : dot ? dot : close) - name);
        const TextArg *a = find_arg(t, name, name_len);
        char buf[160];
        buf[0] = '\0';
        if (!a) {
            append(out, cap, &len, p, (size_t)(close - p + 1)); /* leave unknown placeholders visible */
        } else if (dot) {
            /* {who.his} */
            pronoun_word(loc, a, dot + 1, (size_t)(close - dot - 1), buf, sizeof buf);
            append(out, cap, &len, buf, strlen(buf));
        } else if (colon) {
            /* {n:one|many} */
            const char *bar = memchr(colon + 1, '|', (size_t)(close - colon - 1));
            bool one = a->i == 1;
            if (bar) {
                const char *from = one ? colon + 1 : bar + 1;
                const char *to = one ? bar : close;
                append(out, cap, &len, from, (size_t)(to - from));
            }
        } else {
            switch (a->kind) {
            case ARG_INT: snprintf(buf, sizeof buf, "%lld", a->i); break;
            case ARG_STR:
            case ARG_PERSON: snprintf(buf, sizeof buf, "%s", a->s); break;
            case ARG_KEY: {
                const char *txt = locale_find(loc, a->s) ? locale_get_variant(loc, a->s, side, family) : NULL;
                size_t kl = strlen(a->s);
                if (!txt && kl > 4 && strcmp(a->s + kl - 4, ".one") == 0) {
                    /* "item.x.one" missing: fall back to the plural "item.x" */
                    char base[KEY_LEN];
                    snprintf(base, sizeof base, "%.*s", (int)(kl - 4), a->s);
                    txt = locale_get_variant(loc, base, side, family);
                }
                snprintf(buf, sizeof buf, "%s", txt ? txt : locale_get_variant(loc, a->s, side, family));
                break;
            }
            case ARG_MONEY: fmt_money(loc, s, a->i, buf, sizeof buf); break;
            case ARG_DATE: fmt_date(loc, (int)a->i, buf, sizeof buf); break;
            }
            append(out, cap, &len, buf, strlen(buf));
        }
        p = close + 1;
    }
}

void fmt_text(const Locale *loc, const LocText *t, const char *side, const char *family, char *out, size_t cap)
{
    const char *tmpl = locale_get_variant(loc, t->key, side, family);
    fmt_template(loc, tmpl, t, side, family, out, cap);
}

void fmt_date(const Locale *loc, int day, char *out, size_t cap)
{
    int y, m, d;
    cal_date(day, &y, &m, &d);
    char wd_key[24], mo_key[24];
    snprintf(wd_key, sizeof wd_key, "weekday.%d", cal_weekday(day));
    snprintf(mo_key, sizeof mo_key, "month.%d", m);
    LocText t;
    loctext_set(&t, "date.format");
    loctext_arg_key(&t, "wd", wd_key);
    loctext_arg_int(&t, "d", d);
    loctext_arg_key(&t, "month", mo_key);
    loctext_arg_int(&t, "y", y);
    fmt_text(loc, &t, NULL, NULL, out, cap);
}

void fmt_game(const Locale *loc, const Game *g, const LocText *t, char *out, size_t cap)
{
    fmt_text(loc, t, side_id(g->side), g->data->families[g->family].id, out, cap);
}
