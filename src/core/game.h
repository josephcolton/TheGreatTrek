/* The running game: state plus the commands the front end (or trekbot) sends.
 * The core never waits for input; after every command, ask game_phase() what to show. */
#ifndef TREK_GAME_H
#define TREK_GAME_H

#include "data.h"
#include "rng.h"

#include <stdbool.h>
#include <stdint.h>

#define MAX_PARTY MAX_MEMBERS
#define MAX_MEMBER_CONDS 3
#define MAX_FLAGS 160
#define MAX_COUNTERS 32
#define MAX_JOURNAL 600
#define MAX_SCHEDULED 16
#define MAX_GRAVES MAX_MEMBERS
#define MAX_ARGS 6
#define MAX_RESULT_LINES 10
#define MAX_NOTICES 12
#define EPITAPH_LEN 96

typedef enum {
    PHASE_DIALOG,   /* events, landmarks, rivers, road choices, notices */
    PHASE_STORE,
    PHASE_TRAVEL,
    PHASE_GRAVE,
    PHASE_ENDED,
    PHASE_HUNT,
    PHASE_BARTER
} GamePhase;

typedef enum { ARG_INT, ARG_STR, ARG_KEY, ARG_MONEY, ARG_DATE } ArgKind;

typedef struct {
    char name[12];
    ArgKind kind;
    long long i;      /* ARG_INT, ARG_MONEY (in the side's unit), ARG_DATE (day number) */
    char s[40];       /* ARG_STR (literal), ARG_KEY (locale key) */
} TextArg;

/* A piece of text to show: a locale key plus named arguments, e.g. {who}, {n}. */
typedef struct {
    char key[KEY_LEN];
    TextArg args[MAX_ARGS];
    int n_args;
} LocText;

typedef struct {
    int illness;
    int days;
    int severity;             /* percent: how hard this case hits (60..220) */
} MemberCond;

typedef struct {
    char name[32];
    Role role;
    AgeGroup age;
    Pronoun pronoun;
    int birth_year;
    uint8_t skills[SKILL_COUNT];
    int health;               /* 0..100 */
    MemberCond conds[MAX_MEMBER_CONDS];
    int cond_count;
    int loyalty;              /* servants: 0..100 */
    bool alive;
    bool with_party;
    bool warned;              /* Easy: already warned that this member is close to death */
} Member;

typedef struct {
    int member;
    int day;
    int node;                 /* last node passed */
    int edge;                 /* edge they died on, -1 if at a node */
    int miles_along;
    char cause_key[KEY_LEN];
    char epitaph[EPITAPH_LEN];
} Grave;

typedef struct {
    int day;
    LocText text;
} JournalEntry;

typedef enum {
    DLG_NONE, DLG_EVENT, DLG_LANDMARK, DLG_RIVER, DLG_ROUTE, DLG_NOTICE, DLG_BREAKDOWN, DLG_STRANDED
} DialogKind;

typedef struct {
    DialogKind kind;
    int event;                /* DLG_EVENT */
    int node;                 /* event node */
    int speaker;              /* member index, -1 = none */
    int subject;              /* member an outcome affected, -1 = none */
    bool showing_result;      /* the result page of a choice is up */
    LocText result[MAX_RESULT_LINES];
    int result_count;
    int next_node;            /* after the result page: -1 = end */
    double river_depth;       /* DLG_RIVER */
    int part;                 /* DLG_BREAKDOWN */
} DialogState;

typedef enum { HUNT_STALK, HUNT_WAIT, HUNT_FIRE, HUNT_RELOAD, HUNT_FOLLOW, HUNT_LEAVE, HUNT_ACTION_COUNT } HuntAction;

#define HUNT_LOG 4
typedef struct {
    bool active;
    int hunter;               /* member doing the shooting */
    int animal;               /* -1 = nothing in sight */
    int range;                /* 0 far .. 3 close */
    bool loaded;
    int reload_left;
    bool wounded;
    int turn;
    int meat;                 /* lb taken this hunt */
    int carry;                /* lb that can be carried back */
    int shots;
    int kills;
    bool over;                /* the hunt is finished; showing the summary */
    LocText log[HUNT_LOG];
    int log_count;
} HuntState;

typedef struct {
    bool active;
    int partner;
    int give[MAX_ITEMS];      /* base units offered, by item */
    int take[MAX_ITEMS];      /* base units asked for, by item */
    bool asked;               /* they have said what they need */
    LocText reply;
} BarterState;

