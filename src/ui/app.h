/* Shared state for the SDL front end. */
#ifndef TREK_APP_H
#define TREK_APP_H

#include "data.h"
#include "draw.h"
#include "game.h"
#include "locale.h"
#include "trips.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#define TREK_LOGICAL_W 640
#define TREK_LOGICAL_H 360

typedef enum {
    SCR_TITLE, SCR_TRIPS, SCR_SIDE, SCR_FAMILY, SCR_STORY, SCR_NAMES, SCR_SETTINGS, SCR_CREDITS, SCR_GAME
} ScreenId;

typedef enum {
    OV_NONE, OV_PAUSE, OV_SUPPLIES, OV_PARTY, OV_MAP, OV_JOURNAL, OV_HISTORY, OV_ORDERS, OV_CONFIRM_DELETE
} Overlay;

typedef struct {
    Difficulty difficulty;  /* chosen on the main menu before a trip */
    int text_speed;         /* 0 instant, 1 fast, 2 normal (reserved) */
    bool show_odds;         /* off by default */
    bool fullscreen;
    int travel_speed;       /* 0 slow, 1 normal, 2 fast */
} Settings;

#define MAX_NAME_LIST 64
typedef struct {
    char men[MAX_NAME_LIST][24];
    int men_count;
    char women[MAX_NAME_LIST][24];
    int women_count;
} NameList;

typedef struct {
    NameList side[SIDE_COUNT];
    NameList servants;
} Names;

typedef struct App {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    TTF_Font *big;
    Draw draw;
    Locale *loc;
    GameData *data;
    char *base_path;   /* folder with data/ and assets/ (trailing slash) */
    char *pref_path;   /* where saves and settings live (trailing slash) */
    char *data_dir;
    Settings settings;
    Names names;
    bool running;

    ScreenId screen;
    Overlay overlay;
    int sel;           /* highlighted item on the current screen */
    int scroll;
    float mx, my;      /* mouse in logical coordinates */

    /* new trip */
    Side new_side;
    int new_family;

    /* the trip being played */
    Game *game;
    char trip_id[64];
    bool travelling;
    double travel_timer;
    int store_qty;
    char epitaph[96];
    bool text_input;
    int edit_member;   /* SCR_NAMES: member being renamed, -1 = none */
    char edit_buf[32];
    TripList trips;
    char message[200];
    double message_timer;
    uint64_t test_seed;   /* --seed: fixed seed for new trips (testing) */
} App;

bool app_init(App *app);
void app_shutdown(App *app);

const char *app_text(const App *app, const char *key);
/* Formats a LocText for the current game (side/family variants) into a static buffer. */
const char *app_fmt(App *app, const LocText *t);
/* Formats into a caller buffer. */
void app_fmt_to(App *app, const LocText *t, char *out, size_t cap);
/* Saves the trip after every step that changes it. */
void app_commit(App *app);
void app_save_settings(App *app);
void app_set_message(App *app, const char *text);
void app_close_game(App *app);
void app_switch(App *app, ScreenId s);

/* Random period-correct first name. */
const char *app_random_name(App *app, Side side, bool servant, Pronoun pronoun);

#endif
