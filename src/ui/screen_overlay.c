/* Overlays drawn over the game: pause menu, orders, supplies, party, map, journal, history notes. */
#include "calendar.h"
#include "format.h"
#include "screens.h"
#include "sprites.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void close_overlay(App *app)
{
    app->overlay = OV_NONE;
    app->sel = 0;
    app->map_sheet = 0;
}

static void overlay_panel(App *app, const char *title)
{
    Draw *d = &app->draw;
    draw_rect(d, (SDL_FRect){0, 0, TREK_LOGICAL_W, TREK_LOGICAL_H}, (SDL_Color){0, 0, 0, 160});
    draw_panel(d, (SDL_FRect){16, 12, TREK_LOGICAL_W - 32, TREK_LOGICAL_H - 24});
    draw_text(d, d->font, title, 28, 18, COL_GOLD, TEXT_LEFT);
    draw_rect(d, (SDL_FRect){28, 36, TREK_LOGICAL_W - 56, 1}, COL_BORDER);
}

/* ---------------------------------------------------------------- pause */

typedef enum { PA_RESUME, PA_SAVE_QUIT, PA_GLOSSARY, PA_SETTINGS, PA_COUNT } PauseItem;

static void pause_items(App *app, ChoiceItem *items)
{
    items[PA_RESUME] = (ChoiceItem){app_text(app, "pause.resume"), NULL};
    items[PA_SAVE_QUIT] = (ChoiceItem){app_text(app, "pause.save_quit"), NULL};
    items[PA_GLOSSARY] = (ChoiceItem){app_text(app, "glossary.title"), NULL};
    items[PA_SETTINGS] = (ChoiceItem){app_text(app, "pause.settings"), NULL};
}

static void pause_activate(App *app, int item)
{
    switch (item) {
    case PA_RESUME: close_overlay(app); break;
    case PA_SAVE_QUIT:
        app_commit(app);
        app_close_game(app);
        trips_scan(app->pref_path, &app->trips);
        app_switch(app, SCR_TITLE);
        break;
    case PA_GLOSSARY: app->overlay = OV_GLOSSARY; app->scroll = 0; break;
    case PA_SETTINGS: app_switch(app, SCR_SETTINGS); break;
    default: break;
    }
}

/* ---------------------------------------------------------------- glossary */

static int glossary_terms(App *app, char ids[][32], int max)
{
    const char *list = app_text(app, "glossary.list");
    int n = 0;
    const char *p = list;
    while (*p && n < max) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        snprintf(ids[n++], 32, "%.*s", (int)len, p);
        p += len + (comma ? 1 : 0);
    }
    return n;
}

static void glossary_render(App *app)
{
    Draw *d = &app->draw;
    overlay_panel(app, app_text(app, "glossary.title"));
    char ids[40][32];
    int n = glossary_terms(app, ids, 40);
    float y = 42;
    for (int i = app->scroll; i < n && y < TREK_LOGICAL_H - 50; i++) {
        char key[64];
        snprintf(key, sizeof key, "glossary.%s.term", ids[i]);
        draw_text(d, d->font, app_text(app, key), 36, y, COL_GOLD, TEXT_LEFT);
        snprintf(key, sizeof key, "glossary.%s.text", ids[i]);
        y += draw_wrapped(d, d->font, app_text(app, key), 200, y, 404, COL_TEXT, false) + 4;
    }
    draw_text(d, d->font, app_text(app, "glossary.footer"), TREK_LOGICAL_W / 2.0f, TREK_LOGICAL_H - 32, COL_DIM, TEXT_CENTER);
}

/* ---------------------------------------------------------------- orders */

typedef enum { OR_PACE, OR_RATIONS, OR_NIGHT, OR_SABBATH, OR_COUNT } OrderItem;

static void orders_change(App *app, int item, int delta)
{
    Game *g = app->game;
    Pace p = g->pace;
    Rations r = g->rations;
    NightStance s = g->stance;
    bool sab = g->sabbath;
    switch (item) {
    case OR_PACE: p = (Pace)((p + PACE_COUNT + delta) % PACE_COUNT); break;
    case OR_RATIONS: r = (Rations)((r + RATIONS_COUNT + delta) % RATIONS_COUNT); break;
    case OR_NIGHT: s = (NightStance)((s + NIGHT_COUNT + delta) % NIGHT_COUNT); break;
    case OR_SABBATH: sab = !sab; break;
    default: break;
    }
    game_set_orders(g, p, r, s, sab);
    app_commit(app);
}