typedef enum {
    ENDING_NONE, ENDING_ARRIVED, ENDING_SETTLED, ENDING_TURNED_BACK, ENDING_STRANDED, ENDING_LOST
} EndingKind;

typedef struct Game {
    const GameData *data;
    TrekRng rng;
    uint64_t seed;
    Side side;
    int family;
    Difficulty difficulty;
    GamePhase phase;

    /* time and place */
    int day;                  /* days since 1 Jan 1837 */
    int start_day;
    int node;                 /* current or last node */
    int edge;                 /* edge being travelled, -1 while at a node */
    double miles_along;
    int miles_total;
    Weather weather;
    bool at_node;             /* arrived and not yet left */
    bool visited[MAX_NODES];
    bool setpiece_done[MAX_NODES];
    bool river_crossed[MAX_NODES];

    /* orders */
    Pace pace;
    Rations rations;
    NightStance stance;
    bool sabbath;

    /* party, animals and goods */
    Member party[MAX_PARTY];
    int party_count;
    int inv[MAX_ITEMS];
    int money;                /* in the side's unit (Rds or shillings) */
    int wagon;
    int wagon_parts[WAGON_PART_COUNT];
    int ox_stamina;           /* 0..100 */
    int morale;               /* 0..100 */
    int relation[MAX_GROUPS];
    int days_since_wages;
    int shop_bought[MAX_SHOPS][MAX_ITEMS]; /* packs bought from each shop (scarcity) */

    /* story state */
    char flags[MAX_FLAGS][ID_LEN];
    int flag_count;
    char counter_names[MAX_COUNTERS][ID_LEN];
    int counter_values[MAX_COUNTERS];
    int counter_count;
    int *event_last_day;      /* per event: day last seen, -1 never */
    struct { int event; int day; } scheduled[MAX_SCHEDULED];
    int scheduled_count;

    /* what is on screen */
    DialogState dlg;
    LocText notices[MAX_NOTICES];
    int notice_count;
    int shop;                 /* PHASE_STORE: shop index */
    int pending_graves[MAX_GRAVES];
    int pending_grave_count;
    bool stop_travel;         /* set when something needs the player */
    bool warned_food, warned_water;

    /* history */
    Grave graves[MAX_GRAVES];
    int grave_count;
    JournalEntry journal[MAX_JOURNAL];
    int journal_count;
    EndingKind ending;
    int deaths;

    /* hunting */
    HuntState hunt;
    double region_game[MAX_REGIONS]; /* 1.0 = plenty of game; falls as the party hunts */
    int last_hunt_day;               /* one hunt a day */

    /* barter */
    BarterState barter;
    int partner_traded[MAX_PARTNERS][MAX_BARTER_GOODS]; /* units of each offer already taken */
} Game;

/* ---- views for the front end ---- */
typedef struct {
    LocText text;
    bool enabled;
    LocText locked;
} ChoiceView;

typedef struct {
    DialogKind kind;
    LocText title;
    LocText speaker;          /* empty key = no speaker */
    LocText body[MAX_RESULT_LINES];
    int body_count;
    ChoiceView choices[6];
    int choice_count;
    char image[ID_LEN];
    char history_key[KEY_LEN];
} DialogView;

typedef struct {
    int item;
    LocText name;
    LocText pack;
    int price;                /* per pack, side unit */
    int sell_price;
    int stock;                /* packs left */
    int have;                 /* base units held */
    int pack_size;
} StoreRow;

typedef struct {
    LocText shop_name;
    LocText keeper;
    LocText advice;           /* the keeper's one line of advice */
    LocText excuse;           /* empty if no price shock applies */
    StoreRow rows[MAX_ITEMS];
    int row_count;
    int money;
    int load_lb, capacity_lb;
} StoreView;

/* ---- lifecycle ---- */
Game *game_new(const GameData *data, uint64_t seed, int family, Difficulty diff);
void  game_free(Game *g);
GamePhase game_phase(const Game *g);

/* ---- orders and travel ---- */
void game_set_orders(Game *g, Pace pace, Rations rations, NightStance stance, bool sabbath);
/* Plays one day on the road. Returns true if the party stopped (event, arrival, warning...). */
bool game_travel_day(Game *g);
/* Plays days until something needs the player, at most max_days. */
int  game_travel(Game *g, int max_days);
void game_rest_day(Game *g);

/* ---- dialogs ---- */
bool game_dialog_view(const Game *g, DialogView *out);
void game_choose(Game *g, int choice);

