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
#define MAX_CHOICES 9 /* the number keys reach 9 */

typedef enum {
    PHASE_DIALOG,   /* events, landmarks, rivers, road choices, notices */
    PHASE_STORE,
    PHASE_TRAVEL,
    PHASE_GRAVE,
    PHASE_ENDED,
    PHASE_HUNT,
    PHASE_BARTER
} GamePhase;

/* ARG_PERSON is a party member's name (s) with their pronoun (i), so text can say {who.his}. */
typedef enum { ARG_INT, ARG_STR, ARG_KEY, ARG_MONEY, ARG_DATE, ARG_PERSON } ArgKind;

typedef struct {
    char name[12];
    ArgKind kind;
    long long i;      /* ARG_INT, ARG_MONEY (in the side's unit), ARG_DATE (day number), ARG_PERSON (Pronoun) */
    char s[40];       /* ARG_STR (literal), ARG_KEY (locale key), ARG_PERSON (name) */
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
    int doses;                /* doses of medicine given for it */
    bool treated;             /* medicine is being given for it (always, unless given by hand) */
} MemberCond;

typedef struct {
    char name[32];
    Role role;
    AgeGroup age;
    Pronoun pronoun;
    int birth_year;
    uint8_t skills[SKILL_COUNT];
    char portrait[ID_LEN];    /* from the family; stays with the person if renamed */
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
    int illness;              /* the illness they died of, -1 if not an illness */
    int ill_days;             /* how long they had been ill */
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

/* Hunting is a real-time shooting scene (04_game_design.md section 9), stepped in fixed ticks so a
 * hunt still replays exactly from the seed and the shots. The field is a side view of the veld:
 * x runs 0..HUNT_FIELD_W, y from 0 at the horizon to HUNT_FIELD_H at the hunter's feet. */
#define HUNT_LOG 4
#define HUNT_MAX_ANIMALS 10
#define HUNT_FIELD_W 640
#define HUNT_FIELD_H 210
#define HUNT_TICKS_PER_SEC 30
#define HUNT_MAX_TAKES 16

/* RUSH and FLEE are for guarding the herd: a predator dashes in at the cattle, or makes off. */
typedef enum {
    HUNT_A_NONE, HUNT_A_WALK, HUNT_A_GRAZE, HUNT_A_RUN, HUNT_A_CHARGE, HUNT_A_DOWN, HUNT_A_RUSH, HUNT_A_FLEE
} HuntAnimalState;

typedef struct {
    int kind;                 /* animal index */
    HuntAnimalState state;    /* HUNT_A_NONE = empty slot */
    float x, y;               /* the animal's feet, in field units */
    float dir;                /* +1 facing right, -1 facing left */
    int timer;                /* ticks left in this state */
    int hp;
    float tx, ty;             /* guarding the herd: the beast it is after */
    int prey;                 /* item it carries off, -1 none */
} HuntAnimal;

typedef struct {
    bool active;
    int hunter;               /* member doing the shooting */
    int ticks_left;           /* daylight left */
    int reload_ticks;         /* 0 = loaded */
    int spawn_ticks;          /* until more game may come into sight */
    int shots, shots_max;     /* shots fired; shots the powder and lead allow */
    int kills;
    int meat;                 /* lb shot this hunt; only `carry` comes back */
    int carry;                /* lb that can be carried back */
    bool over;                /* the hunt is finished; showing the summary */
    HuntAnimal animals[HUNT_MAX_ANIMALS];
    float shot_x, shot_y;     /* the last shot, for the smoke */
    int shot_age;             /* ticks since the last shot */
    bool shot_misfire;
    LocText log[HUNT_LOG];
    int log_count;
    /* guarding the herd at night (game_defend_start): the same scene, but predators come for the stock */
    bool defend;
    int foe;                  /* animal index */
    int foes_left;            /* still to come out of the dark */
    int driven;               /* hit or scared off */
    int took[HUNT_MAX_TAKES]; /* item index of each beast carried off */
    int took_count;
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
    bool keep_flock;          /* slaughter sheep only when the stores run low, not every few days */
    bool medicine_by_hand;    /* medicine given by hand to someone ill, not to everyone who falls ill */

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
    ChoiceView choices[MAX_CHOICES];
    int choice_count;
    char image[ID_LEN];
    char portrait[ID_LEN];    /* who is speaking or met; empty = none */
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
/* Eat the flock (a sheep whenever the fresh meat runs out, as the trekkers did) or keep it and eat
 * the wagon's stores, slaughtering only when they run low. */
void game_set_keep_flock(Game *g, bool keep);
/* Medicine is given for an illness, a dose a day while it lasts. With the trek option on automatic
 * (the default) everyone who falls ill gets it; given by hand, the player starts it for someone who
 * is ill, on the party screen. Nothing is decided in advance for anyone who is well. */
void game_set_medicine_by_hand(Game *g, bool by_hand);
/* Starts medicine for every illness a member has now; false if they are not ill or none is left. */
bool game_give_medicine(Game *g, int member);
/* Whether a member is ill and being given medicine for it. */
bool game_medicine_given(const Game *g, int member);
/* Pounds of food the party eats a day at the present rations. */
double game_food_need(const Game *g);
/* Plays one day on the road. Returns true if the party stopped (event, arrival, warning...). */
bool game_travel_day(Game *g);
/* Plays days until something needs the player, at most max_days. */
int  game_travel(Game *g, int max_days);
void game_rest_day(Game *g);
/* Whether any part of the wagon (or its tent) is worn enough to be worth a day's mending. */
bool game_can_mend(const Game *g);
/* A day at the outspan mending the wagon: fits a spare to a badly worn part, mends the rest
 * with bar iron (better with a good smith), greases wheels and axle with tallow, and patches
 * the tent or puts new canvas over it. The day passes as a rest day. */
void game_mend_wagon(Game *g);

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
    bool over;
    bool loaded;
    double reload;            /* 0..1 of the reload done */
    double daylight;          /* 1 at the start, 0 at sundown */
    int meat, carry;
    int shots_left;
    HuntAnimal animals[HUNT_MAX_ANIMALS];
    float shot_x, shot_y;
    int shot_age;
    bool shot_misfire;
    LocText log[HUNT_LOG];
    int log_count;
    LocText summary[4];       /* when over: what was shot and what comes back, or what was lost */
    bool defend;              /* guarding the herd at night */
    int lost;                 /* beasts carried off so far */
} HuntView;

/* Whether a hunt can start now (powder, lead, a shooter, and not already hunted out). */
bool game_can_hunt(const Game *g);
/* Starts a hunt from the travel screen; costs half a day. */
void game_hunt_start(Game *g);
bool game_hunt_view(const Game *g, HuntView *out);
/* Moves the hunt on by one tick (1/HUNT_TICKS_PER_SEC s). */
void game_hunt_tick(Game *g);
/* Fires at a point in the field. Returns false if the gun was not ready. */
bool game_hunt_shoot(Game *g, double x, double y);
/* Guarding the herd at night against a predator (an event's "defend" effect). */
void game_defend_start(Game *g, int animal);
/* Stops hunting and shows the summary. */
void game_hunt_quit(Game *g);
/* After the summary: carries the meat back and returns to the road. */
void game_hunt_finish(Game *g);
/* Size of things at a depth y in the field (far is small). */
double game_hunt_scale(double y);
/* An animal's body (for drawing) and its hit box, in field units: x, y (top left), w, h. */
void game_hunt_body(const Game *g, const HuntAnimal *a, float out[4]);

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
/* The score and what it is made of: the people brought through, the road covered, and herds,
 * money and good relations. */
void game_score_parts(const Game *g, int *people, int *road, int *goods, int *total);
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
/* A party member: {name} gives their name, {name.he} {name.him} {name.his} {name.himself} their
 * pronoun, capitalised as {name.He}. m may be NULL (an empty name). */
void loctext_arg_person(LocText *t, const char *name, const Member *m);
void loctext_arg_key(LocText *t, const char *name, const char *key);
void loctext_arg_money(LocText *t, const char *name, long long v);
void loctext_arg_date(LocText *t, const char *name, int day);

/* ---- ending ---- */
/* Fills out[] with the epilogue lines; returns how many. */
int game_epilogue(const Game *g, LocText *out, int max);
/* About how many days on the road from here to a place, at the present pace with Sundays and
 * stops, and any known hold-ups on the way (route.json delay_days); -1 if it can't be reached. */
int game_days_to(const Game *g, int node);
/* Who lies in a grave and why, for the grave screen: who they were, what they died of, and how
 * it came about. Returns the number of lines (at most 3). */
int game_grave_story(const Game *g, const Grave *gr, LocText *out, int max);
/* What happened at the place the trip ended, after the family arrived (route.json epilogue): the
 * history page after the ending. 0 lines if the trip did not end at a place with one. */
int game_history_epilogue(const Game *g, LocText *out, int max);
/* Probability of each outcome of a choice in the current event (for the "show odds" setting).
 * Returns the number of outcomes written, 0 if not an event choice. */
int game_choice_odds(const Game *g, int choice, double *out, int max);

/* ---- saves ---- */
/* Returns a malloc'd JSON string. */
char *game_save_json(const Game *g);
/* Restores a game; NULL with a message in err on failure. */
Game *game_load_json(const GameData *data, const char *json, char *err, size_t err_len);

#endif