static void orders_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    overlay_panel(app, app_text(app, "orders.title"));
    char keys[OR_COUNT][48];
    snprintf(keys[OR_PACE], 48, "pace.%s", pace_id(g->pace));
    snprintf(keys[OR_RATIONS], 48, "rations.%s", rations_id(g->rations));
    snprintf(keys[OR_NIGHT], 48, "stance.%s", stance_id(g->stance));
    snprintf(keys[OR_SABBATH], 48, "settings.%s", g->sabbath ? "on" : "off");
    static const char *LABELS[OR_COUNT] = {"orders.pace", "orders.rations", "orders.night", "orders.sabbath"};
    float y = 48;
    for (int i = 0; i < OR_COUNT; i++) {
        bool hot = i == app->sel;
        if (hot) {
            draw_rect(d, (SDL_FRect){28, y, 584, (float)d->line_h}, COL_HILITE_BG);
        }
        draw_text(d, d->font, app_text(app, LABELS[i]), 36, y, hot ? COL_GOLD : COL_TEXT, TEXT_LEFT);
        char val[96];
        snprintf(val, sizeof val, "< %s >", app_text(app, keys[i]));
        draw_text(d, d->font, val, 300, y, COL_TEXT, TEXT_LEFT);
        y += d->line_h;
    }
    char about[80];
    static const char *ABOUT[OR_COUNT] = {"orders.pace.about", "orders.rations.about", "orders.night.about", "orders.sabbath.about"};
    snprintf(about, sizeof about, "%s", ABOUT[app->sel < OR_COUNT ? app->sel : 0]);
    draw_wrapped(d, d->font, app_text(app, about), 36, y + 12, 568, COL_DIM, false);
    draw_text(d, d->font, app_text(app, "orders.footer"), TREK_LOGICAL_W / 2.0f, TREK_LOGICAL_H - 32, COL_DIM, TEXT_CENTER);
}

/* ---------------------------------------------------------------- supplies */

static void supplies_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    const GameData *data = app->data;
    overlay_panel(app, app_text(app, "supplies.title"));
    float x = 36, y = 44;
    int col = 0;
    for (int i = 0; i < data->item_count; i++) {
        if (g->inv[i] <= 0) {
            continue;
        }
        char label_key[KEY_LEN + 8], line[160];
        snprintf(label_key, sizeof label_key, "%s.label", data->items[i].name_key);
        const char *label = locale_find(app->loc, label_key);
        snprintf(line, sizeof line, "%s", label ? label : app_text(app, data->items[i].name_key));
        draw_text(d, d->font, line, x, y, COL_TEXT, TEXT_LEFT);
        snprintf(line, sizeof line, "%d", g->inv[i]);
        draw_text(d, d->font, line, x + 260, y, COL_GOLD, TEXT_RIGHT);
        y += d->line_h;
        if (y > 250 && col == 0) {
            col = 1;
            x = 330;
            y = 44;
        }
    }
    LocText t;
    loctext_set(&t, "supplies.wagon");
    char wkey[KEY_LEN];
    snprintf(wkey, sizeof wkey, "%s", data->wagons[g->wagon].name_key);
    loctext_arg_key(&t, "wagon", wkey);
    loctext_arg_int(&t, "wheels", g->wagon_parts[WAGON_PART_WHEELS]);
    loctext_arg_int(&t, "axle", g->wagon_parts[WAGON_PART_AXLE]);
    loctext_arg_int(&t, "dissel", g->wagon_parts[WAGON_PART_DISSEL]);
    loctext_arg_int(&t, "tent", g->wagon_parts[WAGON_PART_TENT]);
    draw_wrapped(d, d->font, app_fmt(app, &t), 36, 268, 568, COL_DIM, false);
    loctext_set(&t, "store.load");
    loctext_arg_int(&t, "load", game_load_lb(g));
    loctext_arg_int(&t, "cap", game_capacity_lb(g));
    draw_text(d, d->font, app_fmt(app, &t), 36, 302, COL_DIM, TEXT_LEFT);
    draw_text(d, d->font, app_text(app, "overlay.close"), TREK_LOGICAL_W / 2.0f, TREK_LOGICAL_H - 32, COL_DIM, TEXT_CENTER);
}

/* ---------------------------------------------------------------- party */

