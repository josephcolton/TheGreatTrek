/* datacheck: validates the files in data/. Usage: datacheck [data_dir]
 * - every JSON file loads and every id it refers to exists (data_load does this)
 * - every event has sources (data_load refuses events without them)
 * - every locale key the data or the code refers to exists in locale/en.json
 * - every family member and every event node's portrait is drawn in assets/sprites/portraits.txt
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
    "hunt.hunter", "hunt.start", "hunt.appears", "hunt.appears_herd", "hunt.appears_lion", "hunt.lion_leaves",
    "hunt.misfire", "hunt.miss", "hunt.kill", "hunt.kill_predator", "hunt.wounded", "hunt.charges", "hunt.charged",
    "hunt.hunter_died", "hunt.out_of_shot", "hunt.give_up", "hunt.sundown", "hunt.summary.shot", "hunt.summary.all",
    "hunt.summary.too_much", "hunt.summary.none", "hunt.summary.go", "hunt.status.loaded", "hunt.status.reloading",
    "hunt.status.no_shot", "hunt.status.meat", "hunt.status.shots", "hunt.status.light", "hunt.footer", "hunt.footer.over",
    "journal.hunt", "journal.hunt_empty", "cause.hunting",
    "landmark.barter", "barter.reply.empty", "barter.reply.insult", "barter.reply.short", "barter.reply.generous",
    "barter.reply.done", "barter.reply.powder", "journal.barter", "journal.barter_powder",
    "river.trouble.person_alone",
    "summary.days", "summary.ill", "summary.died", "summary.lost",
    "grave.who", "grave.who.servant", "grave.cause", "grave.why.drowned", "grave.why.accident", "grave.why.hunting",
    "grave.why.hardship", "grave.why.unknown",
    "notice.fever", "notice.fly", "journal.fly", "ending.history.title", "ending.footer.more",
    "defend.title", "defend.status.lost", "defend.status.night", "defend.footer", "defend.start", "defend.appears",
    "defend.appears_pack", "defend.took", "defend.dropped", "defend.killed", "defend.wounded", "defend.scared",
    "defend.give_up", "defend.quiet", "defend.dawn", "defend.summary.result", "defend.summary.nothing",
    "defend.summary.safe", "defend.summary.lost", "journal.defend", "journal.defend_safe",
    "pronoun.he.he", "pronoun.he.him", "pronoun.he.his", "pronoun.he.hers", "pronoun.he.himself",
    "pronoun.she.he", "pronoun.she.him", "pronoun.she.his", "pronoun.she.hers", "pronoun.she.himself",
    "pronoun.they.he", "pronoun.they.him", "pronoun.they.his", "pronoun.they.hers", "pronoun.they.himself",
    NULL
};

/* Is there a "sprite portrait_<id>" line in the sprite file? */
static bool has_portrait(const char *sheet, const char *id)
{
    char want[64];
    snprintf(want, sizeof want, "sprite portrait_%s", id);
    size_t n = strlen(want);
    for (const char *p = strstr(sheet, want); p; p = strstr(p + 1, want)) {
        if ((p == sheet || p[-1] == '\n') && (p[n] == '\n' || p[n] == '\r' || p[n] == '\0')) {
            return true;
        }
    }
    return false;
}

static int check_portraits(const GameData *d, const char *data_dir)
{
    char *path = util_path_join(data_dir, "../assets/sprites/portraits.txt");
    char *sheet = util_read_file(path, NULL);
    int missing = 0;
    if (!sheet) {
        fprintf(stderr, "cannot read %s\n", path);
        free(path);
        return 1;
    }
    for (int f = 0; f < d->family_count; f++) {
        for (int m = 0; m < d->families[f].member_count; m++) {
            const MemberDef *md = &d->families[f].members[m];
            if (!md->portrait[0] || !has_portrait(sheet, md->portrait)) {
                fprintf(stderr, "family %s: %s has no portrait \"%s\"\n", d->families[f].id, md->name, md->portrait);
                missing++;
            }
        }
    }
    for (int e = 0; e < d->event_count; e++) {
        for (int k = 0; k < d->events[e].node_count; k++) {
            const char *id = d->events[e].nodes[k].portrait;
            if (id[0] && !has_portrait(sheet, id)) {
                fprintf(stderr, "event %s: no portrait \"%s\"\n", d->events[e].id, id);
                missing++;
            }
        }
    }
    free(sheet);
    free(path);
    return missing;
}

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
        for (int i = 0; i < d->illness_count; i++) {
            char key[KEY_LEN];
            snprintf(key, sizeof key, "grave.why.%s", d->illnesses[i].id); /* game_grave_story */
            check_key(key, d->illnesses[i].id, &kc);
        }
        errors += kc.missing;
        /* {who.form}: the form must be one the formatter knows (format.c, pronoun.* keys) */
        static const char *FORMS[] = {"he", "him", "his", "hers", "himself"};
        for (size_t i = 0; i < locale_count(loc); i++) {
            const char *key = locale_key_at(loc, i);
            const char *text = locale_get(loc, key);
            for (const char *p = strchr(text, '{'); p; p = strchr(p + 1, '{')) {
                const char *close = strchr(p, '}');
                const char *dot = close ? memchr(p, '.', (size_t)(close - p)) : NULL;
                if (!dot || memchr(p, ':', (size_t)(close - p))) {
                    continue;
                }
                char form[16];
                snprintf(form, sizeof form, "%.*s", (int)(close - dot - 1), dot + 1);
                if (form[0] >= 'A' && form[0] <= 'Z') {
                    form[0] = (char)(form[0] - 'A' + 'a');
                }
                bool ok = false;
                for (size_t f = 0; f < sizeof FORMS / sizeof FORMS[0]; f++) {
                    ok = ok || strcmp(form, FORMS[f]) == 0;
                }
                if (!ok) {
                    fprintf(stderr, "%s: unknown pronoun form in %.*s\n", key, (int)(close - p + 1), p);
                    errors++;
                }
            }
        }
        printf("locale: %lu keys, %d missing\n", (unsigned long)locale_count(loc), kc.missing);
        locale_free(loc);
    }

    int no_portrait = check_portraits(d, data_dir);
    printf("portraits: %d missing\n", no_portrait);
    errors += no_portrait;

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
