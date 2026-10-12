/* One day on the road: weather, distance, food, water, oxen, health, morale, events. */
#include "calendar.h"
#include "game_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SHEEP_MEAT_LB 35
#define MEAT_KEEP_LB 60
#define COW_FRESH_LB 60
#define COW_BILTONG_LB 70
#define MIN_OXEN_TO_MOVE 4

int g_oxen(const Game *g)
{
    return g->inv[g->data->item_oxen];
}

/* ---------------------------------------------------------------- weather */

static void roll_weather(Game *g)
{
    const RegionDef *r = &g->data->regions[g_current_region(g)];
    const double *w = r->weather[cal_season(g->day)];
    /* Weather is sticky: half the time tomorrow is like today, if today is possible here. */
    if (w[g->weather] > 0 && rng_chance(&g->rng, 0.5)) {
        return;
    }
    int pick = rng_weighted(&g->rng, w, WEATHER_COUNT);
    g->weather = pick < 0 ? WEATHER_CLEAR : (Weather)pick;
}

/* ---------------------------------------------------------------- distance */

static double day_miles(Game *g)
{
    const GameData *d = g->data;
    const Rules *r = &d->rules;
    if (g->edge < 0) {
        return 0;
    }
    const EdgeDef *e = &d->edges[g->edge];
    const WagonDef *w = &d->wagons[g->wagon];
    double miles = r->miles[g->pace] * e->terrain_mul * w->speed;
    int oxen = g_oxen(g);
    double ox_factor = (double)oxen / w->full_span;
    if (ox_factor > 1.0) {
        ox_factor = 1.0;
    }
    if (oxen < w->min_oxen) {
        ox_factor *= 0.6;
    }
    miles *= 0.4 + 0.6 * ox_factor;
    if (g->ox_stamina < r->stamina_slow) {
        miles *= 0.7;
    }
    switch (g->weather) {
    case WEATHER_STORM: miles *= 0.5; break;
    case WEATHER_RAIN: miles *= 0.8; break;
    case WEATHER_FROST: miles *= 0.9; break;
    case WEATHER_HOT: miles *= 0.9; break;
    default: break;
    }
    if (g_best_skill(g, SKILL_VOORLOPER, true) < 2) {
        miles *= 0.85;
    }
    if (game_load_lb(g) > game_capacity_lb(g)) {
        miles *= 0.8;
    }
    if (g->stance == NIGHT_LAAGER) {
        miles -= r->laager_miles;
    }
    return miles < 0.5 ? 0.5 : miles;
}

/* ---------------------------------------------------------------- wagon */

static const char *PART_KEYS[WAGON_PART_COUNT] = {"part.wheels", "part.axle", "part.dissel", "part.tent"};

void g_open_breakdown(Game *g, int part)
{
    memset(&g->dlg, 0, sizeof g->dlg);
    g->dlg.kind = DLG_BREAKDOWN;
    g->dlg.part = part;
    g->dlg.speaker = -1;
    g->dlg.subject = -1;
    g->stop_travel = true;
}

static void wear_wagon(Game *g, double miles)
{
    const GameData *d = g->data;
    const EdgeDef *e = &d->edges[g->edge];
    double chance = d->rules.breakage_per_mile * miles * e->rough * d->wagons[g->wagon].fragility * g_harm(g);
    if (g->pace == PACE_HARD) {
        chance *= 1.6;
    }
    if (g_best_skill(g, SKILL_VOORLOPER, true) < 2) {
        chance *= 1.4;
    }
    if (game_load_lb(g) > game_capacity_lb(g)) {
        chance *= 1.5;
    }
    for (int p = 0; p < WAGON_PART_TENT; p++) {
        if (g->wagon_parts[p] > 0 && rng_chance(&g->rng, chance)) {
            g->wagon_parts[p] -= rng_range(&g->rng, 30, 70);
            if (g->wagon_parts[p] <= 0) {
                g->wagon_parts[p] = 0;
                LocText t;
                loctext_set(&t, "journal.broke");
                loctext_arg_key(&t, "part", PART_KEYS[p]);
                game_journal(g, &t);
                g_open_breakdown(g, p);
                return;
            }
        }
    }
    if (g->weather == WEATHER_STORM && rng_chance(&g->rng, 0.15)) {
        g->wagon_parts[WAGON_PART_TENT] -= rng_range(&g->rng, 10, 30);
        if (g->wagon_parts[WAGON_PART_TENT] < 0) {
            g->wagon_parts[WAGON_PART_TENT] = 0;
        }
    }
}