static void party_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    overlay_panel(app, app_text(app, "party.title"));
    float y = 42;
    static const int HEALTH[] = {80, 60, 40};
    for (int i = 0; i < g->party_count; i++) {
        const Member *m = &g->party[i];
        char role_key[32], line[300], cond[160] = "";
        member_role_key(m->role, m->pronoun, role_key, sizeof role_key);
        SDL_Color c = COL_TEXT;
        if (!m->alive) {
            snprintf(cond, sizeof cond, "%s", app_text(app, "party.dead"));
            c = COL_LOCKED;
        } else if (!m->with_party) {
            snprintf(cond, sizeof cond, "%s", app_text(app, "party.gone"));
            c = COL_LOCKED;
        } else {
            int level = 3;
            for (int k = 0; k < 3; k++) {
                if (m->health >= HEALTH[k]) {
                    level = k;
                    break;
                }
            }
            char hk[40];
            snprintf(hk, sizeof hk, "status.health.%d", level);
            snprintf(cond, sizeof cond, "%s", app_text(app, hk));
            for (int k = 0; k < m->cond_count; k++) {
                size_t len = strlen(cond);
                snprintf(cond + len, sizeof cond - len, ", %s", app_text(app, app->data->illnesses[m->conds[k].illness].name_key));
            }
            if (m->cond_count > 0) {
                c = COL_RED;
            }
        }
        float row_y = y;
        portrait_draw(d->r, "face_", m->portrait, 36, y, 1, !m->alive || !m->with_party);
        snprintf(line, sizeof line, "%s", m->name);
        draw_text(d, d->font, line, 70, y, c, TEXT_LEFT);
        snprintf(line, sizeof line, "%s, %d", app_text(app, role_key), 1837 - m->birth_year);
        draw_text(d, d->font, line, 170, y, COL_DIM, TEXT_LEFT);
        draw_text(d, d->font, cond, 350, y, c, TEXT_LEFT);
        /* skills */
        char skills[160] = "";
        for (int s = 0; s < SKILL_COUNT; s++) {
            if (m->skills[s] >= 2) {
                char sk[40];
                snprintf(sk, sizeof sk, "skill.%s", skill_id((Skill)s));
                size_t len = strlen(skills);
                snprintf(skills + len, sizeof skills - len, "%s%s", len ? ", " : "", app_text(app, sk));
            }
        }
        y += d->line_h;
        if (skills[0]) {
            draw_text(d, d->font, skills, 170, y, COL_DIM, TEXT_LEFT);
            y += d->line_h;
        }
        y += 2;
        if (y < row_y + 30) {
            y = row_y + 30; /* room for the face */
        }
    }
    draw_text(d, d->font, app_text(app, "overlay.close"), TREK_LOGICAL_W / 2.0f, TREK_LOGICAL_H - 32, COL_DIM, TEXT_CENTER);
}

/* ---------------------------------------------------------------- map */

/* The route map: a parchment sheet with the real lie of the land (stretched east-west so that
 * close places stay readable), and a legend with where the wagon is and what comes next. */

#define MAP_X 26.0f
#define MAP_Y 42.0f
#define LEGEND_X 430.0f

static const SDL_Color MAP_PAPER = {216, 198, 158, 255}, MAP_EDGE = {150, 120, 80, 255}, MAP_INK = {58, 42, 28, 255},
                       MAP_FADED = {150, 128, 96, 255}, MAP_WATER = {62, 104, 156, 255}, MAP_SEA = {150, 176, 182, 255},
                       MAP_HILL = {128, 102, 70, 255}, MAP_ROUTE = {160, 40, 28, 255}, MAP_SOFT = {136, 112, 82, 255};

static void map_line(App *app, float x0, float y0, float x1, float y1, SDL_Color c)
{
    SDL_SetRenderDrawColor(app->renderer, c.r, c.g, c.b, c.a);
    SDL_RenderLine(app->renderer, x0, y0, x1, y1);
}

static void map_thick(App *app, float x0, float y0, float x1, float y1, SDL_Color c)
{
    map_line(app, x0, y0, x1, y1, c);
    map_line(app, x0 + 1, y0, x1 + 1, y1, c);
    map_line(app, x0, y0 + 1, x1, y1 + 1, c);
}

static void map_dotted(App *app, float x0, float y0, float x1, float y1, SDL_Color c)
{
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    for (float t = 0; t < len; t += 4) {
        draw_rect(&app->draw, (SDL_FRect){x0 + dx * t / len, y0 + dy * t / len, 1, 1}, c);
    }
}

