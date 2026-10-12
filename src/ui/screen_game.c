/* In-game screens. Which one shows is decided by the core's phase:
 * travel, dialog (events, places, rivers, roads), store, grave, ending. */
#include "calendar.h"
#include "format.h"
#include "scene.h"
#include "screens.h"
#include "sound.h"
#include "sprites.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------- helpers */

bool open_trip(App *app, const char *trip_id)
{
    char err[256];
    Game *g = trips_read(app->pref_path, trip_id, app->data, err, sizeof err);
    if (!g) {
        app_set_message(app, err);
        app_switch(app, SCR_TITLE);
        return false;
    }
    app_close_game(app);
    app->game = g;
    snprintf(app->trip_id, sizeof app->trip_id, "%s", trip_id);
    app_switch(app, SCR_GAME);
    return true;
}

static const char *word_for(App *app, const char *prefix, int value, const int *limits, int n)
{
    static char key[64];
    int level = n;
    for (int i = 0; i < n; i++) {
        if (value >= limits[i]) {
            level = i;
            break;
        }
    }
    snprintf(key, sizeof key, "%s.%d", prefix, level);
    return app_text(app, key);
}

static int party_health(const Game *g)
{
    int sum = 0, n = 0;
    for (int i = 0; i < g->party_count; i++) {
        if (g->party[i].alive && g->party[i].with_party) {
            sum += g->party[i].health;
            n++;
        }
    }
    return n ? sum / n : 0;
}

static int wagon_condition(const Game *g)
{
    int worst = 100;
    for (int p = 0; p < WAGON_PART_COUNT - 1; p++) {
        if (g->wagon_parts[p] < worst) {
            worst = g->wagon_parts[p];
        }
    }
    return worst;
}

static void header_for_game(App *app)
{
    Game *g = app->game;
    char date[64], right[160], weather_key[40];
    fmt_date(app->loc, g->day, date, sizeof date);
    snprintf(weather_key, sizeof weather_key, "weather.%s", weather_id(g->weather));
    char left[160];
    snprintf(left, sizeof left, "%s  ·  %s", date, app_text(app, weather_key));
    int region = g->edge >= 0 ? app->data->edges[g->edge].region : -1;
    if (region >= 0) {
        snprintf(right, sizeof right, "%s", app_text(app, app->data->regions[region].name_key));
    } else {
        snprintf(right, sizeof right, "%s", app_text(app, app->data->nodes[g->node].name_key));
    }
    draw_header(app, left, right);
}

static void commit(App *app)
{
    app_commit(app);
}

/* ---------------------------------------------------------------- travel screen */

typedef enum { TR_GO, TR_REST, TR_HUNT, TR_ORDERS, TR_SUPPLIES, TR_PARTY, TR_MAP, TR_JOURNAL, TR_COUNT } TravelItem;

#define MENU_X 330.0f
#define MENU_Y 186.0f
#define MENU_W 300

typedef enum { RM_REST, RM_MEND, RM_BACK, RM_COUNT } RestMenuItem;

/* The travel menu, or the small rest / mend menu in its place. Returns the number of items. */
static int travel_items(App *app, ChoiceItem *items)
{
    if (app->rest_menu) {
        items[RM_REST] = (ChoiceItem){app_text(app, "travel.rest_day"), NULL};
        items[RM_MEND] = (ChoiceItem){app_text(app, "travel.mend"), NULL};
        items[RM_BACK] = (ChoiceItem){app_text(app, "travel.back"), NULL};
        if (!game_can_mend(app->game)) {
            items[RM_MEND].locked = app_text(app, "travel.mend_locked");
        }
        return RM_COUNT;
    }
    static const char *KEYS[TR_COUNT] = {"travel.go", "travel.rest_or_mend", "travel.hunt", "travel.orders",
                                         "travel.supplies", "travel.party", "travel.map", "travel.journal"};
    for (int i = 0; i < TR_COUNT; i++) {
        items[i] = (ChoiceItem){app_text(app, KEYS[i]), NULL};
    }
    if (!game_can_hunt(app->game)) {
        items[TR_HUNT].locked = "";
    }
    return TR_COUNT;
}

static void rest_menu_activate(App *app, int item)
{
    Game *g = app->game;
    switch (item) {
    case RM_REST:
        game_rest_day(g);
        commit(app);
        break;
    case RM_MEND:
        if (!game_can_mend(g)) {
            return;
        }
        game_mend_wagon(g);
        commit(app);
        break;
    default: break;
    }
    app->rest_menu = false;
    app->sel = item == RM_BACK ? TR_REST : 0;
}

static void travel_activate(App *app, int item)
{
    Game *g = app->game;
    if (app->rest_menu) {
        rest_menu_activate(app, item);
        return;
    }
    switch (item) {
    case TR_GO:
        app->travelling = !app->travelling;
        app->travel_timer = 0;
        break;
    case TR_REST:
        app->travelling = false;
        app->rest_menu = true;
        app->sel = 0;
        break;
    case TR_HUNT:
        if (game_can_hunt(g)) {
            app->travelling = false;
            game_hunt_start(g);
            app->sel = 0;
            commit(app);
        }
        break;
    case TR_ORDERS: app->overlay = OV_ORDERS; app->sel = 0; break;
    case TR_SUPPLIES: app->overlay = OV_SUPPLIES; break;
    case TR_PARTY: app->overlay = OV_PARTY; break;
    case TR_MAP: app->overlay = OV_MAP; break;
    case TR_JOURNAL: app->overlay = OV_JOURNAL; app->scroll = 0; break;
    default: break;
    }
}

