/* Game data loaded once from the JSON files in data/: items, families, route, regions, rivers,
 * shops, rules and events. Read-only during play. Text is stored as locale keys. */
#ifndef TREK_DATA_H
#define TREK_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ID_LEN 32
#define KEY_LEN 64

#define MAX_ITEMS 64
#define MAX_FAMILIES 12
#define MAX_MEMBERS 12
#define MAX_NODES 40
#define MAX_EDGES 64
#define MAX_REGIONS 12
#define MAX_RIVERS 12
#define MAX_SHOPS 24
#define MAX_GROUPS 12
#define MAX_ILLNESSES 12
#define MAX_SHOCKS 16
#define MAX_WAGONS 6
#define MAX_START_ITEMS 24
#define MAX_ANIMALS 16
#define MAX_PARTNERS 12
#define MAX_BARTER_GOODS 12

typedef enum { SIDE_DUTCH, SIDE_ENGLISH, SIDE_COUNT } Side;

typedef enum {
    SEASON_SUMMER, /* Dec-Feb */
    SEASON_AUTUMN, /* Mar-May */
    SEASON_WINTER, /* Jun-Aug */
    SEASON_SPRING, /* Sep-Nov */
    SEASON_COUNT
} Season;

typedef enum {
    WEATHER_CLEAR, WEATHER_HOT, WEATHER_WINDY, WEATHER_COLD,
    WEATHER_FROST, WEATHER_RAIN, WEATHER_STORM, WEATHER_COUNT
} Weather;

typedef enum { PACE_EASY, PACE_NORMAL, PACE_HARD, PACE_COUNT } Pace;
typedef enum { RATIONS_FULL, RATIONS_MEAGRE, RATIONS_STARVING, RATIONS_COUNT } Rations;
typedef enum { NIGHT_REST, NIGHT_LAAGER, NIGHT_GUARD, NIGHT_COUNT } NightStance;
typedef enum { DIFF_EASY, DIFF_NORMAL, DIFF_HARD, DIFF_COUNT } Difficulty;

typedef enum {
    ROLE_HEAD, ROLE_SPOUSE, ROLE_CHILD, ROLE_ELDER, ROLE_SERVANT, ROLE_COUNT
} Role;

typedef enum { AGE_CHILD, AGE_ADULT, AGE_ELDER, AGE_COUNT } AgeGroup;

typedef enum {
    SKILL_SHOOTING, SKILL_HERDING, SKILL_VOORLOPER, SKILL_INTERPRETING,
    SKILL_NURSING, SKILL_SMITHING, SKILL_TRACKING, SKILL_COUNT
} Skill;

typedef enum { PRONOUN_HE, PRONOUN_SHE, PRONOUN_COUNT } Pronoun;

typedef enum { WAGON_PART_WHEELS, WAGON_PART_AXLE, WAGON_PART_DISSEL, WAGON_PART_TENT, WAGON_PART_COUNT } WagonPart;

typedef enum { WATER_STREAMS, WATER_SPRINGS, WATER_DRY, WATER_COUNT } WaterKind;

typedef enum { NODE_TOWN, NODE_RIVER, NODE_MISSION, NODE_LANDMARK, NODE_END } NodeKind;

/* ---- items ---- */
typedef enum {
    ITEMCAT_ANIMALS, ITEMCAT_FOOD, ITEMCAT_WATER, ITEMCAT_GUNS,
    ITEMCAT_WAGON, ITEMCAT_TRADE, ITEMCAT_MEDICINE, ITEMCAT_COUNT
} ItemCategory;

typedef struct {
    char id[ID_LEN];
    char name_key[KEY_LEN];
    char pack_key[KEY_LEN];   /* how it is sold: "unit.muid", "unit.each", "unit.lb" */
    ItemCategory category;
    int pack;                 /* base units per pack (e.g. 180 lb of meal in a muid) */
    double weight_lb;         /* weight of one base unit */
    int price_pence;          /* base price of one pack, in pence */
    double food;              /* lb of food value per base unit (0 = not food) */
    int eat_order;            /* lower is eaten first */
    int morale;               /* morale per day while this is in stock and eaten (coffee, sugar) */
    int water_capacity;       /* gallons of water a vat holds */
    uint8_t sides;            /* bitmask of sides that can buy it in shops */
    char source[KEY_LEN * 2]; /* where the price comes from */
} ItemDef;

