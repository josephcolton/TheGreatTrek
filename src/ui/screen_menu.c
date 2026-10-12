/* Main menu, trip list, settings, credits. */
#include "format.h"
#include "screens.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------- input helpers */

int key_digit(const SDL_Event *e)
{
    if (e->type != SDL_EVENT_KEY_DOWN) {
        return -1;
    }
    SDL_Keycode k = e->key.key;
    if (k >= SDLK_1 && k <= SDLK_9) {
        return (int)(k - SDLK_1);
    }
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) {
        return (int)(k - SDLK_KP_1);
    }
    return -1;
}

bool key_is(const SDL_Event *e, SDL_Keycode k)
{
    return e->type == SDL_EVENT_KEY_DOWN && e->key.key == k;
}

bool is_confirm(const SDL_Event *e)
{
    return key_is(e, SDLK_RETURN) || key_is(e, SDLK_KP_ENTER) || key_is(e, SDLK_SPACE);
}

bool is_back(const SDL_Event *e)
{
    return key_is(e, SDLK_ESCAPE);
}

bool is_click(const SDL_Event *e)
{
    return e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_LEFT;
}

bool nav_updown(App *app, const SDL_Event *e, int n)
{
    if (n <= 0) {
        return false;
    }
    int step = key_is(e, SDLK_UP) ? n - 1 : key_is(e, SDLK_DOWN) ? 1 : 0;
    if (step == 0) {
        return false;
    }
    /* skip greyed-out items, as the list was last drawn (draw_choices) */
    const Draw *d = &app->draw;
    bool known = d->list_n == n && n <= 32;
    int sel = app->sel;
    for (int k = 0; k < n; k++) {
        sel = (sel + step) % n;
        if (!known || !(d->list_locked & (1u << sel))) {
            app->sel = sel;
            break;
        }
    }
    return true;
}

/* ---------------------------------------------------------------- shared drawing */

void draw_header(App *app, const char *left, const char *right)
{
    Draw *d = &app->draw;
    draw_rect(d, (SDL_FRect){0, 0, TREK_LOGICAL_W, 18}, COL_PANEL);
    draw_rect(d, (SDL_FRect){0, 18, TREK_LOGICAL_W, 1}, COL_BORDER);
    if (left) {
        draw_text(d, d->font, left, 8, 1, COL_GOLD, TEXT_LEFT);
    }
    if (right) {
        draw_text(d, d->font, right, TREK_LOGICAL_W - 8, 1, COL_DIM, TEXT_RIGHT);
    }
}

void draw_footer(App *app, const char *text)
{
    Draw *d = &app->draw;
    draw_rect(d, (SDL_FRect){0, TREK_LOGICAL_H - 18, TREK_LOGICAL_W, 18}, COL_PANEL);
    draw_rect(d, (SDL_FRect){0, TREK_LOGICAL_H - 19, TREK_LOGICAL_W, 1}, COL_BORDER);
    if (text) {
        draw_text(d, d->font, text, TREK_LOGICAL_W / 2.0f, TREK_LOGICAL_H - 17, COL_DIM, TEXT_CENTER);
    }
}

/* ---------------------------------------------------------------- title */

typedef enum { TM_CONTINUE, TM_NEW, TM_DIFFICULTY, TM_SETTINGS, TM_CREDITS, TM_QUIT, TM_COUNT } TitleItem;

static int unfinished_trips(const App *app)
{
    int n = 0;
    for (int i = 0; i < app->trips.count; i++) {
        if (app->trips.trips[i].ending == 0) {
            n++;
        }
    }
    return n;
}

