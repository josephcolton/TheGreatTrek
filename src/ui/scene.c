#include "scene.h"

#include "sprites.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Scenes are layered: sky and clouds, far and near hills, the ground with plants and rocks,
 * then the place itself (houses, huts, a river, the laager) and the family's wagon and span.
 * When the wagon is moving, things nearer the viewer slide past faster. Pictures are the
 * pixel-art sprites in assets/sprites/. */

static SDL_Color shade(SDL_Color c, float f)
{
    float r = c.r * f, g = c.g * f, b = c.b * f;
    return (SDL_Color){(Uint8)(r > 255 ? 255 : r), (Uint8)(g > 255 ? 255 : g), (Uint8)(b > 255 ? 255 : b), c.a};
}

static SDL_Color mix(SDL_Color a, SDL_Color b, float f)
{
    return (SDL_Color){(Uint8)(a.r + (b.r - a.r) * f), (Uint8)(a.g + (b.g - a.g) * f), (Uint8)(a.b + (b.b - a.b) * f), 255};
}

static SDL_Color sky_color(const Game *g, bool night)
{
    if (night) {
        return (SDL_Color){20, 24, 44, 255};
    }
    switch (g->weather) {
    case WEATHER_RAIN: return (SDL_Color){96, 106, 116, 255};
    case WEATHER_STORM: return (SDL_Color){62, 66, 80, 255};
    case WEATHER_HOT: return (SDL_Color){150, 168, 182, 255};
    case WEATHER_FROST:
    case WEATHER_COLD: return (SDL_Color){132, 156, 178, 255};
    case WEATHER_WINDY: return (SDL_Color){128, 146, 160, 255};
    default: return (SDL_Color){96, 140, 186, 255};
    }
}

static SDL_Color ground_color(const App *app, int region)
{
    const char *rid = app->data->regions[region].id;
    if (strcmp(rid, "highveld") == 0) {
        return (SDL_Color){168, 152, 94, 255};
    }
    if (strcmp(rid, "eastern_frontier") == 0 || strcmp(rid, "stormberg") == 0) {
        return (SDL_Color){110, 124, 70, 255};
    }
    if (strcmp(rid, "karoo") == 0) {
        return (SDL_Color){176, 140, 96, 255};
    }
    return (SDL_Color){154, 142, 88, 255};
}

SDL_Color scene_ground(const App *app, int region)
{
    return ground_color(app, region);
}

typedef enum { WAGON_TREK, WAGON_TRANSPORT, WAGON_CART } WagonShape;

static WagonShape wagon_shape(const App *app)
{
    const char *id = app->data->wagons[app->game->wagon].id;
    if (strcmp(id, "transport_wagon") == 0) {
        return WAGON_TRANSPORT;
    }
    if (strcmp(id, "scotch_cart") == 0) {
        return WAGON_CART;
    }
    return WAGON_TREK;
}

/* ---------------------------------------------------------------- the land */

/* A smooth line of hills along the horizon; `mesa` flattens the tops like the Karoo koppies. */
static void hills(Draw *d, SDL_FRect r, float horizon, SDL_Color c, float amp, float freq, float seed, float scroll,
                  bool mesa)
{
    for (float x = 0; x < r.w; x += 2) {
        float u = (x + scroll) * freq + seed;
        float h = amp * (0.55f + 0.3f * sinf(u) + 0.15f * sinf(u * 2.7f + seed));
        if (mesa) {
            float top = amp * 0.8f;
            h = h > amp * 0.62f ? top : h * 0.45f;
        }
        draw_rect(d, (SDL_FRect){r.x + x, horizon - h, 2, h + 1}, c);
    }
}

static void sky(Draw *d, SDL_FRect r, float horizon, SDL_Color top, bool night)
{
    SDL_Color low = night ? shade(top, 1.5f) : mix(top, (SDL_Color){236, 228, 210, 255}, 0.45f);
    int bands = 6;
    float bh = (horizon - r.y) / bands;
    for (int i = 0; i < bands; i++) {
        draw_rect(d, (SDL_FRect){r.x, r.y + i * bh, r.w, bh + 1}, mix(top, low, (float)i / (bands - 1)));
    }
}

