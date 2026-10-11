/* The hunting screen: a side view of the veld with the hunter and the game. */
#include "screens.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define HUNT_MENU_X 330.0f
#define HUNT_MENU_Y 178.0f
#define HUNT_MENU_W 296

static const char *ACTION_KEYS[HUNT_ACTION_COUNT] = {
    "hunt.action.stalk", "hunt.action.wait", "hunt.action.fire",
    "hunt.action.reload", "hunt.action.follow", "hunt.action.leave"};

static int hunt_items(App *app, const HuntView *v, ChoiceItem *items, int *map)
{
    int n = 0;
    if (v->over) {
        items[0] = (ChoiceItem){app_text(app, "hunt.action.done"), NULL};
        map[0] = HUNT_LEAVE;
        return 1;
    }
    for (int a = 0; a < HUNT_ACTION_COUNT; a++) {
        if (a == HUNT_FOLLOW && !v->can[a]) {
            continue; /* only offered when there is a wounded animal to follow */
        }
        items[n] = (ChoiceItem){app_text(app, ACTION_KEYS[a]), v->can[a] ? NULL : ""};
        map[n++] = a;
    }
    return n;
}

void hunt_screen_event(App *app, const SDL_Event *e)
{
    HuntView v;
    if (!game_hunt_view(app->game, &v)) {
        return;
    }
    ChoiceItem items[HUNT_ACTION_COUNT];
    int map[HUNT_ACTION_COUNT];
    int n = hunt_items(app, &v, items, map);
    nav_updown(app, e, n);
    int pick = -1;
    int digit = key_digit(e);
    if (digit >= 0 && digit < n) {
        pick = digit;
    } else if (is_confirm(e)) {
        pick = n == 1 ? 0 : app->sel;
    } else if (is_click(e)) {
        pick = choices_hit(&app->draw, items, n, HUNT_MENU_X, HUNT_MENU_Y, HUNT_MENU_W, app->mx, app->my, true);
    }
    if (pick >= 0 && pick < n && !items[pick].locked) {
        game_hunt_act(app->game, (HuntAction)map[pick]);
        if (app->sel >= n) {
            app->sel = 0;
        }
        app_commit(app);
    }
}

/* A small figure for each kind of game, drawn with rectangles. */
static void draw_animal(Draw *d, const char *id, float x, float y, float s)
{
    SDL_Color body = {150, 110, 60, 255}, dark = {60, 44, 30, 255}, light = {230, 220, 200, 255};
    float w = 18, h = 9;
    if (strcmp(id, "springbok") == 0) {
        body = (SDL_Color){196, 140, 80, 255};
        w = 14;
        h = 7;
    } else if (strcmp(id, "blesbok") == 0) {
        body = (SDL_Color){140, 80, 50, 255};
        w = 15;
    } else if (strcmp(id, "gnu") == 0) {
        body = (SDL_Color){50, 44, 40, 255};
    } else if (strcmp(id, "hartebeest") == 0) {
        body = (SDL_Color){170, 90, 60, 255};
    } else if (strcmp(id, "quagga") == 0) {
        body = (SDL_Color){150, 110, 70, 255};
    } else if (strcmp(id, "ostrich") == 0) {
        body = (SDL_Color){30, 30, 30, 255};
        w = 12;
        h = 8;
    } else if (strcmp(id, "kudu") == 0) {
        body = (SDL_Color){130, 110, 90, 255};
    } else if (strcmp(id, "eland") == 0) {
        body = (SDL_Color){190, 150, 100, 255};
        w = 24;
        h = 12;
    } else if (strcmp(id, "buffalo") == 0) {
        body = (SDL_Color){40, 36, 34, 255};
        w = 24;
        h = 12;
    } else if (strcmp(id, "lion") == 0) {
        body = (SDL_Color){200, 160, 80, 255};
        w = 20;
        h = 9;
    }
    w *= s;
    h *= s;
    float legs = (strcmp(id, "ostrich") == 0 ? 14 : 8) * s;
    draw_rect(d, (SDL_FRect){x, y - legs - h, w, h}, body);
    draw_rect(d, (SDL_FRect){x - 4 * s, y - legs - h - 4 * s, 5 * s, 6 * s}, body); /* head and neck */
    draw_rect(d, (SDL_FRect){x + 1 * s, y - legs, 2 * s, legs}, body);
    draw_rect(d, (SDL_FRect){x + w - 3 * s, y - legs, 2 * s, legs}, body);
    if (strcmp(id, "quagga") == 0) {
        for (int i = 0; i < 3; i++) {
            draw_rect(d, (SDL_FRect){x + i * 3 * s, y - legs - h, 1 * s, h}, light);
        }
    } else if (strcmp(id, "springbok") == 0) {
        draw_rect(d, (SDL_FRect){x, y - legs - h * 0.4f, w, h * 0.4f}, light);
    } else if (strcmp(id, "lion") == 0) {
        draw_rect(d, (SDL_FRect){x - 6 * s, y - legs - h - 6 * s, 8 * s, 9 * s}, (SDL_Color){140, 90, 40, 255});
    } else if (strcmp(id, "buffalo") == 0 || strcmp(id, "gnu") == 0 || strcmp(id, "kudu") == 0) {
        draw_rect(d, (SDL_FRect){x - 6 * s, y - legs - h - 6 * s, 9 * s, 2 * s}, dark); /* horns */
    }
}

