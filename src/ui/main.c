/* The Great Trek: SDL init, main loop, screen dispatch.
 * Testing options:
 *   --screenshot FILE   render a frame to a PNG after any --keys, then exit
 *   --check-text        lay out every text the game can show; list any that run off the screen
 *   --keys "a b c"      feed key presses first: 1-9, a-z, enter, esc, up, down, left, right,
 *                       space, bksp, "text:Word", "wait:N" (N frames of 1/10 s),
 *                       "move:X,Y" and "click:X,Y" (mouse, in logical 640x360 pixels)
 *   --pref DIR          keep saves and settings in DIR instead of the user folder
 *   --seed N            use seed N for new trips */
#include "app.h"
#include "music.h"
#include "screens.h"
#include "util.h"

#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <stdlib.h>
#include <string.h>

static void dispatch_screen_event(App *app, const SDL_Event *e);

static void dispatch_event(App *app, SDL_Event *e)
{
    if (e->type == SDL_EVENT_QUIT) {
        if (app->game) {
            app_commit(app);
        }
        app->running = false;
        return;
    }
    if (e->type == SDL_EVENT_MOUSE_MOTION || e->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        e->type == SDL_EVENT_MOUSE_BUTTON_UP || e->type == SDL_EVENT_MOUSE_WHEEL) {
        SDL_ConvertEventToRenderCoordinates(app->renderer, e);
        if (e->type == SDL_EVENT_MOUSE_MOTION) {
            app->mx = e->motion.x;
            app->my = e->motion.y;
            bool ours = app->draw.warped && SDL_fabsf(app->mx - app->draw.warp_x) < 1 && SDL_fabsf(app->my - app->draw.warp_y) < 1;
            app->draw.warped = false;
            if (!ours) {
                app->draw.mouse_last = true;
                app->draw.follow = false;
            }
            return;
        }
        if (e->type != SDL_EVENT_MOUSE_WHEEL) {
            app->mx = e->button.x;
            app->my = e->button.y;
        }
    }
    if (e->type == SDL_EVENT_KEY_DOWN && e->key.key == SDLK_F11) {
        app->settings.fullscreen = !app->settings.fullscreen;
        SDL_SetWindowFullscreen(app->window, app->settings.fullscreen);
        app_save_settings(app);
        return;
    }
    if (e->type == SDL_EVENT_KEY_DOWN) {
        /* after a key, the selection is the keys' until the mouse moves; if the key moved it, the
         * next frame brings the mouse onto it (draw_sync_hover) */
        int before = app->sel;
        app->draw.mouse_last = false;
        dispatch_screen_event(app, e);
        app->draw.follow = app->sel != before;
        return;
    }
    dispatch_screen_event(app, e);
}

static void dispatch_screen_event(App *app, const SDL_Event *e)
{
    if (app->overlay != OV_NONE && app->screen == SCR_GAME) {
        overlay_event(app, e);
        return;
    }
    switch (app->screen) {
    case SCR_TITLE: title_event(app, e); break;
    case SCR_TRIPS: trips_event(app, e); break;
    case SCR_LEVEL: level_event(app, e); break;
    case SCR_SIDE: side_event(app, e); break;
    case SCR_FAMILY: family_event(app, e); break;
    case SCR_STORY: story_event(app, e); break;
    case SCR_NAMES: names_event(app, e); break;
    case SCR_SETTINGS: settings_event(app, e); break;
    case SCR_CREDITS: credits_event(app, e); break;
    case SCR_SCORES: scores_event(app, e); break;
    case SCR_GAME: game_screen_event(app, e); break;
    }
}

/* Which tune fits what is on screen (data/music.json "scenes"); NULL for quiet. Music plays on
 * the menus, in towns and missions, at church, in camp at night, at a grave and at the end;
 * the road itself is left to the sound of the wagon. */