/* Text with a ring of paper round it, so it reads over lines and rivers. */
static void map_label(App *app, TTF_Font *f, const char *s, float x, float y, SDL_Color c, TextAlign a)
{
    Draw *d = &app->draw;
    static const float OFF[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (int i = 0; i < 8; i++) {
        draw_text(d, f, s, x + OFF[i][0], y + OFF[i][1], MAP_PAPER, a);
    }
    draw_text(d, f, s, x, y, c, a);
}

static void map_peak(Draw *d, float x, float y)
{
    for (int i = 0; i < 4; i++) {
        draw_rect(d, (SDL_FRect){x - i, y - 4 + i, 2 * i + 1, 1}, MAP_HILL);
    }
    draw_rect(d, (SDL_FRect){x, y - 4, 1, 1}, MAP_PAPER);
}

/* Small marks for each kind of place; `known` draws them in ink, otherwise faded. */
static void map_mark(Draw *d, NodeKind kind, float x, float y, SDL_Color c)
{
    switch (kind) {
    case NODE_TOWN: /* a little house */
        draw_rect(d, (SDL_FRect){x - 3, y - 1, 7, 4}, c);
        draw_rect(d, (SDL_FRect){x - 2, y - 2, 5, 1}, c);
        draw_rect(d, (SDL_FRect){x - 1, y - 3, 3, 1}, c);
        draw_rect(d, (SDL_FRect){x, y + 1, 1, 2}, MAP_PAPER);
        break;
    case NODE_MISSION: /* a church cross */
        draw_rect(d, (SDL_FRect){x, y - 4, 1, 8}, c);
        draw_rect(d, (SDL_FRect){x - 2, y - 2, 5, 1}, c);
        break;
    case NODE_RIVER: /* a drift: a ring on the water */
        draw_rect(d, (SDL_FRect){x - 2, y - 3, 5, 7}, c);
        draw_rect(d, (SDL_FRect){x - 3, y - 2, 7, 5}, c);
        draw_rect(d, (SDL_FRect){x - 1, y - 1, 3, 3}, MAP_WATER);
        break;
    case NODE_END: /* a flag */
        draw_rect(d, (SDL_FRect){x, y - 5, 1, 9}, c);
        draw_rect(d, (SDL_FRect){x + 1, y - 5, 5, 3}, MAP_ROUTE);
        break;
    default: /* a landmark: a triangle */
        for (int i = 0; i < 4; i++) {
            draw_rect(d, (SDL_FRect){x - i, y - 2 + i, 2 * i + 1, 1}, c);
        }
        break;
    }
}

static void map_wagon(Draw *d, float x, float y)
{
    SDL_Color canvas = {244, 236, 214, 255};
    draw_rect(d, (SDL_FRect){x - 6, y - 6, 13, 12}, (SDL_Color){MAP_ROUTE.r, MAP_ROUTE.g, MAP_ROUTE.b, 90});
    draw_rect(d, (SDL_FRect){x - 4, y - 4, 8, 4}, canvas);
    draw_rect(d, (SDL_FRect){x - 3, y - 5, 6, 1}, canvas);
    draw_rect(d, (SDL_FRect){x - 4, y, 8, 1}, MAP_INK);
    draw_rect(d, (SDL_FRect){x - 4, y + 1, 2, 2}, MAP_INK);
    draw_rect(d, (SDL_FRect){x + 2, y + 1, 2, 2}, MAP_INK);
}

static void map_grave(Draw *d, float x, float y)
{
    draw_rect(d, (SDL_FRect){x + 1, y, 1, 5}, MAP_INK);
    draw_rect(d, (SDL_FRect){x, y + 1, 3, 1}, MAP_INK);
}

/* Whether a place is known: visited, or the next stop on a road from a visited place. */
static bool map_known(const Game *g, int n)
{
    const GameData *data = g->data;
    if (g->visited[n]) {
        return true;
    }
    for (int e = 0; e < data->edge_count; e++) {
        if (data->edges[e].to == n && g->visited[data->edges[e].from]) {
            return true;
        }
    }
    return false;
}

/* The wagon's place in lon/lat: at its node, or the right share of the way along its road. */
static void wagon_lonlat(const Game *g, double *lon, double *lat)
{
    const GameData *data = g->data;
    const NodeDef *cur = &data->nodes[g->node];
    *lon = cur->lon;
    *lat = cur->lat;
    if (g->edge >= 0) {
        const NodeDef *to = &data->nodes[data->edges[g->edge].to];
        double f = g->miles_along / data->edges[g->edge].miles;
        *lon += (to->lon - cur->lon) * f;
        *lat += (to->lat - cur->lat) * f;
    }
}

/* The sheet to show: the one chosen with Tab, or else the first one the wagon is on. */
static int map_sheet(const App *app)
{
    const GameData *data = app->data;
    if (app->map_sheet > 0 && app->map_sheet <= data->map_count) {
        return app->map_sheet - 1;
    }
    double lon, lat;
    wagon_lonlat(app->game, &lon, &lat);
    for (int s = 0; s < data->map_count; s++) {
        const MapData *m = &data->maps[s];
        if (lon >= m->lon0 && lon <= m->lon1 && lat <= m->lat0 && lat >= m->lat1) {
            return s;
        }
    }
    return 0;
}

/* The sea: the coast closed by the sheet's sea corners, filled a row at a time. */
static void map_sea(Draw *d, const MapData *m, float ox, float oy, SDL_Color c)
{
    const MapLine *poly = &m->coast;
    if (poly->n < 3) {
        return;
    }
    for (int row = 0; row < (int)m->h; row++) {
        float y = row + 0.5f;
        float xs[MAP_MAX_POINTS];
        int n = 0;
        for (int i = 0; i < poly->n; i++) {
            int j = (i + 1) % poly->n;
            float y0 = poly->y[i], y1 = poly->y[j];
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                xs[n++] = poly->x[i] + (y - y0) / (y1 - y0) * (poly->x[j] - poly->x[i]);
            }
        }
        for (int a = 1; a < n; a++) {
            for (int b = a; b > 0 && xs[b - 1] > xs[b]; b--) {
                float t = xs[b];
                xs[b] = xs[b - 1];
                xs[b - 1] = t;
            }
        }
        for (int k = 0; k + 1 < n; k += 2) {
            float x0 = SDL_clamp(xs[k], 0.0f, m->w), x1 = SDL_clamp(xs[k + 1], 0.0f, m->w);
            if (x1 > x0) {
                draw_rect(d, (SDL_FRect){ox + x0, oy + row, x1 - x0, 1}, c);
            }
        }
    }
}

