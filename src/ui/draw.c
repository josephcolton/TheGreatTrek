#include "draw.h"

#include <stdio.h>
#include <string.h>

const SDL_Color COL_BG = {28, 22, 16, 255};
const SDL_Color COL_PANEL = {42, 34, 25, 255};
const SDL_Color COL_BORDER = {112, 90, 60, 255};
const SDL_Color COL_TEXT = {236, 224, 198, 255};
const SDL_Color COL_DIM = {168, 150, 120, 255};
const SDL_Color COL_GOLD = {232, 186, 92, 255};
const SDL_Color COL_RED = {214, 104, 84, 255};
const SDL_Color COL_GREEN = {150, 196, 120, 255};
const SDL_Color COL_LOCKED = {110, 98, 82, 255};
const SDL_Color COL_HILITE_BG = {70, 56, 38, 255};
const SDL_Color COL_SKY = {120, 150, 170, 255};
const SDL_Color COL_GROUND = {150, 128, 82, 255};

/* ---------------------------------------------------------------- text cache */

#define CACHE_SIZE 512

typedef struct {
    TTF_Font *font;
    SDL_Color color;
    char text[200];
    SDL_Texture *tex;
    int w, h;
    unsigned last_used;
} CacheEntry;

static CacheEntry cache[CACHE_SIZE];
static unsigned cache_clock;

static bool same_color(SDL_Color a, SDL_Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static CacheEntry *get_text(SDL_Renderer *r, TTF_Font *f, const char *s, SDL_Color c)
{
    cache_clock++;
    size_t len = strlen(s);
    bool cacheable = len < sizeof cache[0].text;
    int oldest = 0;
    for (int i = 0; i < CACHE_SIZE; i++) {
        CacheEntry *e = &cache[i];
        if (cacheable && e->tex && e->font == f && same_color(e->color, c) && strcmp(e->text, s) == 0) {
            e->last_used = cache_clock;
            return e;
        }
        if (!e->tex || e->last_used < cache[oldest].last_used) {
            oldest = i;
            if (!e->tex) {
                break;
            }
        }
    }
    SDL_Surface *surf = TTF_RenderText_Solid(f, s, len, c);
    if (!surf) {
        return NULL;
    }
    CacheEntry *e = &cache[oldest];
    if (e->tex) {
        SDL_DestroyTexture(e->tex);
    }
    e->tex = SDL_CreateTextureFromSurface(r, surf);
    e->w = surf->w;
    e->h = surf->h;
    SDL_DestroySurface(surf);
    if (!e->tex) {
        return NULL;
    }
    SDL_SetTextureScaleMode(e->tex, SDL_SCALEMODE_NEAREST);
    e->font = f;
    e->color = c;
    snprintf(e->text, sizeof e->text, "%s", cacheable ? s : "");
    e->last_used = cache_clock;
    return e;
}

static int g_scale = 1; /* shared with the measuring helpers */

void draw_init(Draw *d, SDL_Renderer *r, TTF_Font *font, TTF_Font *big, float font_pt, float big_pt)
{
    d->r = r;
    d->font = font;
    d->big = big;
    d->font_pt = font_pt;
    d->big_pt = big_pt;
    d->scale = 0;
    draw_update_scale(d);
}

void draw_update_scale(Draw *d)
{
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(d->r, &w, &h);
    int scale = SDL_min(w / 640, h / 360);
    if (scale < 1) {
        scale = 1;
    }
    if (scale == d->scale) {
        return;
    }
    d->scale = scale;
    g_scale = scale;
    TTF_SetFontSize(d->font, d->font_pt * scale);
    TTF_SetFontSize(d->big, d->big_pt * scale);
    d->line_h = TTF_GetFontLineSkip(d->font) / scale;
    draw_shutdown(); /* drop cached text rendered at the old size */
}

void draw_shutdown(void)
{
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].tex) {
            SDL_DestroyTexture(cache[i].tex);
        }
        cache[i].tex = NULL;
    }
}

/* ---------------------------------------------------------------- shapes */