static const char *music_scene(const App *app)
{
    const Game *g = app->game;
    if (!g) {
        return "title";
    }
    switch (game_phase(g)) {
    case PHASE_ENDED: return "ending";
    case PHASE_GRAVE: return "grave";
    case PHASE_STORE:
    case PHASE_BARTER: return "place";
    case PHASE_DIALOG:
        if (g->dlg.kind == DLG_EVENT && g->dlg.event >= 0) {
            const EventDef *ev = &app->data->events[g->dlg.event];
            if (strncmp(ev->id, "church", 6) == 0) {
                return "church";
            }
            if (ev->night) {
                return "night";
            }
        }
        if (g->dlg.kind == DLG_LANDMARK) {
            NodeKind k = app->data->nodes[g->node].kind;
            if (k == NODE_TOWN || k == NODE_MISSION) {
                return "place";
            }
        }
        return NULL;
    default: return NULL;
    }
}

static void update(App *app, double dt)
{
    music_want(music_scene(app));
    music_update(dt);
    if (app->message_timer > 0) {
        app->message_timer -= dt;
    }
    if (app->screen == SCR_GAME) {
        game_screen_update(app, dt);
    }
    /* Over the hunting field the gun sight stands in for the mouse pointer. */
    static bool sight;
    bool want = app->screen == SCR_GAME && app->overlay == OV_NONE && hunt_sight_active(app);
    if (want != sight) {
        sight = want;
        if (want) {
            SDL_HideCursor();
        } else {
            SDL_ShowCursor();
        }
    }
}

static void render(App *app)
{
    draw_update_scale(&app->draw);
    app->draw.list_n = 0; /* set again by whichever list the keys move this frame */
    SDL_SetRenderDrawColor(app->renderer, COL_BG.r, COL_BG.g, COL_BG.b, 255);
    SDL_RenderClear(app->renderer);
    switch (app->screen) {
    case SCR_TITLE: title_render(app); break;
    case SCR_TRIPS: trips_render(app); break;
    case SCR_LEVEL: level_render(app); break;
    case SCR_SIDE: side_render(app); break;
    case SCR_FAMILY: family_render(app); break;
    case SCR_STORY: story_render(app); break;
    case SCR_NAMES: names_render(app); break;
    case SCR_SETTINGS: settings_render(app); break;
    case SCR_CREDITS: credits_render(app); break;
    case SCR_SCORES: scores_render(app); break;
    case SCR_GAME:
        if (app->overlay != OV_NONE) {
            /* the screen behind an overlay is only a picture: its lists must not move the
             * overlay's selection (a greyed-out line there would push it along) */
            app->draw.sel = NULL;
            game_screen_render(app);
            app->draw.sel = &app->sel;
            app->draw.list_n = 0;
            overlay_render(app);
        } else {
            game_screen_render(app);
        }
        break;
    }
}

/* ---------------------------------------------------------------- scripted keys for testing */

static SDL_Keycode key_from_name(const char *s)
{
    static const struct { const char *name; SDL_Keycode key; } KEYS[] = {
        {"enter", SDLK_RETURN}, {"esc", SDLK_ESCAPE}, {"up", SDLK_UP}, {"down", SDLK_DOWN},
        {"left", SDLK_LEFT}, {"right", SDLK_RIGHT}, {"space", SDLK_SPACE}, {"bksp", SDLK_BACKSPACE}, {"tab", SDLK_TAB}};
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) {
        if (strcmp(s, KEYS[i].name) == 0) {
            return KEYS[i].key;
        }
    }
    if (strlen(s) == 1) {
        char c = s[0];
        if (c >= '1' && c <= '9') {
            return SDLK_1 + (SDL_Keycode)(c - '1');
        }
        if (c >= 'a' && c <= 'z') {
            return SDLK_A + (SDL_Keycode)(c - 'a');
        }
    }
    return SDLK_UNKNOWN;
}