static void title_items(App *app, ChoiceItem *items, char *diff_buf, size_t cap)
{
    char key[48];
    snprintf(key, sizeof key, "difficulty.%s", difficulty_id(app->settings.difficulty));
    snprintf(diff_buf, cap, "%s %s", app_text(app, "menu.difficulty"), app_text(app, key));
    items[TM_CONTINUE] = (ChoiceItem){app_text(app, "menu.continue"), app->trips.count == 0 ? app_text(app, "menu.no_trips") : NULL};
    items[TM_NEW] = (ChoiceItem){app_text(app, "menu.new"), NULL};
    items[TM_DIFFICULTY] = (ChoiceItem){diff_buf, NULL};
    items[TM_SETTINGS] = (ChoiceItem){app_text(app, "menu.settings"), NULL};
    items[TM_CREDITS] = (ChoiceItem){app_text(app, "menu.credits"), NULL};
    items[TM_QUIT] = (ChoiceItem){app_text(app, "menu.quit"), NULL};
}

#define TITLE_MENU_X 220.0f
#define TITLE_MENU_Y 150.0f
#define TITLE_MENU_W 200

static void title_activate(App *app, int item)
{
    switch (item) {
    case TM_CONTINUE:
        if (app->trips.count == 0) {
            break;
        }
        if (unfinished_trips(app) == 1 && app->trips.count == 1) {
            open_trip(app, app->trips.trips[0].id);
        } else {
            app_switch(app, SCR_TRIPS);
        }
        break;
    case TM_NEW: app_switch(app, SCR_SIDE); break;
    case TM_DIFFICULTY:
        app->settings.difficulty = (Difficulty)((app->settings.difficulty + 1) % DIFF_COUNT);
        app_save_settings(app);
        break;
    case TM_SETTINGS: app_switch(app, SCR_SETTINGS); break;
    case TM_CREDITS: app_switch(app, SCR_CREDITS); break;
    case TM_QUIT: app->running = false; break;
    default: break;
    }
}

void title_event(App *app, const SDL_Event *e)
{
    ChoiceItem items[TM_COUNT];
    char diff[96];
    title_items(app, items, diff, sizeof diff);
    nav_updown(app, e, TM_COUNT);
    int digit = key_digit(e);
    if (digit >= 0 && digit < TM_COUNT) {
        app->sel = digit;
        title_activate(app, digit);
    } else if (is_confirm(e)) {
        title_activate(app, app->sel);
    } else if (is_back(e)) {
        app->running = false;
    } else if (is_click(e)) {
        int hit = choices_hit(&app->draw, items, TM_COUNT, TITLE_MENU_X, TITLE_MENU_Y, TITLE_MENU_W, app->mx, app->my, true);
        if (hit >= 0) {
            app->sel = hit;
            title_activate(app, hit);
        }
    }
}

void title_render(App *app)
{
    Draw *d = &app->draw;
    float cx = TREK_LOGICAL_W / 2.0f;
    /* a simple horizon: sky, veld and a far ridge */
    draw_rect(d, (SDL_FRect){0, 0, TREK_LOGICAL_W, 110}, (SDL_Color){52, 44, 40, 255});
    draw_rect(d, (SDL_FRect){0, 95, TREK_LOGICAL_W, 15}, (SDL_Color){70, 58, 44, 255});
    draw_text(d, d->big, app_text(app, "app.title"), cx, 30, COL_TEXT, TEXT_CENTER);
    draw_text(d, d->font, app_text(app, "app.subtitle"), cx, 66, COL_GOLD, TEXT_CENTER);
    ChoiceItem items[TM_COUNT];
    char diff[96];
    title_items(app, items, diff, sizeof diff);
    draw_choices(d, items, TM_COUNT, TITLE_MENU_X, TITLE_MENU_Y, TITLE_MENU_W, app->mx, app->my, app->sel, true, NULL);
    char key[48];
    snprintf(key, sizeof key, "difficulty.%s.about", difficulty_id(app->settings.difficulty));
    draw_wrapped(d, d->font, app_text(app, key), 120, 300, 400, COL_DIM, false);
    if (app->message_timer > 0) {
        draw_text(d, d->font, app->message, cx, 128, COL_RED, TEXT_CENTER);
    }
}