static void travel_event(App *app, const SDL_Event *e)
{
    ChoiceItem items[TR_COUNT];
    int n = travel_items(app, items);
    if (app->travelling && (e->type == SDL_EVENT_KEY_DOWN || is_click(e))) {
        app->travelling = false; /* any key stops the wagons */
        return;
    }
    nav_updown(app, e, n);
    int digit = key_digit(e);
    if (digit >= 0 && digit < n) {
        app->sel = digit;
        travel_activate(app, digit);
    } else if (is_confirm(e)) {
        travel_activate(app, app->sel);
    } else if (key_is(e, SDLK_M)) {
        app->overlay = OV_MAP;
    } else if (key_is(e, SDLK_J)) {
        app->overlay = OV_JOURNAL;
        app->scroll = 0;
    } else if (key_is(e, SDLK_G)) {
        app->overlay = OV_GLOSSARY;
        app->scroll = 0;
    } else if (is_click(e)) {
        int hit = choices_hit(&app->draw, items, n, MENU_X, MENU_Y, MENU_W, app->mx, app->my, true);
        if (hit >= 0) {
            app->sel = hit;
            travel_activate(app, hit);
        }
    }
}

/* Nodes along the road taken so far and ahead (up to the next unchosen fork). */
static int route_path(const Game *g, int *nodes, int max)
{
    const GameData *d = g->data;
    int n = 0, cur = d->rules.start_node;
    nodes[n++] = cur;
    while (n < max) {
        int pick = -1, outs = 0, only = -1;
        for (int e = 0; e < d->edge_count; e++) {
            if (d->edges[e].from != cur) {
                continue;
            }
            outs++;
            only = e;
            if (e == g->edge || g->visited[d->edges[e].to]) {
                pick = e;
            }
        }
        if (pick < 0 && outs == 1) {
            pick = only;
        }
        if (pick < 0) {
            break;
        }
        cur = d->edges[pick].to;
        nodes[n++] = cur;
    }
    return n;
}

static int edge_between(const GameData *d, int a, int b)
{
    for (int e = 0; e < d->edge_count; e++) {
        if (d->edges[e].from == a && d->edges[e].to == b) {
            return e;
        }
    }
    return -1;
}

static void draw_trail(App *app, float y)
{
    Draw *dr = &app->draw;
    Game *g = app->game;
    const GameData *d = app->data;
    int nodes[MAX_NODES];
    int n = route_path(g, nodes, MAX_NODES);
    double total = 0, here = 0;
    double pos[MAX_NODES];
    pos[0] = 0;
    for (int i = 1; i < n; i++) {
        int e = edge_between(d, nodes[i - 1], nodes[i]);
        total += e >= 0 ? d->edges[e].miles : 0;
        pos[i] = total;
        if (nodes[i - 1] == g->node && e == g->edge) {
            here = pos[i - 1] + g->miles_along;
        }
        if (nodes[i] == g->node && g->edge < 0) {
            here = pos[i];
        }
    }
    bool fork_ahead = d->nodes[nodes[n - 1]].kind != NODE_END;
    float x0 = 20, x1 = fork_ahead ? 560.0f : 620.0f;
    float scale = total > 0 ? (float)((x1 - x0) / total) : 0;
    draw_rect(dr, (SDL_FRect){x0, y + 6, x1 - x0, 1}, COL_DIM);
    for (int i = 0; i < n; i++) {
        float x = x0 + (float)pos[i] * scale;
        SDL_Color c = g->visited[nodes[i]] ? COL_GOLD : COL_DIM;
        draw_rect(dr, (SDL_FRect){x - 2, y + 4, 5, 5}, c);
    }
    if (fork_ahead) {
        draw_text(dr, dr->font, "?", x1 + 12, y - 2, COL_DIM, TEXT_LEFT);
    }
    float wx = x0 + (float)here * scale;
    draw_rect(dr, (SDL_FRect){wx - 3, y - 2, 7, 6}, COL_TEXT);
}