static void map_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    const GameData *data = app->data;
    int sheet = map_sheet(app);
    const MapData *m = &data->maps[sheet];
    char title[160];
    snprintf(title, sizeof title, "%s  ·  %s", app_text(app, "map.title"), app_text(app, m->title_key));
    overlay_panel(app, title);
#define MX(x) (MAP_X + (x))
#define MY(y) (MAP_Y + (y))
    SDL_FRect paper = {MAP_X, MAP_Y, m->w, m->h};
    draw_rect(d, (SDL_FRect){paper.x - 2, paper.y - 2, paper.w + 4, paper.h + 4}, MAP_EDGE);
    draw_rect(d, paper, MAP_PAPER);
    SDL_Rect clip = {(int)paper.x, (int)paper.y, (int)paper.w, (int)paper.h};
    SDL_SetRenderClipRect(app->renderer, &clip);

    /* the sea, and the coast line (the polygon's last points are the sheet's sea corners) */
    const MapLine *coast = &m->coast;
    map_sea(d, m, MAP_X, MAP_Y, MAP_SEA);
    for (int i = 0; i + 1 < coast->n; i++) {
        if (coast->x[i + 1] > m->w * 2 || coast->y[i + 1] > m->h * 2) {
            break; /* the sea corners lie far off the sheet */
        }
        map_line(app, MX(coast->x[i]), MY(coast->y[i]), MX(coast->x[i + 1]), MY(coast->y[i + 1]), MAP_WATER);
    }
    /* mountains, rivers */
    for (int i = 0; i < m->peak_count; i++) {
        map_peak(d, MX(m->peak_x[i]), MY(m->peak_y[i]));
    }
    for (int r = 0; r < m->river_count; r++) {
        const MapLine *rv = &m->rivers[r];
        for (int i = 0; i + 1 < rv->n; i++) {
            map_thick(app, MX(rv->x[i]), MY(rv->y[i]), MX(rv->x[i + 1]), MY(rv->y[i + 1]), MAP_WATER);
        }
    }
    /* the peoples of the region and the mountain ranges, written softly with no borders */
    for (int i = 0; i < m->text_count; i++) {
        const MapText *t = &m->texts[i];
        SDL_Color c = t->kind == MAP_TEXT_SEA ? MAP_WATER : (t->kind == MAP_TEXT_FEATURE ? MAP_HILL : MAP_SOFT);
        draw_text(d, d->small, app_text(app, t->key), MX(t->x), MY(t->y) - 5, c, TEXT_CENTER);
    }
    for (int r = 0; r < m->river_count; r++) {
        const MapLine *rv = &m->rivers[r];
        int k = rv->label_at;
        if (k < 0) {
            continue;
        }
        map_label(app, d->small, app_text(app, rv->key), MX(rv->x[k]), MY(rv->y[k]) + 2, MAP_WATER, TEXT_CENTER);
    }
    /* roads: travelled in red, known in ink, the rest dotted */
    for (int e = 0; e < data->edge_count; e++) {
        const EdgeDef *ed = &data->edges[e];
        const NodeDef *a = &data->nodes[ed->from], *b = &data->nodes[ed->to];
        float x0 = MX(a->map_x[sheet]), y0 = MY(a->map_y[sheet]), x1 = MX(b->map_x[sheet]), y1 = MY(b->map_y[sheet]);
        if (g->visited[ed->from] && g->visited[ed->to]) {
            map_thick(app, x0, y0, x1, y1, MAP_ROUTE);
        } else if (g->visited[ed->from]) {
            map_line(app, x0, y0, x1, y1, MAP_INK);
        } else {
            map_dotted(app, x0, y0, x1, y1, MAP_FADED);
        }
    }
    /* the road so far on the current leg */
    float wx, wy;
    const NodeDef *cur = &data->nodes[g->node];
    if (g->edge >= 0) {
        const NodeDef *to = &data->nodes[data->edges[g->edge].to];
        float f = (float)(g->miles_along / data->edges[g->edge].miles);
        wx = cur->map_x[sheet] + (to->map_x[sheet] - cur->map_x[sheet]) * f;
        wy = cur->map_y[sheet] + (to->map_y[sheet] - cur->map_y[sheet]) * f;
        map_thick(app, MX(cur->map_x[sheet]), MY(cur->map_y[sheet]), MX(wx), MY(wy), MAP_ROUTE);
    } else {
        wx = cur->map_x[sheet];
        wy = cur->map_y[sheet];
    }
    /* places: names only for places the party knows of */
    for (int n = 0; n < data->node_count; n++) {
        const NodeDef *nd = &data->nodes[n];
        bool known = map_known(g, n);
        float x = MX(nd->map_x[sheet]), y = MY(nd->map_y[sheet]);
        map_mark(d, nd->kind, x, y, known ? MAP_INK : MAP_FADED);
        if (!known) {
            continue;
        }
        const char *name = app_text(app, nd->name_key);
        switch (nd->map_label[sheet]) {
        case MAP_LABEL_LEFT: map_label(app, d->font, name, x - 7, y - 8, MAP_INK, TEXT_RIGHT); break;
        case MAP_LABEL_ABOVE: map_label(app, d->font, name, x, y - 21, MAP_INK, TEXT_CENTER); break;
        case MAP_LABEL_BELOW: map_label(app, d->font, name, x, y + 5, MAP_INK, TEXT_CENTER); break;
        default: map_label(app, d->font, name, x + 7, y - 8, MAP_INK, TEXT_LEFT); break;
        }
    }
    for (int i = 0; i < g->grave_count; i++) {
        const NodeDef *nd = &data->nodes[g->graves[i].node];
        map_grave(d, MX(nd->map_x[sheet]) - 9, MY(nd->map_y[sheet]) + 2);
    }
    map_wagon(d, MX(wx), MY(wy));
    /* a north arrow */
    float nx = MX(m->w) - 14, ny = MY(0) + 8;
    draw_rect(d, (SDL_FRect){nx, ny + 4, 1, 10}, MAP_INK);
    for (int i = 0; i < 3; i++) {
        draw_rect(d, (SDL_FRect){nx - i, ny + 2 + i, 2 * i + 1, 1}, MAP_INK);
    }
    draw_text(d, d->small, app_text(app, "map.north"), nx + 1, ny - 9, MAP_INK, TEXT_CENTER);
    SDL_SetRenderClipRect(app->renderer, NULL);
