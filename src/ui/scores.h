/* High scores: <pref>/scores.json keeps every finished trip's score. A trip is scored when it
 * ends, and once it is scored its save is deleted: a finished trip cannot be played on. */
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
/* Scores every finished trip in `trips` and deletes its save (trips that ended before the
 * game did this, or whose ending screen was never closed). Returns how many were deleted;
 * scan the trips again afterwards. */
int scores_take_finished(const char *pref_path, const TripList *trips, const GameData *data);
/* Reads the scores, best first. */
int scores_load(const char *pref_path, ScoreEntry *out, int max);
/* Deletes every high score. */
void scores_clear(const char *pref_path);

#endif