static void clouds(Draw *d, SDL_FRect r, const Game *g, double t)
{
    bool grey = g->weather == WEATHER_RAIN || g->weather == WEATHER_STORM;
    int n = grey ? 7 : (g->weather == WEATHER_CLEAR || g->weather == WEATHER_HOT ? 2 : 4);
    SDL_Color c = grey ? (SDL_Color){120, 124, 132, 230} : (SDL_Color){244, 242, 236, 220};
    for (int i = 0; i < n; i++) {
        float w = 30.0f + (i * 37) % 40;
        float x = (float)fmod(i * 173.0 + t * (4 + i % 3), r.w + w * 2) - w;
        float y = r.y + 6 + (i * 13) % 26;
        draw_rect(d, (SDL_FRect){r.x + x, y + 4, w, 5}, c);
        draw_rect(d, (SDL_FRect){r.x + x + w * 0.15f, y + 1, w * 0.45f, 4}, c);
        draw_rect(d, (SDL_FRect){r.x + x + w * 0.45f, y, w * 0.35f, 5}, c);
    }
}

static void rain(Draw *d, SDL_FRect r, bool storm, double t)
{
    SDL_Color c = {200, 210, 224, 150};
    int n = storm ? 90 : 50;
    for (int i = 0; i < n; i++) {
        float x = (float)fmod(i * 37.0 + t * 60, r.w);
        float y = (float)fmod(i * 53.0 + t * 240, r.h);
        draw_rect(d, (SDL_FRect){r.x + x, r.y + y, 1, 4}, c);
    }
}

/* Plants and rocks for a region, spread over the ground. Each has a depth: 0 at the horizon,
 * 1 at the front. Deeper things sit lower and slide past faster when the wagon moves. */
typedef struct {
    const char *sprite;
    float x, depth;
} Decor;

static int region_decor(const App *app, int region, Decor *out, int max)
{
    const char *rid = app->data->regions[region].id;
    const char *kinds[6];
    int nk = 0;
    if (strcmp(rid, "eastern_frontier") == 0) {
        kinds[nk++] = "tree_big";
        kinds[nk++] = "aloe";
        kinds[nk++] = "bush";
        kinds[nk++] = "tree_small";
        kinds[nk++] = "aloe";
    } else if (strcmp(rid, "karoo") == 0) {
        kinds[nk++] = "bush";
        kinds[nk++] = "koppie";
        kinds[nk++] = "tuft";
        kinds[nk++] = "bush";
    } else if (strcmp(rid, "stormberg") == 0) {
        kinds[nk++] = "koppie";
        kinds[nk++] = "bush";
        kinds[nk++] = "aloe";
    } else {
        kinds[nk++] = "tuft";
        kinds[nk++] = "tree_small";
        kinds[nk++] = "tuft";
        kinds[nk++] = "bush";
        kinds[nk++] = "tuft";
    }
    int n = 0;
    for (int i = 0; i < max; i++) {
        unsigned h = (unsigned)(i * 2654435761u + (unsigned)region * 40503u);
        out[n].sprite = kinds[(h >> 7) % (unsigned)nk];
        out[n].x = (float)((h >> 3) % 700u);
        /* keep big things to the back so they do not hide the wagon */
        bool big = strcmp(out[n].sprite, "koppie") == 0 || strcmp(out[n].sprite, "tree_big") == 0;
        out[n].depth = big ? (float)((h >> 13) % 30u) / 100.0f : (float)((h >> 13) % 100u) / 100.0f;
        n++;
    }
    return n;
}

static void ground(Draw *d, SDL_FRect r, float horizon, SDL_Color c, bool frost)
{
    float gh = r.y + r.h - horizon;
    for (int i = 0; i < 4; i++) {
        draw_rect(d, (SDL_FRect){r.x, horizon + gh * i / 4.0f, r.w, gh / 4.0f + 1}, shade(c, 0.92f + 0.06f * i));
    }
    if (frost) {
        for (int i = 0; i < 80; i++) {
            draw_rect(d, (SDL_FRect){r.x + (float)((i * 97) % (int)r.w), horizon + (float)((i * 31) % (int)gh), 2, 1},
                      (SDL_Color){230, 236, 240, 200});
        }
    }
}