/* ---------------------------------------------------------------- trip list */

static void trip_line(App *app, const TripInfo *t, char *out, size_t cap)
{
    char fam_key[64], diff_key[48], place_key[64], date[64];
    snprintf(fam_key, sizeof fam_key, "family.%s", t->family_id);
    snprintf(diff_key, sizeof diff_key, "difficulty.%s", t->difficulty);
    snprintf(place_key, sizeof place_key, "place.%s", t->node);
    fmt_date(app->loc, t->day, date, sizeof date);
    LocText lt;
    loctext_set(&lt, t->ending ? "trips.row_finished" : "trips.row");
    loctext_arg_key(&lt, "family", fam_key);
    loctext_arg_key(&lt, "difficulty", diff_key);
    loctext_arg_key(&lt, "place", place_key);
    loctext_arg_str(&lt, "date", date);
    loctext_arg_int(&lt, "alive", t->alive);
    loctext_arg_int(&lt, "party", t->party);
    app_fmt_to(app, &lt, out, cap);
}

#define TRIPS_X 20.0f
#define TRIPS_Y 50.0f
#define TRIPS_W 600

void trips_event(App *app, const SDL_Event *e)
{
    int n = app->trips.count;
    if (app->overlay == OV_CONFIRM_DELETE) {
        if (key_is(e, SDLK_Y)) {
            trips_delete(app->pref_path, app->trips.trips[app->sel].id);
            trips_scan(app->pref_path, &app->trips);
            if (app->sel >= app->trips.count) {
                app->sel = app->trips.count > 0 ? app->trips.count - 1 : 0;
            }
            app->overlay = OV_NONE;
        } else if (key_is(e, SDLK_N) || is_back(e)) {
            app->overlay = OV_NONE;
        }
        return;
    }
    nav_updown(app, e, n);
    if (is_back(e)) {
        app_switch(app, SCR_TITLE);
        return;
    }
    if (n > 0 && (key_is(e, SDLK_DELETE) || key_is(e, SDLK_D)) && app->trips.trips[app->sel].ending != 0) {
        app->overlay = OV_CONFIRM_DELETE; /* only finished trips can be deleted */
        return;
    }
    if (n > 0 && is_confirm(e)) {
        open_trip(app, app->trips.trips[app->sel].id);
        return;
    }
    if (is_click(e)) {
        int row = (int)((app->my - TRIPS_Y) / (app->draw.line_h * 2));
        if (app->my >= TRIPS_Y && row >= 0 && row < n) {
            app->sel = row;
            open_trip(app, app->trips.trips[row].id);
        }
    }
}

void trips_render(App *app)
{
    Draw *d = &app->draw;
    draw_header(app, app_text(app, "trips.title"), NULL);
    draw_text(d, d->font, app_text(app, "trips.about"), TRIPS_X, 26, COL_DIM, TEXT_LEFT);
    float y = TRIPS_Y;
    for (int i = 0; i < app->trips.count; i++) {
        const TripInfo *t = &app->trips.trips[i];
        char line[300];
        trip_line(app, t, line, sizeof line);
        bool hot = i == app->sel;
        if (hot) {
            draw_rect(d, (SDL_FRect){TRIPS_X - 4, y, TRIPS_W + 8, (float)d->line_h * 2 - 2}, COL_HILITE_BG);
        }
        draw_wrapped(d, d->font, line, TRIPS_X, y, TRIPS_W, t->ending ? COL_DIM : (hot ? COL_GOLD : COL_TEXT), false);
        y += d->line_h * 2;
    }
    draw_footer(app, app_text(app, app->overlay == OV_CONFIRM_DELETE ? "trips.confirm_delete" : "trips.footer"));
}

/* ---------------------------------------------------------------- settings */

#include "sound.h"

typedef enum { ST_ODDS, ST_FULLSCREEN, ST_SPEED, ST_SOUND, ST_BACK, ST_COUNT } SettingItem;

