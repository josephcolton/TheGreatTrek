/* --check-text: lays out every piece of text the game can show, with the real fonts, and lists any
 * that would run off the screen: each event page and result, each landmark, each fork, each
 * history note and each history page after an ending. It plays nothing; it sets up a game in the
 * state that shows each page and measures it, for both sides. */
#include "screens.h"

#include <stdio.h>
#include <string.h>

#define BOTTOM (TREK_LOGICAL_H - 22.0f) /* the footer starts here */

static int report(const char *what, const char *id, float bottom)
{
    if (bottom <= BOTTOM) {
        return 0;
    }
    printf("too long: %s %s (ends at %.0f, room to %.0f)\n", what, id, bottom, BOTTOM);
    return 1;
}

static int check_dialog(App *app, const char *what, const char *id)
{
    DialogView v;
    if (!game_dialog_view(app->game, &v)) {
        return 0;
    }
    return report(what, id, dialog_text_bottom(app, &v));
}

/* A page of plain text at the overlay's width: the history note, and each history page. */
static float page_bottom(App *app, const LocText *lines, int n, float x, float y, int w, float gap)
{
    for (int i = 0; i < n; i++) {
        y += draw_wrapped(&app->draw, app->font, app_fmt(app, &lines[i]), x, y, w, COL_TEXT, true) + gap;
    }
    return y;
}

static int check_side(App *app, int family)
{
    const GameData *d = app->data;
    Game *g = game_new(d, 1, family, DIFF_NORMAL);
    if (!g) {
        return 0;
    }
    app->game = g;
    int bad = 0;
    char id[160];

    /* every event page, and every result page */
    for (int e = 0; e < d->event_count; e++) {
        const EventDef *ev = &d->events[e];
        for (int k = 0; k < ev->node_count; k++) {
            memset(&g->dlg, 0, sizeof g->dlg);
            g->dlg.kind = DLG_EVENT;
            g->dlg.event = e;
            g->dlg.node = k;
            g->dlg.speaker = g->dlg.subject = 0;
            snprintf(id, sizeof id, "%s/%s", ev->id, ev->nodes[k].id);
            bad += check_dialog(app, "event", id);
            for (int c = 0; c < ev->nodes[k].choice_count; c++) {
                const Choice *ch = &ev->nodes[k].choices[c];
                for (int o = 0; o < ch->outcome_count; o++) {
                    if (!ch->outcomes[o].text_key[0]) {
                        continue;
                    }
                    g->dlg.showing_result = true;
                    g->dlg.result_count = 1;
                    loctext_set(&g->dlg.result[0], ch->outcomes[o].text_key);
                    loctext_arg_person(&g->dlg.result[0], "who", &g->party[0]);
                    loctext_arg_person(&g->dlg.result[0], "speaker", &g->party[0]);
                    loctext_arg_person(&g->dlg.result[0], "head", &g->party[0]);
                    snprintf(id, sizeof id, "%s", ch->outcomes[o].text_key);
                    bad += check_dialog(app, "result", id);
                    g->dlg.showing_result = false;
                }
            }
        }
        /* the history note behind the event */
        if (ev->history_key[0]) {
            LocText t;
            loctext_set(&t, ev->history_key);
            bad += report("history note", ev->history_key, page_bottom(app, &t, 1, 36, 46, 568, 0) + 14);
        }
    }

    /* every place: what you see on arriving, its forks, and its history page after an ending */
    for (int n = 0; n < d->node_count; n++) {
        memset(&g->dlg, 0, sizeof g->dlg);
        g->node = n;
        g->edge = -1;
        g->at_node = true;
        g->dlg.kind = DLG_LANDMARK;
        bad += check_dialog(app, "place", d->nodes[n].id);
        g->dlg.kind = DLG_ROUTE;
        bad += check_dialog(app, "fork", d->nodes[n].id);
        LocText t;
        loctext_set(&t, d->nodes[n].history_key);
        bad += report("place history", d->nodes[n].history_key, page_bottom(app, &t, 1, 36, 46, 568, 0) + 14);
        g->ending = ENDING_ARRIVED;
        LocText lines[MAX_EPILOGUE + 1];
        loctext_set(&lines[0], "ending.history.title");
        int count = 1 + game_history_epilogue(g, lines + 1, MAX_EPILOGUE);
        if (count > 1) {
            bad += report("history page", d->nodes[n].id, page_bottom(app, lines, count, 24, 28, 592, 5));
        }
        g->ending = ENDING_NONE;
    }
    memset(&g->dlg, 0, sizeof g->dlg);
    app->game = NULL;
    game_free(g);
    return bad;
}

int text_check(App *app)
{
    draw_update_scale(&app->draw);
    int bad = 0;
    bool side_done[SIDE_COUNT] = {false};
    for (int f = 0; f < app->data->family_count; f++) {
        Side s = app->data->families[f].side;
        if (!side_done[s]) {
            side_done[s] = true;
            bad += check_side(app, f);
        }
    }
    printf(bad ? "check-text: %d too long\n" : "check-text: ok\n", bad);
    return bad;
}