static void draw_status(App *app, float x, float y)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    const GameData *data = app->data;
    char buf[200];
    int line = d->line_h;
    static const int HEALTH[] = {80, 60, 40};
    static const int MORALE[] = {70, 45, 25};
    static const int STAMINA[] = {60, 30, 10};
    static const int WAGON[] = {70, 40, 1};

    LocText t;
    loctext_set(&t, "status.food");
    loctext_arg_int(&t, "days", (long long)game_food_days(g));
    loctext_arg_int(&t, "lb", game_food_lb(g));
    loctext_arg_int(&t, "sheep", g->inv[data->item_sheep]);
    draw_text(d, d->font, app_fmt(app, &t), x, y, COL_TEXT, TEXT_LEFT);
    loctext_set(&t, "status.water");
    loctext_arg_int(&t, "gal", g->inv[data->item_water]);
    loctext_arg_int(&t, "cap", game_water_capacity(g));
    draw_text(d, d->font, app_fmt(app, &t), x, y + line, COL_TEXT, TEXT_LEFT);
    loctext_set(&t, "status.oxen");
    loctext_arg_int(&t, "n", g->inv[data->item_oxen]);
    loctext_arg_str(&t, "cond", word_for(app, "status.stamina", g->ox_stamina, STAMINA, 3));
    draw_text(d, d->font, app_fmt(app, &t), x, y + 2 * line, COL_TEXT, TEXT_LEFT);
    loctext_set(&t, "status.wagon");
    loctext_arg_str(&t, "cond", word_for(app, "status.wagon", wagon_condition(g), WAGON, 3));
    draw_text(d, d->font, app_fmt(app, &t), x, y + 3 * line, COL_TEXT, TEXT_LEFT);
    loctext_set(&t, "status.health");
    loctext_arg_str(&t, "cond", word_for(app, "status.health", party_health(g), HEALTH, 3));
    draw_text(d, d->font, app_fmt(app, &t), x, y + 4 * line, COL_TEXT, TEXT_LEFT);
    loctext_set(&t, "status.morale");
    loctext_arg_str(&t, "cond", word_for(app, "status.morale", g->morale, MORALE, 3));
    draw_text(d, d->font, app_fmt(app, &t), x, y + 5 * line, COL_TEXT, TEXT_LEFT);
    loctext_set(&t, "status.money");
    loctext_arg_money(&t, "n", g->money);
    draw_text(d, d->font, app_fmt(app, &t), x, y + 6 * line, COL_TEXT, TEXT_LEFT);
    char key[48];
    snprintf(buf, sizeof buf, "%s", "");
    snprintf(key, sizeof key, "pace.%s", pace_id(g->pace));
    loctext_set(&t, "status.orders");
    loctext_arg_key(&t, "pace", key);
    char rk[48], sk[48];
    snprintf(rk, sizeof rk, "rations.%s", rations_id(g->rations));
    snprintf(sk, sizeof sk, "stance.%s", stance_id(g->stance));
    loctext_arg_key(&t, "rations", rk);
    loctext_arg_key(&t, "night", sk);
    draw_text(d, d->font, app_fmt(app, &t), x, y + 7 * line, COL_DIM, TEXT_LEFT);
}

static void travel_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    header_for_game(app);
    int region = g->edge >= 0 ? app->data->edges[g->edge].region : -1;
    bool mountains = region >= 0 && strcmp(app->data->regions[region].id, "stormberg") == 0;
    scene_draw(app, (SDL_FRect){0, 19, TREK_LOGICAL_W, 132}, mountains ? SCENE_MOUNTAIN : SCENE_ROAD, -1, app->travelling);
    draw_rect(d, (SDL_FRect){0, 151, TREK_LOGICAL_W, 1}, COL_BORDER);
    draw_trail(app, 160);
    LocText t;
    int next = game_next_node(g);
    if (next >= 0) {
        loctext_set(&t, "travel.next");
        loctext_arg_key(&t, "place", app->data->nodes[next].name_key);
        loctext_arg_int(&t, "miles", game_miles_to_next(g));
    } else {
        loctext_set(&t, "travel.at");
        loctext_arg_key(&t, "place", app->data->nodes[g->node].name_key);
    }
    draw_text(d, d->font, app_fmt(app, &t), 20, 172, COL_TEXT, TEXT_LEFT);
    loctext_set(&t, "travel.done");
    loctext_arg_int(&t, "miles", g->miles_total);
    loctext_arg_int(&t, "days", g->day - g->start_day);
    draw_text(d, d->font, app_fmt(app, &t), 620, 172, COL_DIM, TEXT_RIGHT);
    draw_status(app, 20, 192);
    ChoiceItem items[TR_COUNT];
    int n = travel_items(app, items);
    if (app->travelling && !app->rest_menu) {
        items[TR_GO].text = app_text(app, "travel.stop");
    }
    draw_choices(d, items, n, MENU_X, MENU_Y, MENU_W, app->mx, app->my, app->sel, true, NULL);
    draw_footer(app, app_text(app, app->travelling ? "travel.footer_moving" : "travel.footer"));
}

/* ---------------------------------------------------------------- dialog screen */

#define DLG_X 24.0f
#define DLG_W 592

typedef struct {
    ChoiceItem items[MAX_CHOICES];
    char text[MAX_CHOICES][400];
    char locked[MAX_CHOICES][200];
    int n;
    int map_item;             /* the extra "Look at the map" line, -1 if none */
} DialogChoices;