static void run_keys(App *app, const char *keys)
{
    char *copy = util_strdup(keys);
    for (char *tok = strtok(copy, " "); tok && app->running; tok = strtok(NULL, " ")) {
        if (strncmp(tok, "wait:", 5) == 0) {
            int frames = atoi(tok + 5);
            for (int i = 0; i < frames; i++) {
                update(app, 0.1);
                render(app); /* as the real loop does: some screens settle while drawing */
            }
            continue;
        }
        SDL_Event e;
        SDL_zero(e);
        if (strncmp(tok, "move:", 5) == 0 || strncmp(tok, "click:", 6) == 0) {
            bool click = tok[0] == 'c';
            float lx = 0, ly = 0, wx, wy;
            const char *xy = tok + (click ? 6 : 5);
            lx = (float)atof(xy);
            const char *comma = strchr(xy, ',');
            ly = comma ? (float)atof(comma + 1) : 0;
            SDL_RenderCoordinatesToWindow(app->renderer, lx, ly, &wx, &wy);
            e.type = SDL_EVENT_MOUSE_MOTION;
            e.motion.windowID = SDL_GetWindowID(app->window);
            e.motion.x = wx;
            e.motion.y = wy;
            dispatch_event(app, &e);
            if (click) {
                SDL_zero(e);
                e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                e.button.windowID = SDL_GetWindowID(app->window);
                e.button.button = SDL_BUTTON_LEFT;
                e.button.down = true;
                e.button.x = wx;
                e.button.y = wy;
                dispatch_event(app, &e);
            }
            update(app, 0.016);
            continue;
        }
        if (strncmp(tok, "text:", 5) == 0) {
            e.type = SDL_EVENT_TEXT_INPUT;
            e.text.text = tok + 5;
            dispatch_event(app, &e);
            continue;
        }
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = key_from_name(tok);
        e.key.down = true;
        dispatch_event(app, &e);
        update(app, 0.016);
    }
    free(copy);
}

static bool save_screenshot(App *app, const char *path)
{
    render(app);
    SDL_Surface *shot = SDL_RenderReadPixels(app->renderer, NULL);
    bool ok = shot && IMG_SavePNG(shot, path);
    if (!ok) {
        SDL_Log("Screenshot failed: %s", SDL_GetError());
    }
    SDL_DestroySurface(shot);
    return ok;
}

int main(int argc, char **argv)
{
    const char *screenshot = NULL, *keys = NULL, *pref = NULL;
    uint64_t seed = 0;
    bool check_text = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--check-text") == 0) {
            check_text = true;
        }
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (strcmp(argv[i], "--keys") == 0 && i + 1 < argc) {
            keys = argv[++i];
        } else if (strcmp(argv[i], "--pref") == 0 && i + 1 < argc) {
            pref = argv[++i];
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--music-wav") == 0 && i + 2 < argc) {
            /* --music-wav TUNE FILE: write a tune to a WAV to listen to, then stop */
            char *base = SDL_GetBasePath() ? util_strdup(SDL_GetBasePath()) : util_strdup("./");
            char *data = util_path_join(base, "data");
            bool ok = music_load(data) && music_write_wav(argv[i + 1], argv[i + 2]);
            free(data);
            free(base);
            return ok ? 0 : 1;
        }
    }

    App app;
    if (!app_init(&app, pref)) { /* --pref: a folder for saves and settings instead of the user's own */
        app_shutdown(&app);
        return 1;
    }
    app.test_seed = seed;
    if (check_text) {
        int bad = text_check(&app);
        app_shutdown(&app);
        return bad ? 1 : 0;
    }
    if (keys) {
        run_keys(&app, keys);
    }
    if (screenshot) {
        bool ok = save_screenshot(&app, screenshot);
        app_shutdown(&app);
        return ok ? 0 : 1;
    }

    Uint64 last = SDL_GetTicksNS();
    while (app.running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            dispatch_event(&app, &ev);
        }
        Uint64 now = SDL_GetTicksNS();
        double dt = (double)(now - last) / 1e9;
        last = now;
        update(&app, dt);
        render(&app);
        SDL_RenderPresent(app.renderer);
    }
    app_shutdown(&app);
    return 0;
}