#undef MX
#undef MY

    /* the legend */
    float lx = LEGEND_X, ly = MAP_Y;
    draw_text(d, d->font, app_text(app, "map.legend"), lx, ly, COL_GOLD, TEXT_LEFT);
    ly += d->line_h + 2;
    static const struct { NodeKind kind; const char *key; } MARKS[] = {
        {NODE_TOWN, "map.legend.town"}, {NODE_MISSION, "map.legend.mission"}, {NODE_RIVER, "map.legend.drift"},
        {NODE_LANDMARK, "map.legend.landmark"}, {NODE_END, "map.legend.end"}};
    SDL_FRect key_box = {lx - 4, ly - 2, 186, 0};
    float row = 13;
    key_box.h = row * 9 + 6;
    draw_rect(d, key_box, MAP_PAPER);
    for (size_t i = 0; i < sizeof MARKS / sizeof MARKS[0]; i++) {
        map_mark(d, MARKS[i].kind, lx + 6, ly + 6, MAP_INK);
        draw_text(d, d->small, app_text(app, MARKS[i].key), lx + 18, ly, MAP_INK, TEXT_LEFT);
        ly += row;
    }
    map_wagon(d, lx + 6, ly + 6);
    draw_text(d, d->small, app_text(app, "map.legend.wagon"), lx + 18, ly, MAP_INK, TEXT_LEFT);
    ly += row;
    map_grave(d, lx + 5, ly + 3);
    draw_text(d, d->small, app_text(app, "map.legend.grave"), lx + 18, ly, MAP_INK, TEXT_LEFT);
    ly += row;
    map_thick(app, lx, ly + 6, lx + 12, ly + 6, MAP_ROUTE);
    draw_text(d, d->small, app_text(app, "map.legend.travelled"), lx + 18, ly, MAP_INK, TEXT_LEFT);
    ly += row;
    map_line(app, lx, ly + 6, lx + 12, ly + 6, MAP_INK);
    map_dotted(app, lx, ly + 10, lx + 12, ly + 10, MAP_FADED);
    draw_text(d, d->small, app_text(app, "map.legend.roads"), lx + 18, ly, MAP_INK, TEXT_LEFT);
    ly += row + 10;

    /* where the wagon is, and what comes next */
    LocText t;
    int next = game_next_node(g);
    if (g->edge >= 0 && next >= 0) {
        loctext_set(&t, "travel.next");
        loctext_arg_key(&t, "place", data->nodes[next].name_key);
        loctext_arg_int(&t, "miles", game_miles_to_next(g));
    } else {
        loctext_set(&t, "travel.at");
        loctext_arg_key(&t, "place", data->nodes[g->node].name_key);
    }
    ly += draw_wrapped(d, d->font, app_fmt(app, &t), lx, ly, 180, COL_TEXT, false);
    loctext_set(&t, "travel.done");
    loctext_arg_int(&t, "miles", g->miles_total);
    loctext_arg_int(&t, "days", g->day - g->start_day);
    ly += draw_wrapped(d, d->font, app_fmt(app, &t), lx, ly, 180, COL_DIM, false);
    ly += 4 + draw_wrapped(d, d->small, app_text(app, "map.note"), lx, ly + 4, 180, COL_DIM, false);
    if (data->map_count > 1) {
        draw_wrapped(d, d->small, app_text(app, "map.other_sheet"), lx, ly + 4, 180, COL_GOLD, false);
    }
    draw_text(d, d->font, app_text(app, "overlay.close"), lx + 90, TREK_LOGICAL_H - 32, COL_DIM, TEXT_CENTER);
}

