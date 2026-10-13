/* Gamepad play (see gamepad.h). */
#include "gamepad.h"

#include "screens.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* How the pad feels in menus. These are input feel, not game numbers. */
#define MENU_TILT 0.55f     /* how far the stick goes over before it counts as a press */
#define REPEAT_DELAY 0.38   /* a held direction repeats after this long ... */
#define REPEAT_RATE 0.09    /* ... and then this often */
#define TRIGGER_ON 0.5f
#define TRIGGER_OFF 0.3f

/* The triggers are axes; they are treated as two more buttons. */
enum { PAD_LT = SDL_GAMEPAD_BUTTON_COUNT, PAD_RT };

typedef enum {
    CTX_MENU, CTX_CONFIRM, CTX_NAMES, CTX_TRIPS, CTX_SCORES,
    CTX_TRAVEL, CTX_DIALOG, CTX_STORE, CTX_BARTER, CTX_HUNT, CTX_HUNT_OVER, CTX_GRAVE, CTX_ENDING,
    CTX_JOURNAL, CTX_MAP, CTX_OVERLAY
} Context;

static GamepadDispatch dispatch_fn;
static SDL_Gamepad *pad;
static bool lt_down, rt_down;
static int held_button = -1;      /* d-pad button held in a menu */
static double held_timer;
static SDL_Keycode stick_dir;     /* the left stick pushed in a menu */
static double stick_timer;
static const char *forced_style; /* --check-text measures each kind of pad's names */
static bool test_stick;
static float test_x, test_y;

void gamepad_init(GamepadDispatch dispatch)
{
    dispatch_fn = dispatch;
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        SDL_Log("No gamepad support: %s", SDL_GetError());
    }
    /* pads already plugged in also arrive as SDL_EVENT_GAMEPAD_ADDED */
}

void gamepad_shutdown(void)
{
    if (pad) {
        SDL_CloseGamepad(pad);
        pad = NULL;
    }
}

static void open_first(void)
{
    int n = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&n);
    for (int i = 0; ids && i < n && !pad; i++) {
        pad = SDL_OpenGamepad(ids[i]);
    }
    SDL_free(ids);
}

static Context context(const App *app)
{
    if (app->overlay == OV_CONFIRM_DELETE) {
        return CTX_CONFIRM;
    }
    if (app->screen == SCR_GAME && app->game) {
        switch (app->overlay) {
        case OV_NONE: break;
        case OV_JOURNAL: return CTX_JOURNAL;
        case OV_MAP: return CTX_MAP;
        default: return CTX_OVERLAY;
        }
        switch (game_phase(app->game)) {
        case PHASE_TRAVEL: return CTX_TRAVEL;
        case PHASE_DIALOG: return CTX_DIALOG;
        case PHASE_STORE: return CTX_STORE;
        case PHASE_BARTER: return CTX_BARTER;
        case PHASE_HUNT: return app->game->hunt.over ? CTX_HUNT_OVER : CTX_HUNT;
        case PHASE_GRAVE: return CTX_GRAVE;
        case PHASE_ENDED: return CTX_ENDING;
        }
    }
    switch (app->screen) {
    case SCR_NAMES: return app->edit_member >= 0 ? CTX_MENU : CTX_NAMES;
    case SCR_TRIPS: return CTX_TRIPS;
    case SCR_SCORES: return CTX_SCORES;
    default: return CTX_MENU;
    }
}

/* The key a button stands for on this screen; 0 = nothing here. */
static SDL_Keycode button_key(const App *app, int button)
{
    Context c = context(app);
    bool road = c == CTX_TRAVEL || c == CTX_DIALOG;
    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return SDLK_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return SDLK_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return SDLK_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return SDLK_RIGHT;
    case SDL_GAMEPAD_BUTTON_SOUTH: /* A: choose, go on; fire in a hunt; yes to a question */
        return c == CTX_CONFIRM ? SDLK_Y : c == CTX_HUNT ? SDLK_SPACE : SDLK_RETURN;
    case SDL_GAMEPAD_BUTTON_EAST: /* B: back, close, leave; no to a question */
        return c == CTX_CONFIRM ? SDLK_N : SDLK_ESCAPE;
    case SDL_GAMEPAD_BUTTON_WEST: /* X: supplies; sell; less; delete */
        return road ? SDLK_S : c == CTX_STORE ? SDLK_S : c == CTX_BARTER ? SDLK_BACKSPACE :
               (c == CTX_TRIPS || c == CTX_SCORES) ? SDLK_D : 0;
    case SDL_GAMEPAD_BUTTON_NORTH: /* Y: party; a new name; propose; back to the wagons */
        return road ? SDLK_P : c == CTX_NAMES ? SDLK_R : c == CTX_BARTER ? SDLK_P : c == CTX_HUNT ? SDLK_RETURN : 0;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: /* LB: journal; page up; the other map; ask */
        return road ? SDLK_J : c == CTX_JOURNAL ? SDLK_PAGEUP : c == CTX_MAP ? SDLK_LEFT : c == CTX_BARTER ? SDLK_A : 0;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: /* RB: glossary; page down; the other map */
        return road ? SDLK_G : c == CTX_JOURNAL ? SDLK_PAGEDOWN : c == CTX_MAP ? SDLK_RIGHT : 0;
    case SDL_GAMEPAD_BUTTON_BACK: /* the map */
        return (road || c == CTX_ENDING) ? SDLK_M : 0;
    case SDL_GAMEPAD_BUTTON_START: /* the pause menu, in a trip */
        return app->screen == SCR_GAME ? SDLK_ESCAPE : 0;
    case PAD_LT: /* the history behind an event */
        return c == CTX_DIALOG ? SDLK_H : 0;
    case PAD_RT: /* fire */
        return c == CTX_HUNT ? SDLK_SPACE : 0;
    default: return 0;
    }
}