static void settings_items(App *app, ChoiceItem *items, char bufs[ST_COUNT][128])
{
    snprintf(bufs[ST_ODDS], 128, "%s %s", app_text(app, "settings.odds"),
             app_text(app, app->settings.show_odds ? "settings.on" : "settings.off"));
    snprintf(bufs[ST_FULLSCREEN], 128, "%s %s", app_text(app, "settings.fullscreen"),
             app_text(app, app->settings.fullscreen ? "settings.on" : "settings.off"));
    char key[48];
    snprintf(key, sizeof key, "settings.speed.%d", app->settings.travel_speed);
    snprintf(bufs[ST_SPEED], 128, "%s %s", app_text(app, "settings.speed"), app_text(app, key));
    snprintf(bufs[ST_SOUND], 128, "%s %s", app_text(app, "settings.sound"),
             app_text(app, app->settings.sound ? "settings.on" : "settings.off"));
    snprintf(bufs[ST_BACK], 128, "%s", app_text(app, "settings.back"));
    for (int i = 0; i < ST_COUNT; i++) {
        items[i] = (ChoiceItem){bufs[i], NULL};
    }
}

static void settings_activate(App *app, int item)
{
    switch (item) {
    case ST_ODDS: app->settings.show_odds = !app->settings.show_odds; break;
    case ST_FULLSCREEN:
        app->settings.fullscreen = !app->settings.fullscreen;
        SDL_SetWindowFullscreen(app->window, app->settings.fullscreen);
        break;
    case ST_SPEED: app->settings.travel_speed = (app->settings.travel_speed + 1) % 3; break;
    case ST_SOUND:
        app->settings.sound = !app->settings.sound;
        sound_set_enabled(app->settings.sound);
        sound_play(SFX_CLICK);
        break;
    case ST_BACK:
        app_switch(app, app->game ? SCR_GAME : SCR_TITLE);
        break;
    default: break;
    }
    app_save_settings(app);
}

void settings_event(App *app, const SDL_Event *e)
{
    ChoiceItem items[ST_COUNT];
    char bufs[ST_COUNT][128];
    settings_items(app, items, bufs);
    nav_updown(app, e, ST_COUNT);
    int digit = key_digit(e);
    if (digit >= 0 && digit < ST_COUNT) {
        app->sel = digit;
        settings_activate(app, digit);
    } else if (is_confirm(e)) {
        settings_activate(app, app->sel);
    } else if (is_back(e)) {
        settings_activate(app, ST_BACK);
    } else if (is_click(e)) {
        int hit = choices_hit(&app->draw, items, ST_COUNT, 160, 70, 320, app->mx, app->my, true);
        if (hit >= 0) {
            app->sel = hit;
            settings_activate(app, hit);
        }
    }
}

void settings_render(App *app)
{
    draw_header(app, app_text(app, "settings.title"), NULL);
    ChoiceItem items[ST_COUNT];
    char bufs[ST_COUNT][128];
    settings_items(app, items, bufs);
    float bottom = 0;
    draw_choices(&app->draw, items, ST_COUNT, 160, 70, 320, app->mx, app->my, app->sel, true, &bottom);
    draw_wrapped(&app->draw, app->font, app_text(app, "settings.odds.about"), 160, bottom + 20, 320, COL_DIM, false);
    draw_footer(app, app_text(app, "footer.back"));
}

/* ---------------------------------------------------------------- credits */

void credits_event(App *app, const SDL_Event *e)
{
    if (is_back(e) || is_confirm(e) || is_click(e)) {
        app_switch(app, SCR_TITLE);
    }
}

void credits_render(App *app)
{
    draw_header(app, app_text(app, "credits.title"), NULL);
    draw_wrapped(&app->draw, app->font, app_text(app, "credits.body"), 30, 30, 580, COL_TEXT, false);
    draw_footer(app, app_text(app, "footer.back"));
}