/* ---------------------------------------------------------------- journal */

static void journal_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    overlay_panel(app, app_text(app, "journal.title"));
    float y = 42;
    /* newest first */
    for (int i = g->journal_count - 1 - app->scroll; i >= 0 && y < TREK_LOGICAL_H - 50; i--) {
        char date[64];
        int yy, mm, dd;
        cal_date(g->journal[i].day, &yy, &mm, &dd);
        char mk[16];
        snprintf(mk, sizeof mk, "month.%d", mm);
        snprintf(date, sizeof date, "%d %s", dd, app_text(app, mk));
        draw_text(d, d->font, date, 36, y, COL_DIM, TEXT_LEFT);
        y += draw_wrapped(d, d->font, app_fmt(app, &g->journal[i].text), 130, y, 474, COL_TEXT, false);
    }
    draw_text(d, d->font, app_text(app, "journal.footer"), TREK_LOGICAL_W / 2.0f, TREK_LOGICAL_H - 32, COL_DIM, TEXT_CENTER);
}

/* ---------------------------------------------------------------- history note */

static void history_render(App *app)
{
    DialogView v;
    overlay_panel(app, app_text(app, "history.title"));
    if (game_dialog_view(app->game, &v) && v.history_key[0]) {
        draw_wrapped(&app->draw, app->font, app_text(app, v.history_key), 36, 46, 568, COL_TEXT, false);
    }
    draw_text(&app->draw, app->font, app_text(app, "overlay.close"), TREK_LOGICAL_W / 2.0f, TREK_LOGICAL_H - 32, COL_DIM, TEXT_CENTER);
}

/* ---------------------------------------------------------------- dispatch */

