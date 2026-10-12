/* High scores: <pref>/scores.json keeps every finished trip's score, so a score stays after its
 * trip is deleted. A trip is recorded when it ends; trips finished before there was a score file
 * are added the first time the list is read. */
#ifndef TREK_SCORES_H
#define TREK_SCORES_H

#include "game.h"
#include "trips.h"

#define MAX_SCORES 100

typedef struct {
    char trip_id[64];
    char family_id[ID_LEN];
    char surname[32];
    Difficulty difficulty;
    EndingKind ending;
    char node[ID_LEN];      /* where the trip ended */
    int day;
    int alive, party;
    int miles;
    int score;
} ScoreEntry;

/* Records (or updates) a finished trip's score. */
void scores_record(const char *pref_path, const char *trip_id, const Game *g);
/* Reads the scores, best first, after adding any finished trip in `trips` not yet recorded. */
int scores_load(const char *pref_path, const TripList *trips, const GameData *data, ScoreEntry *out, int max);

#endif