void draw_rect(Draw *d, SDL_FRect rect, SDL_Color c)
{
    SDL_SetRenderDrawColor(d->r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(d->r, &rect);
}

void draw_frame(Draw *d, SDL_FRect rect, SDL_Color c)
{
    SDL_SetRenderDrawColor(d->r, c.r, c.g, c.b, c.a);
    SDL_RenderRect(d->r, &rect);
}

void draw_panel(Draw *d, SDL_FRect rect)
{
    draw_rect(d, rect, COL_PANEL);
    draw_frame(d, rect, COL_BORDER);
}

/* ---------------------------------------------------------------- text */

int text_width(TTF_Font *f, const char *s)
{
    int w = 0, h = 0;
    if (s && *s) {
        TTF_GetStringSize(f, s, 0, &w, &h);
    }
    return w / g_scale;
}

int draw_text(Draw *d, TTF_Font *f, const char *s, float x, float y, SDL_Color c, TextAlign a)
{
    if (!s || !*s) {
        return 0;
    }
    CacheEntry *e = get_text(d->r, f, s, c);
    if (!e) {
        return 0;
    }
    float w = (float)e->w / d->scale, h = (float)e->h / d->scale;
    SDL_FRect dst = {x, y, w, h};
    if (a == TEXT_CENTER) {
        dst.x = x - w / 2.0f;
    } else if (a == TEXT_RIGHT) {
        dst.x = x - w;
    }
    SDL_RenderTexture(d->r, e->tex, NULL, &dst);
    return (int)w;
}

/* Word wrap. Words are drawn one by one so glossary terms can take their own colour. */
int draw_wrapped(Draw *d, TTF_Font *f, const char *s, float x, float y, int w, SDL_Color c, bool dry_run)
{
    if (!s) {
        return 0;
    }
    int line_h = TTF_GetFontLineSkip(f) / g_scale;
    int space = text_width(f, " ");
    float cx = x, cy = y;
    bool gold = false;
    const char *p = s;
    while (*p) {
        if (*p == '\n') {
            cx = x;
            cy += line_h;
            p++;
            continue;
        }
        if (*p == ' ') {
            p++;
            continue;
        }
        /* one word (may contain [[ ]] markers) */
        char word[160];
        int n = 0;
        bool word_gold = gold;
        while (*p && *p != ' ' && *p != '\n' && n < (int)sizeof word - 1) {
            if (p[0] == '[' && p[1] == '[') {
                gold = true;
                word_gold = true;
                p += 2;
                continue;
            }
            if (p[0] == ']' && p[1] == ']') {
                gold = false;
                p += 2;
                continue;
            }
            word[n++] = *p++;
        }
        word[n] = '\0';
        if (n == 0) {
            continue;
        }
        int ww = text_width(f, word);
        if (cx > x && cx + ww > x + w) {
            cx = x;
            cy += line_h;
        }
        if (!dry_run) {
            draw_text(d, f, word, cx, cy, word_gold ? COL_GOLD : c, TEXT_LEFT);
        }
        cx += ww + space;
    }
    return (int)(cy - y) + line_h;
}

/* ---------------------------------------------------------------- choices */

static int choice_height(Draw *d, const ChoiceItem *it, int w, bool numbered)
{
    int indent = numbered ? 22 : 8;
    int h = draw_wrapped(d, d->font, it->text, 0, 0, w - indent - 6, COL_TEXT, true);
    if (it->locked && it->locked[0]) {
        h += d->line_h; /* room for the reason it is locked */
    }
    return h + 4;
}

int choices_hit(Draw *d, const ChoiceItem *items, int n, float x, float y, int w, float mx, float my, bool numbered)
{
    float cy = y;
    for (int i = 0; i < n; i++) {
        int h = choice_height(d, &items[i], w, numbered);
        if (mx >= x && mx < x + w && my >= cy && my < cy + h) {
            return i;
        }
        cy += h;
    }
    return -1;
}

int draw_choices(Draw *d, const ChoiceItem *items, int n, float x, float y, int w, float mouse_x, float mouse_y,
                 int selected, bool numbered, float *out_bottom)
{
    int hover = choices_hit(d, items, n, x, y, w, mouse_x, mouse_y, numbered);
    float cy = y;
    int indent = numbered ? 22 : 8;
    for (int i = 0; i < n; i++) {
        const ChoiceItem *it = &items[i];
        int h = choice_height(d, it, w, numbered);
        bool hot = (i == hover || i == selected);
        if (hot && !it->locked) {
            draw_rect(d, (SDL_FRect){x, cy, (float)w, (float)h - 2}, COL_HILITE_BG);
        }
        SDL_Color c = it->locked ? COL_LOCKED : (hot ? COL_GOLD : COL_TEXT);
        if (numbered) {
            char num[16];
            snprintf(num, sizeof num, "%d", i + 1);
            draw_text(d, d->font, num, x + 6, cy + 1, it->locked ? COL_LOCKED : COL_GOLD, TEXT_LEFT);
        }
        int th = draw_wrapped(d, d->font, it->text, x + indent, cy + 1, w - indent - 6, c, false);
        if (it->locked && it->locked[0]) {
            draw_text(d, d->font, it->locked, x + indent + 10, cy + 1 + th, COL_LOCKED, TEXT_LEFT);
        }
        cy += h;
    }
    if (out_bottom) {
        *out_bottom = cy;
    }
    return hover;
}