/* ---- store ---- */
bool game_store_view(const Game *g, StoreView *out);
bool game_store_buy(Game *g, int item, int packs);
bool game_store_sell(Game *g, int item, int packs);
void game_store_leave(Game *g);

/* ---- hunting ---- */
typedef struct {
    LocText hunter;
    LocText animal;           /* empty key = nothing in sight */
    char animal_id[ID_LEN];
    AnimalTier tier;
    int range;
    bool loaded;
    int reload_left;
    bool wounded;
    int turns_left;
    int meat, carry;
    int powder, lead;
    bool over;
    LocText log[HUNT_LOG];
    int log_count;
    bool can[HUNT_ACTION_COUNT];
} HuntView;

/* Whether a hunt can start now (powder, lead, a shooter, and not already hunted out). */
bool game_can_hunt(const Game *g);
/* Starts a hunt from the travel screen; costs half a day. */
void game_hunt_start(Game *g);
bool game_hunt_view(const Game *g, HuntView *out);
void game_hunt_act(Game *g, HuntAction a);

/* ---- barter ---- */
typedef struct {
    int item;
    LocText name;
    int have;                 /* give rows: what you hold; take rows: what they still have */
    int amount;               /* units in the deal */
    int step;                 /* units per press */
    bool wanted_highly;       /* shown once they have said what they need */
} BarterRow;

typedef struct {
    LocText partner;
    LocText reply;
    BarterRow give[MAX_BARTER_GOODS];
    int give_count;
    BarterRow take[MAX_BARTER_GOODS];
    int take_count;
    double give_value, take_value, need_value; /* the meter: deal goes when give >= need */
    bool contraband;          /* the offer includes goods that may not be sold beyond the frontier */
} BarterView;

/* Barter partner at the current place, or -1. */
int  game_barter_partner_here(const Game *g);
void game_barter_start(Game *g, int partner);
bool game_barter_view(const Game *g, BarterView *out);
/* Changes the deal: theirs = the right-hand list; row is an index into it; delta is +1/-1 steps. */
void game_barter_adjust(Game *g, bool theirs, int row, int delta);
void game_barter_propose(Game *g);
void game_barter_ask(Game *g);
void game_barter_leave(Game *g);

/* ---- graves ---- */
const Grave *game_pending_grave(const Game *g);
void game_grave_done(Game *g, const char *epitaph);

/* ---- queries ---- */
int  game_load_lb(const Game *g);
int  game_capacity_lb(const Game *g);
int  game_food_lb(const Game *g);
double game_food_days(const Game *g);
int  game_water_capacity(const Game *g);
int  game_alive_count(const Game *g);
int  game_member_count_with_party(const Game *g);
bool game_has_flag(const Game *g, const char *flag);
int  game_counter(const Game *g, const char *name);
int  game_miles_to_next(const Game *g);
int  game_next_node(const Game *g);
int  game_score(const Game *g);
/* Price of one pack of item at the current shop in the side's unit (0 = not for sale). */
int  game_price(const Game *g, int item, bool selling);
/* Pence to the side's unit (Rds or shillings), rounded, at least 1 if pence > 0. */
int  game_pence_to_unit(const Game *g, int pence);
/* Adds a journal line. */
void game_journal(Game *g, const LocText *t);

/* ---- text helpers ---- */
/* Locale key for a member's role, e.g. "role.child.she" (daughter) or "role.servant". */
void member_role_key(Role role, Pronoun pronoun, char *out, size_t cap);
void loctext_set(LocText *t, const char *key);
void loctext_arg_int(LocText *t, const char *name, long long v);
void loctext_arg_str(LocText *t, const char *name, const char *s);
void loctext_arg_key(LocText *t, const char *name, const char *key);
void loctext_arg_money(LocText *t, const char *name, long long v);
void loctext_arg_date(LocText *t, const char *name, int day);

/* ---- ending ---- */
/* Fills out[] with the epilogue lines; returns how many. */
int game_epilogue(const Game *g, LocText *out, int max);
/* Probability of each outcome of a choice in the current event (for the "show odds" setting).
 * Returns the number of outcomes written, 0 if not an event choice. */
int game_choice_odds(const Game *g, int choice, double *out, int max);

/* ---- saves ---- */
/* Returns a malloc'd JSON string. */
char *game_save_json(const Game *g);
/* Restores a game; NULL with a message in err on failure. */
Game *game_load_json(const GameData *data, const char *json, char *err, size_t err_len);

#endif
