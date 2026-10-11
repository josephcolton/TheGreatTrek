#include "scene.h"

#include <math.h>
#include <string.h>

static SDL_Color shade(SDL_Color c, float f)
{
    float r = c.r * f, g = c.g * f, b = c.b * f;
    return (SDL_Color){(Uint8)(r > 255 ? 255 : r), (Uint8)(g > 255 ? 255 : g), (Uint8)(b > 255 ? 255 : b), 255};
}

static SDL_Color sky_color(const Game *g, bool night)
{
    if (night) {
        return (SDL_Color){24, 28, 48, 255};
    }
    switch (g->weather) {
    case WEATHER_RAIN: return (SDL_Color){96, 106, 116, 255};
    case WEATHER_STORM: return (SDL_Color){62, 66, 80, 255};
    case WEATHER_HOT: return (SDL_Color){178, 170, 132, 255};
    case WEATHER_FROST:
    case WEATHER_COLD: return (SDL_Color){150, 168, 182, 255};
    case WEATHER_WINDY: return (SDL_Color){140, 140, 128, 255};
    default: return COL_SKY;
    }
}

static SDL_Color ground_color(const App *app, int region)
{
    const char *rid = app->data->regions[region].id;
    if (strcmp(rid, "highveld") == 0) {
        return (SDL_Color){164, 150, 92, 255};
    }
    if (strcmp(rid, "eastern_frontier") == 0 || strcmp(rid, "stormberg") == 0) {
        return (SDL_Color){104, 120, 70, 255};
    }
    if (strcmp(rid, "karoo") == 0) {
        return (SDL_Color){170, 136, 92, 255};
    }
    return (SDL_Color){150, 140, 88, 255};
}

static void wagon(Draw *d, float x, float y, int pairs, bool moving, double t)
{
    SDL_Color canvas = {226, 216, 190, 255}, wood = {98, 70, 44, 255}, ox = {74, 52, 36, 255};
    float bob = moving ? (float)((int)(t * 4) % 2) : 0;
    draw_rect(d, (SDL_FRect){x, y - 18 + bob, 56, 16}, canvas);
    draw_rect(d, (SDL_FRect){x - 2, y - 2 + bob, 60, 6}, wood);
    draw_rect(d, (SDL_FRect){x + 2, y + 4, 10, 10}, wood);
    draw_rect(d, (SDL_FRect){x + 44, y + 4, 12, 12}, wood);
    for (int p = 0; p < pairs; p++) {
        float ox_x = x - 18 - p * 16.0f;
        float leg = moving ? (float)((int)(t * 6 + p) % 2) : 0;
        draw_rect(d, (SDL_FRect){ox_x, y + 2, 13, 7}, ox);
        draw_rect(d, (SDL_FRect){ox_x + 1, y + 9, 2, 4 + leg}, ox);
        draw_rect(d, (SDL_FRect){ox_x + 10, y + 9, 2, 5 - leg}, ox);
    }
    if (pairs > 0) {
        draw_rect(d, (SDL_FRect){x - 18 - (pairs - 1) * 16.0f - 2, y + 5, (pairs - 1) * 16.0f + 20, 1}, wood);
    }
}

static void house(Draw *d, float x, float base, float w, float h, SDL_Color wall, SDL_Color roof)
{
    draw_rect(d, (SDL_FRect){x, base - h, w, h}, wall);
    for (int i = 0; i < 5; i++) {
        float inset = i * (w / 10.0f);
        draw_rect(d, (SDL_FRect){x - 2 + inset, base - h - 2 - i * 2.0f, w + 4 - 2 * inset, 2}, roof);
    }
    draw_rect(d, (SDL_FRect){x + w / 2 - 2, base - 7, 4, 7}, shade(roof, 0.6f));
}

static void hut(Draw *d, float x, float base, SDL_Color wall, SDL_Color thatch)
{
    draw_rect(d, (SDL_FRect){x, base - 8, 14, 8}, wall);
    draw_rect(d, (SDL_FRect){x - 1, base - 11, 16, 3}, thatch);
    draw_rect(d, (SDL_FRect){x + 2, base - 13, 10, 2}, thatch);
    draw_rect(d, (SDL_FRect){x + 5, base - 15, 4, 2}, thatch);
}