/* ---- wagons ---- */
typedef struct {
    char id[ID_LEN];
    char name_key[KEY_LEN];
    int capacity_lb;
    int min_oxen;
    int full_span;
    double speed;      /* multiplier on daily miles */
    double fragility;  /* multiplier on breakage chance */
    int price_pence;
} WagonDef;

/* ---- families ---- */
typedef struct {
    char name[32];
    Role role;
    AgeGroup age;
    Pronoun pronoun;
    int birth_year;
    uint8_t skills[SKILL_COUNT];
} MemberDef;

typedef struct {
    int item;
    int qty;
} ItemQty;

typedef struct {
    char id[ID_LEN];
    Side side;
    char name_key[KEY_LEN];
    char surname[32];
    char summary_key[KEY_LEN];
    char story_key[KEY_LEN];      /* prefix: story_key.1, .2, .3 */
    MemberDef members[MAX_MEMBERS];
    int member_count;
    int money;                    /* in the side's unit: Rds or shillings */
    int wagon;
    ItemQty start_items[MAX_START_ITEMS];
    int start_item_count;
    /* perks */
    double repair_mul;            /* time to repair (smith: 0.5) */
    double hunt_mul;              /* hunting yield */
    int morale_bonus;
    double barter_mul;
    double illness_mul;
} FamilyDef;

/* ---- regions, rivers, route ---- */
typedef struct {
    char id[ID_LEN];
    char name_key[KEY_LEN];
    int grazing[12];                              /* ox stamina gain per day by month */
    double weather[SEASON_COUNT][WEATHER_COUNT];  /* weights */
    double event_mul;
    double water_find;                            /* chance a dry day still finds water */
} RegionDef;

typedef struct {
    char id[ID_LEN];
    char name_key[KEY_LEN];
    double depth_ft[12];   /* typical depth by month */
    double spread_ft;      /* random spread */
    double ford_max_ft;    /* deeper than this cannot be forded */
    double float_min_ft;   /* floating needs at least this much water */
} RiverDef;

typedef struct {
    char id[ID_LEN];
    char name_key[KEY_LEN];
    char desc_key[KEY_LEN];
    char history_key[KEY_LEN];
    NodeKind kind;
    double lat, lon;
    int map_x, map_y;
    int shop;                   /* -1 if none */
    int river;                  /* -1 if none */
    int setpiece;               /* event index played on first arrival, -1 if none */
    double price_mul;           /* distance modifier for shop prices */
} NodeDef;

typedef struct {
    int from, to;
    int miles;
    int region;
    char terrain[ID_LEN];
    double terrain_mul;         /* speed multiplier */
    double rough;               /* breakage multiplier */
    WaterKind water;
    char label_key[KEY_LEN];    /* shown when choosing the road */
    char advice_key[KEY_LEN];
    char route_flag[ID_LEN];    /* flag set when this edge is taken (e.g. "east_road") */
} EdgeDef;

/* ---- shops, groups ---- */
typedef struct {
    char id[ID_LEN];
    char name_key[KEY_LEN];
    char keeper_key[KEY_LEN];
    int group;
    ItemQty stock[MAX_ITEMS];
    int stock_count;
} ShopDef;

typedef struct {
    char id[ID_LEN];
    char name_key[KEY_LEN];
    int start_relation[SIDE_COUNT];
    double side_price_mul[SIDE_COUNT];
    double sell_spread;
} GroupDef;

typedef struct {
    char id[ID_LEN];
    char name_key[KEY_LEN];
    int damage_per_day;     /* health lost per day while ill */
    double recover_chance;  /* per day */
    double nursing_bonus;   /* added to recover_chance per nursing point in the party */
    double contagion;       /* chance per day to pass to another member */
    int min_days;
    double weight;          /* how often this is the illness picked by a random illness roll */
    double child_mul;       /* children catch it this much more often */
    bool accident;          /* picked by accident rolls, not illness rolls */
} IllnessDef;

typedef struct {
    char id[ID_LEN];
    int from_day, to_day;   /* day numbers (days since 1 Jan 1837) */
    int nodes[MAX_NODES];
    int node_count;
    ItemCategory category;
    int item;               /* -1 = whole category */
    double mul_start, mul_end;
    char excuse_key[KEY_LEN];
    char history_key[KEY_LEN];
} PriceShock;

/* ---- hunting ---- */
typedef enum { TIER_PASSIVE, TIER_BIG, TIER_PREDATOR } AnimalTier;

