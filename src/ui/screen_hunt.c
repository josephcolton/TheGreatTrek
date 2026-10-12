/* The hunting screen: a side view of the veld. Game comes into sight and moves across; the
 * player aims with the mouse and clicks to fire, then waits while the gun is loaded again.
 * Guarding the herd uses the same screen at night: the camp and the stock are at the bottom by
 * the fire, and predators creep in out of the dark. */
#include "scene.h"
#include "screens.h"
#include "sprites.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FIELD_TOP 62.0f
#define PANEL_TOP (FIELD_TOP + HUNT_FIELD_H + 1)

static double tick_acc;

static bool in_field(float x, float y)
{
    return x >= 0 && x < HUNT_FIELD_W && y >= FIELD_TOP && y < FIELD_TOP + HUNT_FIELD_H;
}

bool hunt_sight_active(const App *app)
{
    const Game *g = app->game;
    return g && game_phase(g) == PHASE_HUNT && !g->hunt.over && in_field(app->mx, app->my);
}

void hunt_screen_update(App *app, double dt)
{
    Game *g = app->game;
    bool was_over = g->hunt.over;
    if (dt > 0.25) {
        dt = 0.25; /* after a stall, don't race through the afternoon */
    }
    tick_acc += dt;
    const double tick = 1.0 / HUNT_TICKS_PER_SEC;
    while (tick_acc >= tick) {
        tick_acc -= tick;
        game_hunt_tick(g);
    }
    if (!was_over && g->hunt.over) {
        app_commit(app);
    }
}

static void shoot(App *app, float x, float y)
{
    if (!in_field(x, y)) {
        return;
    }
    if (game_hunt_shoot(app->game, x, y - FIELD_TOP)) {
        app_commit(app);
    }
}

void hunt_screen_event(App *app, const SDL_Event *e)
{
    Game *g = app->game;
    HuntView v;
    if (!game_hunt_view(g, &v)) {
        return;
    }
    if (v.over) {
        if (is_confirm(e) || is_click(e)) {
            game_hunt_finish(g);
            tick_acc = 0;
            app->sel = 0;
            app_commit(app);
        }
        return;
    }
    if (is_click(e)) {
        shoot(app, app->mx, app->my);
    } else if (key_is(e, SDLK_SPACE) || key_is(e, SDLK_F)) {
        shoot(app, app->mx, app->my);
    } else if (key_is(e, SDLK_RETURN) || key_is(e, SDLK_KP_ENTER)) {
        game_hunt_quit(g);
        app_commit(app);
    } else if (e->type == SDL_EVENT_KEY_DOWN) {
        /* The arrow keys move the sight, for anyone without a mouse. */
        float step = 8;
        if (!in_field(app->mx, app->my)) {
            app->mx = HUNT_FIELD_W / 2.0f;
            app->my = FIELD_TOP + HUNT_FIELD_H / 2.0f;
        }
        if (key_is(e, SDLK_LEFT)) {
            app->mx -= step;
        } else if (key_is(e, SDLK_RIGHT)) {
            app->mx += step;
        } else if (key_is(e, SDLK_UP)) {
            app->my -= step;
        } else if (key_is(e, SDLK_DOWN)) {
            app->my += step;
        }
        app->mx = SDL_clamp(app->mx, 1.0f, HUNT_FIELD_W - 2.0f);
        app->my = SDL_clamp(app->my, FIELD_TOP + 1, FIELD_TOP + HUNT_FIELD_H - 2);
    }
}

/* ---------------------------------------------------------------- drawing */

/* An animal's sprite, standing with its feet at (ax, ay) on screen. The body is centred on ax
 * and the head sticks out in front, matching the hit box (game_hunt_body). */
static void draw_animal(App *app, const AnimalDef *def, const HuntAnimal *a, float ax, float ay, double t, SDL_Color tint)
{
    char name[48];
    const char *frame = "walk_0";
    if (a->state == HUNT_A_DOWN) {
        frame = "down";
    } else if (a->state == HUNT_A_GRAZE) {
        frame = "graze";
    } else {
        int rate = a->state == HUNT_A_WALK ? 4 : 10;
        frame = (int)(t * rate + a->x * 0.05f) % 2 ? "walk_1" : "walk_0";
    }
    snprintf(name, sizeof name, "%s_%s", def->id, frame);
    const Sprite *sp = sprite_find(name);
    if (!sp) {
        return;
    }
    float s = (float)game_hunt_scale(a->y);
    float left = ax - (float)def->body_w * s / 2 - (a->dir < 0 ? 5 * s : 0);
    sprite_draw_tinted(app->renderer, sp, left, ay - sp->h * s, s, a->dir < 0, tint);
}

