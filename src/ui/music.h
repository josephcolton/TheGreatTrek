/* Music: public-domain psalm, hymn and folk tunes from data/music.json, played in a retro
 * tracker style (pulse-wave melody, square inner parts, triangle bass, a little echo). The
 * notes are rendered when a tune is first needed; nothing is read from audio files. */
#ifndef TREK_MUSIC_H
#define TREK_MUSIC_H

#include <stdbool.h>

/* Reads data_dir/music.json. Call after sound_init(); without an audio device it does nothing. */
bool music_init(const char *data_dir);
void music_shutdown(void);
/* 0 (off) to 100. */
void music_set_volume(int percent);
/* The scene whose tune should play ("title", "night", ...), or NULL for silence. A change
 * fades the old tune out and the new one in. */
void music_want(const char *scene);
/* For testing by ear: writes one pass of a tune to a 16-bit WAV file. Works without an audio
 * device once music_load() has read the tunes. */
bool music_load(const char *data_dir);
bool music_write_wav(const char *tune_id, const char *path);
/* Call every frame: fades, and keeps the tune queued (it loops). */
void music_update(double dt);

#endif