static void build_choices(App *app, const DialogView *v, DialogChoices *dc)
{
    dc->n = v->choice_count;
    for (int i = 0; i < v->choice_count; i++) {
        app_fmt_to(app, &v->choices[i].text, dc->text[i], sizeof dc->text[i]);
        if (app->settings.show_odds) {
            double odds[8];
            int n = game_choice_odds(app->game, i, odds, 8);
            if (n > 1) {
                size_t len = strlen(dc->text[i]);
                snprintf(dc->text[i] + len, sizeof dc->text[i] - len, " (");
                for (int k = 0; k < n; k++) {
                    len = strlen(dc->text[i]);
                    snprintf(dc->text[i] + len, sizeof dc->text[i] - len, "%s%d%%", k ? "/" : "", (int)lround(odds[k] * 100));
                }
                len = strlen(dc->text[i]);
                snprintf(dc->text[i] + len, sizeof dc->text[i] - len, ")");
            }
        }
        dc->items[i].text = dc->text[i];
        dc->items[i].locked = NULL;
        if (!v->choices[i].enabled) {
            app_fmt_to(app, &v->choices[i].locked, dc->locked[i], sizeof dc->locked[i]);
            dc->items[i].locked = dc->locked[i];
        }
    }
    /* in a place, and where the road divides, the map is one of the choices */
    dc->map_item = -1;
    if ((v->kind == DLG_LANDMARK || v->kind == DLG_ROUTE) && dc->n < MAX_CHOICES) {
        dc->map_item = dc->n;
        snprintf(dc->text[dc->n], sizeof dc->text[dc->n], "%s", app_text(app, "dialog.see_map"));
        dc->items[dc->n] = (ChoiceItem){dc->text[dc->n], NULL};
        dc->n++;
    }
}

#define SCENE_H 76.0f

#define PORTRAIT_W 42.0f
#define PORTRAIT_H 50.0f

/* Lays out the dialog from y0; returns the y where choices start. With a speaker's portrait, the
 * text moves right of it and the choices start below it. */
static float dialog_layout(App *app, const DialogView *v, bool draw, float y0)
{
    Draw *d = &app->draw;
    float y = y0;
    float x = DLG_X, w = DLG_W;
    if (v->title.key[0]) {
        if (draw) {
            draw_text(d, d->font, app_fmt(app, &v->title), DLG_X, y, COL_GOLD, TEXT_LEFT);
        }
        y += d->line_h + 4;
    }
    float portrait_bottom = y;
    if (portrait_exists("portrait_", v->portrait)) {
        if (draw) {
            portrait_draw(d->r, "portrait_", v->portrait, DLG_X, y + 2, 1, false);
        }
        portrait_bottom = y + 2 + PORTRAIT_H + 4;
        x += PORTRAIT_W + 10;
        w -= PORTRAIT_W + 10;
    }
    if (v->speaker.key[0]) {
        if (draw) {
            draw_text(d, d->font, app_fmt(app, &v->speaker), x, y, COL_DIM, TEXT_LEFT);
        }
        y += d->line_h;
    }
    for (int i = 0; i < v->body_count; i++) {
        y += draw_wrapped(d, d->font, app_fmt(app, &v->body[i]), x, y, (int)w, COL_TEXT, !draw) + 6;
    }
    return (y > portrait_bottom ? y : portrait_bottom) + 6;
}

static float choices_bottom(App *app, const DialogChoices *dc, float y)
{
    float h = y;
    for (int i = 0; i < dc->n; i++) {
        h += draw_wrapped(&app->draw, app->font, dc->items[i].text, 0, 0, DLG_W - 28, COL_TEXT, true) + 4;
        if (dc->items[i].locked && dc->items[i].locked[0]) {
            h += app->draw.line_h;
        }
    }
    return h;
}

static float text_start(App *app, const DialogView *v, const DialogChoices *dc)
{
    float y = dialog_layout(app, v, false, 26);
    float bottom = choices_bottom(app, dc, y);
    return bottom + SCENE_H <= TREK_LOGICAL_H - 22 ? 26 + SCENE_H : 26;
}

float dialog_text_bottom(App *app, const DialogView *v)
{
    DialogChoices dc;
    build_choices(app, v, &dc);
    return choices_bottom(app, &dc, dialog_layout(app, v, false, 26));
}

static void dialog_event(App *app, const SDL_Event *e)
{
    Game *g = app->game;
    DialogView v;
    if (!game_dialog_view(g, &v)) {
        return;
    }
    DialogChoices dc;
    build_choices(app, &v, &dc);
    if (key_is(e, SDLK_H) && v.history_key[0]) {
        app->overlay = OV_HISTORY;
        return;
    }
    if (key_is(e, SDLK_M)) {
        app->overlay = OV_MAP;
        return;
    }
    if (key_is(e, SDLK_J)) {
        app->overlay = OV_JOURNAL;
        app->scroll = 0;
        return;
    }
    if (key_is(e, SDLK_P)) {
        app->overlay = OV_PARTY;
        return;
    }
    if (key_is(e, SDLK_G)) {
        app->overlay = OV_GLOSSARY;
        app->scroll = 0;
        return;
    }
    if (key_is(e, SDLK_S)) {
        app->overlay = OV_SUPPLIES;
        return;
    }
    nav_updown(app, e, dc.n);
    int pick = -1;
    int digit = key_digit(e);
    if (digit >= 0 && digit < dc.n) {
        pick = digit;
    } else if (is_confirm(e)) {
        pick = dc.n == 1 ? 0 : app->sel;
    } else if (is_click(e)) {
        float y = dialog_layout(app, &v, false, text_start(app, &v, &dc));
        pick = choices_hit(&app->draw, dc.items, dc.n, DLG_X, y, DLG_W, app->mx, app->my, true);
    }
    if (pick >= 0 && pick == dc.map_item) {
        app->overlay = OV_MAP;
        return;
    }
    if (pick >= 0 && pick < dc.n && !dc.items[pick].locked) {
        game_choose(g, pick);
        app->sel = 0;
        app->scroll = 0;
        commit(app);
    }
}

