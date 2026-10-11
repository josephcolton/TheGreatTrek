/* The hunting screen: a side view of the veld. Game comes into sight and moves across; the
 * player aims with the mouse and clicks to fire, then waits while the gun is loaded again. */
#include "scene.h"
#include "screens.h"

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

static SDL_Color animal_color(const char *id)
{
    static const struct { const char *id; SDL_Color c; } COLORS[] = {
        {"springbok", {196, 140, 80, 255}}, {"blesbok", {140, 80, 50, 255}},  {"gnu", {50, 44, 40, 255}},
        {"hartebeest", {170, 90, 60, 255}}, {"quagga", {150, 110, 70, 255}}, {"ostrich", {30, 30, 30, 255}},
        {"kudu", {130, 110, 90, 255}},      {"eland", {190, 150, 100, 255}}, {"buffalo", {40, 36, 34, 255}},
        {"lion", {200, 160, 80, 255}}};
    for (size_t i = 0; i < sizeof COLORS / sizeof COLORS[0]; i++) {
        if (strcmp(COLORS[i].id, id) == 0) {
            return COLORS[i].c;
        }
    }
    return (SDL_Color){150, 110, 60, 255};
}

/* A figure for each kind of game, drawn with rectangles; ax, ay are its feet on screen. */
static void draw_animal(Draw *d, const AnimalDef *def, const HuntAnimal *a, float ax, float ay, double t)
{
    const char *id = def->id;
    SDL_Color body = animal_color(id), dark = {60, 44, 30, 255}, light = {230, 220, 200, 255};
    float s = (float)game_hunt_scale(a->y);
    float w = (float)def->body_w * s, h = (float)def->body_h * s, legs = (float)def->legs * s;
    float x = ax - w / 2;
    bool ostrich = strcmp(id, "ostrich") == 0;
    if (a->state == HUNT_A_DOWN) {
        /* lying in the grass */
        draw_rect(d, (SDL_FRect){x, ay - h * 0.7f, w, h * 0.7f}, body);
        draw_rect(d, (SDL_FRect){a->dir > 0 ? x + w : x - 4 * s, ay - 4 * s, 4 * s, 4 * s}, body);
        draw_rect(d, (SDL_FRect){x + 2 * s, ay - 1, w - 4 * s, 1}, dark);
        return;
    }
    float top = ay - legs - h;
    bool moving = a->state == HUNT_A_RUN || a->state == HUNT_A_CHARGE || a->state == HUNT_A_WALK;
    int rate = a->state == HUNT_A_WALK ? 4 : 10;
    float stride = moving ? (float)((int)(t * rate + a->x) % 2) * 2 * s : 0;
    draw_rect(d, (SDL_FRect){x, top, w, h}, body);
    /* head and neck: up when walking or running, down to the grass when grazing */
    float hx = a->dir > 0 ? x + w - 1 * s : x - 4 * s;
    if (a->state == HUNT_A_GRAZE) {
        draw_rect(d, (SDL_FRect){hx, top + h * 0.5f, 5 * s, legs + h * 0.5f - 1 * s}, body);
    } else if (ostrich) {
        draw_rect(d, (SDL_FRect){hx + 1 * s, top - 10 * s, 2 * s, 11 * s}, (SDL_Color){200, 170, 150, 255});
        draw_rect(d, (SDL_FRect){hx + (a->dir > 0 ? 1 : -1) * s, top - 12 * s, 4 * s, 3 * s}, (SDL_Color){200, 170, 150, 255});
    } else {
        draw_rect(d, (SDL_FRect){hx, top - 4 * s, 5 * s, 6 * s}, body);
    }
    /* legs */
    SDL_Color leg_c = ostrich ? (SDL_Color){200, 170, 150, 255} : body;
    draw_rect(d, (SDL_FRect){x + 1 * s + stride, ay - legs, 2 * s, legs}, leg_c);
    draw_rect(d, (SDL_FRect){x + w - 3 * s - stride, ay - legs, 2 * s, legs}, leg_c);
    /* markings */
    if (strcmp(id, "quagga") == 0) {
        float fore = a->dir > 0 ? x + w * 0.55f : x;
        for (int i = 0; i < 3; i++) {
            draw_rect(d, (SDL_FRect){fore + i * 3 * s, top, 1 * s, h}, light);
        }
    } else if (strcmp(id, "springbok") == 0) {
        draw_rect(d, (SDL_FRect){x, top + h * 0.6f, w, h * 0.4f}, light);
        draw_rect(d, (SDL_FRect){x, top + h * 0.45f, w, 1 * s}, dark);
    } else if (strcmp(id, "blesbok") == 0) {
        draw_rect(d, (SDL_FRect){hx + 1 * s, top - 4 * s, 2 * s, 5 * s}, light); /* the white blaze */
    } else if (ostrich) {
        draw_rect(d, (SDL_FRect){a->dir > 0 ? x : x + w - 4 * s, top, 4 * s, 3 * s}, light); /* white tail plumes */
    } else if (strcmp(id, "lion") == 0) {
        draw_rect(d, (SDL_FRect){hx - 2 * s, top - 6 * s, 8 * s, 9 * s}, (SDL_Color){140, 90, 40, 255});
        draw_rect(d, (SDL_FRect){a->dir > 0 ? x - 4 * s : x + w, top + 2 * s, 4 * s, 1 * s}, body); /* tail */
    } else if (strcmp(id, "kudu") == 0) {
        if (a->state != HUNT_A_GRAZE) {
            draw_rect(d, (SDL_FRect){hx + 1 * s, top - 11 * s, 1 * s, 7 * s}, dark); /* tall twisted horns */
            draw_rect(d, (SDL_FRect){hx + 3 * s, top - 10 * s, 1 * s, 6 * s}, dark);
        }
    } else if (strcmp(id, "buffalo") == 0 || strcmp(id, "gnu") == 0 ||
               strcmp(id, "hartebeest") == 0 || strcmp(id, "eland") == 0) {
        if (a->state != HUNT_A_GRAZE) {
            draw_rect(d, (SDL_FRect){hx - 1 * s, top - 6 * s, 7 * s, 2 * s}, dark); /* horns */
        }
    }
}

