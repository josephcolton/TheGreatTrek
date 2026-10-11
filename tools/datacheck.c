/* datacheck: validates the files in data/. Usage: datacheck [data_dir]
 * - every JSON file loads and every id it refers to exists (data_load does this)
 * - every event has sources (data_load refuses events without them)
 * - every locale key the data or the code refers to exists in locale/en.json
 * - reports how many prices come from a period source and how many are estimates */
#include "data.h"
#include "locale.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const Locale *loc;
    int missing;
} KeyCheck;

static void check_key(const char *key, const char *where, void *ctx)
{
    KeyCheck *kc = ctx;
    if (!locale_find(kc->loc, key)) {
        fprintf(stderr, "missing locale key \"%s\" (used by %s)\n", key, where);
        kc->missing++;
    }
}

/* Keys the C code asks for directly. Keep in step with the sources (grep for loctext_set). */
static const char *CODE_KEYS[] = {
    "app.title", "app.subtitle",
    "journal.start", "journal.arrived", "journal.arrived_end", "journal.left", "journal.broke",
    "journal.recovered", "journal.crossed", "journal.traded", "journal.gave_up", "journal.paid_in_sheep", "journal.slaughtered_cow",
    "notice.death", "notice.servant_left", "notice.member_left", "notice.no_water", "notice.ox_died",
    "notice.fell_ill", "notice.near_death", "notice.accident", "notice.wages_unpaid", "notice.food_low", "notice.water_low",
    "title.notice", "choice.continue", "speaker.member", "lock.generic",
    "result.gained", "result.lost", "result.money_gained", "result.money_lost", "result.died",
    "landmark.look", "landmark.trade", "landmark.rest", "landmark.cross", "landmark.leave", "landmark.rested",
    "river.depth.shallow", "river.depth.waist", "river.depth.deep", "river.depth.flood",
    "river.ford", "river.float", "river.carry", "river.wait", "river.waited", "river.safe",
    "river.trouble.goods", "river.trouble.oxen", "river.trouble.wagon", "river.trouble.person",
    "lock.too_deep", "lock.too_shallow", "lock.need_tallow", "lock.no_spare", "lock.no_iron",
    "route.title", "route.body", "route.default",
    "breakdown.title", "breakdown.body", "breakdown.spare", "breakdown.mend", "breakdown.rig",
    "breakdown.spare.done", "breakdown.mend.done", "breakdown.rig.done",
    "part.wheels", "part.axle", "part.dissel", "part.tent",
    "stranded.title", "stranded.body", "stranded.wait", "stranded.give_up", "stranded.bought",
    "stranded.lent", "stranded.no_money", "stranded.nobody", "stranded.yoke", "stranded.yoked", "lock.no_cattle",
    "cause.unknown", "cause.hardship", "cause.accident", "cause.drowned",
    "advice.oxen", "advice.spare", "advice.water", "advice.food", "advice.medicine", "advice.ok",
    "money.rds", "money.pounds", "money.shillings", "money.pounds_shillings", "date.format",
    "role.head", "role.spouse", "role.child", "role.elder", "role.servant", "role.spouse.she", "role.spouse.he", "role.child.she", "role.child.he",
    "weekday.0", "weekday.1", "weekday.2", "weekday.3", "weekday.4", "weekday.5", "weekday.6",
    "month.1", "month.2", "month.3", "month.4", "month.5", "month.6",
    "month.7", "month.8", "month.9", "month.10", "month.11", "month.12",
    "history.river.orange", "history.river.caledon", "history.river.vet", "history.river.vaal",
    NULL
};

int main(int argc, char **argv)
{
    const char *data_dir = argc > 1 ? argv[1] : "data";
    int errors = 0;

    char err[512] = "";
    int warnings = 0;
    GameData *d = data_load(data_dir, err, sizeof err, &warnings);
    if (!d) {
        fprintf(stderr, "error: %s\n", err);
        return 1;
    }
    printf("data: %d items, %d families, %d places, %d roads, %d events\n", d->item_count, d->family_count,
           d->node_count, d->edge_count, d->event_count);

    char *path = util_path_join(data_dir, "locale/en");
    Locale *loc = locale_load_dir(path, err, sizeof err);
    if (!loc) {
        fprintf(stderr, "error: %s\n", err);
        errors++;
    }
    free(path);

    if (loc) {
        KeyCheck kc = {loc, 0};
        data_each_key(d, check_key, &kc);
        for (int i = 0; CODE_KEYS[i]; i++) {
            check_key(CODE_KEYS[i], "code", &kc);
        }
        for (int i = 0; i < d->node_count; i++) {
            check_key(d->nodes[i].history_key, d->nodes[i].id, &kc);
        }
        errors += kc.missing;
        printf("locale: %lu keys, %d missing\n", (unsigned long)locale_count(loc), kc.missing);
        locale_free(loc);
    }

    int sourced = 0, estimated = 0;
    for (int i = 0; i < d->item_count; i++) {
        if (d->items[i].price_pence <= 0) {
            continue;
        }
        if (strncmp(d->items[i].source, "estimate", 8) == 0) {
            estimated++;
        } else {
            sourced++;
        }
    }
    printf("prices: %d from period sources, %d estimates\n", sourced, estimated);

    data_free(d);
    if (warnings) {
        printf("datacheck: %d warning(s)\n", warnings);
    }
    printf(errors ? "datacheck: %d error(s)\n" : "datacheck: ok\n", errors);
    return errors ? 1 : 0;
}
