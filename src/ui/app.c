#include "app.h"

#include "format.h"
#include "util.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_FILE "assets/fonts/VT323-Regular.ttf"
#define FONT_BODY_PT 16.0f
#define FONT_BIG_PT 32.0f

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

/* ---------------------------------------------------------------- settings */

static void load_settings(App *app)
{
    Settings *s = &app->settings;
    s->difficulty = DIFF_NORMAL;
    s->text_speed = 0;
    s->show_odds = false;
    s->fullscreen = false;
    s->travel_speed = 1;
    char *path = util_path_join(app->pref_path, "settings.json");
    char *json = util_read_file(path, NULL);
    free(path);
    cJSON *o = json ? cJSON_Parse(json) : NULL;
    free(json);
    if (!o) {
        return;
    }
    const cJSON *v;
    if (cJSON_IsString(v = cJSON_GetObjectItemCaseSensitive(o, "difficulty"))) {
        for (int d = 0; d < DIFF_COUNT; d++) {
            if (strcmp(v->valuestring, difficulty_id((Difficulty)d)) == 0) {
                s->difficulty = (Difficulty)d;
            }
        }
    }
    s->show_odds = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "show_odds"));
    s->fullscreen = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "fullscreen"));
    if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(o, "travel_speed"))) {
        s->travel_speed = v->valueint < 0 ? 0 : (v->valueint > 2 ? 2 : v->valueint);
    }
    cJSON_Delete(o);
}

void app_save_settings(App *app)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "difficulty", difficulty_id(app->settings.difficulty));
    cJSON_AddBoolToObject(o, "show_odds", app->settings.show_odds);
    cJSON_AddBoolToObject(o, "fullscreen", app->settings.fullscreen);
    cJSON_AddNumberToObject(o, "travel_speed", app->settings.travel_speed);
    char *json = cJSON_Print(o);
    cJSON_Delete(o);
    char *path = util_path_join(app->pref_path, "settings.json");
    util_write_file(path, json, strlen(json));
    free(path);
    free(json);
}

/* ---------------------------------------------------------------- names */

static void read_names(const cJSON *arr, char out[][24], int *count)
{
    const cJSON *n;
    cJSON_ArrayForEach(n, arr)
    {
        if (cJSON_IsString(n) && *count < MAX_NAME_LIST) {
            snprintf(out[(*count)++], 24, "%s", n->valuestring);
        }
    }
}

static void load_names(App *app)
{
    char *path = util_path_join(app->data_dir, "names.json");
    char *json = util_read_file(path, NULL);
    free(path);
    cJSON *o = json ? cJSON_Parse(json) : NULL;
    free(json);
    if (!o) {
        return;
    }
    for (int s = 0; s < SIDE_COUNT; s++) {
        const cJSON *side = cJSON_GetObjectItemCaseSensitive(o, side_id((Side)s));
        read_names(cJSON_GetObjectItemCaseSensitive(side, "men"), app->names.side[s].men, &app->names.side[s].men_count);
        read_names(cJSON_GetObjectItemCaseSensitive(side, "women"), app->names.side[s].women, &app->names.side[s].women_count);
    }
    const cJSON *sv = cJSON_GetObjectItemCaseSensitive(o, "servants");
    read_names(cJSON_GetObjectItemCaseSensitive(sv, "men"), app->names.servants.men, &app->names.servants.men_count);
    read_names(cJSON_GetObjectItemCaseSensitive(sv, "women"), app->names.servants.women, &app->names.servants.women_count);
    cJSON_Delete(o);
}

const char *app_random_name(App *app, Side side, bool servant, Pronoun pronoun)
{
    const NameList *l = servant ? &app->names.servants : &app->names.side[side];
    int n = pronoun == PRONOUN_SHE ? l->women_count : l->men_count;
    if (n == 0) {
        return NULL;
    }
    /* Not part of the simulation, so SDL's random number generator is fine here. */
    int i = SDL_rand(n);
    return pronoun == PRONOUN_SHE ? l->women[i] : l->men[i];
}

/* ---------------------------------------------------------------- init */