static void stars_and_moon(Draw *d, SDL_FRect r, float horizon);

void scene_backdrop(App *app, SDL_FRect r, float horizon, int region, float light)
{
    Draw *d = &app->draw;
    const Game *g = app->game;
    SDL_Color skyc = sky_color(g, false), groundc = ground_color(app, region);
    skyc = (SDL_Color){(Uint8)(skyc.r * light), (Uint8)(skyc.g * light), (Uint8)(skyc.b * (0.3f + 0.7f * light)), 255};
    groundc = shade(groundc, light);
    SDL_FRect sky_r = {r.x, r.y, r.w, horizon - r.y};
    sky(d, sky_r, horizon, skyc, false);
    if (light < 0.4f) {
        stars_and_moon(d, sky_r, horizon); /* guarding the herd at night */
    } else {
        clouds(d, sky_r, g, SDL_GetTicks() / 1000.0);
    }
    bool karoo = strcmp(app->data->regions[region].id, "karoo") == 0;
    hills(d, r, horizon + 1, mix(skyc, groundc, 0.45f), karoo ? 18 : 12, 0.017f, 0.7f * region, 0, karoo);
    hills(d, r, horizon + 1, mix(skyc, groundc, 0.75f), karoo ? 9 : 6, 0.029f, 2.1f + region, 0, false);
}

/* ---------------------------------------------------------------- the wagon and span */

/* Draws the wagon with its back end at x and its wheels on the ground at y, the span of oxen in
 * front, and the voorloper leading. Pairs of oxen are yoked side by side: the far ox is drawn
 * a little higher and darker. */
static void wagon(SDL_Renderer *rd, Draw *d, WagonShape shape, float x, float y, int pairs, bool moving, double t,
                  SDL_Color tint)
{
    static const char *NAMES[3][2] = {{"wagon_trek_0", "wagon_trek_1"},
                                      {"wagon_transport_0", "wagon_transport_1"},
                                      {"wagon_cart_0", "wagon_cart_1"}};
    int frame = moving ? (int)(t * 5) % 2 : 0;
    const Sprite *w = sprite_find(NAMES[shape][frame]);
    if (!w) {
        return;
    }
    if (shape == WAGON_CART && pairs > 2) {
        pairs = 2;
    }
    float top = y - w->h;
    float front = x + w->w - 2;
    if (pairs > 0) {
        /* the trek chain runs from the disselboom past every pair */
        SDL_Color chain = shade((SDL_Color){70, 60, 52, 255}, tint.r / 255.0f);
        draw_rect(d, (SDL_FRect){front - 2, y - 6, pairs * 20.0f + 2, 1}, chain);
    }
    for (int p = pairs - 1; p >= 0; p--) {
        float ox = front - 2 + p * 20.0f;
        int leg = moving ? (int)(t * 6 + p) % 2 : 0;
        char far_name[24], near_name[24];
        snprintf(far_name, sizeof far_name, "ox_%s_%d", p % 2 ? "red" : "black", 1 - leg);
        snprintf(near_name, sizeof near_name, "ox_%s_%d", p % 2 ? "black" : "red", leg);
        const Sprite *far_ox = sprite_find(far_name), *near_ox = sprite_find(near_name);
        if (far_ox) {
            sprite_draw_tinted(rd, far_ox, ox + 2, y - far_ox->h - 3, 1, false, shade(tint, 0.72f));
        }
        if (near_ox) {
            sprite_draw_tinted(rd, near_ox, ox, y - near_ox->h, 1, false, tint);
        }
    }
    sprite_draw_tinted(rd, w, x, top, 1, false, tint);
    if (pairs > 0) {
        float vx = front + pairs * 20.0f + 2;
        const Sprite *boy = sprite_find(moving && (int)(t * 6) % 2 ? "voorloper_1" : "voorloper_0");
        if (boy) {
            draw_rect(d, (SDL_FRect){vx - 5, y - 9, 6, 1}, shade((SDL_Color){90, 64, 40, 255}, tint.r / 255.0f));
            sprite_draw_tinted(rd, boy, vx, y - boy->h, 1, false, tint);
        }
    }
}