static int broken_part(const Game *g)
{
    for (int p = 0; p < WAGON_PART_TENT; p++) {
        if (g->wagon_parts[p] <= 0) {
            return p;
        }
    }
    return -1;
}

/* ---------------------------------------------------------------- food and water */

static void eat(Game *g)
{
    const GameData *d = g->data;
    const Rules *r = &d->rules;
    double need = 0;
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i)) {
            need += r->food_lb[g->rations] * (g->party[i].age == AGE_CHILD ? r->child_food : 1.0);
        }
    }
    /* Trekkers lived largely off their flocks: slaughter a sheep when the meat runs out. */
    if (g->inv[d->item_meat] < need && g->inv[d->item_sheep] > 0) {
        g->inv[d->item_sheep]--;
        g->inv[d->item_meat] += SHEEP_MEAT_LB;
    } else if (g->inv[d->item_meat] < need && game_food_lb(g) < need * 3 && g->inv[d->item_cows] > 0) {
        /* No sheep left and little food: slaughter a cow, eat some fresh and dry the rest as biltong. */
        g->inv[d->item_cows]--;
        g->inv[d->item_meat] += COW_FRESH_LB;
        g->inv[d->item_biltong] += COW_BILTONG_LB;
        LocText t;
        loctext_set(&t, "journal.slaughtered_cow");
        game_journal(g, &t);
    }
    /* Eat in eat_order: fresh meat first, then dried food, then meal. */
    for (int order = 0; order <= 9 && need > 0.01; order++) {
        for (int i = 0; i < d->item_count && need > 0.01; i++) {
            const ItemDef *it = &d->items[i];
            if (it->food <= 0 || it->eat_order != order || g->inv[i] <= 0) {
                continue;
            }
            int units = (int)ceil(need / it->food);
            if (units > g->inv[i]) {
                units = g->inv[i];
            }
            g->inv[i] -= units;
            need -= units * it->food;
        }
    }
    if (g->inv[d->item_meat] > MEAT_KEEP_LB) {
        g->inv[d->item_meat] = MEAT_KEEP_LB; /* fresh meat spoils */
    }
    int health = r->ration_health[g->rations];
    int morale = r->ration_morale[g->rations];
    if (need > 0.5) {
        health -= 4; /* went hungry today */
        morale -= 3;
    }
    /* Coffee and sugar lift spirits. */
    for (int i = 0; i < d->item_count; i++) {
        if (d->items[i].morale > 0 && g->inv[i] > 0 && g->day % 2 == 0) {
            g->inv[i]--;
            morale += d->items[i].morale;
        }
    }
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i)) {
            g->party[i].health += health;
        }
    }
    g_add_morale(g, morale);
}

/* Returns true if people and oxen had water today. */
static bool drink(Game *g, bool travelling)
{
    const GameData *d = g->data;
    WaterKind wk = WATER_STREAMS;
    if (g->edge >= 0) {
        wk = d->edges[g->edge].water;
    }
    bool source;
    switch (wk) {
    case WATER_STREAMS: source = true; break;
    case WATER_SPRINGS: source = rng_chance(&g->rng, 0.75); break;
    default: source = rng_chance(&g->rng, d->regions[g_current_region(g)].water_find); break;
    }
    if (!travelling && g->edge < 0) {
        source = true; /* resting at a town or drift */
    }
    int need = (int)ceil(game_member_count_with_party(g) * d->rules.water_gal_per_person);
    if (source) {
        g->inv[d->item_water] = game_water_capacity(g);
        return true;
    }
    if (g->inv[d->item_water] >= need) {
        g->inv[d->item_water] -= need;
        /* People drink from the vats; the oxen go thirsty. */
        g->ox_stamina -= d->rules.no_water_ox_cost;
        return true;
    }
    g->inv[d->item_water] = 0;
    g->ox_stamina -= d->rules.no_water_ox_cost;
    for (int i = 0; i < g->party_count; i++) {
        if (g_member_active(g, i)) {
            g->party[i].health -= d->rules.no_water_health;
        }
    }
    g_notice_key(g, "notice.no_water");
    return false;
}

