/* Starting a new trip: side, family, the family's story, naming the party. */
#include "screens.h"
#include "sprites.h"

#include <stdio.h>
#include <string.h>

static char new_names[MAX_MEMBERS][32];

static int side_families(const App *app, Side side, int *out)
{
    int n = 0;
    for (int i = 0; i < app->data->family_count; i++) {
        if (app->data->families[i].side == side) {
            out[n++] = i;
        }
    }
    return n;
}

/* ---------------------------------------------------------------- side */

#define SIDE_Y 40.0f
#define SIDE_W 290.0f

/* ---------------------------------------------------------------- level */

/* The first step of a new trip: how hard. It is fixed for the trip; the last choice is offered first. */
#define LEVEL_X 140.0f
#define LEVEL_Y 60.0f
#define LEVEL_W 360

static void level_items(App *app, ChoiceItem *items)
{
    for (int i = 0; i < DIFF_COUNT; i++) {
        char key[48];
        snprintf(key, sizeof key, "difficulty.%s", difficulty_id((Difficulty)i));
        items[i] = (ChoiceItem){app_text(app, key), NULL};
    }
}

static void choose_level(App *app, int level)
{
    app->settings.difficulty = (Difficulty)level;
    app_save_settings(app);
    app_switch(app, SCR_SIDE);
}

void level_event(App *app, const SDL_Event *e)
{
    ChoiceItem items[DIFF_COUNT];
    level_items(app, items);
    nav_updown(app, e, DIFF_COUNT);
    int digit = key_digit(e);
    if (digit >= 0 && digit < DIFF_COUNT) {
        choose_level(app, digit);
    } else if (is_confirm(e)) {
        choose_level(app, app->sel);
    } else if (is_back(e)) {
        app_switch(app, SCR_TITLE);
    } else if (is_click(e)) {
        int hit = choices_hit(&app->draw, items, DIFF_COUNT, LEVEL_X, LEVEL_Y, LEVEL_W, app->mx, app->my, true);
        if (hit >= 0) {
            choose_level(app, hit);
        }
    }
}

void level_render(App *app)
{
    Draw *d = &app->draw;
    draw_header(app, app_text(app, "level.title"), NULL);
    ChoiceItem items[DIFF_COUNT];
    level_items(app, items);
    float bottom = 0;
    draw_choices(d, items, DIFF_COUNT, LEVEL_X, LEVEL_Y, LEVEL_W, app->mx, app->my, app->sel, true, &bottom);
    /* what the highlighted level means */
    int shown = app->sel >= 0 && app->sel < DIFF_COUNT ? app->sel : 0;
    char key[48];
    snprintf(key, sizeof key, "difficulty.%s.about", difficulty_id((Difficulty)shown));
    draw_wrapped(d, d->font, app_text(app, key), LEVEL_X, bottom + 16, LEVEL_W, COL_DIM, false);
    draw_footer(app, app_text(app, "level.footer"));
}

/* ---------------------------------------------------------------- side */

static SDL_FRect side_rect(int s)
{
    return (SDL_FRect){20.0f + s * (SIDE_W + 20.0f), SIDE_Y, SIDE_W, 280};
}

static void choose_side(App *app, Side s)
{
    app->new_side = s;
    app_switch(app, SCR_FAMILY);
}

void side_event(App *app, const SDL_Event *e)
{
    if (key_is(e, SDLK_LEFT) || key_is(e, SDLK_RIGHT)) {
        app->sel = 1 - app->sel;
    }
    int digit = key_digit(e);
    if (digit == 0 || digit == 1) {
        choose_side(app, (Side)digit);
    } else if (is_confirm(e)) {
        choose_side(app, (Side)app->sel);
    } else if (is_back(e)) {
        app_switch(app, SCR_LEVEL);
        app->sel = app->settings.difficulty;
    } else if (is_click(e)) {
        for (int s = 0; s < SIDE_COUNT; s++) {
            SDL_FRect r = side_rect(s);
            if (app->mx >= r.x && app->mx < r.x + r.w && app->my >= r.y && app->my < r.y + r.h) {
                choose_side(app, (Side)s);
            }
        }
    }
}

void side_render(App *app)
{
    Draw *d = &app->draw;
    draw_header(app, app_text(app, "side.title"), NULL);
    int hover = -1;
    for (int s = 0; s < SIDE_COUNT; s++) {
        SDL_FRect r = side_rect(s);
        if (app->mx >= r.x && app->mx < r.x + r.w && app->my >= r.y && app->my < r.y + r.h) {
            hover = s;
        }
    }
    draw_sync_hover(d, hover, app->sel, side_rect(app->sel), true);
    for (int s = 0; s < SIDE_COUNT; s++) {
        SDL_FRect r = side_rect(s);
        bool hot = s == app->sel;
        draw_panel(d, r);
        if (hot) {
            draw_frame(d, (SDL_FRect){r.x + 2, r.y + 2, r.w - 4, r.h - 4}, COL_GOLD);
        }
        char key[48];
        snprintf(key, sizeof key, "side.%s", side_id((Side)s));
        char num[64];
        snprintf(num, sizeof num, "%d  %s", s + 1, app_text(app, key));
        draw_text(d, d->font, num, r.x + 10, r.y + 8, COL_GOLD, TEXT_LEFT);
        snprintf(key, sizeof key, "side.%s.about", side_id((Side)s));
        draw_wrapped(d, d->font, app_text(app, key), r.x + 10, r.y + 30, (int)r.w - 20, COL_TEXT, false);
    }
    draw_footer(app, app_text(app, "side.footer"));
}