/* ---------------------------------------------------------------- places */

static void river_band(Draw *d, SDL_FRect r, float wy, float h, double t)
{
    SDL_Color water = {64, 106, 146, 255}, bank = {120, 100, 70, 255};
    draw_rect(d, (SDL_FRect){r.x, wy - 2, r.w, 2}, bank);
    draw_rect(d, (SDL_FRect){r.x, wy, r.w, h}, water);
    draw_rect(d, (SDL_FRect){r.x, wy, r.w, 2}, (SDL_Color){90, 132, 170, 255});
    draw_rect(d, (SDL_FRect){r.x, wy + h, r.w, 2}, bank);
    for (int i = 0; i < 26; i++) {
        float wx = (float)fmod(i * 67.0 + t * 16.0, r.w);
        draw_rect(d, (SDL_FRect){r.x + wx, wy + 3 + (i % 4) * (h - 5) / 4.0f, 6, 1}, (SDL_Color){150, 184, 206, 255});
    }
}

static void stars_and_moon(Draw *d, SDL_FRect r, float horizon)
{
    for (int i = 0; i < 60; i++) {
        Uint8 b = (Uint8)(160 + (i * 37) % 90);
        draw_rect(d, (SDL_FRect){r.x + (float)((i * 151) % (int)r.w), r.y + (float)((i * 37) % (int)(horizon - r.y - 4)), 1, 1},
                  (SDL_Color){b, b, (Uint8)(b - 20), 255});
    }
    float mx = r.x + r.w * 0.8f, my = r.y + 12;
    draw_rect(d, (SDL_FRect){mx, my, 8, 8}, (SDL_Color){236, 232, 204, 255});
    draw_rect(d, (SDL_FRect){mx - 1, my + 1, 10, 6}, (SDL_Color){236, 232, 204, 255});
    draw_rect(d, (SDL_FRect){mx + 3, my + 1, 6, 6}, (SDL_Color){20, 24, 44, 255}); /* a crescent */
}

/* Each town on the road has its own picture; Grahamstown's is the default. */
typedef enum { TOWN_GRAHAMSTOWN, TOWN_CRADOCK, TOWN_COLESBERG, TOWN_PHILIPPOLIS } TownLook;

static TownLook town_look(const App *app)
{
    const char *id = app->game ? app->data->nodes[app->game->node].id : "";
    if (strcmp(id, "cradock") == 0) {
        return TOWN_CRADOCK;
    }
    if (strcmp(id, "colesberg") == 0) {
        return TOWN_COLESBERG;
    }
    if (strcmp(id, "philippolis") == 0) {
        return TOWN_PHILIPPOLIS;
    }
    return TOWN_GRAHAMSTOWN;
}

/* Coleskop, the steep hill with a flat cap of rock that stands over Colesberg. */
static void coleskop(Draw *d, SDL_FRect r, float horizon, SDL_Color c)
{
    SDL_Color dark = shade(c, 0.72f), cap = shade(c, 0.6f);
    for (float dx = -70; dx <= 70; dx += 1) {
        float u = fabsf(dx) / 70.0f;
        float h = 52 * powf(1 - u, 1.4f) + 1.2f * sinf(dx * 0.5f);
        if (h > 36) {
            h = 36 + (h - 36) * 0.1f; /* the flat top */
        }
        float x = r.x + 440 + dx;
        /* lit from the left: the shadow side starts a little right of the middle and widens downward */
        float lit = h - (36 - 0.6f * dx);
        draw_rect(d, (SDL_FRect){x, horizon - h, 1, h + 1}, dark);
        if (lit < h) {
            draw_rect(d, (SDL_FRect){x, horizon - h, 1, h - fmaxf(lit, 0) + 1}, c);
        }
        if (h > 30) {
            float band = fminf(5, h - 30) + (((int)dx * 7) % 3 == 0 ? 1 : 0);
            draw_rect(d, (SDL_FRect){x, horizon - h, 1, band}, cap);
        }
    }
}

