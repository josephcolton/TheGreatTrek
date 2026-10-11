/* The Great Trek: SDL init, main loop.
 * Options: --screenshot <file.png>  render one frame to a PNG and exit (for testing). */
#include "app.h"
#include "text.h"

#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <string.h>

static const SDL_Color COLOR_BG = {28, 22, 16, 255};
static const SDL_Color COLOR_TEXT = {235, 222, 196, 255};
static const SDL_Color COLOR_DIM = {170, 150, 120, 255};

static void render_title(App *app)
{
    SDL_SetRenderDrawColor(app->renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(app->renderer);
    float cx = TREK_LOGICAL_W / 2.0f;
    text_draw(app->renderer, app->font_title, app_text(app, "app.title"), cx, 120, 0, COLOR_TEXT, TEXT_CENTER);
    text_draw(app->renderer, app->font_body, app_text(app, "app.subtitle"), cx, 160, 0, COLOR_DIM, TEXT_CENTER);
    text_draw(app->renderer, app->font_body, app_text(app, "app.press_key"), cx, 300, 0, COLOR_DIM, TEXT_CENTER);
}

static bool save_screenshot(App *app, const char *path)
{
    render_title(app);
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
    const char *screenshot = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        }
    }

    App app;
    if (!app_init(&app, NULL)) {
        app_shutdown(&app);
        return 1;
    }
    if (screenshot) {
        bool ok = save_screenshot(&app, screenshot);
        app_shutdown(&app);
        return ok ? 0 : 1;
    }

    while (app.running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                app.running = false;
            } else if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
                app.running = false;
            }
        }
        render_title(&app);
        SDL_RenderPresent(app.renderer);
    }
    app_shutdown(&app);
    return 0;
}
