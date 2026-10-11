/* Drawing on the 640x360 logical canvas: colours, cached text, wrapping, panels, choice lists. */
#ifndef TREK_DRAW_H
#define TREK_DRAW_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>

typedef enum { TEXT_LEFT, TEXT_CENTER, TEXT_RIGHT } TextAlign;

/* Palette: dark parchment and lamp light. */
extern const SDL_Color COL_BG, COL_PANEL, COL_BORDER, COL_TEXT, COL_DIM, COL_GOLD, COL_RED, COL_GREEN,
    COL_LOCKED, COL_HILITE_BG, COL_SKY, COL_GROUND;

typedef struct {
    SDL_Renderer *r;
    TTF_Font *font;      /* body text */
    TTF_Font *big;       /* titles */
    TTF_Font *small;     /* map labels */
    float font_pt, big_pt, small_pt;
    int scale;           /* window pixels per logical pixel; text is rendered at this resolution */
    int line_h;          /* body line height, logical pixels */
} Draw;

void draw_init(Draw *d, SDL_Renderer *r, TTF_Font *font, TTF_Font *big, TTF_Font *small, float font_pt, float big_pt,
               float small_pt);
/* Call when the window size changes: re-renders text at the new resolution. */
void draw_update_scale(Draw *d);
void draw_shutdown(void);

void draw_rect(Draw *d, SDL_FRect rect, SDL_Color c);
void draw_frame(Draw *d, SDL_FRect rect, SDL_Color c);
/* A filled panel with a border. */
void draw_panel(Draw *d, SDL_FRect rect);

/* One line of text, no wrapping. Returns the width drawn. */
int  draw_text(Draw *d, TTF_Font *f, const char *s, float x, float y, SDL_Color c, TextAlign a);
int  text_width(TTF_Font *f, const char *s);
/* Wrapped text in a box of width w. [[word]] is drawn in gold (glossary terms).
 * Returns the height used. If dry_run, only measures. */
int  draw_wrapped(Draw *d, TTF_Font *f, const char *s, float x, float y, int w, SDL_Color c, bool dry_run);

/* A vertical list of numbered choices. Returns the index under the mouse, or -1. */
typedef struct {
    const char *text;
    const char *locked;   /* NULL if enabled; "" for greyed out with no reason shown */
} ChoiceItem;

int  draw_choices(Draw *d, const ChoiceItem *items, int n, float x, float y, int w, float mouse_x, float mouse_y,
                  int selected, bool numbered, float *out_bottom);
/* Hit test for draw_choices' layout without drawing. */
int  choices_hit(Draw *d, const ChoiceItem *items, int n, float x, float y, int w, float mx, float my, bool numbered);

#endif
