#include "sprites.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SPRITES 384
#define MAX_ROWS 128
#define MAX_COLS 160

static Sprite sprites[MAX_SPRITES];
static int sprite_count;

typedef struct {
    bool set;
    Uint8 r, g, b;
} PalEntry;

/* Splits the next line off *p (handles \n and \r\n); returns NULL at the end. */
static char *next_line(char **p)
{
    if (!*p || !**p) {
        return NULL;
    }
    char *line = *p;
    char *nl = strchr(line, '\n');
    if (nl) {
        *nl = '\0';
        *p = nl + 1;
    } else {
        *p = line + strlen(line);
    }
    size_t n = strlen(line);
    if (n > 0 && line[n - 1] == '\r') {
        line[n - 1] = '\0';
    }
    return line;
}

static void add_sprite(SDL_Renderer *r, const char *name, char rows[][MAX_COLS + 1], int h, const PalEntry *pal,
                       const char *file)
{
    int w = (int)strlen(rows[0]);
    for (int y = 1; y < h; y++) {
        if ((int)strlen(rows[y]) != w) {
            SDL_Log("%s: sprite %s: row %d is %d wide, not %d", file, name, y + 1, (int)strlen(rows[y]), w);
            return;
        }
    }
    if (sprite_count >= MAX_SPRITES || w <= 0) {
        return;
    }
    Uint8 *px = calloc((size_t)w * h, 4);
    if (!px) {
        return;
    }
    bool warned = false;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned char ch = (unsigned char)rows[y][x];
            Uint8 *p = px + ((size_t)y * w + x) * 4;
            if (ch != '.' && pal[ch].set) {
                p[0] = pal[ch].r;
                p[1] = pal[ch].g;
                p[2] = pal[ch].b;
                p[3] = 255;
            } else if (ch != '.' && !warned) {
                SDL_Log("%s: sprite %s: no colour for '%c'", file, name, ch);
                warned = true;
            }
        }
    }
    SDL_Texture *tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (tex) {
        SDL_UpdateTexture(tex, NULL, px, w * 4);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        Sprite *s = &sprites[sprite_count++];
        snprintf(s->name, sizeof s->name, "%s", name);
        s->tex = tex;
        s->w = w;
        s->h = h;
    }
    free(px);
}

static void load_file(SDL_Renderer *r, const char *path)
{
    size_t size = 0;
    char *text = SDL_LoadFile(path, &size);
    if (!text) {
        SDL_Log("Cannot read %s: %s", path, SDL_GetError());
        return;
    }
    static PalEntry pal[256];
    static char rows[MAX_ROWS][MAX_COLS + 1];
    memset(pal, 0, sizeof pal);
    char *p = text, *line;
    enum { TOP, PALETTE, SPRITE } mode = TOP;
    char name[40] = "";
    int h = 0;
    while ((line = next_line(&p)) != NULL) {
        if (line[0] == '#' || (mode != SPRITE && line[0] == '\0')) {
            continue;
        }
        if (strcmp(line, "end") == 0) {
            if (mode == SPRITE && h > 0) {
                add_sprite(r, name, rows, h, pal, path);
            }
            mode = TOP;
            continue;
        }
        switch (mode) {
        case TOP:
            if (strcmp(line, "palette") == 0) {
                mode = PALETTE;
            } else if (strncmp(line, "sprite ", 7) == 0) {
                snprintf(name, sizeof name, "%s", line + 7);
                h = 0;
                mode = SPRITE;
            }
            break;
        case PALETTE: {
            int cr, cg, cb;
            if (line[0] && sscanf(line + 1, "%d %d %d", &cr, &cg, &cb) == 3) {
                PalEntry *e = &pal[(unsigned char)line[0]];
                e->set = true;
                e->r = (Uint8)cr;
                e->g = (Uint8)cg;
                e->b = (Uint8)cb;
            }
            break;
        }
        case SPRITE:
            if (h < MAX_ROWS && strlen(line) <= MAX_COLS) {
                snprintf(rows[h++], MAX_COLS + 1, "%s", line);
            }
            break;
        }
    }
    SDL_free(text);
}

