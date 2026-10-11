#include "app.h"

#include "util.h"

#include <stdlib.h>

static TTF_Font *open_font(const App *app, const char *name, float size)
{
    char *path = util_path_join(app->base_path, name);
    TTF_Font *font = TTF_OpenFont(path, size);
    if (!font) {
        SDL_Log("Cannot open font %s: %s", path, SDL_GetError());
    }
    free(path);
    return font;
}

static Locale *load_locale(const App *app, const char *name)
{
    char *path = util_path_join(app->base_path, name);
    char *json = util_read_file(path, NULL);
    Locale *loc = NULL;
    if (!json) {
        SDL_Log("Cannot read %s", path);
    } else {
        char err[256] = "";
        loc = locale_load(json, err, sizeof err);
        if (!loc) {
            SDL_Log("%s: %s", path, err);
        }
        free(json);
    }
    free(path);
    return loc;
}

bool app_init(App *app, const char *title)
{
    SDL_zerop(app);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    if (!TTF_Init()) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        return false;
    }
    const char *base = SDL_GetBasePath();
    app->base_path = util_strdup(base ? base : "./");

    app->locale = load_locale(app, "data/locale/en.json");
    if (!app->locale) {
        return false;
    }
    if (!SDL_CreateWindowAndRenderer(title ? title : locale_get(app->locale, "app.title"),
                                     TREK_LOGICAL_W * 2, TREK_LOGICAL_H * 2,
                                     SDL_WINDOW_RESIZABLE, &app->window, &app->renderer)) {
        SDL_Log("Cannot create window: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(app->renderer, 1);
    SDL_SetRenderLogicalPresentation(app->renderer, TREK_LOGICAL_W, TREK_LOGICAL_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    app->font_body = open_font(app, "assets/fonts/VT323-Regular.ttf", 16.0f);
    app->font_title = open_font(app, "assets/fonts/VT323-Regular.ttf", 32.0f);
    if (!app->font_body || !app->font_title) {
        return false;
    }
    app->running = true;
    return true;
}

void app_shutdown(App *app)
{
    if (app->font_body) {
        TTF_CloseFont(app->font_body);
    }
    if (app->font_title) {
        TTF_CloseFont(app->font_title);
    }
    if (app->renderer) {
        SDL_DestroyRenderer(app->renderer);
    }
    if (app->window) {
        SDL_DestroyWindow(app->window);
    }
    locale_free(app->locale);
    free(app->base_path);
    TTF_Quit();
    SDL_Quit();
}

const char *app_text(const App *app, const char *key)
{
    return locale_get(app->locale, key);
}