static void dialog_render(App *app)
{
    Game *g = app->game;
    DialogView v;
    header_for_game(app);
    if (!game_dialog_view(g, &v)) {
        return;
    }
    DialogChoices dc;
    build_choices(app, &v, &dc);
    float top = text_start(app, &v, &dc);
    if (top > 26) {
        scene_draw(app, (SDL_FRect){0, 19, TREK_LOGICAL_W, SCENE_H}, scene_for_game(app), -1, false);
        draw_rect(&app->draw, (SDL_FRect){0, 19 + SCENE_H, TREK_LOGICAL_W, 1}, COL_BORDER);
    }
    float y = dialog_layout(app, &v, true, top);
    draw_rect(&app->draw, (SDL_FRect){DLG_X, y - 4, DLG_W, 1}, COL_BORDER);
    draw_choices(&app->draw, dc.items, dc.n, DLG_X, y, DLG_W, app->mx, app->my, app->sel, true, NULL);
    draw_footer(app, app_text(app, v.history_key[0] ? "dialog.footer_history" : "dialog.footer"));
}

/* ---------------------------------------------------------------- store */

#define STORE_Y 92.0f
#define STORE_ROWS 13

static void store_event(App *app, const SDL_Event *e)
{
    Game *g = app->game;
    StoreView sv;
    if (!game_store_view(g, &sv)) {
        return;
    }
    if (app->store_qty < 1) {
        app->store_qty = 1;
    }
    if (nav_updown(app, e, sv.row_count)) {
        app->store_qty = 1;
        return;
    }
    if (e->type == SDL_EVENT_MOUSE_WHEEL && sv.row_count > 0) {
        /* the wheel moves down the goods, and the list scrolls with it */
        app->sel = SDL_clamp(app->sel - (int)e->wheel.y, 0, sv.row_count - 1);
        app->store_qty = 1;
        return;
    }
    if (key_is(e, SDLK_RIGHT)) {
        app->store_qty++;
    } else if (key_is(e, SDLK_LEFT) && app->store_qty > 1) {
        app->store_qty--;
    } else if (is_confirm(e) && sv.row_count > 0) {
        if (!game_store_buy(g, sv.rows[app->sel].item, app->store_qty)) {
            app_set_message(app, app_text(app, "store.cannot_buy"));
        }
        commit(app);
    } else if (key_is(e, SDLK_S) && sv.row_count > 0) {
        if (!game_store_sell(g, sv.rows[app->sel].item, app->store_qty)) {
            app_set_message(app, app_text(app, "store.cannot_sell"));
        }
        commit(app);
    } else if (is_back(e)) {
        game_store_leave(g);
        app->sel = 0;
        commit(app);
    } else if (is_click(e)) {
        int row = (int)((app->my - STORE_Y) / app->draw.line_h) + app->scroll;
        if (app->my >= STORE_Y && row >= app->scroll && row < sv.row_count && row < app->scroll + STORE_ROWS) {
            if (row == app->sel) {
                if (!game_store_buy(g, sv.rows[row].item, app->store_qty)) {
                    app_set_message(app, app_text(app, "store.cannot_buy"));
                }
                commit(app);
            } else {
                app->sel = row;
                app->store_qty = 1;
            }
        }
    }
}

#define STORE_ARROW_X 616.0f

/* A small gold arrow at the edge of the goods list, pointing to goods out of sight. */
static void store_arrow(Draw *d, float cx, float y, bool up)
{
    for (int i = 0; i < 7; i++) {
        float w = 2.0f * i + 1;
        draw_rect(d, (SDL_FRect){cx - i, up ? y + i : y - i, w, 1}, COL_GOLD);
    }
}