/* ---------------------------------------------------------------- oxen */

static void tend_oxen(Game *g, bool travelling)
{
    const GameData *d = g->data;
    const Rules *r = &d->rules;
    const RegionDef *reg = &d->regions[g_current_region(g)];
    int delta = reg->grazing[cal_month(g->day) - 1];
    if (travelling) {
        delta -= r->ox_cost[g->pace];
        if (game_load_lb(g) > game_capacity_lb(g)) {
            delta -= 3;
        }
        if (g->stance == NIGHT_LAAGER) {
            delta -= r->laager_ox_cost;
        }
    } else {
        delta += r->rest_stamina;
    }
    if (g->weather == WEATHER_HOT || g->weather == WEATHER_FROST) {
        delta -= 2;
    }
    /* A good herder keeps the span in better condition. */
    delta += g_best_skill(g, SKILL_HERDING, true) / 2;
    /* Grazing feeds the oxen, but tired oxen recover only so far each day. */
    g->ox_stamina += delta;
    if (g->ox_stamina > 100) {
        g->ox_stamina = 100;
    }
    if (g->ox_stamina < 0) {
        g->ox_stamina = 0;
    }
    if (g->ox_stamina < r->stamina_collapse && g_oxen(g) > 0 && rng_chance(&g->rng, r->collapse_chance * g_harm(g))) {
        g->inv[d->item_oxen]--;
        g_notice_key(g, "notice.ox_died");
    }
}

void g_check_stranded(Game *g)
{
    if (g->edge >= 0 && g_oxen(g) < MIN_OXEN_TO_MOVE && g->dlg.kind == DLG_NONE) {
        memset(&g->dlg, 0, sizeof g->dlg);
        g->dlg.kind = DLG_STRANDED;
        g->dlg.speaker = -1;
        g->dlg.subject = -1;
        g->stop_travel = true;
    }
}

/* ---------------------------------------------------------------- health */

static int pick_illness(Game *g, bool accident)
{
    const GameData *d = g->data;
    double w[MAX_ILLNESSES];
    for (int i = 0; i < d->illness_count; i++) {
        w[i] = d->illnesses[i].accident == accident ? d->illnesses[i].weight : 0;
    }
    return rng_weighted(&g->rng, w, d->illness_count);
}

void g_add_illness(Game *g, int member, int illness)
{
    if (!g_member_active(g, member) || illness < 0) {
        return;
    }
    Member *m = &g->party[member];
    for (int c = 0; c < m->cond_count; c++) {
        if (m->conds[c].illness == illness) {
            return;
        }
    }
    if (m->cond_count >= MAX_MEMBER_CONDS) {
        return;
    }
    m->conds[m->cond_count].illness = illness;
    m->conds[m->cond_count].days = 0;
    /* Most cases are mild or middling; about one in five is grave. */
    m->conds[m->cond_count].severity = rng_chance(&g->rng, 0.2) ? rng_range(&g->rng, 150, 220) : rng_range(&g->rng, 60, 140);
    m->cond_count++;
}

