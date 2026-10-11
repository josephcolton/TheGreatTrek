/* Drawing text on the logical canvas. */
#ifndef TREK_TEXT_H
#define TREK_TEXT_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

typedef enum { TEXT_LEFT, TEXT_CENTER } TextAlign;

/* Draws text at (x, y), wrapped to wrap_w pixels (0 = no wrap). Returns the height drawn. */
int text_draw(SDL_Renderer *r, TTF_Font *font, const char *text, float x, float y,
              int wrap_w, SDL_Color color, TextAlign align);

#endif
