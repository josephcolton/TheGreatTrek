/* Simple drawn scenes: the veld by region and weather, places, rivers, night camps.
 * Everything is drawn with rectangles, so there are no image files to license. */
#ifndef TREK_SCENE_H
#define TREK_SCENE_H

#include "app.h"

typedef enum {
    SCENE_ROAD,      /* open country with the wagon */
    SCENE_TOWN,
    SCENE_RIVER,
    SCENE_MISSION,
    SCENE_LAAGER,
    SCENE_MOUNTAIN,
    SCENE_NIGHT,     /* camp fire under the stars */
    SCENE_END
} SceneKind;

/* Draws a scene into rect. region < 0 uses the game's current region. */
void scene_draw(App *app, SDL_FRect rect, SceneKind kind, int region, bool moving);
/* Sky, clouds and two lines of hills down to the horizon, dimmed by light (1 = full day). */
void scene_backdrop(App *app, SDL_FRect r, float horizon, int region, float light);
/* The title screen's picture: the highveld with thorn trees, and a wagon and span going by.
 * Needs no game. */
void scene_title(App *app, SDL_FRect r);
/* The colour of the ground in a region. */
SDL_Color scene_ground(const App *app, int region);
/* The scene that fits the game's current dialog or place. */
SceneKind scene_for_game(const App *app);

#endif