static void health_day(Game *g, bool travelling, bool had_water)
{
    const GameData *d = g->data;
    const Rules *r = &d->rules;
    const FamilyDef *f = &d->families[g->family];
    Season season = cal_season(g->day);
    int nursing = g_best_skill(g, SKILL_NURSING, true);
    double harm = g_harm(g);

    for (int i = 0; i < g->party_count; i++) {
        if (!g_member_active(g, i)) {
            continue;
        }
        Member *m = &g->party[i];
        if (travelling) {
            /* A hard pace wears people down on about half the days. */
            if (r->health_cost[g->pace] > 0 && rng_chance(&g->rng, 0.5)) {
                m->health -= r->health_cost[g->pace];
            }
        } else {
            m->health += r->rest_health;
        }
        if ((g->weather == WEATHER_FROST || g->weather == WEATHER_COLD) && g->wagon_parts[WAGON_PART_TENT] < 30) {
            m->health -= 1;
        }

        /* Illnesses run their course. */
        for (int c = 0; c < m->cond_count; c++) {
            const IllnessDef *il = &d->illnesses[m->conds[c].illness];
            m->conds[c].days++;
            /* Small children and the old were the most likely to die of illness. */
            double frail = m->age == AGE_CHILD ? il->child_mul : (m->age == AGE_ELDER ? 1.3 : 1.0);
            m->health -= (int)lround(il->damage_per_day * frail * m->conds[c].severity / 100.0);
            if (m->conds[c].days >= il->min_days) {
                double p = il->recover_chance + il->nursing_bonus * nursing;
                if (!travelling) {
                    p += 0.05;
                }
                if (g->inv[d->item_medicine] > 0) {
                    g->inv[d->item_medicine]--;
                    p += 0.08;
                }
                if (rng_chance(&g->rng, p)) {
                    LocText t;
                    loctext_set(&t, "journal.recovered");
                    loctext_arg_person(&t, "who", m);
                    loctext_arg_key(&t, "illness", il->name_key);
                    game_journal(g, &t);
                    m->conds[c] = m->conds[m->cond_count - 1];
                    m->cond_count--;
                    c--;
                    continue;
                }
            }
            /* Some illnesses spread through the party. */
            if (il->contagion > 0 && rng_chance(&g->rng, il->contagion)) {
                int other = g_random_member(g, false, -1);
                if (other >= 0 && other != i && g->party[other].cond_count == 0) {
                    g_add_illness(g, other, m->conds[c].illness);
                    LocText t;
                    loctext_set(&t, "notice.fell_ill");
                    loctext_arg_person(&t, "who", &g->party[other]);
                    loctext_arg_key(&t, "illness", il->name_key);
                    g_notice(g, &t);
                }
            }
        }

        /* New illness or accident. */
        double p_ill = r->illness_per_day[season] * harm * f->illness_mul;
        if (m->health < 50) {
            p_ill *= 1.5;
        }
        if (!had_water) {
            p_ill *= 2.0;
        }
        if (m->cond_count == 0 && rng_chance(&g->rng, p_ill * (m->age == AGE_CHILD ? 1.5 : 1.0))) {
            int il = pick_illness(g, false);
            if (il >= 0) {
                g_add_illness(g, i, il);
                LocText t;
                loctext_set(&t, "notice.fell_ill");
                loctext_arg_person(&t, "who", m);
                loctext_arg_key(&t, "illness", d->illnesses[il].name_key);
                g_notice(g, &t);
            }
        }
        /* The fever is caught from the country, in the low country in the rains (regions.json). */
        const RegionDef *rg = &d->regions[g_current_region(g)];
        double fever = rg->fever[cal_month(g->day) - 1] * harm;
        if (fever > 0 && d->illness_fever >= 0 && rng_chance(&g->rng, fever * (m->age == AGE_CHILD ? 1.2 : 1.0))) {
            bool has = false;
            for (int c = 0; c < m->cond_count; c++) {
                has = has || m->conds[c].illness == d->illness_fever;
            }
            if (!has && m->cond_count < MAX_MEMBER_CONDS) {
                g_add_illness(g, i, d->illness_fever);
                LocText t;
                loctext_set(&t, "notice.fever");
                loctext_arg_person(&t, "who", m);
                g_notice(g, &t);
            }
        }
        if (travelling && rng_chance(&g->rng, r->accident_per_day * harm)) {
            int il = pick_illness(g, true);
            if (il >= 0) {
                g_add_illness(g, i, il);
                LocText t;
                loctext_set(&t, "notice.accident");
                loctext_arg_person(&t, "who", m);
                loctext_arg_key(&t, "illness", d->illnesses[il].name_key);
                g_notice(g, &t);
            }
        }

        if (m->health > 100) {
            m->health = 100;
        }
        if (m->health <= 0 && g->difficulty == DIFF_EASY && !m->warned) {
            /* Easy promises a warning before a death (04_game_design.md section 8.5). */
            m->warned = true;
            m->health = 10;
            g_notice_member(g, "notice.near_death", i);
        } else if (m->health >= 50) {
            m->warned = false;
        }
        if (m->health <= 0) {
            const char *cause = m->cond_count > 0 ? d->illnesses[m->conds[0].illness].cause_key : "cause.hardship";
            g_kill(g, i, cause);
        }
    }
}