bool app_init(App *app)
{
    SDL_zerop(app);
    app->edit_member = -1;
    SDL_SetAppMetadata("The Great Trek", "0.1.0", "org.thegreattrek.game");
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
    char *pref = SDL_GetPrefPath("TheGreatTrek", "TheGreatTrek");
    app->pref_path = util_strdup(pref ? pref : "./");
    SDL_free(pref);
    app->data_dir = util_path_join(app->base_path, "data");

    char err[512] = "";
    int warnings = 0;
    app->data = data_load(app->data_dir, err, sizeof err, &warnings);
    if (!app->data) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The Great Trek", err, NULL);
        return false;
    }
    char *loc_dir = util_path_join(app->data_dir, "locale/en");
    app->loc = locale_load_dir(loc_dir, err, sizeof err);
    free(loc_dir);
    if (!app->loc) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The Great Trek", err, NULL);
        return false;
    }
    load_settings(app);
    load_names(app);

    if (!SDL_CreateWindowAndRenderer(locale_get(app->loc, "app.title"), TREK_LOGICAL_W * 2, TREK_LOGICAL_H * 2,
                                     SDL_WINDOW_RESIZABLE, &app->window, &app->renderer)) {
        SDL_Log("Cannot create window: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(app->renderer, 1);
    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderLogicalPresentation(app->renderer, TREK_LOGICAL_W, TREK_LOGICAL_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    if (app->settings.fullscreen) {
        SDL_SetWindowFullscreen(app->window, true);
    }
    app->font = open_font(app, FONT_FILE, FONT_BODY_PT);
    app->big = open_font(app, FONT_FILE, FONT_BIG_PT);
    if (!app->font || !app->big) {
        return false;
    }
    draw_init(&app->draw, app->renderer, app->font, app->big, FONT_BODY_PT, FONT_BIG_PT);
    trips_scan(app->pref_path, &app->trips);
    app->screen = SCR_TITLE;
    app->running = true;
    return true;
}

void app_close_game(App *app)
{
    game_free(app->game);
    app->game = NULL;
    app->travelling = false;
    app->trip_id[0] = '\0';
    app->overlay = OV_NONE;
}

void app_shutdown(App *app)
{
    app_close_game(app);
    draw_shutdown();
    if (app->font) {
        TTF_CloseFont(app->font);
    }
    if (app->big) {
        TTF_CloseFont(app->big);
    }
    if (app->renderer) {
        SDL_DestroyRenderer(app->renderer);
    }
    if (app->window) {
        SDL_DestroyWindow(app->window);
    }
    locale_free(app->loc);
    data_free(app->data);
    free(app->base_path);
    free(app->pref_path);
    free(app->data_dir);
    TTF_Quit();
    SDL_Quit();
}

/* ---------------------------------------------------------------- text */

const char *app_text(const App *app, const char *key)
{
    if (app->game) {
        return locale_get_variant(app->loc, key, side_id(app->game->side), app->data->families[app->game->family].id);
    }
    return locale_get(app->loc, key);
}

void app_fmt_to(App *app, const LocText *t, char *out, size_t cap)
{
    if (app->game) {
        fmt_game(app->loc, app->game, t, out, cap);
    } else {
        fmt_text(app->loc, t, NULL, NULL, out, cap);
    }
}

const char *app_fmt(App *app, const LocText *t)
{
    /* A small ring of buffers so a few results can be used at once. */
    static char bufs[8][1024];
    static int next;
    char *b = bufs[next];
    next = (next + 1) % 8;
    app_fmt_to(app, t, b, sizeof bufs[0]);
    return b;
}

void app_commit(App *app)
{
    if (app->game && app->trip_id[0]) {
        trips_write(app->pref_path, app->trip_id, app->game);
    }
}

void app_set_message(App *app, const char *text)
{
    snprintf(app->message, sizeof app->message, "%s", text);
    app->message_timer = 3.0;
}

void app_switch(App *app, ScreenId s)
{
    app->screen = s;
    app->sel = 0;
    app->scroll = 0;
    app->overlay = OV_NONE;
}
