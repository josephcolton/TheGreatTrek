/* Screen handlers. Each screen has an event handler and a render function;
 * main.c calls the ones for app->screen (and app->overlay on top). */
#ifndef TREK_SCREENS_H
#define TREK_SCREENS_H

#include "app.h"

/* input helpers */
int  key_digit(const SDL_Event *e);     /* '1'..'9' -> 0..8, else -1 */
bool key_is(const SDL_Event *e, SDL_Keycode k);
bool is_confirm(const SDL_Event *e);    /* Enter, Space, keypad Enter */
bool is_back(const SDL_Event *e);       /* Escape */
bool is_click(const SDL_Event *e);
/* Up/down arrows move app->sel within [0, n). Returns true if handled. */
bool nav_updown(App *app, const SDL_Event *e, int n);
/* Where a dialog's text and choices end, laid out with no scene above (the most room it gets). */
float dialog_text_bottom(App *app, const DialogView *v);
/* --check-text: lays out every piece of text the game can show and reports any that runs off
 * the screen. Returns how many do. */
int text_check(App *app);

/* shared drawing */
void draw_header(App *app, const char *left, const char *right);
void draw_footer(App *app, const char *text);

/* menu screens (screen_menu.c) */
void title_event(App *app, const SDL_Event *e);
void title_render(App *app);
void trips_event(App *app, const SDL_Event *e);
void trips_render(App *app);
void settings_event(App *app, const SDL_Event *e);
void settings_render(App *app);
void credits_event(App *app, const SDL_Event *e);
void credits_render(App *app);
void level_event(App *app, const SDL_Event *e);
void level_render(App *app);
void scores_event(App *app, const SDL_Event *e);
void scores_render(App *app);

/* new trip (screen_new.c) */
void side_event(App *app, const SDL_Event *e);
void side_render(App *app);
void family_event(App *app, const SDL_Event *e);
void family_render(App *app);
void story_event(App *app, const SDL_Event *e);
void story_render(App *app);
void names_event(App *app, const SDL_Event *e);
void names_render(App *app);

/* in game (screen_game.c) */
void game_screen_event(App *app, const SDL_Event *e);
void game_screen_update(App *app, double dt);
void game_screen_render(App *app);
/* Opens a saved trip and goes to the game screen. */
bool open_trip(App *app, const char *trip_id);

/* hunting (screen_hunt.c) */
void hunt_screen_event(App *app, const SDL_Event *e);
void hunt_screen_render(App *app);
void hunt_screen_update(App *app, double dt);
/* Whether the mouse is over the hunting field, where a gun sight replaces the pointer. */
bool hunt_sight_active(const App *app);

/* barter (screen_barter.c) */
void barter_screen_event(App *app, const SDL_Event *e);
void barter_screen_render(App *app);

/* overlays (screen_overlay.c) */
void overlay_event(App *app, const SDL_Event *e);
void overlay_render(App *app);

#endif