static void town(SDL_Renderer *rd, Draw *d, TownLook look, SDL_FRect r, float base, WagonShape ws, int pairs,
                 SDL_Color tint)
{
    switch (look) {
    case TOWN_CRADOCK:
        /* a Karoo town on the Great Fish River: flat-roofed houses, a church, willows on the furrows */
        sprite_stand(rd, "willow", r.x + 34, base - 12, 1, false, tint);
        sprite_stand(rd, "church_karoo", r.x + 112, base - 8, 1, false, tint);
        sprite_stand(rd, "house_flat", r.x + 186, base - 10, 1, false, tint);
        sprite_stand(rd, "house_flat_small", r.x + 470, base - 10, 1, true, tint);
        sprite_stand(rd, "house_flat", r.x + 530, base - 12, 1, false, tint);
        sprite_stand(rd, "willow", r.x + 606, base - 8, 1, true, tint);
        /* water furrows along the gardens on either side of the road */
        draw_rect(d, (SDL_FRect){r.x, base - 5, 230, 2}, (SDL_Color){84, 120, 150, 255});
        draw_rect(d, (SDL_FRect){r.x + 440, base - 5, r.w - 440, 2}, (SDL_Color){84, 120, 150, 255});
        break;
    case TOWN_COLESBERG:
        /* a young frontier town in a hollow under Coleskop (drawn with the hills) */
        sprite_stand(rd, "koppie", r.x + 40, base - 16, 1, false, tint);
        sprite_stand(rd, "house_flat_small", r.x + 92, base - 12, 1, false, tint);
        sprite_stand(rd, "house_flat", r.x + 160, base - 9, 1, true, tint);
        sprite_stand(rd, "house_flat_small", r.x + 470, base - 14, 1, true, tint);
        sprite_stand(rd, "house_cape", r.x + 540, base - 10, 1, false, tint);
        sprite_stand(rd, "house_flat_small", r.x + 612, base - 7, 1, false, tint);
        break;
    case TOWN_PHILIPPOLIS:
        /* the mission among the Griqua: clay houses, reed-mat houses and a stone cattle kraal */
        sprite_stand(rd, "mat_house", r.x + 26, base - 14, 1, false, tint);
        sprite_stand(rd, "house_griqua", r.x + 78, base - 10, 1, false, tint);
        sprite_stand(rd, "church_mission", r.x + 160, base - 6, 1, false, tint);
        sprite_stand(rd, "house_griqua", r.x + 450, base - 12, 1, true, tint);
        sprite_stand(rd, "ox_red_0", r.x + 543, base - 8, 1, true, tint);
        sprite_stand(rd, "ox_black_0", r.x + 578, base - 7, 1, false, tint);
        sprite_stand(rd, "kraal", r.x + 560, base - 6, 1, false, tint);
        sprite_stand(rd, "mat_house", r.x + 622, base - 9, 1, true, tint);
        break;
    case TOWN_GRAHAMSTOWN:
    default:
        sprite_stand(rd, "house_cottage", r.x + 70, base - 8, 1, false, tint);
        sprite_stand(rd, "house_cape", r.x + 150, base - 4, 1, false, tint);
        sprite_stand(rd, "tree_small", r.x + 214, base - 6, 1, false, tint);
        sprite_stand(rd, "church", r.x + 556, base - 2, 1, false, tint);
        sprite_stand(rd, "house_cottage", r.x + 480, base - 6, 1, true, tint);
        break;
    }
    wagon(rd, d, ws, r.x + 250, base + 6, pairs > 4 ? 4 : pairs, false, 0, tint);
}