static void mountain(Draw *d, float cx, float base, float h, float w, SDL_Color c)
{
    int steps = (int)(h / 3);
    for (int i = 0; i < steps; i++) {
        float f = (float)i / steps;
        float ww = w * (1.0f - f);
        draw_rect(d, (SDL_FRect){cx - ww / 2, base - (i + 1) * 3.0f, ww, 3}, c);
    }
}

void scene_draw(App *app, SDL_FRect r, SceneKind kind, int region, bool moving)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    if (!g) {
        return;
    }
    if (region < 0) {
        region = g->edge >= 0 ? app->data->edges[g->edge].region : 0;
        if (g->edge < 0) {
            for (int e = 0; e < app->data->edge_count; e++) {
                if (app->data->edges[e].to == g->node) {
                    region = app->data->edges[e].region;
                }
            }
        }
    }
    bool night = kind == SCENE_NIGHT;
    double t = moving ? SDL_GetTicks() / 1000.0 : 0;
    SDL_Color sky = sky_color(g, night), ground = ground_color(app, region);
    if (night) {
        ground = shade(ground, 0.3f);
    }
    float horizon = r.y + r.h * 0.55f;
    draw_rect(d, (SDL_FRect){r.x, r.y, r.w, horizon - r.y}, sky);
    draw_rect(d, (SDL_FRect){r.x, horizon, r.w, r.y + r.h - horizon}, ground);
    if (night) {
        for (int i = 0; i < 40; i++) {
            draw_rect(d, (SDL_FRect){r.x + (float)((i * 151) % (int)r.w), r.y + (float)((i * 37) % (int)(horizon - r.y - 4)), 1, 1},
                      (SDL_Color){220, 220, 200, 255});
        }
    }
    SDL_Color hill = shade(sky, 0.7f);
    if (kind == SCENE_MOUNTAIN) {
        mountain(d, r.x + 120, horizon + 2, 40, 200, shade(ground, 0.7f));
        mountain(d, r.x + 330, horizon + 2, 55, 260, shade(ground, 0.6f));
        mountain(d, r.x + 540, horizon + 2, 35, 180, shade(ground, 0.7f));
    } else {
        for (int i = 0; i < 6; i++) {
            float hx = r.x + i * 120.0f - 30;
            draw_rect(d, (SDL_FRect){hx, horizon - 10 - (i % 3) * 4, 90, 10 + (i % 3) * 4.0f}, hill);
        }
    }
    SDL_Color tuft = shade(ground, 0.75f);
    for (int i = 0; i < 24; i++) {
        float tx = (float)fmod(i * 53.0 + t * 30.0, r.w);
        float ty = horizon + 6 + (float)((i * 37) % (int)(r.y + r.h - horizon - 8));
        draw_rect(d, (SDL_FRect){r.x + r.w - tx, ty, 3, 2}, tuft);
    }
    float base = horizon + (r.h * 0.45f) * 0.5f;
    int oxen = g->inv[app->data->item_oxen];
    int pairs = oxen / 2 > 7 ? 7 : oxen / 2;
    SDL_Color wall = {214, 204, 180, 255}, roof = {120, 90, 60, 255};
    switch (kind) {
    case SCENE_TOWN:
        house(d, r.x + 60, base, 40, 18, wall, roof);
        house(d, r.x + 120, base - 4, 30, 14, wall, (SDL_Color){100, 80, 60, 255});
        house(d, r.x + 170, base, 50, 20, (SDL_Color){190, 120, 90, 255}, roof);
        house(d, r.x + 470, base - 2, 36, 16, wall, roof);
        /* church tower */
        draw_rect(d, (SDL_FRect){r.x + 540, base - 34, 14, 34}, wall);
        draw_rect(d, (SDL_FRect){r.x + 536, base - 38, 22, 4}, roof);
        wagon(d, r.x + 330, base, pairs > 4 ? 4 : pairs, false, 0);
        break;
    case SCENE_MISSION:
        for (int i = 0; i < 9; i++) {
            hut(d, r.x + 30 + i * 22.0f + (i % 2) * 5, base - (i % 3) * 3, (SDL_Color){160, 120, 80, 255}, (SDL_Color){120, 100, 60, 255});
        }
        mountain(d, r.x + 520, horizon + 2, 45, 200, (SDL_Color){50, 50, 46, 255}); /* the Black Mountain */
        draw_rect(d, (SDL_FRect){r.x + 260, base - 26, 30, 26}, wall);
        draw_rect(d, (SDL_FRect){r.x + 271, base - 36, 8, 10}, wall);
        draw_rect(d, (SDL_FRect){r.x + 274, base - 42, 2, 6}, roof);
        wagon(d, r.x + 380, base + 6, pairs > 3 ? 3 : pairs, false, 0);
        break;
    case SCENE_RIVER:
    case SCENE_END: {
        SDL_Color water = {70, 110, 150, 255};
        float wy = base - 4;
        draw_rect(d, (SDL_FRect){r.x, wy, r.w, 14}, water);
        for (int i = 0; i < 20; i++) {
            float wx = (float)fmod(i * 67.0 + SDL_GetTicks() / 60.0, r.w);
            draw_rect(d, (SDL_FRect){r.x + wx, wy + 3 + (i % 3) * 3, 6, 1}, (SDL_Color){150, 180, 200, 255});
        }
        if (kind == SCENE_END) {
            for (int i = 0; i < 5; i++) {
                float tx = r.x + 60 + i * 120.0f;
                draw_rect(d, (SDL_FRect){tx, wy - 20, 4, 20}, (SDL_Color){90, 70, 50, 255});
                draw_rect(d, (SDL_FRect){tx - 10, wy - 34, 24, 16}, (SDL_Color){100, 140, 70, 255});
            }
        }
        wagon(d, r.x + 420, wy - 6, pairs > 5 ? 5 : pairs, false, 0);
        break;
    }
    case SCENE_LAAGER:
        for (int i = 0; i < 9; i++) {
            float a = (float)i / 9.0f * 6.283f;
            float lx = r.x + 320 + cosf(a) * 150, ly = base + sinf(a) * 14;
            draw_rect(d, (SDL_FRect){lx - 12, ly - 12, 24, 10}, (SDL_Color){226, 216, 190, 255});
            draw_rect(d, (SDL_FRect){lx - 13, ly - 2, 26, 3}, (SDL_Color){98, 70, 44, 255});
        }
        break;
    case SCENE_NIGHT: {
        wagon(d, r.x + 380, base, 0, false, 0);
        /* the fire */
        float fx = r.x + 300, fy = base + 8;
        int flick = (int)(SDL_GetTicks() / 120) % 3;
        draw_rect(d, (SDL_FRect){fx - 8, fy, 16, 3}, (SDL_Color){90, 60, 40, 255});
        draw_rect(d, (SDL_FRect){fx - 5, fy - 6 - flick, 10, 6 + flick}, (SDL_Color){230, 140, 50, 255});
        draw_rect(d, (SDL_FRect){fx - 2, fy - 10 - flick, 4, 5}, (SDL_Color){250, 210, 90, 255});
        break;
    }
    case SCENE_MOUNTAIN:
    case SCENE_ROAD:
    default:
        wagon(d, r.x + r.w * 0.55f, base, pairs, moving, t);
        break;
    }
}