static void press(App *app, SDL_Keycode key)
{
    if (!key || !dispatch_fn) {
        return;
    }
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = key;
    e.key.down = true;
    app->pad_event = true;
    dispatch_fn(app, &e);
    app->pad_event = false;
}

static bool is_dpad(int button)
{
    return button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN ||
           button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
}

static void button_down(App *app, int button)
{
    app->pad_last = true;
    if (is_dpad(button)) {
        Context c = context(app);
        if (c == CTX_HUNT) {
            return; /* in a hunt the d-pad moves the sight smoothly (gamepad_update) */
        }
        press(app, button_key(app, button));
        held_button = button;
        held_timer = REPEAT_DELAY;
        return;
    }
    press(app, button_key(app, button));
}

static void trigger(App *app, bool *down, float v, int button)
{
    if (!*down && v > TRIGGER_ON) {
        *down = true;
        button_down(app, button);
    } else if (*down && v < TRIGGER_OFF) {
        *down = false;
    }
}

bool gamepad_event(App *app, const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!pad) {
            pad = SDL_OpenGamepad(e->gdevice.which);
        }
        return true;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (pad && SDL_GetGamepadID(pad) == e->gdevice.which) {
            SDL_CloseGamepad(pad);
            pad = NULL;
            open_first();
        }
        return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (pad && e->gbutton.which == SDL_GetGamepadID(pad)) {
            button_down(app, e->gbutton.button);
        }
        return true;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (e->gbutton.button == held_button) {
            held_button = -1;
        }
        return true;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        if (!pad || e->gaxis.which != SDL_GetGamepadID(pad)) {
            return true;
        }
        float v = e->gaxis.value / 32767.0f;
        if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
            trigger(app, &lt_down, v, PAD_LT);
        } else if (e->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            trigger(app, &rt_down, v, PAD_RT);
        } else if (fabsf(v) > MENU_TILT) {
            app->pad_last = true;
        }
        return true;
    }
    default:
        return e->type >= SDL_EVENT_GAMEPAD_AXIS_MOTION && e->type < SDL_EVENT_FINGER_DOWN;
    }
}

static void left_stick(float *x, float *y)
{
    *x = *y = 0;
    if (test_stick) {
        *x = test_x;
        *y = test_y;
    } else if (pad) {
        *x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        *y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    }
}

void gamepad_update(App *app, double dt)
{
    float sx, sy;
    left_stick(&sx, &sy);
    if (context(app) == CTX_HUNT) {
        /* the stick aims; so does the d-pad, at a steady speed */
        if (pad) {
            sx += (float)SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) - (float)SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
            sy += (float)SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN) - (float)SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP);
        }
        if (sx != 0 || sy != 0) {
            hunt_pad_aim(app, SDL_clamp(sx, -1.0f, 1.0f), SDL_clamp(sy, -1.0f, 1.0f), dt);
        }
        held_button = -1;
        stick_dir = 0;
        return;
    }
    /* menus: the stick pressed over is an arrow key, repeating while it is held */
    SDL_Keycode dir = 0;
    if (fabsf(sx) > MENU_TILT || fabsf(sy) > MENU_TILT) {
        dir = fabsf(sx) > fabsf(sy) ? (sx < 0 ? SDLK_LEFT : SDLK_RIGHT) : (sy < 0 ? SDLK_UP : SDLK_DOWN);
    }
    if (dir && dir != stick_dir) {
        stick_dir = dir;
        stick_timer = REPEAT_DELAY;
        app->pad_last = true;
        press(app, dir);
    } else if (dir) {
        stick_timer -= dt;
        if (stick_timer <= 0) {
            stick_timer = REPEAT_RATE;
            press(app, dir);
        }
    } else {
        stick_dir = 0;
    }
    /* a held d-pad repeats too */
    if (held_button >= 0) {
        if (!pad || !SDL_GetGamepadButton(pad, (SDL_GamepadButton)held_button)) {
            held_button = -1;
        } else {
            held_timer -= dt;
            if (held_timer <= 0) {
                held_timer = REPEAT_RATE;
                press(app, button_key(app, held_button));
            }
        }
    }
}