/* ---------------------------------------------------------------- family */

#define FAM_X 20.0f
#define FAM_Y 30.0f
#define FAM_W 600

static void family_items(App *app, ChoiceItem *items, char bufs[][400], int *fams, int n)
{
    for (int i = 0; i < n; i++) {
        const FamilyDef *f = &app->data->families[fams[i]];
        snprintf(bufs[i], 400, "%s. %s", app_text(app, f->name_key), app_text(app, f->summary_key));
        items[i] = (ChoiceItem){bufs[i], NULL};
    }
}

static void choose_family(App *app, int family)
{
    app->new_family = family;
    const FamilyDef *f = &app->data->families[family];
    for (int i = 0; i < f->member_count; i++) {
        snprintf(new_names[i], sizeof new_names[i], "%s", f->members[i].name);
    }
    app_switch(app, SCR_STORY);
}

void family_event(App *app, const SDL_Event *e)
{
    int fams[MAX_FAMILIES];
    int n = side_families(app, app->new_side, fams);
    ChoiceItem items[MAX_FAMILIES];
    char bufs[MAX_FAMILIES][400];
    family_items(app, items, bufs, fams, n);
    nav_updown(app, e, n);
    int digit = key_digit(e);
    if (digit >= 0 && digit < n) {
        choose_family(app, fams[digit]);
    } else if (is_confirm(e) && n > 0) {
        choose_family(app, fams[app->sel]);
    } else if (is_back(e)) {
        app_switch(app, SCR_SIDE);
        app->sel = app->new_side;
    } else if (is_click(e)) {
        int hit = choices_hit(&app->draw, items, n, FAM_X, FAM_Y, FAM_W, app->mx, app->my, true);
        if (hit >= 0) {
            choose_family(app, fams[hit]);
        }
    }
}

void family_render(App *app)
{
    int fams[MAX_FAMILIES];
    int n = side_families(app, app->new_side, fams);
    ChoiceItem items[MAX_FAMILIES];
    char bufs[MAX_FAMILIES][400];
    family_items(app, items, bufs, fams, n);
    draw_header(app, app_text(app, "family.title"), NULL);
    draw_choices(&app->draw, items, n, FAM_X, FAM_Y, FAM_W, app->mx, app->my, app->sel, true, NULL);
    LocText footer;
    loctext_set(&footer, "family.footer");
    loctext_arg_int(&footer, "n", n);
    draw_footer(app, app_fmt(app, &footer));
}

/* ---------------------------------------------------------------- story */

void story_event(App *app, const SDL_Event *e)
{
    if (is_confirm(e) || is_click(e)) {
        app_switch(app, SCR_NAMES);
        app->edit_member = -1;
    } else if (is_back(e)) {
        app_switch(app, SCR_FAMILY);
    }
}

void story_render(App *app)
{
    Draw *d = &app->draw;
    const FamilyDef *f = &app->data->families[app->new_family];
    draw_header(app, app_text(app, f->name_key), NULL);
    float y = 28;
    for (int p = 1; p <= 3; p++) {
        char key[80];
        snprintf(key, sizeof key, "%s.%d", f->story_key, p);
        y += draw_wrapped(d, d->font, app_text(app, key), 24, y, 592, COL_TEXT, false) + 8;
    }
    /* the family and their people side by side, if there is room under the story */
    float gap = 6, pw = 42;
    float row_w = f->member_count * (pw + gap) - gap;
    float py = TREK_LOGICAL_H - 18 - 50 - 8;
    if (y <= py) {
        float x = (TREK_LOGICAL_W - row_w) / 2;
        for (int i = 0; i < f->member_count; i++) {
            portrait_draw(d->r, "portrait_", f->members[i].portrait, x + i * (pw + gap), py, 1, false);
        }
    }
    draw_footer(app, app_text(app, "story.footer"));
}

/* ---------------------------------------------------------------- names */

#define NAMES_X 40.0f
#define NAMES_Y 40.0f

static int age_in_1837(int born)
{
    return 1837 - born;
}