SceneKind scene_for_game(const App *app)
{
    const Game *g = app->game;
    const GameData *d = app->data;
    if (!g) {
        return SCENE_ROAD;
    }
    if (g->dlg.kind == DLG_EVENT) {
        const EventDef *ev = &d->events[g->dlg.event];
        if (ev->night) {
            return SCENE_NIGHT;
        }
        if (!ev->setpiece) {
            int region = g->edge >= 0 ? d->edges[g->edge].region : -1;
            return (region >= 0 && strcmp(d->regions[region].id, "stormberg") == 0) ? SCENE_MOUNTAIN : SCENE_ROAD;
        }
    }
    if (g->dlg.kind == DLG_RIVER) {
        return SCENE_RIVER;
    }
    if (g->edge < 0 || g->at_node) {
        const NodeDef *n = &d->nodes[g->node];
        switch (n->kind) {
        case NODE_TOWN: return SCENE_TOWN;
        case NODE_RIVER: return SCENE_RIVER;
        case NODE_MISSION: return SCENE_MISSION;
        case NODE_END: return SCENE_END;
        default:
            if (strcmp(n->id, "sand_river") == 0) {
                return SCENE_LAAGER;
            }
            if (strcmp(n->id, "stormberg") == 0) {
                return SCENE_MOUNTAIN;
            }
            return SCENE_ROAD;
        }
    }
    return SCENE_ROAD;
}