typedef struct {
    char id[ID_LEN];
    char name_key[KEY_LEN];
    AnimalTier tier;
    int meat;                       /* lb of meat from one animal */
    double spook;                   /* chance it runs when you move closer */
    double size;                    /* bigger is easier to hit */
    double flee_wounded;            /* chance a wounded animal runs off */
    double charge;                  /* chance a wounded or close animal turns on the hunter */
    double region_weight[MAX_REGIONS];
} AnimalDef;

typedef struct {
    double misfire[WEATHER_COUNT];
    double hit_by_range[4];         /* far, long, medium, close */
    int reload_turns;
    int max_turns;
    int carry_on_foot;
    int carry_per_horse;
    int shots_per_lb;
    double biltong_yield;           /* lb of biltong from a lb of meat */
} HuntRules;

/* ---- barter ---- */
typedef struct {
    int item;
    double value;     /* pence per base unit */
    int stock;        /* offers only: base units available */
} BarterGood;

typedef struct {
    char id[ID_LEN];
    char name_key[KEY_LEN];
    int group;
    int node;
    double fairness;  /* they accept when what you give is worth at least this share of what you take */
    BarterGood wants[MAX_BARTER_GOODS];
    int want_count;
    BarterGood offers[MAX_BARTER_GOODS];
    int offer_count;
} BarterPartner;

/* ---- rules (tunables) ---- */
typedef struct {
    double miles[PACE_COUNT];
    int ox_cost[PACE_COUNT];
    int health_cost[PACE_COUNT];
    double food_lb[RATIONS_COUNT];
    int ration_health[RATIONS_COUNT];
    int ration_morale[RATIONS_COUNT];
    double child_food;
    double water_gal_per_person;
    int stamina_slow, stamina_collapse;
    double collapse_chance;
    int rest_stamina, rest_health, rest_morale;
    int no_water_ox_cost, no_water_health;
    double laager_miles, guard_morale;
    int laager_ox_cost;
    double night_risk_mul[NIGHT_COUNT];
    double base_event_rate;
    int sabbath_morale;
    double breakage_per_mile;
    double illness_per_day[SEASON_COUNT];
    double accident_per_day;
    double harm_mul[DIFF_COUNT];
    double start_money_mul[DIFF_COUNT];
    double warn_food_days, warn_water_days;
    double servant_wage;        /* per month, in the side's unit, Dutch = Rds */
    double servant_wage_english;
    int start_day;              /* day number of the start date */
    int start_node;
    double rds_per_pound;       /* 13.333 */
} Rules;

/* ---- events ---- */
typedef enum {
    COND_SIDE, COND_FAMILY, COND_REGION, COND_NODE, COND_EDGE_WATER, COND_MONTH,
    COND_SEASON, COND_WEATHER, COND_FLAG, COND_NOT_FLAG, COND_COUNTER_GTE,
    COND_ITEM_GTE, COND_ITEM_LT, COND_MONEY_GTE, COND_MEMBER_ROLE,
    COND_MEMBER_ROLE_HEALTHY, COND_MEMBER_SKILL_GTE, COND_RELATION_GTE,
    COND_RELATION_LT, COND_STANCE, COND_DATE_GTE, COND_DATE_LT,
    COND_DAYS_GTE, COND_DISTANCE_GTE, COND_CHANCE, COND_ANY, COND_NOT,
    COND_COUNT
} CondType;

typedef struct Cond {
    CondType type;
    int a;               /* index: item, group, skill, role, region, node, ... */
    int b;               /* integer value */
    double x;            /* chance */
    char str[ID_LEN];    /* flag or counter name */
    struct Cond *kids;   /* for ANY / NOT */
    int kid_count;
} Cond;

typedef enum { WHO_RANDOM, WHO_ALL, WHO_ROLE, WHO_SPEAKER, WHO_HEAD, WHO_CHILD } WhoKind;

typedef enum {
    EFF_ITEM, EFF_MONEY, EFF_OX_STAMINA, EFF_WAGON_DAMAGE, EFF_HEALTH, EFF_ILLNESS,
    EFF_MORALE, EFF_LOYALTY, EFF_RELATION, EFF_FLAG, EFF_CLEAR_FLAG, EFF_COUNTER,
    EFF_TIME_DAYS, EFF_DISTANCE, EFF_SCHEDULE, EFF_DEATH, EFF_LEAVE, EFF_JOURNAL,
    EFF_COUNT
} EffectType;

typedef struct {
    EffectType type;
    int a;               /* item, group, part, illness, event index */
    int amount;
    double famount;
    char str[ID_LEN];
    WhoKind who;
    Role role;
    int count;
} Effect;