/* A thorn tree or bush, standing on the ground at (x, y). */
static void bush(Draw *d, float x, float y, float s, SDL_Color leaf, bool tree)
{
    SDL_Color trunk = {80, 60, 40, 255};
    if (tree) {
        draw_rect(d, (SDL_FRect){x - 1 * s, y - 16 * s, 2 * s, 16 * s}, trunk);
        draw_rect(d, (SDL_FRect){x - 12 * s, y - 20 * s, 24 * s, 4 * s}, leaf);
        draw_rect(d, (SDL_FRect){x - 8 * s, y - 23 * s, 16 * s, 3 * s}, leaf);
    } else {
        draw_rect(d, (SDL_FRect){x - 6 * s, y - 6 * s, 12 * s, 6 * s}, leaf);
        draw_rect(d, (SDL_FRect){x - 4 * s, y - 8 * s, 8 * s, 2 * s}, leaf);
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
    snprintf(title, sizeof title, "%s  ·  %s", app_text(app, "hunt.title"), app_fmt(app, &v.hunter));
    draw_header(app, title, NULL);

    /* the sky, the far hills and the veld, which darkens as the sun goes down */
    int region = g->edge >= 0 ? data->edges[g->edge].region : 0;
    float dusk = (float)(v.daylight < 0.25 ? 0.6 + 1.6 * v.daylight : 1.0);
    SDL_Color sky = COL_SKY, ground = scene_ground(app, region);
    sky = (SDL_Color){(Uint8)(sky.r * dusk), (Uint8)(sky.g * dusk), (Uint8)(sky.b * (0.3f + 0.7f * dusk)), 255};
    ground = (SDL_Color){(Uint8)(ground.r * dusk), (Uint8)(ground.g * dusk), (Uint8)(ground.b * dusk), 255};
    draw_rect(d, (SDL_FRect){0, 19, TREK_LOGICAL_W, FIELD_TOP - 19}, sky);
    SDL_Color hill = {(Uint8)(ground.r * 0.75f), (Uint8)(ground.g * 0.8f), (Uint8)(ground.b * 0.85f), 255};
    for (int i = 0; i < 7; i++) {
        float hx = i * 100.0f - 20;
        float hh = 6 + (float)((i * 7) % 5) * 2;
        draw_rect(d, (SDL_FRect){hx, FIELD_TOP - hh, 90, hh}, hill);
        draw_rect(d, (SDL_FRect){hx + 15, FIELD_TOP - hh - 3, 60, 3}, hill);
    }
    draw_rect(d, (SDL_FRect){0, FIELD_TOP, HUNT_FIELD_W, HUNT_FIELD_H}, ground);
    SDL_Color tuft = {(Uint8)(ground.r * 0.8f), (Uint8)(ground.g * 0.8f), (Uint8)(ground.b * 0.75f), 255};
    for (int i = 0; i < 60; i++) {
        float ty = (float)((i * 41) % HUNT_FIELD_H);
        float s = (float)game_hunt_scale(ty);
        draw_rect(d, (SDL_FRect){(float)((i * 97 + i * i * 13) % 640), FIELD_TOP + ty, 3 * s, 2 * s}, tuft);
    }

    /* bushes and game, far things first */
    SDL_Color leaf = {(Uint8)(70 * dusk), (Uint8)(96 * dusk), (Uint8)(50 * dusk), 255};
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
    size_t b = 0;
    const size_t nb = sizeof BUSHES / sizeof BUSHES[0];
    for (int k = 0; k <= n; k++) {
        float next_y = k < n ? v.animals[order[k]].y : 1e9f;
        for (; b < nb; b++) {
            if (BUSHES[b].y > next_y) {
                break;
            }
            bush(d, BUSHES[b].x, FIELD_TOP + BUSHES[b].y, (float)game_hunt_scale(BUSHES[b].y), leaf, BUSHES[b].tree);
        }
        if (k < n) {
            const HuntAnimal *a = &v.animals[order[k]];
            draw_animal(d, &data->animals[a->kind], a, a->x, FIELD_TOP + a->y, t);
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
        SDL_FRect box = {120, FIELD_TOP + 50, 400, 110};
        draw_rect(d, box, (SDL_Color){COL_BG.r, COL_BG.g, COL_BG.b, 230});
        draw_frame(d, box, COL_BORDER);
        float y = box.y + 10;
        for (int i = 0; i < 2; i++) {
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
    loctext_set(&lt, "hunt.status.meat");
    loctext_arg_int(&lt, "meat", v.meat);
    loctext_arg_int(&lt, "carry", v.carry);
    float meat_y = py + d->line_h;
    draw_text(d, d->font, app_text(app, "hunt.status.light"), 520, py, COL_DIM, TEXT_RIGHT);
    bar(d, 526, py + 5, 100, v.daylight, (SDL_Color){230, 180, 80, 255});
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
    draw_footer(app, app_text(app, v.over ? "hunt.footer.over" : "hunt.footer"));
}
