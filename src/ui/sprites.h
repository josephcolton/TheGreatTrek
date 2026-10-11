/* Pixel-art sprites, loaded from text files in assets/sprites/: a palette of one character per
 * colour, then each sprite as rows of characters ('.' is transparent). See any of the files. */
#ifndef TREK_SPRITES_H
#define TREK_SPRITES_H

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct {
    char name[40];
    SDL_Texture *tex;
    int w, h;
} Sprite;

/* Loads every *.txt file in dir. Returns the number of sprites loaded. */
int sprites_load(SDL_Renderer *r, const char *dir);
void sprites_free(void);
/* NULL if there is no sprite of that name (a warning is logged once). */
const Sprite *sprite_find(const char *name);
/* Draws with the top-left corner at (x, y), scaled, optionally mirrored left to right. */
void sprite_draw(SDL_Renderer *r, const Sprite *s, float x, float y, float scale, bool flip);
/* The same, with every colour multiplied by tint (for dusk and night). */
void sprite_draw_tinted(SDL_Renderer *r, const Sprite *s, float x, float y, float scale, bool flip, SDL_Color tint);
/* Draws by name with the bottom middle at (x, y): the usual way to stand a thing on the ground. */
void sprite_stand(SDL_Renderer *r, const char *name, float x, float y, float scale, bool flip, SDL_Color tint);

/* A portrait in a frame: prefix is "portrait_" (40x48) or "face_" (24x26), id is the person's
 * portrait id. faded draws it greyed (for the dead and the gone). Returns the framed size, or
 * {0, 0} if there is no such sprite. */
SDL_FPoint portrait_draw(SDL_Renderer *r, const char *prefix, const char *id, float x, float y, float scale,
                         bool faded);
bool portrait_exists(const char *prefix, const char *id);

#endif
