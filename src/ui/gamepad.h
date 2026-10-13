/* Gamepad play. The buttons are turned into the key presses each screen already understands, chosen
 * by what is on screen (A buys in a store but fires in a hunt), so the screens need no gamepad code
 * of their own. The d-pad and the left stick move through menus with key repeat; in a hunt the stick
 * moves the gun sight directly. On-screen hints switch to the pad's button names once it is used. */
#ifndef TREK_GAMEPAD_H
#define TREK_GAMEPAD_H

#include "app.h"

/* Called for every key press the pad makes, as if it came from the keyboard. */
typedef void (*GamepadDispatch)(App *app, SDL_Event *e);

void gamepad_init(GamepadDispatch dispatch);
void gamepad_shutdown(void);
/* Handles pad events (connecting, buttons, triggers). Returns true if the event was the pad's. */
bool gamepad_event(App *app, const SDL_Event *e);
/* Every frame: held d-pad and stick repeat in menus; the stick aims in a hunt. */
void gamepad_update(App *app, double dt);
/* A short rumble, for a shot. */
void gamepad_rumble(float strength, int ms);
/* The name of a pad button for on-screen hints: token is "a", "b", "x", "y", "lb", "rb", "lt",
 * "rt", "start", "back" or "stick". Follows the pad in use (Xbox, PlayStation, Nintendo). */
const char *gamepad_label(const App *app, const char *token);

/* Testing (--keys): press a button by name ("a", "lb", "up" ...), or hold the left stick. */
bool gamepad_test_press(App *app, const char *name);
void gamepad_test_stick(float x, float y);
/* --check-text: name the buttons as on this kind of pad ("xbox", "ps", "switch"); NULL = the real one. */
void gamepad_test_style(const char *style);

#endif