/* The camp at night: wagons, the fire and the stock along the bottom of the field. */
static void draw_camp(App *app, double t)
{
    Draw *d = &app->draw;
    float base = FIELD_TOP + HUNT_FIELD_H;
    /* firelight on the ground, warmest near the fire */
    for (int i = 0; i < 6; i++) {
        float w = 360.0f - i * 50.0f, h = 70.0f - i * 10.0f;
        draw_rect(d, (SDL_FRect){430 - w / 2, base - h, w, h}, (SDL_Color){230, 140, 60, 14});
    }
    const Sprite *wagon = sprite_find("laager_wagon");
    if (wagon) {
        sprite_draw_tinted(app->renderer, wagon, 20, base - wagon->h * 2.0f - 2, 2.0f, false, (SDL_Color){120, 110, 120, 255});
        sprite_draw_tinted(app->renderer, wagon, 566, base - wagon->h * 2.0f - 2, 2.0f, true, (SDL_Color){120, 110, 120, 255});
    }
    static const struct { float x; bool red; bool flip; } OXEN[] = {
        {150, true, false}, {205, false, true}, {270, true, false}, {330, false, false}, {505, true, true}};
    for (size_t i = 0; i < sizeof OXEN / sizeof OXEN[0]; i++) {
        const Sprite *ox = sprite_find(OXEN[i].red ? "ox_red_0" : "ox_black_0");
        if (ox) {
            float light = OXEN[i].x > 330 ? 0.75f : 0.55f;
            SDL_Color c = {(Uint8)(255 * light), (Uint8)(220 * light), (Uint8)(200 * light), 255};
            sprite_draw_tinted(app->renderer, ox, OXEN[i].x, base - 14 - ox->h * 1.6f, 1.6f, OXEN[i].flip, c);
        }
    }
    /* sheep pressed together: pale woolly backs */
    for (int i = 0; i < 9; i++) {
        float x = 215.0f + (float)((i * 37) % 120), y = base - 12.0f - (float)((i * 5) % 9);
        draw_rect(d, (SDL_FRect){x, y - 5, 9, 5}, (SDL_Color){150, 146, 132, 255});
        draw_rect(d, (SDL_FRect){x + 8, y - 6, 3, 3}, (SDL_Color){60, 54, 50, 255});
    }
    char name[16];
    snprintf(name, sizeof name, "fire_%d", (int)(t * 8) % 3);
    const Sprite *fire = sprite_find(name);
    if (fire) {
        sprite_draw(app->renderer, fire, 430 - fire->w, base - 4 - fire->h * 2.0f, 2.0f, false);
    }
}

typedef struct {
    float x, y;
    bool tree;
} Bush;

static const Bush BUSHES[] = {{70, 30, true}, {540, 22, false}, {250, 70, false}, {430, 110, true},
                              {120, 150, false}, {600, 175, false}, {330, 40, false}};

static void bar(Draw *d, float x, float y, float w, double frac, SDL_Color c)
{
    draw_rect(d, (SDL_FRect){x, y, w, 6}, COL_BORDER);
    draw_rect(d, (SDL_FRect){x + 1, y + 1, (float)((w - 2) * (frac < 0 ? 0 : frac > 1 ? 1 : frac)), 4}, c);
}