static void store_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    StoreView sv;
    if (!game_store_view(g, &sv)) {
        return;
    }
    char right[80];
    LocText t;
    loctext_set(&t, "store.money");
    loctext_arg_money(&t, "n", sv.money);
    app_fmt_to(app, &t, right, sizeof right);
    draw_header(app, app_fmt(app, &sv.shop_name), right);
    char line[600];
    snprintf(line, sizeof line, "%s: %s", app_fmt(app, &sv.keeper), app_fmt(app, &sv.advice));
    float y = 24;
    y += draw_wrapped(d, d->font, line, 16, y, 608, COL_TEXT, false);
    if (sv.excuse.key[0]) {
        y += draw_wrapped(d, d->font, app_fmt(app, &sv.excuse), 16, y, 608, COL_GOLD, false);
    }
    float hy = STORE_Y - d->line_h - 2;
    draw_text(d, d->font, app_text(app, "store.col.item"), 20, hy, COL_DIM, TEXT_LEFT);
    draw_text(d, d->font, app_text(app, "store.col.price"), 330, hy, COL_DIM, TEXT_RIGHT);
    draw_text(d, d->font, app_text(app, "store.col.sell"), 410, hy, COL_DIM, TEXT_RIGHT);
    draw_text(d, d->font, app_text(app, "store.col.have"), 480, hy, COL_DIM, TEXT_RIGHT);
    draw_text(d, d->font, app_text(app, "store.col.stock"), 545, hy, COL_DIM, TEXT_RIGHT);
    if (app->sel < app->scroll) {
        app->scroll = app->sel;
    } else if (app->sel >= app->scroll + STORE_ROWS) {
        app->scroll = app->sel - STORE_ROWS + 1;
    }
    for (int i = app->scroll; i < sv.row_count && i < app->scroll + STORE_ROWS; i++) {
        const StoreRow *r = &sv.rows[i];
        float ry = STORE_Y + (i - app->scroll) * d->line_h;
        bool hot = i == app->sel;
        if (hot) {
            draw_rect(d, (SDL_FRect){14, ry, 612, (float)d->line_h}, COL_HILITE_BG);
        }
        snprintf(line, sizeof line, "%s, %s", app_text(app, app->data->items[r->item].name_key), app_fmt(app, &r->pack));
        char label_key[KEY_LEN + 8];
        snprintf(label_key, sizeof label_key, "%s.label", app->data->items[r->item].name_key);
        const char *label = locale_find(app->loc, label_key);
        if (label) {
            snprintf(line, sizeof line, "%s, %s", label, app_fmt(app, &r->pack));
        }
        draw_text(d, d->font, line, 20, ry, hot ? COL_GOLD : COL_TEXT, TEXT_LEFT);
        char money[48];
        fmt_money(app->loc, g->side, r->price, money, sizeof money);
        draw_text(d, d->font, money, 330, ry, COL_TEXT, TEXT_RIGHT);
        fmt_money(app->loc, g->side, r->sell_price, money, sizeof money);
        draw_text(d, d->font, money, 410, ry, COL_DIM, TEXT_RIGHT);
        snprintf(line, sizeof line, "%d", r->have);
        draw_text(d, d->font, line, 480, ry, COL_TEXT, TEXT_RIGHT);
        snprintf(line, sizeof line, "%d", r->stock);
        draw_text(d, d->font, line, 545, ry, COL_DIM, TEXT_RIGHT);
        if (hot) {
            snprintf(line, sizeof line, "x%d", app->store_qty);
            draw_text(d, d->font, line, 560, ry, COL_GOLD, TEXT_LEFT);
        }
    }
    loctext_set(&t, "store.load");
    loctext_arg_int(&t, "load", sv.load_lb);
    loctext_arg_int(&t, "cap", sv.capacity_lb);
    int shown = sv.row_count < STORE_ROWS ? sv.row_count : STORE_ROWS;
    float by = STORE_Y + shown * d->line_h + 4;
    /* more goods than fit: gold arrows at the top and bottom of the list, and how many are hidden */
    int above = app->scroll, below = sv.row_count - (app->scroll + shown);
    if (above > 0) {
        store_arrow(d, STORE_ARROW_X, STORE_Y + 2, true);
        LocText more;
        loctext_set(&more, "store.more_above");
        loctext_arg_int(&more, "n", above);
        draw_text(d, d->small, app_fmt(app, &more), 626, STORE_Y - d->line_h + 2, COL_GOLD, TEXT_RIGHT);
    }
    if (below > 0) {
        store_arrow(d, STORE_ARROW_X, STORE_Y + shown * d->line_h - 2, false);
        LocText more;
        loctext_set(&more, "store.more_below");
        loctext_arg_int(&more, "n", below);
        draw_text(d, d->small, app_fmt(app, &more), 626, STORE_Y + shown * d->line_h + 1, COL_GOLD, TEXT_RIGHT);
    }
    draw_text(d, d->font, app_fmt(app, &t), 20, by, sv.load_lb > sv.capacity_lb ? COL_RED : COL_DIM, TEXT_LEFT);
    loctext_set(&t, "status.food");
    loctext_arg_int(&t, "days", (long long)game_food_days(g));
    loctext_arg_int(&t, "lb", game_food_lb(g));
    loctext_arg_int(&t, "sheep", g->inv[app->data->item_sheep]);
    draw_text(d, d->font, app_fmt(app, &t), 330, by, COL_DIM, TEXT_LEFT);
    /* the road ahead, to set against the food: about how many days to each journey's end */
    char plan[400];
    snprintf(plan, sizeof plan, "%s", app_text(app, "store.plan"));
    for (int n = 0, shown_ends = 0; n < app->data->node_count; n++) {
        int days = app->data->nodes[n].epilogue_count > 0 ? game_days_to(g, n) : -1;
        if (days <= 0) {
            continue;
        }
        LocText p;
        loctext_set(&p, shown_ends++ ? "store.plan.next" : "store.plan.first");
        loctext_arg_key(&p, "place", app->data->nodes[n].name_key);
        loctext_arg_int(&p, "days", days);
        size_t len = strlen(plan);
        app_fmt_to(app, &p, plan + len, sizeof plan - len);
    }
    draw_wrapped(d, d->small, plan, 20, by + d->line_h, 600, COL_DIM, false);
    if (app->message_timer > 0) {
        draw_text(d, d->font, app->message, 320, by + d->line_h * 2 + 4, COL_RED, TEXT_CENTER);
    }
    draw_footer(app, app_text(app, "store.footer"));
}