void gamepad_rumble(float strength, int ms)
{
    if (pad) {
        Uint16 low = (Uint16)(SDL_clamp(strength, 0.0f, 1.0f) * 0xFFFF);
        SDL_RumbleGamepad(pad, low, (Uint16)(low / 2), (Uint32)ms);
    }
}

/* ---------------------------------------------------------------- button names */

static const char *style(void)
{
    if (forced_style) {
        return forced_style;
    }
    if (!pad) {
        return "xbox";
    }
    switch (SDL_GetGamepadType(pad)) {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5: return "ps";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return "switch";
    case SDL_GAMEPAD_TYPE_XBOX360: return "xbox360";
    default: return "xbox";
    }
}

const char *gamepad_label(const App *app, const char *token)
{
    static const struct { const char *token; SDL_GamepadButton button; } FACE[] = {
        {"a", SDL_GAMEPAD_BUTTON_SOUTH}, {"b", SDL_GAMEPAD_BUTTON_EAST},
        {"x", SDL_GAMEPAD_BUTTON_WEST}, {"y", SDL_GAMEPAD_BUTTON_NORTH}};
    static const char *LABEL[] = {
        [SDL_GAMEPAD_BUTTON_LABEL_A] = "a", [SDL_GAMEPAD_BUTTON_LABEL_B] = "b",
        [SDL_GAMEPAD_BUTTON_LABEL_X] = "x", [SDL_GAMEPAD_BUTTON_LABEL_Y] = "y",
        [SDL_GAMEPAD_BUTTON_LABEL_CROSS] = "cross", [SDL_GAMEPAD_BUTTON_LABEL_CIRCLE] = "circle",
        [SDL_GAMEPAD_BUTTON_LABEL_SQUARE] = "square", [SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE] = "triangle"};
    char key[48];
    for (size_t i = 0; i < sizeof FACE / sizeof FACE[0]; i++) {
        if (strcmp(token, FACE[i].token) == 0) {
            /* the face buttons are named by what is printed on them, wherever they sit */
            SDL_GamepadButtonLabel l = pad ? SDL_GetGamepadButtonLabel(pad, FACE[i].button) : SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
            if (forced_style) {
                static const SDL_GamepadButtonLabel PS[] = {SDL_GAMEPAD_BUTTON_LABEL_CROSS, SDL_GAMEPAD_BUTTON_LABEL_CIRCLE,
                                                            SDL_GAMEPAD_BUTTON_LABEL_SQUARE, SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE};
                static const SDL_GamepadButtonLabel NINTENDO[] = {SDL_GAMEPAD_BUTTON_LABEL_B, SDL_GAMEPAD_BUTTON_LABEL_A,
                                                                  SDL_GAMEPAD_BUTTON_LABEL_Y, SDL_GAMEPAD_BUTTON_LABEL_X};
                l = strcmp(forced_style, "ps") == 0 ? PS[i] : strcmp(forced_style, "switch") == 0 ? NINTENDO[i] : SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
            }
            const char *name = (l > SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN && l <= SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE) ? LABEL[l] : token;
            snprintf(key, sizeof key, "pad.button.%s", name);
            return locale_get(app->loc, key);
        }
    }
    snprintf(key, sizeof key, "pad.%s.%s", style(), token);
    const char *t = locale_find(app->loc, key);
    if (!t) {
        snprintf(key, sizeof key, "pad.xbox.%s", token);
        t = locale_get(app->loc, key);
    }
    return t;
}

/* ---------------------------------------------------------------- testing */

bool gamepad_test_press(App *app, const char *name)
{
    static const struct { const char *name; int button; } NAMES[] = {
        {"a", SDL_GAMEPAD_BUTTON_SOUTH}, {"b", SDL_GAMEPAD_BUTTON_EAST}, {"x", SDL_GAMEPAD_BUTTON_WEST},
        {"y", SDL_GAMEPAD_BUTTON_NORTH}, {"lb", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
        {"rb", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER}, {"lt", PAD_LT}, {"rt", PAD_RT},
        {"start", SDL_GAMEPAD_BUTTON_START}, {"back", SDL_GAMEPAD_BUTTON_BACK},
        {"up", SDL_GAMEPAD_BUTTON_DPAD_UP}, {"down", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {"left", SDL_GAMEPAD_BUTTON_DPAD_LEFT}, {"right", SDL_GAMEPAD_BUTTON_DPAD_RIGHT}};
    for (size_t i = 0; i < sizeof NAMES / sizeof NAMES[0]; i++) {
        if (strcmp(name, NAMES[i].name) == 0) {
            button_down(app, NAMES[i].button);
            held_button = -1; /* a tap, not a hold */
            return true;
        }
    }
    return false;
}

void gamepad_test_style(const char *s)
{
    forced_style = s;
}

void gamepad_test_stick(float x, float y)
{
    test_stick = x != 0 || y != 0;
    test_x = x;
    test_y = y;
}