void hunt_screen_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    const GameData *data = app->data;
    HuntView v;
    if (!game_hunt_view(g, &v)) {
        return;
    }
    double t = SDL_GetTicks() / 1000.0;
    char title[160];
    snprintf(title, sizeof title, "%s  ·  %s", app_text(app, v.defend ? "defend.title" : "hunt.title"), app_fmt(app, &v.hunter));
    draw_header(app, title, NULL);

    /* the sky, the far hills and the veld, which darkens as the sun goes down */
    int region = g->edge >= 0 ? data->edges[g->edge].region : 0;
    float dusk = v.defend ? 0.28f : (float)(v.daylight < 0.25 ? 0.6 + 1.6 * v.daylight : 1.0);
    SDL_Color ground = scene_ground(app, region);
    ground = (SDL_Color){(Uint8)(ground.r * dusk), (Uint8)(ground.g * dusk), (Uint8)(ground.b * dusk), 255};
    scene_backdrop(app, (SDL_FRect){0, 19, TREK_LOGICAL_W, FIELD_TOP - 19}, FIELD_TOP, region, dusk);
    for (int i = 0; i < 5; i++) {
        float bh = HUNT_FIELD_H / 5.0f;
        float f = 0.92f + 0.04f * i;
        draw_rect(d, (SDL_FRect){0, FIELD_TOP + i * bh, HUNT_FIELD_W, bh + 1},
                  (SDL_Color){(Uint8)(ground.r * f), (Uint8)(ground.g * f), (Uint8)(ground.b * f), 255});
    }
    SDL_Color tuft = {(Uint8)(ground.r * 0.8f), (Uint8)(ground.g * 0.8f), (Uint8)(ground.b * 0.75f), 255};
    for (int i = 0; i < 60; i++) {
        float ty = (float)((i * 41) % HUNT_FIELD_H);
        float s = (float)game_hunt_scale(ty);
        draw_rect(d, (SDL_FRect){(float)((i * 97 + i * i * 13) % 640), FIELD_TOP + ty, 3 * s, 2 * s}, tuft);
    }

    /* bushes and game, far things first */
    SDL_Color tint = {(Uint8)(255 * dusk), (Uint8)(255 * dusk), (Uint8)(255 * (0.3f + 0.7f * dusk)), 255};
    int order[HUNT_MAX_ANIMALS], n = 0;
    for (int i = 0; i < HUNT_MAX_ANIMALS; i++) {
        if (v.animals[i].state != HUNT_A_NONE) {
            int j = n++;
            while (j > 0 && v.animals[order[j - 1]].y > v.animals[i].y) {
                order[j] = order[j - 1];
                j--;
            }
            order[j] = i;
        }
    }
    if (v.defend) {
        draw_camp(app, t);
    }
    size_t b = 0;
    const size_t nb = sizeof BUSHES / sizeof BUSHES[0];
    for (int k = 0; k <= n; k++) {
        float next_y = k < n ? v.animals[order[k]].y : 1e9f;
        for (; b < nb; b++) {
            if (BUSHES[b].y > next_y) {
                break;
            }
            float bs = (float)game_hunt_scale(BUSHES[b].y) * (BUSHES[b].tree ? 0.6f : 1.0f);
            const Sprite *sp = sprite_find(BUSHES[b].tree ? "tree_big" : "bush");
            if (sp) {
                sprite_draw_tinted(app->renderer, sp, BUSHES[b].x - sp->w * bs / 2, FIELD_TOP + BUSHES[b].y - sp->h * bs, bs,
                                   b % 2, tint);
            }
        }
        if (k < n) {
            const HuntAnimal *a = &v.animals[order[k]];
            const AnimalDef *def = &data->animals[a->kind];
            SDL_Color at = tint;
            if (v.defend) {
                /* at night only the firelight shows them; far off, just their eyes catch it */
                float light = 0.22f + 0.78f * (a->y / HUNT_FIELD_H) * (a->y / HUNT_FIELD_H);
                at = (SDL_Color){(Uint8)(255 * light), (Uint8)(235 * light), (Uint8)(210 * light), 255};
            }
            draw_animal(app, def, a, a->x, FIELD_TOP + a->y, t, at);
            if (v.defend && (a->state == HUNT_A_WALK || a->state == HUNT_A_RUSH) && a->y < HUNT_FIELD_H * 0.6f) {
                float sc = (float)game_hunt_scale(a->y);
                float hx = a->x + a->dir * ((float)def->body_w * sc / 2 + 3 * sc);
                float hy = FIELD_TOP + a->y - ((float)(def->body_h + def->legs) * sc) - 1;
                draw_rect(d, (SDL_FRect){hx - 1, hy, 1.5f, 1.5f}, (SDL_Color){250, 230, 130, 255});
                draw_rect(d, (SDL_FRect){hx + 1.5f * a->dir, hy, 1.5f, 1.5f}, (SDL_Color){250, 230, 130, 255});
            }
        }
    }

    /* the gun barrel, pointing toward the sight */
    float gx = HUNT_FIELD_W / 2.0f, gy = FIELD_TOP + HUNT_FIELD_H;
    bool aiming = in_field(app->mx, app->my);
    float dx = (aiming ? app->mx : gx) - gx, dy = (aiming ? app->my : FIELD_TOP + 80) - gy;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1) {
        dx = 0;
        dy = -1;
    } else {
        dx /= len;
        dy /= len;
    }
    for (int i = 0; i < 30; i++) {
        float w = i < 12 ? 6 : 4;
        SDL_Color c = i < 12 ? (SDL_Color){110, 76, 46, 255} : (SDL_Color){70, 70, 74, 255};
        draw_rect(d, (SDL_FRect){gx + dx * i * 1.2f - w / 2, gy - 4 + dy * i * 1.2f - w / 2, w, w}, c);
    }
    float tx = gx + dx * 38, ty = gy - 4 + dy * 38; /* the muzzle */
    /* the shot: a puff of dust where the ball strikes, a flash and powder smoke at the muzzle */
    if (v.shot_age < 10 && !v.shot_misfire) {
        float r = 2 + v.shot_age * 0.6f;
        draw_rect(d, (SDL_FRect){v.shot_x - r, FIELD_TOP + v.shot_y - r, 2 * r, 2 * r}, (SDL_Color){180, 160, 120, 160});
    }
    if (v.shot_age < 5 && !v.shot_misfire) {
        draw_rect(d, (SDL_FRect){tx - 5, ty - 5, 10, 10}, (SDL_Color){250, 210, 90, 255});
    }
    if (v.shot_age < 40) {
        float r = 4 + v.shot_age * 0.6f;
        int alpha = (v.shot_misfire ? 60 : 200) - v.shot_age * 5;
        if (alpha > 0) {
            draw_rect(d, (SDL_FRect){tx - r, ty - r - v.shot_age * 0.8f, 2 * r, 1.5f * r}, (SDL_Color){230, 230, 225, (Uint8)alpha});
        }
    }
    /* the sight */
    if (!v.over && in_field(app->mx, app->my)) {
        SDL_Color c = v.loaded ? COL_GOLD : COL_DIM;
        float cx = app->mx, cy = app->my;
        draw_rect(d, (SDL_FRect){cx - 7, cy, 5, 1}, c);
        draw_rect(d, (SDL_FRect){cx + 3, cy, 5, 1}, c);
        draw_rect(d, (SDL_FRect){cx, cy - 7, 1, 5}, c);
        draw_rect(d, (SDL_FRect){cx, cy + 3, 1, 5}, c);
    }

    /* the summary when the hunt is over */
    if (v.over) {
        SDL_FRect box = {110, FIELD_TOP + 30, 420, 150};
        draw_rect(d, box, (SDL_Color){COL_BG.r, COL_BG.g, COL_BG.b, 230});
        draw_frame(d, box, COL_BORDER);
        float y = box.y + 10;
        for (int i = 0; i < 4; i++) {
            if (v.summary[i].key[0]) {
                y += draw_wrapped(d, d->font, app_fmt(app, &v.summary[i]), box.x + 12, y, box.w - 24, COL_TEXT, false) + 4;
            }
        }
        draw_text(d, d->font, app_text(app, "hunt.summary.go"), box.x + box.w / 2, box.y + box.h - d->line_h - 8, COL_GOLD,
                  TEXT_CENTER);
    }

    /* the panel: the gun, the meat, the light, and what just happened */
    draw_rect(d, (SDL_FRect){0, PANEL_TOP - 1, TREK_LOGICAL_W, 1}, COL_BORDER);
    float py = PANEL_TOP + 2;
    LocText lt;
    if (v.shots_left <= 0) {
        loctext_set(&lt, "hunt.status.no_shot");
    } else {
        loctext_set(&lt, v.loaded ? "hunt.status.loaded" : "hunt.status.reloading");
    }
    draw_text(d, d->font, app_fmt(app, &lt), 12, py, v.loaded ? COL_GOLD : COL_DIM, TEXT_LEFT);
    bar(d, 130, py + 5, 70, v.reload, v.loaded ? COL_GOLD : COL_DIM);
    if (v.defend) {
        loctext_set(&lt, "defend.status.lost");
        loctext_arg_int(&lt, "n", v.lost);
    } else {
        loctext_set(&lt, "hunt.status.meat");
        loctext_arg_int(&lt, "meat", v.meat);
        loctext_arg_int(&lt, "carry", v.carry);
    }
    float meat_y = py + d->line_h;
    draw_text(d, d->font, app_text(app, v.defend ? "defend.status.night" : "hunt.status.light"), 520, py, COL_DIM, TEXT_RIGHT);
    bar(d, 526, py + 5, 100, v.daylight, v.defend ? (SDL_Color){110, 120, 190, 255} : (SDL_Color){230, 180, 80, 255});
    py += d->line_h;
    draw_text(d, d->font, app_fmt(app, &lt), 12, meat_y, COL_TEXT, TEXT_LEFT);
    loctext_set(&lt, "hunt.status.shots");
    loctext_arg_int(&lt, "n", v.shots_left);
    draw_text(d, d->font, app_fmt(app, &lt), 628, py, COL_DIM, TEXT_RIGHT);
    py += d->line_h;
    for (int i = v.log_count - 2 < 0 ? 0 : v.log_count - 2; i < v.log_count; i++) {
        draw_text(d, d->font, app_fmt(app, &v.log[i]), 12, py, i == v.log_count - 1 ? COL_TEXT : COL_DIM, TEXT_LEFT);
        py += d->line_h;
    }
    draw_footer(app, app_text(app, v.over ? "hunt.footer.over" : v.defend ? "defend.footer" : "hunt.footer"));
}
