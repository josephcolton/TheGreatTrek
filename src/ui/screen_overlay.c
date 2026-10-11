/* Overlays drawn over the game: pause menu, orders, supplies, party, map, journal, history notes. */
#include "calendar.h"
#include "format.h"
#include "screens.h"

#include <stdio.h>
#include <string.h>

static void close_overlay(App *app)
{
    app->overlay = OV_NONE;
    app->sel = 0;
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

typedef enum { PA_RESUME, PA_SAVE_QUIT, PA_SETTINGS, PA_COUNT } PauseItem;

static void pause_items(App *app, ChoiceItem *items)
{
    items[PA_RESUME] = (ChoiceItem){app_text(app, "pause.resume"), NULL};
    items[PA_SAVE_QUIT] = (ChoiceItem){app_text(app, "pause.save_quit"), NULL};
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
    case PA_SETTINGS: app_switch(app, SCR_SETTINGS); break;
    default: break;
    }
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
        snprintf(line, sizeof line, "%s", m->name);
        draw_text(d, d->font, line, 36, y, c, TEXT_LEFT);
        snprintf(line, sizeof line, "%s, %d", app_text(app, role_key), 1837 - m->birth_year);
        draw_text(d, d->font, line, 140, y, COL_DIM, TEXT_LEFT);
        draw_text(d, d->font, cond, 330, y, c, TEXT_LEFT);
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
            draw_text(d, d->font, skills, 140, y, COL_DIM, TEXT_LEFT);
            y += d->line_h;
        }
        y += 2;
    }
    draw_text(d, d->font, app_text(app, "overlay.close"), TREK_LOGICAL_W / 2.0f, TREK_LOGICAL_H - 32, COL_DIM, TEXT_CENTER);
}

/* ---------------------------------------------------------------- map */

static void map_line(App *app, float x0, float y0, float x1, float y1, SDL_Color c)
{
    SDL_SetRenderDrawColor(app->renderer, c.r, c.g, c.b, c.a);
    SDL_RenderLine(app->renderer, x0, y0, x1, y1);
}

static void map_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    const GameData *data = app->data;
    overlay_panel(app, app_text(app, "map.title"));
    /* map_xy is on a 640x360 sheet; fit it into the panel */
    float ox = 30, oy = 40, sx = 580.0f / 640.0f, sy = 290.0f / 360.0f;
#define MX(x) (ox + (x) * sx)
#define MY(y) (oy + (y) * sy)
    /* rivers, schematic */
    SDL_Color river = {90, 130, 170, 255};
    map_line(app, MX(60), MY(225), MX(183), MY(210), river);
    map_line(app, MX(183), MY(210), MX(347), MY(212), river);
    map_line(app, MX(347), MY(212), MX(420), MY(200), river);
    map_line(app, MX(347), MY(212), MX(390), MY(170), river);
    map_line(app, MX(390), MY(170), MX(430), MY(120), river);
    map_line(app, MX(330), MY(60), MX(443), MY(46), river);
    map_line(app, MX(443), MY(46), MX(520), MY(30), river);
    /* the peoples of the region, labelled softly over their country (no borders) */
    static const struct { const char *key; float x, y; } PEOPLES[] = {
        {"map.people.griqua", 120, 175}, {"map.people.barolong", 330, 132}, {"map.people.basotho", 450, 175},
        {"map.people.ndebele", 400, 18}, {"map.people.xhosa", 520, 330}, {"map.people.colony", 160, 300}};
    for (size_t i = 0; i < sizeof PEOPLES / sizeof PEOPLES[0]; i++) {
        draw_text(d, d->font, app_text(app, PEOPLES[i].key), MX(PEOPLES[i].x), MY(PEOPLES[i].y), (SDL_Color){120, 104, 80, 255}, TEXT_CENTER);
    }
    /* roads */
    for (int e = 0; e < data->edge_count; e++) {
        const EdgeDef *ed = &data->edges[e];
        const NodeDef *a = &data->nodes[ed->from], *b = &data->nodes[ed->to];
        bool known = g->visited[ed->from];
        bool taken = g->visited[ed->from] && g->visited[ed->to];
        SDL_Color c = taken ? COL_GOLD : (known ? COL_DIM : (SDL_Color){80, 68, 52, 255});
        map_line(app, MX(a->map_x), MY(a->map_y), MX(b->map_x), MY(b->map_y), c);
    }
    /* places */
    for (int n = 0; n < data->node_count; n++) {
        const NodeDef *nd = &data->nodes[n];
        bool near = g->visited[n];
        for (int e = 0; e < data->edge_count && !near; e++) {
            near = data->edges[e].to == n && g->visited[data->edges[e].from];
        }
        SDL_Color c = g->visited[n] ? COL_GOLD : (near ? COL_TEXT : COL_LOCKED);
        draw_rect(d, (SDL_FRect){MX(nd->map_x) - 2, MY(nd->map_y) - 2, 5, 5}, c);
        draw_text(d, d->font, near ? app_text(app, nd->name_key) : "?", MX(nd->map_x) + 5, MY(nd->map_y) - 8, c, TEXT_LEFT);
    }
    /* graves of this party */
    for (int i = 0; i < g->grave_count; i++) {
        const NodeDef *nd = &data->nodes[g->graves[i].node];
        float gx = MX(nd->map_x) - 6, gy = MY(nd->map_y) + 4;
        draw_rect(d, (SDL_FRect){gx + 1, gy, 1, 6}, COL_TEXT);
        draw_rect(d, (SDL_FRect){gx, gy + 2, 3, 1}, COL_TEXT);
    }
    /* the wagon */
    float wx, wy;
    const NodeDef *cur = &data->nodes[g->node];
    if (g->edge >= 0) {
        const NodeDef *to = &data->nodes[data->edges[g->edge].to];
        float f = (float)(g->miles_along / data->edges[g->edge].miles);
        wx = cur->map_x + (to->map_x - cur->map_x) * f;
        wy = cur->map_y + (to->map_y - cur->map_y) * f;
    } else {
        wx = (float)cur->map_x;
        wy = (float)cur->map_y;
    }
    draw_rect(d, (SDL_FRect){MX(wx) - 3, MY(wy) - 3, 7, 7}, COL_RED);
#undef MX
#undef MY
    draw_text(d, d->font, app_text(app, "overlay.close"), TREK_LOGICAL_W / 2.0f, TREK_LOGICAL_H - 32, COL_DIM, TEXT_CENTER);
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
            int hit = choices_hit(&app->draw, items, PA_COUNT, 190, 120, 260, app->mx, app->my, true);
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
        draw_choices(&app->draw, items, PA_COUNT, 190, 120, 260, app->mx, app->my, app->sel, true, NULL);
        draw_wrapped(&app->draw, app->font, app_text(app, "pause.about"), 120, 220, 400, COL_DIM, false);
        break;
    }
    case OV_ORDERS: orders_render(app); break;
    case OV_SUPPLIES: supplies_render(app); break;
    case OV_PARTY: party_render(app); break;
    case OV_MAP: map_render(app); break;
    case OV_JOURNAL: journal_render(app); break;
    case OV_HISTORY: history_render(app); break;
    default: break;
    }
}