void scene_draw(App *app, SDL_FRect r, SceneKind kind, int region, bool moving)
{
    Draw *d = &app->draw;
    SDL_Renderer *rd = app->renderer;
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
    SDL_Rect clip = {(int)r.x, (int)r.y, (int)r.w, (int)r.h};
    SDL_SetRenderClipRect(rd, &clip);
    bool night = kind == SCENE_NIGHT;
    double now = SDL_GetTicks() / 1000.0;
    double t = moving ? now : 0;
    SDL_Color tint = night ? (SDL_Color){110, 110, 150, 255} : (SDL_Color){255, 255, 255, 255};
    SDL_Color skyc = sky_color(g, night), groundc = ground_color(app, region);
    if (night) {
        groundc = shade(groundc, 0.32f);
    } else if (g->weather == WEATHER_RAIN || g->weather == WEATHER_STORM) {
        groundc = shade(groundc, 0.82f);
    }
    float horizon = r.y + r.h * 0.5f;
    sky(d, r, horizon, skyc, night);
    if (night) {
        stars_and_moon(d, r, horizon);
    } else {
        clouds(d, r, g, now);
    }
    const char *rid = app->data->regions[region].id;
    bool karoo = strcmp(rid, "karoo") == 0;
    SDL_Color far_hill = mix(skyc, groundc, 0.45f), near_hill = mix(skyc, groundc, 0.75f);
    if (kind == SCENE_MOUNTAIN) {
        hills(d, r, horizon + 1, far_hill, 46, 0.018f, 1.3f, (float)(t * 2), false);
        hills(d, r, horizon + 1, near_hill, 30, 0.031f, 4.1f, (float)(t * 5), false);
    } else if (kind == SCENE_MISSION) {
        /* Thaba Nchu, the Black Mountain, stands over the mission */
        hills(d, r, horizon + 1, far_hill, 10, 0.02f, 2.0f, 0, false);
        /* a long, round-shouldered ridge with a few gullies */
        SDL_Color dark = shade(near_hill, 0.68f), gully = shade(near_hill, 0.56f);
        for (float dx = -130; dx <= 130; dx += 1) {
            float u = dx / 130.0f;
            float h = 34 * powf(1 - u * u, 0.45f) + 2 * sinf(dx * 0.21f) + 1.5f * sinf(dx * 0.07f + 1);
            if (h < 0) {
                continue;
            }
            draw_rect(d, (SDL_FRect){r.x + 470 + dx, horizon - h, 1, h + 1}, dx < -40 ? gully : dark);
            draw_rect(d, (SDL_FRect){r.x + 470 + dx, horizon - h, 1, 1}, shade(near_hill, 0.8f));
        }
    } else if (kind == SCENE_TOWN && town_look(app) == TOWN_COLESBERG) {
        hills(d, r, horizon + 1, far_hill, 14, 0.017f, 0.7f * region, 0, true);
        coleskop(d, r, horizon + 1, near_hill);
    } else {
        hills(d, r, horizon + 1, far_hill, karoo ? 22 : 14, 0.017f, 0.7f * region, (float)(t * 2), karoo);
        hills(d, r, horizon + 1, near_hill, karoo ? 12 : 8, 0.029f, 2.1f + region, (float)(t * 6), false);
    }
    ground(d, r, horizon, groundc, g->weather == WEATHER_FROST && !night);

    float gh = r.y + r.h - horizon;
    float base = horizon + gh * 0.62f; /* where the wagon's wheels touch the ground */
    int oxen = g->inv[app->data->item_oxen];
    int pairs = oxen / 2 > 7 ? 7 : oxen / 2;
    WagonShape ws = wagon_shape(app);

    /* the land's plants and rocks, behind the wagon line */
    Decor decor[18];
    int nd = (kind == SCENE_ROAD || kind == SCENE_MOUNTAIN) ? region_decor(app, region, decor, 18) : 0;
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            switch (kind) {
            case SCENE_TOWN:
                town(rd, d, town_look(app), r, base, ws, pairs, tint);
                break;
            case SCENE_MISSION:
                for (int i = 0; i < 7; i++) {
                    sprite_stand(rd, i % 2 ? "hut_1" : "hut_0", r.x + 30 + i * 26.0f, base - 10 + (i % 3) * 3, 1, i % 2, tint);
                }
                sprite_stand(rd, "church_mission", r.x + 240, base - 4, 1, false, tint);
                wagon(rd, d, ws, r.x + 300, base + 8, pairs > 3 ? 3 : pairs, false, 0, tint);
                break;
            case SCENE_RIVER:
            case SCENE_END: {
                float wy = base - 12;
                river_band(d, r, wy, 12, now);
                if (kind == SCENE_END) {
                    for (int i = 0; i < 5; i++) {
                        sprite_stand(rd, "tree_big", r.x + 50 + i * 130.0f, wy - 2, 1, i % 2, tint);
                    }
                } else {
                    sprite_stand(rd, "tree_small", r.x + 80, wy - 2, 1, false, tint);
                    sprite_stand(rd, "tree_big", r.x + 560, wy - 2, 1, true, tint);
                }
                wagon(rd, d, ws, r.x + 330, base + 10, pairs > 5 ? 5 : pairs, false, 0, tint);
                break;
            }
            case SCENE_LAAGER:
                /* the wagons drawn up nose to tail in a ring, the back of the ring first */
                for (int half = 0; half < 2; half++) {
                    for (int i = 0; i < 18; i++) {
                        float a = (float)i / 18.0f * 6.2832f;
                        float s = sinf(a);
                        if ((half == 0) != (s < 0)) {
                            continue;
                        }
                        float lx = r.x + 320 + cosf(a) * 112, ly = base + s * 13;
                        sprite_stand(rd, "laager_wagon", lx, ly, 1, s > 0, tint);
                    }
                    if (half == 0) {
                        char fire[8];
                        snprintf(fire, sizeof fire, "fire_%d", (int)(now * 8) % 3);
                        sprite_stand(rd, "sitter_man", r.x + 300, base + 2, 1, false, tint);
                        sprite_stand(rd, "sitter_woman", r.x + 340, base + 2, 1, true, tint);
                        sprite_stand(rd, fire, r.x + 320, base + 2, 1, false, tint);
                    }
                }
                break;
            case SCENE_NIGHT: {
                wagon(rd, d, ws, r.x + 380, base + 2, 0, false, 0, tint);
                float fx = r.x + 280, fy = base + 12;
                /* the camp is drawn twice as large: it is the subject of the scene */
                sprite_stand(rd, "sitter_man", fx - 38, fy, 2, false, tint);
                sprite_stand(rd, "sitter_woman", fx + 38, fy, 2, true, tint);
                sprite_stand(rd, "pot", fx + 18, fy, 2, false, tint);
                char fire[8];
                snprintf(fire, sizeof fire, "fire_%d", (int)(now * 8) % 3);
                /* firelight on the ground, fading out from the fire */
                for (int i = 0; i < 4; i++) {
                    float rw = 64 - i * 14.0f, rh = 8 - i * 1.5f;
                    for (float dy = -rh; dy <= rh; dy += 1) {
                        float w = rw * sqrtf(1 - (dy / rh) * (dy / rh));
                        draw_rect(d, (SDL_FRect){fx - w, fy - 2 + dy, 2 * w, 1}, (SDL_Color){230, 150, 70, 24});
                    }
                }
                sprite_stand(rd, fire, fx, fy, 2, false, (SDL_Color){255, 255, 255, 255});
                break;
            }
            case SCENE_MOUNTAIN:
            case SCENE_ROAD:
            default: {
                /* the oxen take ~7 sprites; leave room so the voorloper stays on screen */
                float wx = r.x + (ws == WAGON_TRANSPORT ? 150 : 180) - pairs * 9.0f;
                wagon(rd, d, ws, wx, base + 4, pairs, moving, t, tint);
                break;
            }
            }
        }
        for (int i = 0; i < nd; i++) {
            bool front = decor[i].depth > 0.8f;
            if (front != (pass == 1)) {
                continue;
            }
            float speed = 10 + decor[i].depth * 50;
            float x = (float)fmod(decor[i].x - t * speed + 2000.0, 700.0) - 30;
            float y = horizon + 3 + decor[i].depth * (gh - 6);
            if (front) {
                y = base + 8 + (decor[i].depth - 0.8f) * 5 * (r.y + r.h - base - 8);
            }
            sprite_stand(rd, decor[i].sprite, r.x + x, y, 1, i % 2, tint);
        }
    }
    if (!night && (g->weather == WEATHER_RAIN || g->weather == WEATHER_STORM)) {
        rain(d, r, g->weather == WEATHER_STORM, now);
    }
    SDL_SetRenderClipRect(rd, NULL);
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