void hunt_screen_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    HuntView v;
    if (!game_hunt_view(g, &v)) {
        return;
    }
    char title[160];
    snprintf(title, sizeof title, "%s  ·  %s", app_text(app, "hunt.title"), app_fmt(app, &v.hunter));
    draw_header(app, title, NULL);
    /* the veld */
    SDL_FRect scene = {0, 19, TREK_LOGICAL_W, 132};
    float horizon = scene.y + 70;
    draw_rect(d, (SDL_FRect){0, scene.y, scene.w, horizon - scene.y}, COL_SKY);
    draw_rect(d, (SDL_FRect){0, horizon, scene.w, scene.y + scene.h - horizon}, (SDL_Color){164, 150, 92, 255});
    for (int i = 0; i < 30; i++) {
        float tx = (float)((i * 97) % 640), ty = horizon + 6 + (float)((i * 41) % 54);
        draw_rect(d, (SDL_FRect){tx, ty, 3, 2}, (SDL_Color){130, 118, 70, 255});
    }
    /* the hunter, crouching, with his gun */
    float ground = scene.y + scene.h - 14;
    SDL_Color coat = {80, 60, 40, 255}, hat = {50, 40, 30, 255};
    draw_rect(d, (SDL_FRect){60, ground - 22, 8, 14}, coat);
    draw_rect(d, (SDL_FRect){59, ground - 27, 10, 5}, hat);
    draw_rect(d, (SDL_FRect){61, ground - 8, 3, 8}, coat);
    draw_rect(d, (SDL_FRect){66, ground - 8, 3, 8}, coat);
    draw_rect(d, (SDL_FRect){66, ground - 18, 26, 2}, (SDL_Color){60, 50, 40, 255});
    if (v.animal.key[0]) {
        static const float RANGE_X[4] = {560, 470, 360, 250};
        float scale = 0.8f + 0.25f * v.range;
        draw_animal(d, v.animal_id, RANGE_X[v.range], ground, scale);
        char label[120];
        char range_key[24];
        snprintf(range_key, sizeof range_key, "hunt.range.%d", v.range);
        snprintf(label, sizeof label, "%s, %s %s", app_fmt(app, &v.animal), app_text(app, range_key),
                 v.wounded ? app_text(app, "hunt.wounded_tag") : "");
        draw_text(d, d->font, label, 620, scene.y + 4, COL_BG, TEXT_RIGHT);
    } else {
        draw_text(d, d->font, app_text(app, "hunt.nothing_in_sight"), 620, scene.y + 4, COL_BG, TEXT_RIGHT);
    }
    draw_rect(d, (SDL_FRect){0, 151, TREK_LOGICAL_W, 1}, COL_BORDER);

    /* what has happened */
    float y = 158;
    for (int i = 0; i < v.log_count; i++) {
        SDL_Color c = i == v.log_count - 1 ? COL_TEXT : COL_DIM;
        y += draw_wrapped(d, d->font, app_fmt(app, &v.log[i]), 16, y, 300, c, false);
    }
    /* status */
    LocText t;
    float sy = 262;
    if (v.loaded) {
        loctext_set(&t, "hunt.status.loaded");
    } else {
        loctext_set(&t, v.reload_left > 0 ? "hunt.status.reloading" : "hunt.status.empty");
        loctext_arg_int(&t, "n", v.reload_left);
    }
    draw_text(d, d->font, app_fmt(app, &t), 16, sy, v.loaded ? COL_GOLD : COL_DIM, TEXT_LEFT);
    loctext_set(&t, "hunt.status.meat");
    loctext_arg_int(&t, "meat", v.meat);
    loctext_arg_int(&t, "carry", v.carry);
    draw_text(d, d->font, app_fmt(app, &t), 16, sy + d->line_h, COL_TEXT, TEXT_LEFT);
    loctext_set(&t, "hunt.status.turns");
    loctext_arg_int(&t, "n", v.turns_left);
    draw_text(d, d->font, app_fmt(app, &t), 16, sy + 2 * d->line_h, COL_DIM, TEXT_LEFT);
    loctext_set(&t, "hunt.status.powder");
    loctext_arg_int(&t, "p", v.powder);
    loctext_arg_int(&t, "l", v.lead);
    draw_text(d, d->font, app_fmt(app, &t), 16, sy + 3 * d->line_h, COL_DIM, TEXT_LEFT);

    ChoiceItem items[HUNT_ACTION_COUNT];
    int map[HUNT_ACTION_COUNT];
    int n = hunt_items(app, &v, items, map);
    draw_choices(d, items, n, HUNT_MENU_X, HUNT_MENU_Y, HUNT_MENU_W, app->mx, app->my, app->sel, true, NULL);
    draw_footer(app, app_text(app, "hunt.footer"));
}
