/* The barter screen: two lists, your goods and theirs, and a meter showing how they see the deal. */
#include "format.h"
#include "screens.h"

#include <stdio.h>
#include <string.h>

#define COL_W 296.0f
#define ROWS_Y 92.0f

static int barter_col; /* 0 = your goods, 1 = theirs */

static const char *item_label(App *app, int item)
{
    char key[KEY_LEN + 8];
    snprintf(key, sizeof key, "%s.label", app->data->items[item].name_key);
    const char *label = locale_find(app->loc, key);
    return label ? label : app_text(app, app->data->items[item].name_key);
}

void barter_screen_event(App *app, const SDL_Event *e)
{
    Game *g = app->game;
    BarterView v;
    if (!game_barter_view(g, &v)) {
        return;
    }
    int n = barter_col ? v.take_count : v.give_count;
    if (key_is(e, SDLK_LEFT) || key_is(e, SDLK_RIGHT) || key_is(e, SDLK_TAB)) {
        barter_col = 1 - barter_col;
        app->sel = 0;
        return;
    }
    nav_updown(app, e, n);
    if (key_is(e, SDLK_EQUALS) || key_is(e, SDLK_PLUS) || key_is(e, SDLK_KP_PLUS) || is_confirm(e)) {
        game_barter_adjust(g, barter_col == 1, app->sel, 1);
    } else if (key_is(e, SDLK_MINUS) || key_is(e, SDLK_KP_MINUS) || key_is(e, SDLK_BACKSPACE)) {
        game_barter_adjust(g, barter_col == 1, app->sel, -1);
    } else if (key_is(e, SDLK_P)) {
        game_barter_propose(g);
    } else if (key_is(e, SDLK_A)) {
        game_barter_ask(g);
    } else if (is_back(e)) {
        game_barter_leave(g);
        app->sel = 0;
        barter_col = 0;
    } else if (is_click(e)) {
        int col = app->mx < 320 ? 0 : 1;
        int row = (int)((app->my - ROWS_Y) / app->draw.line_h);
        int count = col ? v.take_count : v.give_count;
        if (app->my >= ROWS_Y && row >= 0 && row < count) {
            barter_col = col;
            app->sel = row;
            /* click the right half of a row to add, the left half to take away */
            float x0 = col ? 330.0f : 16.0f;
            game_barter_adjust(g, col == 1, row, app->mx > x0 + COL_W * 0.6f ? 1 : -1);
        }
    }
    app_commit(app);
}

static void draw_list(App *app, const BarterRow *rows, int n, float x, bool theirs, bool active)
{
    Draw *d = &app->draw;
    for (int i = 0; i < n; i++) {
        const BarterRow *r = &rows[i];
        float y = ROWS_Y + i * d->line_h;
        bool hot = active && i == app->sel;
        if (hot) {
            draw_rect(d, (SDL_FRect){x - 4, y, COL_W, (float)d->line_h}, COL_HILITE_BG);
        }
        SDL_Color c = (!theirs && r->have == 0) ? COL_LOCKED : (hot ? COL_GOLD : COL_TEXT);
        draw_text(d, d->font, item_label(app, r->item), x, y, c, TEXT_LEFT);
        LocText t;
        loctext_set(&t, theirs ? "barter.left" : "barter.have");
        loctext_arg_int(&t, "n", r->have);
        draw_text(d, d->font, app_fmt(app, &t), x + 150, y, COL_DIM, TEXT_LEFT);
        if (r->wanted_highly) {
            draw_text(d, d->font, app_text(app, "barter.wanted"), x + COL_W - 40, y, COL_GREEN, TEXT_RIGHT);
        }
        char amt[32];
        snprintf(amt, sizeof amt, "%d", r->amount);
        draw_text(d, d->font, amt, x + COL_W - 12, y, r->amount ? COL_GOLD : COL_DIM, TEXT_RIGHT);
    }
}

void barter_screen_render(App *app)
{
    Draw *d = &app->draw;
    Game *g = app->game;
    BarterView v;
    if (!game_barter_view(g, &v)) {
        return;
    }
    char title[160];
    snprintf(title, sizeof title, "%s  ·  %s", app_text(app, "barter.title"), app_fmt(app, &v.partner));
    draw_header(app, title, NULL);
    draw_wrapped(d, d->font, app_fmt(app, &v.reply), 16, 24, 608, COL_TEXT, false);
    draw_text(d, d->font, app_text(app, "barter.you_offer"), 16, ROWS_Y - d->line_h - 2, barter_col == 0 ? COL_GOLD : COL_DIM, TEXT_LEFT);
    draw_text(d, d->font, app_text(app, "barter.they_offer"), 330, ROWS_Y - d->line_h - 2, barter_col == 1 ? COL_GOLD : COL_DIM, TEXT_LEFT);
    draw_list(app, v.give, v.give_count, 16, false, barter_col == 0);
    draw_list(app, v.take, v.take_count, 330, true, barter_col == 1);

    /* the meter: how the other side sees the deal */
    float my = 274;
    draw_text(d, d->font, app_text(app, "barter.meter"), 16, my, COL_DIM, TEXT_LEFT);
    SDL_FRect bar = {200, my + 3, 300, 10};
    draw_rect(d, bar, COL_PANEL);
    draw_frame(d, bar, COL_BORDER);
    double ratio = v.need_value > 0 ? v.give_value / v.need_value : 0;
    if (ratio > 1.5) {
        ratio = 1.5;
    }
    SDL_Color fill = ratio < 1.0 ? COL_RED : (ratio > 1.25 ? COL_GREEN : COL_GOLD);
    draw_rect(d, (SDL_FRect){bar.x + 1, bar.y + 1, (float)(ratio / 1.5 * (bar.w - 2)), bar.h - 2}, fill);
    draw_rect(d, (SDL_FRect){bar.x + bar.w / 1.5f, bar.y - 2, 1, bar.h + 4}, COL_TEXT);
    const char *word = v.take_value <= 0 ? "" : (ratio < 1.0 ? "barter.meter.short" : (ratio > 1.25 ? "barter.meter.generous" : "barter.meter.fair"));
    if (word[0]) {
        draw_text(d, d->font, app_text(app, word), 510, my, fill.r == COL_RED.r ? COL_RED : COL_TEXT, TEXT_LEFT);
    }
    if (v.contraband) {
        draw_wrapped(d, d->font, app_text(app, "barter.contraband"), 16, my + d->line_h + 4, 608, COL_RED, false);
    }
    draw_footer(app, app_text(app, "barter.footer"));
}