int sprites_load(SDL_Renderer *r, const char *dir)
{
    int count = 0;
    char **files = SDL_GlobDirectory(dir, "*.txt", 0, &count);
    if (!files) {
        SDL_Log("No sprites in %s: %s", dir, SDL_GetError());
        return 0;
    }
    for (int i = 0; i < count; i++) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, files[i]);
        load_file(r, path);
    }
    SDL_free(files);
    return sprite_count;
}

void sprites_free(void)
{
    for (int i = 0; i < sprite_count; i++) {
        SDL_DestroyTexture(sprites[i].tex);
    }
    sprite_count = 0;
}

const Sprite *sprite_find(const char *name)
{
    for (int i = 0; i < sprite_count; i++) {
        if (strcmp(sprites[i].name, name) == 0) {
            return &sprites[i];
        }
    }
    static char missing[16][64];
    static int missing_n;
    for (int i = 0; i < missing_n; i++) {
        if (strcmp(missing[i], name) == 0) {
            return NULL;
        }
    }
    if (missing_n < 16) {
        snprintf(missing[missing_n++], sizeof missing[0], "%s", name);
    }
    SDL_Log("No sprite named %s", name);
    return NULL;
}

void sprite_draw_tinted(SDL_Renderer *r, const Sprite *s, float x, float y, float scale, bool flip, SDL_Color tint)
{
    if (!s) {
        return;
    }
    SDL_SetTextureColorMod(s->tex, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(s->tex, tint.a);
    SDL_FRect dst = {x, y, s->w * scale, s->h * scale};
    SDL_RenderTextureRotated(r, s->tex, NULL, &dst, 0, NULL, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void sprite_draw(SDL_Renderer *r, const Sprite *s, float x, float y, float scale, bool flip)
{
    sprite_draw_tinted(r, s, x, y, scale, flip, (SDL_Color){255, 255, 255, 255});
}

void sprite_stand(SDL_Renderer *r, const char *name, float x, float y, float scale, bool flip, SDL_Color tint)
{
    const Sprite *s = sprite_find(name);
    if (s) {
        sprite_draw_tinted(r, s, x - s->w * scale / 2, y - s->h * scale, scale, flip, tint);
    }
}

static const Sprite *find_portrait(const char *prefix, const char *id)
{
    if (!id || !id[0]) {
        return NULL;
    }
    char name[64];
    snprintf(name, sizeof name, "%s%s", prefix, id);
    return sprite_find(name);
}

bool portrait_exists(const char *prefix, const char *id)
{
    return find_portrait(prefix, id) != NULL;
}

SDL_FPoint portrait_draw(SDL_Renderer *r, const char *prefix, const char *id, float x, float y, float scale,
                         bool faded)
{
    const Sprite *s = find_portrait(prefix, id);
    if (!s) {
        return (SDL_FPoint){0, 0};
    }
    float w = s->w * scale + 2, h = s->h * scale + 2;
    /* a dark wooden frame around a warm ground, like a small painted likeness */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 92, 66, 40, 255);
    SDL_RenderFillRect(r, &(SDL_FRect){x, y, w, h});
    SDL_SetRenderDrawColor(r, faded ? 150 : 196, faded ? 146 : 178, faded ? 136 : 140, 255);
    SDL_RenderFillRect(r, &(SDL_FRect){x + 1, y + 1, w - 2, h - 2});
    SDL_Color tint = faded ? (SDL_Color){150, 146, 140, 255} : (SDL_Color){255, 255, 255, 255};
    sprite_draw_tinted(r, s, x + 1, y + 1, scale, false, tint);
    return (SDL_FPoint){w, h};
}