/* ---------------------------------------------------------------- servants */

static void servants_day(Game *g)
{
    const Rules *r = &g->data->rules;
    g->days_since_wages++;
    bool payday = g->days_since_wages >= 30;
    int wage = (int)lround(g->side == SIDE_DUTCH ? r->servant_wage : r->servant_wage_english);
    if (payday) {
        g->days_since_wages = 0;
    }
    for (int i = 0; i < g->party_count; i++) {
        if (!g_member_active(g, i) || g->party[i].role != ROLE_SERVANT) {
            continue;
        }
        Member *m = &g->party[i];
        if (payday) {
            /* Wages in coin if possible, otherwise in sheep: a sheep was worth about 3 Rds. */
            int sheep_wage = g->side == SIDE_DUTCH ? (wage + 2) / 3 : (wage * 2 + 8) / 9;
            if (g->money >= wage) {
                g->money -= wage;
                m->loyalty += 5;
            } else if (g->inv[g->data->item_sheep] >= sheep_wage) {
                g->inv[g->data->item_sheep] -= sheep_wage;
                m->loyalty += 3;
                LocText t;
                loctext_set(&t, "journal.paid_in_sheep");
                loctext_arg_person(&t, "who", m);
                loctext_arg_int(&t, "n", sheep_wage);
                game_journal(g, &t);
            } else {
                m->loyalty -= 15;
                g_notice_member(g, "notice.wages_unpaid", i);
            }
        }
        /* Loyalty drifts toward the party's morale. */
        if (g->day % 5 == 0) {
            m->loyalty += (g->morale > m->loyalty) ? 1 : -1;
        }
        if (m->loyalty > 100) {
            m->loyalty = 100;
        }
        if (m->loyalty < 0) {
            m->loyalty = 0;
        }
        if (m->loyalty < 15 && rng_chance(&g->rng, 0.05)) {
            g_member_leaves(g, i);
        }
    }
}

/* ---------------------------------------------------------------- warnings */

static void warnings(Game *g)
{
    const Rules *r = &g->data->rules;
    if (g->difficulty == DIFF_HARD) {
        return; /* Hard gives no supply warnings (04_game_design.md section 8.5) */
    }
    double days = game_food_days(g);
    if (days < r->warn_food_days && !g->warned_food) {
        g->warned_food = true;
        g_notice_key(g, "notice.food_low");
    } else if (days >= r->warn_food_days * 2) {
        g->warned_food = false;
    }
    if (g->edge >= 0 && g->data->edges[g->edge].water == WATER_DRY) {
        double need = game_member_count_with_party(g) * r->water_gal_per_person;
        double wdays = need > 0 ? g->inv[g->data->item_water] / need : 99;
        if (wdays < r->warn_water_days && !g->warned_water) {
            g->warned_water = true;
            g_notice_key(g, "notice.water_low");
        }
    } else {
        g->warned_water = false;
    }
}

/* ---------------------------------------------------------------- a day */

static bool fire_scheduled(Game *g)
{
    for (int i = 0; i < g->scheduled_count; i++) {
        if (g->scheduled[i].day <= g->day) {
            int ev = g->scheduled[i].event;
            g->scheduled[i] = g->scheduled[--g->scheduled_count];
            g_start_event(g, ev);
            return true;
        }
    }
    return false;
}

