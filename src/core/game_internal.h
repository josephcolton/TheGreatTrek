/* Functions shared between the game_*.c files. Not part of the public API. */
#ifndef TREK_GAME_INTERNAL_H
#define TREK_GAME_INTERNAL_H

#include "game.h"

/* game.c */
void g_settle(Game *g);
void g_notice(Game *g, const LocText *t);
void g_notice_key(Game *g, const char *key);
void g_notice_member(Game *g, const char *key, int member);
void g_arrive(Game *g, int node);
void g_depart(Game *g);
void g_start_edge(Game *g, int edge);
void g_add_item(Game *g, int item, int qty);
void g_add_morale(Game *g, int delta);
void g_add_relation(Game *g, int group, int delta);
void g_set_flag(Game *g, const char *flag);
void g_clear_flag(Game *g, const char *flag);
void g_add_counter(Game *g, const char *name, int delta);
void g_kill(Game *g, int member, const char *cause_key);
void g_member_leaves(Game *g, int member);
bool g_member_active(const Game *g, int m);
bool g_member_healthy(const Game *g, int m);
int  g_best_skill(const Game *g, Skill s, bool healthy_only);
int  g_best_skill_member(const Game *g, Skill s);
int  g_random_member(Game *g, bool children_only, int role);
int  g_head(const Game *g);
int  g_current_region(const Game *g);
double g_harm(const Game *g);
void g_add_money_pence(Game *g, int pence);
int g_wagon_trade_cost(const Game *g, int wagon);
int g_smith_cost(const Game *g); /* pence for a town smith to put the whole wagon right */ /* pence to pay (negative: paid to you) */

/* game_travel.c */
void g_pass_day(Game *g, bool travelling, bool allow_events);
void g_open_breakdown(Game *g, int part);
void g_check_stranded(Game *g);
int  g_oxen(const Game *g);

/* game_events.c */
bool g_eval(const Game *g, const Cond *c);
bool g_eval_all(const Game *g, const Cond *c, int n);
void g_start_event(Game *g, int event);
bool g_maybe_event(Game *g, bool night);
void g_event_view(const Game *g, DialogView *v);
void g_event_choose(Game *g, int choice);
void g_add_result(Game *g, const LocText *t);
void g_add_result_key(Game *g, const char *key);
void g_add_illness(Game *g, int member, int illness);

/* game_river.c, game_landmark.c */
void g_open_river(Game *g);
void g_river_view(const Game *g, DialogView *v);
void g_river_choose(Game *g, int choice);
void g_landmark_view(const Game *g, DialogView *v);
void g_landmark_choose(Game *g, int choice);
void g_route_view(const Game *g, DialogView *v);
void g_route_choose(Game *g, int choice);
void g_breakdown_view(const Game *g, DialogView *v);
void g_breakdown_choose(Game *g, int choice);
void g_stranded_view(const Game *g, DialogView *v);
void g_stranded_choose(Game *g, int choice);

#endif