/* ---------------------------------------------------------------- grave */

static void grave_event(App *app, const SDL_Event *e)
{
    if (!SDL_TextInputActive(app->window)) {
        SDL_StartTextInput(app->window);
    }
    if (e->type == SDL_EVENT_TEXT_INPUT) {
        size_t len = strlen(app->epitaph);
        snprintf(app->epitaph + len, sizeof app->epitaph - len, "%s", e->text.text);
    } else if (key_is(e, SDLK_BACKSPACE)) {
        size_t len = strlen(app->epitaph);
        while (len > 0 && ((unsigned char)app->epitaph[len - 1] & 0xC0) == 0x80) {
            len--;
        }
        if (len > 0) {
            len--;
        }
        app->epitaph[len] = '\0';
    } else if (key_is(e, SDLK_RETURN) || key_is(e, SDLK_KP_ENTER)) {
        game_grave_done(app->game, app->epitaph);
        app->epitaph[0] = '\0';
        SDL_StopTextInput(app->window);
        commit(app);
    }
}

static void grave_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    const Grave *gr = game_pending_grave(g);
    header_for_game(app);
    if (!gr) {
        return;
    }
    float cx = TREK_LOGICAL_W / 2.0f;
    SDL_Color stone = {120, 116, 108, 255}, stone_dark = {90, 86, 80, 255};
    draw_rect(d, (SDL_FRect){cx - 110, 60, 220, 200}, stone);
    draw_rect(d, (SDL_FRect){cx - 90, 46, 180, 16}, stone);
    draw_frame(d, (SDL_FRect){cx - 110, 60, 220, 200}, stone_dark);
    draw_rect(d, (SDL_FRect){0, 260, TREK_LOGICAL_W, 80}, (SDL_Color){92, 84, 56, 255});
    const Member *m = &g->party[gr->member];
    portrait_draw(d->r, "portrait_", m->portrait, cx - 180, 96, 1, true);
    draw_text(d, d->big, m->name, cx, 70, COL_BG, TEXT_CENTER);
    LocText t;
    loctext_set(&t, "grave.years");
    loctext_arg_int(&t, "born", m->birth_year);
    int y, mo, dd;
    cal_date(gr->day, &y, &mo, &dd);
    loctext_arg_int(&t, "died", y);
    draw_text(d, d->font, app_fmt(app, &t), cx, 110, COL_BG, TEXT_CENTER);
    loctext_set(&t, "grave.place");
    loctext_arg_date(&t, "date", gr->day);
    loctext_arg_key(&t, "place", app->data->nodes[gr->node].name_key);
    draw_wrapped(d, d->font, app_fmt(app, &t), cx - 96, 132, 192, COL_BG, false);
    /* beside the stone: who this was, and why they died */
    LocText story[3];
    int ns = game_grave_story(g, gr, story, 3);
    float sy = 64;
    for (int i = 0; i < ns; i++) {
        sy += draw_wrapped(d, d->font, app_fmt(app, &story[i]), cx + 122, sy, 192, i == 1 ? COL_GOLD : COL_TEXT, false) + 6;
    }
    char shown[120];
    snprintf(shown, sizeof shown, "%s%s", app->epitaph, (SDL_GetTicks() / 400) % 2 ? "_" : " ");
    draw_wrapped(d, d->font, shown, cx - 96, 190, 192, (SDL_Color){40, 34, 28, 255}, false);
    draw_text(d, d->font, app_text(app, "grave.prompt"), cx, 300, COL_TEXT, TEXT_CENTER);
    draw_footer(app, app_text(app, "grave.footer"));
}

/* ---------------------------------------------------------------- ending */

static void ending_event(App *app, const SDL_Event *e)
{
    if (key_is(e, SDLK_M)) {
        app->overlay = OV_MAP; /* the whole road, once more */
        return;
    }
    if (is_confirm(e) || is_back(e) || is_click(e)) {
        /* page 0 is the family's story; page 1, if there is one, what happened at that place afterwards */
        LocText lines[MAX_EPILOGUE];
        if (app->ending_page == 0 && !is_back(e) && game_history_epilogue(app->game, lines, MAX_EPILOGUE) > 0) {
            app->ending_page = 1;
            return;
        }
        app_close_game(app);
        app_scan_trips(app);
        app_switch(app, SCR_TITLE);
    }
}

