/* Shared state for the SDL front end. */
#ifndef TREK_APP_H
#define TREK_APP_H

#include "locale.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#define TREK_LOGICAL_W 640
#define TREK_LOGICAL_H 360

typedef struct App {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font_body;
    TTF_Font *font_title;
    Locale *locale;
    char *base_path; /* folder holding data/ and assets/, with a trailing slash */
    bool running;
} App;

bool app_init(App *app, const char *title);
void app_shutdown(App *app);
/* Text for a locale key (shorthand for locale_get). */
const char *app_text(const App *app, const char *key);

#endif