void overlay_event(App *app, const SDL_Event *e)
{
    switch (app->overlay) {
    case OV_PAUSE: {
        ChoiceItem items[PA_COUNT];
        pause_items(app, items);
        nav_updown(app, e, PA_COUNT);
        int digit = key_digit(e);
        if (digit >= 0 && digit < PA_COUNT) {
            pause_activate(app, digit);
        } else if (is_confirm(e)) {
            pause_activate(app, app->sel);
        } else if (is_back(e)) {
            close_overlay(app);
        } else if (is_click(e)) {
            int hit = choices_hit(&app->draw, items, PA_COUNT, 190, 110, 260, app->mx, app->my, true);
            if (hit >= 0) {
                pause_activate(app, hit);
            }
        }
        break;
    }
    case OV_ORDERS:
        nav_updown(app, e, OR_COUNT);
        if (key_is(e, SDLK_LEFT)) {
            orders_change(app, app->sel, -1);
        } else if (key_is(e, SDLK_RIGHT) || (is_confirm(e) && !key_is(e, SDLK_RETURN))) {
            orders_change(app, app->sel, 1);
        } else if (is_back(e) || key_is(e, SDLK_RETURN) || key_is(e, SDLK_KP_ENTER)) {
            close_overlay(app);
        } else if (is_click(e)) {
            int row = (int)((app->my - 48) / app->draw.line_h);
            if (app->my >= 48 && row >= 0 && row < OR_COUNT) {
                app->sel = row;
                orders_change(app, row, app->mx < 300 ? 0 : 1);
            } else {
                close_overlay(app);
            }
        }
        break;
    case OV_GLOSSARY:
        if (key_is(e, SDLK_DOWN)) {
            app->scroll++;
        } else if (key_is(e, SDLK_UP) && app->scroll > 0) {
            app->scroll--;
        } else if (e->type == SDL_EVENT_MOUSE_WHEEL) {
            app->scroll -= (int)e->wheel.y;
            if (app->scroll < 0) {
                app->scroll = 0;
            }
        } else if (e->type == SDL_EVENT_KEY_DOWN || is_click(e)) {
            close_overlay(app);
        }
        break;
    case OV_JOURNAL:
        if (key_is(e, SDLK_DOWN) && app->scroll < app->game->journal_count - 1) {
            app->scroll++;
        } else if (key_is(e, SDLK_UP) && app->scroll > 0) {
            app->scroll--;
        } else if (key_is(e, SDLK_PAGEDOWN)) {
            app->scroll += 10;
            if (app->scroll > app->game->journal_count - 1) {
                app->scroll = app->game->journal_count - 1;
            }
        } else if (key_is(e, SDLK_PAGEUP)) {
            app->scroll = app->scroll > 10 ? app->scroll - 10 : 0;
        } else if (e->type == SDL_EVENT_MOUSE_WHEEL) {
            app->scroll -= (int)e->wheel.y;
            if (app->scroll < 0) {
                app->scroll = 0;
            }
            if (app->scroll > app->game->journal_count - 1) {
                app->scroll = app->game->journal_count - 1;
            }
        } else if (e->type == SDL_EVENT_KEY_DOWN || is_click(e)) {
            close_overlay(app);
        }
        break;
    case OV_MAP:
        if (key_is(e, SDLK_TAB) || key_is(e, SDLK_LEFT) || key_is(e, SDLK_RIGHT)) {
            app->map_sheet = (map_sheet(app) + 1) % (app->data->map_count > 0 ? app->data->map_count : 1) + 1;
        } else if (e->type == SDL_EVENT_KEY_DOWN || is_click(e)) {
            close_overlay(app);
        }
        break;
    default:
        if (e->type == SDL_EVENT_KEY_DOWN || is_click(e)) {
            close_overlay(app);
        }
        break;
    }
}

void overlay_render(App *app)
{
    switch (app->overlay) {
    case OV_PAUSE: {
        overlay_panel(app, app_text(app, "pause.title"));
        ChoiceItem items[PA_COUNT];
        pause_items(app, items);
        draw_choices(&app->draw, items, PA_COUNT, 190, 110, 260, app->mx, app->my, app->sel, true, NULL);
        draw_wrapped(&app->draw, app->font, app_text(app, "pause.about"), 120, 240, 400, COL_DIM, false);
        break;
    }
    case OV_ORDERS: orders_render(app); break;
    case OV_SUPPLIES: supplies_render(app); break;
    case OV_PARTY: party_render(app); break;
    case OV_MAP: map_render(app); break;
    case OV_JOURNAL: journal_render(app); break;
    case OV_HISTORY: history_render(app); break;
    case OV_GLOSSARY: glossary_render(app); break;
    default: break;
    }
}
