/* Trip files: one save per trip in <pref>/trips/<id>.json, always the latest state.
 * Writes go to a .tmp file first and are then renamed over the save, so a crash
 * leaves the previous save intact (04_game_design.md section 13). */
#ifndef TREK_TRIPS_H
#define TREK_TRIPS_H

#include "game.h"

#include <SDL3/SDL.h>
#include <stdbool.h>

#define MAX_TRIPS 64

typedef struct {
    char id[64];
    char family_id[32];
    char surname[32];
    char side[16];
    char difficulty[16];
    char node[32];
    int day;
    int alive;
    int party;
    int ending;
    int phase;              /* GamePhase: an ending is only over once it is PHASE_ENDED */
    SDL_Time modified;
} TripInfo;

typedef struct {
    TripInfo trips[MAX_TRIPS];
    int count;
} TripList;

/* Fills list with saved trips, newest first. */
void trips_scan(const char *pref_path, TripList *list);
bool trips_write(const char *pref_path, const char *trip_id, const Game *g);
Game *trips_read(const char *pref_path, const char *trip_id, const GameData *data, char *err, size_t err_len);
bool trips_delete(const char *pref_path, const char *trip_id);
/* A new unique id from the time and seed. */
void trips_new_id(char *out, size_t cap, uint64_t seed);

#endif
