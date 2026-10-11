/* Sound effects, synthesised at start-up (no audio files, nothing to license). */
#ifndef TREK_SOUND_H
#define TREK_SOUND_H

#include <stdbool.h>

typedef enum {
    SFX_CLICK, SFX_GUNSHOT, SFX_MISFIRE, SFX_WHIP, SFX_WATER, SFX_LOWING, SFX_BELL, SFX_COUNT
} Sfx;

bool sound_init(void);
void sound_shutdown(void);
void sound_play(Sfx s);
void sound_set_enabled(bool on);

#endif