static void ending_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    header_for_game(app);
    LocText lines[40];
    LocText history[MAX_EPILOGUE];
    int nh = game_history_epilogue(g, history, MAX_EPILOGUE);
    if (app->ending_page == 1 && nh > 0) {
        float y = 28;
        y += draw_wrapped(d, d->font, app_text(app, "ending.history.title"), 24, y, 592, COL_GOLD, false) + 6;
        for (int i = 0; i < nh && y < TREK_LOGICAL_H - 30; i++) {
            y += draw_wrapped(d, d->font, app_fmt(app, &history[i]), 24, y, 592, COL_TEXT, false) + 5;
        }
        draw_footer(app, app_text(app, "ending.footer"));
        return;
    }
    int n = game_epilogue(g, lines, 40);
    float y = 28;
    for (int i = 0; i < n && y < TREK_LOGICAL_H - 30; i++) {
        y += draw_wrapped(d, d->font, app_fmt(app, &lines[i]), 24, y, 592, i == 0 ? COL_GOLD : COL_TEXT, false) + 4;
    }
    draw_footer(app, app_text(app, nh > 0 ? "ending.footer.more" : "ending.footer"));
}

/* ---------------------------------------------------------------- dispatch */

void game_screen_event(App *app, const SDL_Event *e)
{
    Game *g = app->game;
    if (!g) {
        app_switch(app, SCR_TITLE);
        return;
    }
    GamePhase phase = game_phase(g);
    if (phase != PHASE_TRAVEL) {
        app->rest_menu = false;
    }
    if (is_back(e) && app->rest_menu) {
        app->rest_menu = false;
        app->sel = TR_REST;
        return;
    }
    if (is_back(e) && phase != PHASE_STORE && phase != PHASE_ENDED && phase != PHASE_BARTER) {
        app->travelling = false;
        app->overlay = OV_PAUSE;
        app->sel = 0;
        return;
    }
    switch (phase) {
    case PHASE_TRAVEL: travel_event(app, e); break;
    case PHASE_DIALOG: dialog_event(app, e); break;
    case PHASE_STORE: store_event(app, e); break;
    case PHASE_GRAVE: grave_event(app, e); break;
    case PHASE_ENDED: ending_event(app, e); break;
    case PHASE_HUNT: hunt_screen_event(app, e); break;
    case PHASE_BARTER: barter_screen_event(app, e); break;
    }
}

/* Plays sounds when the game changes: a whip as the wagons start, a shot or a misfire,
 * the river, an ox lowing when one dies, a bell for a grave. */
static void sound_watch(App *app)
{
    static const Game *last_game;
    static DialogKind last_dlg;
    static int last_shots, last_notices, last_graves;
    static bool last_travelling;
    Game *g = app->game;
    if (g != last_game) {
        last_game = g;
        last_dlg = g ? g->dlg.kind : DLG_NONE;
        last_shots = g ? g->hunt.shots : 0;
        last_notices = g ? g->notice_count : 0;
        last_graves = g ? g->pending_grave_count : 0;
        last_travelling = app->travelling;
        return;
    }
    if (!g) {
        return;
    }
    if (app->travelling && !last_travelling) {
        sound_play(SFX_WHIP);
    }
    if (g->hunt.shots > last_shots) {
        sound_play(g->hunt.shot_misfire ? SFX_MISFIRE : SFX_GUNSHOT);
    }
    if (g->dlg.kind == DLG_RIVER && last_dlg != DLG_RIVER) {
        sound_play(SFX_WATER);
    }
    if (g->notice_count > last_notices && strcmp(g->notices[g->notice_count - 1].key, "notice.ox_died") == 0) {
        sound_play(SFX_LOWING);
    }
    if (g->pending_grave_count > last_graves) {
        sound_play(SFX_BELL);
    }
    last_dlg = g->dlg.kind;
    last_shots = g->hunt.shots;
    last_notices = g->notice_count;
    last_graves = g->pending_grave_count;
    last_travelling = app->travelling;
}

void game_screen_update(App *app, double dt)
{
    sound_watch(app);
    Game *g = app->game;
    if (g && app->overlay == OV_NONE && game_phase(g) == PHASE_HUNT) {
        hunt_screen_update(app, dt);
        return;
    }
    if (!g || !app->travelling || app->overlay != OV_NONE) {
        return;
    }
    if (game_phase(g) != PHASE_TRAVEL) {
        app->travelling = false;
        return;
    }
    static const double DAY_SECONDS[3] = {0.9, 0.5, 0.2};
    app->travel_timer += dt;
    if (app->travel_timer >= DAY_SECONDS[app->settings.travel_speed]) {
        app->travel_timer = 0;
        bool stopped = game_travel_day(g);
        commit(app);
        if (stopped) {
            app->travelling = false;
            app->sel = 0;
        }
    }
}

void game_screen_render(App *app)
{
    Game *g = app->game;
    if (!g) {
        return;
    }
    if (game_phase(g) != PHASE_ENDED) {
        app->ending_page = 0;
    }
    switch (game_phase(g)) {
    case PHASE_TRAVEL: travel_render(app); break;
    case PHASE_DIALOG: dialog_render(app); break;
    case PHASE_STORE: store_render(app); break;
    case PHASE_GRAVE: grave_render(app); break;
    case PHASE_ENDED: ending_render(app); break;
    case PHASE_HUNT: hunt_screen_render(app); break;
    case PHASE_BARTER: barter_screen_render(app); break;
    }
}