typedef struct {
    double weight;
    int skill;           /* -1 = none */
    double per_point;    /* weight added per skill point of the best member */
    char text_key[KEY_LEN];
    Effect *effects;
    int effect_count;
    int next;            /* node index, -1 = end */
} Outcome;

typedef struct {
    char text_key[KEY_LEN];
    char locked_key[KEY_LEN];
    Cond *requires;
    int require_count;
    Outcome *outcomes;
    int outcome_count;
} Choice;

typedef struct {
    char id[ID_LEN];
    char speaker_key[KEY_LEN];  /* fixed speaker, or empty to use the event's speaker pick */
    char text_key[KEY_LEN];
    char image[ID_LEN];
    Choice choices[4];
    int choice_count;
} EventNode;

typedef enum { SPEAKER_NONE, SPEAKER_ROLE, SPEAKER_SKILL, SPEAKER_RANDOM } SpeakerPick;

typedef struct {
    char id[ID_LEN];
    uint8_t sides;            /* bitmask */
    uint64_t regions;         /* bitmask; 0 = any */
    uint16_t months;          /* bitmask of months 0..11; 0 = any */
    bool night;
    bool setpiece;            /* only played by a node, never picked at random */
    double weight;
    bool once;
    int cooldown_days;
    Cond *requires;
    int require_count;
    Cond *mod_conds;          /* weight_mods: condition -> multiplier */
    double *mod_muls;
    int mod_count;
    SpeakerPick speaker_pick;
    int speaker_arg;          /* role or skill */
    char history_key[KEY_LEN];
    bool harmful;             /* scaled by difficulty */
    EventNode *nodes;
    int node_count;
} EventDef;

/* ---- everything ---- */
typedef struct GameData {
    Rules rules;
    ItemDef items[MAX_ITEMS];           int item_count;
    WagonDef wagons[MAX_WAGONS];        int wagon_count;
    FamilyDef families[MAX_FAMILIES];   int family_count;
    RegionDef regions[MAX_REGIONS];     int region_count;
    RiverDef rivers[MAX_RIVERS];        int river_count;
    NodeDef nodes[MAX_NODES];           int node_count;
    EdgeDef edges[MAX_EDGES];           int edge_count;
    ShopDef shops[MAX_SHOPS];           int shop_count;
    GroupDef groups[MAX_GROUPS];        int group_count;
    IllnessDef illnesses[MAX_ILLNESSES]; int illness_count;
    PriceShock shocks[MAX_SHOCKS];      int shock_count;
    EventDef *events;                   int event_count;
    AnimalDef animals[MAX_ANIMALS];     int animal_count;
    BarterPartner partners[MAX_PARTNERS]; int partner_count;
    int contraband[MAX_ITEMS];          int contraband_count;
    HuntRules hunt;
    /* well-known item indices, resolved at load */
    int item_oxen, item_water, item_vat, item_meat, item_medicine, item_sheep, item_tallow, item_iron, item_cows, item_biltong;
    int item_gunpowder, item_lead, item_horses;
    int item_spare[WAGON_PART_COUNT];
    int group_trekkers;
} GameData;

/* Loads every data file under data_dir. Returns NULL and fills err on failure.
 * Warnings (e.g. events without sources) are counted in *warnings if not NULL. */
GameData *data_load(const char *data_dir, char *err, size_t err_len, int *warnings);
void      data_free(GameData *d);

int data_find_item(const GameData *d, const char *id);
int data_find_family(const GameData *d, const char *id);
int data_find_node(const GameData *d, const char *id);
int data_find_region(const GameData *d, const char *id);
int data_find_group(const GameData *d, const char *id);
int data_find_event(const GameData *d, const char *id);
int data_find_illness(const GameData *d, const char *id);
int data_find_shop(const GameData *d, const char *id);
int data_find_wagon(const GameData *d, const char *id);

const char *side_id(Side s);
const char *pace_id(Pace p);
const char *rations_id(Rations r);
const char *stance_id(NightStance s);
const char *difficulty_id(Difficulty d);
const char *weather_id(Weather w);
const char *skill_id(Skill s);
const char *role_id(Role r);
const char *season_id(Season s);
int side_from_id(const char *s);
int skill_from_id(const char *s);
int role_from_id(const char *s);

/* Calls fn for every locale key the data refers to (for datacheck). */
void data_each_key(const GameData *d, void (*fn)(const char *key, const char *where, void *ctx), void *ctx);

#endif