/* The fly: in tsetse country the oxen, cattle and horses sicken and die over the weeks (nagana). */
static void fly_day(Game *g)
{
    const GameData *d = g->data;
    double share = d->regions[g_current_region(g)].fly[cal_month(g->day) - 1] * g_harm(g);
    if (share <= 0) {
        return;
    }
    const int kinds[] = {d->item_oxen, d->item_cows, d->item_horses};
    for (size_t k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
        int item = kinds[k];
        int died = 0;
        for (int n = 0; n < g->inv[item]; n++) {
            died += rng_chance(&g->rng, share) ? 1 : 0;
        }
        if (died == 0) {
            continue;
        }
        g_add_item(g, item, -died);
        LocText t;
        bool first = !game_has_flag(g, "fly_seen");
        loctext_set(&t, first ? "notice.fly" : "journal.fly");
        loctext_arg_int(&t, "n", died);
        loctext_arg_key(&t, "item", d->items[item].name_key);
        if (first) {
            g_set_flag(g, "fly_seen");
            g_notice(g, &t);
        } else {
            game_journal(g, &t);
        }
    }
}

void g_pass_day(Game *g, bool travelling, bool allow_events)
{
    const Rules *r = &g->data->rules;
    roll_weather(g);
    fly_day(g);
    /* Game slowly comes back to country the party has hunted. */
    for (int r = 0; r < g->data->region_count; r++) {
        if (g->region_game[r] < 1.0) {
            g->region_game[r] += 0.01;
        }
    }
    bool sabbath = g->sabbath && cal_weekday(g->day) == 0;
    if (sabbath && travelling) {
        travelling = false;
        g_add_morale(g, r->sabbath_morale);
        g_add_relation(g, g->data->group_trekkers, g->side == SIDE_DUTCH ? 0 : (g->day % 28 == 0 ? 1 : 0));
    }
    double miles = 0;
    if (travelling && g->edge >= 0) {
        if (broken_part(g) >= 0) {
            g_open_breakdown(g, broken_part(g));
            travelling = false;
        } else if (g_oxen(g) < MIN_OXEN_TO_MOVE) {
            travelling = false;
        } else {
            miles = day_miles(g);
            g->miles_along += miles;
            g->miles_total += (int)lround(miles);
            wear_wagon(g, miles);
        }
    }
    eat(g);
    bool had_water = drink(g, travelling);
    tend_oxen(g, travelling);
    health_day(g, travelling, had_water);
    servants_day(g);
    if (!travelling) {
        g_add_morale(g, r->rest_morale);
    } else if (g->pace == PACE_HARD) {
        g_add_morale(g, -1);
    }
    if (g->weather == WEATHER_STORM) {
        g_add_morale(g, -1);
    }
    if (g->stance == NIGHT_GUARD) {
        g_add_morale(g, (int)r->guard_morale);
    }
    if (allow_events && g->dlg.kind == DLG_NONE) {
        if (!fire_scheduled(g)) {
            if (!g_maybe_event(g, false)) {
                g_maybe_event(g, true);
            }
        }
    }
    g->day++;
    warnings(g);
    g_check_stranded(g);
}

static bool arrived(Game *g)
{
    if (g->edge >= 0 && g->miles_along >= g->data->edges[g->edge].miles) {
        int to = g->data->edges[g->edge].to;
        g_arrive(g, to);
        return true;
    }
    return false;
}

bool game_travel_day(Game *g)
{
    if (g->phase != PHASE_TRAVEL) {
        return true;
    }
    g->stop_travel = false;
    g_pass_day(g, true, true);
    if (g->dlg.kind == DLG_NONE) {
        arrived(g);
    }
    g_settle(g);
    return g->stop_travel || g->phase != PHASE_TRAVEL;
}

int game_travel(Game *g, int max_days)
{
    int days = 0;
    while (days < max_days && g->phase == PHASE_TRAVEL) {
        days++;
        if (game_travel_day(g)) {
            break;
        }
    }
    return days;
}

bool game_can_mend(const Game *g)
{
    if (g->phase != PHASE_TRAVEL) {
        return false;
    }
    const Rules *r = &g->data->rules;
    for (int p = 0; p < WAGON_PART_TENT; p++) {
        if (g->wagon_parts[p] < r->mend_cap) {
            return true;
        }
    }
    bool canvas = g->inv[g->data->item_spare[WAGON_PART_TENT]] > 0;
    return g->wagon_parts[WAGON_PART_TENT] < (canvas ? r->mend_tent_cap + 10 : r->mend_tent_cap);
}