static void start_trip(App *app)
{
    uint64_t seed = SDL_GetTicksNS() ^ ((uint64_t)SDL_GetPerformanceCounter() << 17);
    SDL_Time now = 0;
    SDL_GetCurrentTime(&now);
    seed ^= (uint64_t)now;
    if (app->test_seed) {
        seed = app->test_seed;
    }
    Game *g = game_new(app->data, seed, app->new_family, app->settings.difficulty);
    if (!g) {
        return;
    }
    for (int i = 0; i < g->party_count; i++) {
        if (new_names[i][0]) {
            snprintf(g->party[i].name, sizeof g->party[i].name, "%.31s", new_names[i]);
        }
    }
    app_close_game(app);
    app->game = g;
    trips_new_id(app->trip_id, sizeof app->trip_id, seed);
    app_commit(app);
    trips_scan(app->pref_path, &app->trips);
    app_switch(app, SCR_GAME);
    if (!app->settings.help_seen) {
        /* the first trip opens with how to play; it is in the pause menu after that */
        app->overlay = OV_HELP;
        app->settings.help_seen = true;
        app_save_settings(app);
    }
}

static void begin_edit(App *app, int member)
{
    app->edit_member = member;
    snprintf(app->edit_buf, sizeof app->edit_buf, "%s", new_names[member]);
    SDL_StartTextInput(app->window);
}

static void end_edit(App *app, bool keep)
{
    if (keep && app->edit_buf[0]) {
        snprintf(new_names[app->edit_member], sizeof new_names[0], "%s", app->edit_buf);
    }
    app->edit_member = -1;
    SDL_StopTextInput(app->window);
}

void names_event(App *app, const SDL_Event *e)
{
    const FamilyDef *f = &app->data->families[app->new_family];
    int n = f->member_count + 1; /* last row: begin the trek */
    if (app->edit_member >= 0) {
        if (e->type == SDL_EVENT_TEXT_INPUT) {
            size_t len = strlen(app->edit_buf);
            snprintf(app->edit_buf + len, sizeof app->edit_buf - len, "%s", e->text.text);
        } else if (key_is(e, SDLK_BACKSPACE)) {
            size_t len = strlen(app->edit_buf);
            /* remove one UTF-8 character */
            while (len > 0 && ((unsigned char)app->edit_buf[len - 1] & 0xC0) == 0x80) {
                len--;
            }
            if (len > 0) {
                len--;
            }
            app->edit_buf[len] = '\0';
        } else if (key_is(e, SDLK_RETURN) || key_is(e, SDLK_KP_ENTER)) {
            end_edit(app, true);
        } else if (is_back(e)) {
            end_edit(app, false);
        }
        return;
    }
    nav_updown(app, e, n);
    if (is_back(e)) {
        app_switch(app, SCR_STORY);
        return;
    }
    if (key_is(e, SDLK_R) && app->sel < f->member_count) {
        const MemberDef *m = &f->members[app->sel];
        const char *name = app_random_name(app, f->side, m->role == ROLE_SERVANT, m->pronoun);
        if (name) {
            snprintf(new_names[app->sel], sizeof new_names[0], "%s", name);
        }
        return;
    }
    int row = -1;
    if (is_confirm(e)) {
        row = app->sel;
    } else if (is_click(e)) {
        int r = (int)((app->my - NAMES_Y) / app->draw.line_h);
        if (app->my >= NAMES_Y && r >= 0 && r < n) {
            row = r;
            app->sel = r;
        }
    }
    if (row >= 0) {
        if (row == f->member_count) {
            start_trip(app);
        } else {
            begin_edit(app, row);
        }
    }
}

void names_render(App *app)
{
    Draw *d = &app->draw;
    const FamilyDef *f = &app->data->families[app->new_family];
    draw_header(app, app_text(app, "names.title"), app_text(app, f->name_key));
    float y = NAMES_Y;
    for (int i = 0; i <= f->member_count; i++) {
        bool hot = i == app->sel;
        if (hot) {
            draw_rect(d, (SDL_FRect){NAMES_X - 4, y, 470, (float)d->line_h}, COL_HILITE_BG);
            if (i < f->member_count) {
                portrait_draw(d->r, "portrait_", f->members[i].portrait, 520, NAMES_Y, 2, false);
            }
        }
        if (i == f->member_count) {
            draw_text(d, d->font, app_text(app, "names.begin"), NAMES_X, y, COL_GOLD, TEXT_LEFT);
            break;
        }
        const MemberDef *m = &f->members[i];
        const char *name = (i == app->edit_member) ? app->edit_buf : new_names[i];
        char role_key[32], line[160];
        member_role_key(m->role, m->pronoun, role_key, sizeof role_key);
        draw_text(d, d->font, name, NAMES_X, y, i == app->edit_member ? COL_GOLD : COL_TEXT, TEXT_LEFT);
        if (i == app->edit_member && (SDL_GetTicks() / 400) % 2 == 0) {
            draw_text(d, d->font, "_", NAMES_X + text_width(d->font, name), y, COL_GOLD, TEXT_LEFT);
        }
        LocText t;
        loctext_set(&t, "names.row");
        loctext_arg_key(&t, "role", role_key);
        loctext_arg_int(&t, "age", age_in_1837(m->birth_year));
        app_fmt_to(app, &t, line, sizeof line);
        draw_text(d, d->font, line, NAMES_X + 150, y, COL_DIM, TEXT_LEFT);
        y += d->line_h;
    }
    draw_footer(app, app_text(app, app->edit_member >= 0 ? "names.footer_edit" : "names.footer"));
}