static void mend_note(Game *g, const char *key, int part)
{
    LocText t;
    loctext_set(&t, key);
    loctext_arg_key(&t, "part", PART_KEYS[part]);
    g_notice(g, &t);
}

void game_mend_wagon(Game *g)
{
    if (!game_can_mend(g)) {
        return;
    }
    const GameData *d = g->data;
    const Rules *r = &d->rules;
    int smith_member = g_best_skill_member(g, SKILL_SMITHING);
    int smith = g_best_skill(g, SKILL_SMITHING, true);
    double perk = d->families[g->family].repair_mul > 0 ? d->families[g->family].repair_mul : 1.0;
    bool short_of_iron = false;
    for (int p = 0; p < WAGON_PART_TENT; p++) {
        if (g->wagon_parts[p] >= r->mend_cap) {
            continue;
        }
        if (g->wagon_parts[p] < r->mend_spare_below && g->inv[d->item_spare[p]] > 0) {
            g_add_item(g, d->item_spare[p], -1);
            g->wagon_parts[p] = 100;
            mend_note(g, "mend.spare", p);
            continue;
        }
        double gain = (r->mend_gain + r->mend_gain_per_smith * smith) / perk;
        if (g->inv[d->item_iron] >= r->mend_iron_lb) {
            g_add_item(g, d->item_iron, -r->mend_iron_lb);
        } else {
            gain *= r->mend_no_iron_mul;
            short_of_iron = true;
        }
        g->wagon_parts[p] += (int)lround(gain);
        if (g->wagon_parts[p] > r->mend_cap) {
            g->wagon_parts[p] = r->mend_cap;
        }
    }
    /* grease for the hubs and axle arms */
    if ((g->wagon_parts[WAGON_PART_WHEELS] < r->mend_cap || g->wagon_parts[WAGON_PART_AXLE] < r->mend_cap) &&
        g->inv[d->item_tallow] >= r->mend_tallow_lb) {
        g_add_item(g, d->item_tallow, -r->mend_tallow_lb);
        for (int p = WAGON_PART_WHEELS; p <= WAGON_PART_AXLE; p++) {
            if (g->wagon_parts[p] < r->mend_cap) {
                g->wagon_parts[p] += r->mend_grease_gain;
                if (g->wagon_parts[p] > r->mend_cap) {
                    g->wagon_parts[p] = r->mend_cap;
                }
            }
        }
    }
    /* the tent: new canvas if it is badly worn and there is some, otherwise patches */
    int *tent = &g->wagon_parts[WAGON_PART_TENT];
    if (*tent < r->mend_tent_cap + 10 && g->inv[d->item_spare[WAGON_PART_TENT]] > 0) {
        g_add_item(g, d->item_spare[WAGON_PART_TENT], -1);
        *tent = 100;
        mend_note(g, "mend.canvas", WAGON_PART_TENT);
    } else if (*tent < r->mend_tent_cap) {
        *tent += r->mend_tent_patch;
        if (*tent > r->mend_tent_cap) {
            *tent = r->mend_tent_cap;
        }
    }
    if (short_of_iron) {
        g_notice_key(g, "mend.no_iron");
    }
    LocText t;
    loctext_set(&t, smith > 0 && smith_member >= 0 ? "mend.done" : "mend.done_all");
    if (smith_member >= 0) {
        loctext_arg_person(&t, "who", &g->party[smith_member]);
    }
    loctext_arg_int(&t, "wheels", g->wagon_parts[WAGON_PART_WHEELS]);
    loctext_arg_int(&t, "axle", g->wagon_parts[WAGON_PART_AXLE]);
    loctext_arg_int(&t, "dissel", g->wagon_parts[WAGON_PART_DISSEL]);
    loctext_arg_int(&t, "tent", g->wagon_parts[WAGON_PART_TENT]);
    g_notice(g, &t);
    g->stop_travel = false;
    g_pass_day(g, false, true);
    g_settle(g);
}

void game_rest_day(Game *g)
{
    if (g->phase != PHASE_TRAVEL) {
        return;
    }
    g->stop_travel = false;
    g_pass_day(g, false, true);
    g_settle(g);
}
