/* Music (see music.h). */
#include "music.h"

#include "sound.h"
#include "util.h"

#include <SDL3/SDL.h>
#include <cJSON.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RATE 22050
#define MAX_TUNES 16
#define MAX_SCENES 16
#define MAX_VOICES 4
#define PI 3.14159265358979

typedef struct {
    int midi;
    float beats;
} Note;

typedef struct {
    char id[32];
    float bpm;
    Note *voices[MAX_VOICES];
    int counts[MAX_VOICES];
    int voice_count;
    float *pcm; /* rendered when first wanted */
    int frames;
} Tune;

static Tune tunes[MAX_TUNES];
static int tune_count;
static struct {
    char scene[32];
    int tune;
} scenes[MAX_SCENES];
static int scene_count;

static SDL_AudioStream *stream;
static float volume = 0.36f; /* the setting, squared */
static float level = 0.22f;  /* a tune's peak at 100% volume, from music.json */
static int playing = -1, wanted = -1;
static int pos;        /* next frame of the playing tune to queue */
static float fade = 0; /* 0..1, the playing tune's fade */

/* ---------------------------------------------------------------- synthesis */

static double note_hz(int midi)
{
    return 440.0 * pow(2.0, (midi - 69) / 12.0);
}

/* One voice into out: the melody is a 25% pulse with vibrato, the inner parts soft squares, the
 * bass a triangle, the way a tracker module of the 1990s would play a hymn. */
static void render_voice(const Tune *t, int v, float *out, int frames)
{
    double sec_per_beat = 60.0 / t->bpm;
    double at = 0, phase = 0;
    bool melody = v == 0, bass = v == t->voice_count - 1;
    float amp = melody ? 0.20f : (bass ? 0.26f : 0.075f);
    for (int n = 0; n < t->counts[v]; n++) {
        const Note *note = &t->voices[v][n];
        double len = note->beats * sec_per_beat;
        int start = (int)(at * RATE), count = (int)(len * RATE);
        at += len;
        if (note->midi <= 0) {
            continue;
        }
        double hz = note_hz(note->midi);
        double sounding = len - 0.035; /* a little gap, so repeated notes are heard as two */
        for (int i = 0; i < count && start + i < frames; i++) {
            double s = (double)i / RATE;
            if (s > sounding + 0.04) {
                break;
            }
            double f = hz;
            if (melody && s > 0.18) {
                f *= pow(2.0, 0.22 * sin(2 * PI * 5.5 * s) / 12.0); /* vibrato, after the attack */
            }
            phase += f / RATE;
            phase -= floor(phase);
            double w;
            if (bass) {
                w = 4.0 * fabs(phase - 0.5) - 1.0;
            } else {
                w = phase < (melody ? 0.25 : 0.5) ? 1.0 : -1.0;
            }
            double env = s < 0.006 ? s / 0.006 : 0.72 + 0.28 * exp(-(s - 0.006) * 9.0);
            if (s > sounding) {
                env *= 1.0 - (s - sounding) / 0.04;
            }
            out[start + i] += (float)(w * env * amp);
        }
    }
}

static void render_tune(Tune *t)
{
    float beats = 0;
    for (int n = 0; n < t->counts[0]; n++) {
        beats += t->voices[0][n].beats;
    }
    /* the tune, then two beats of rest before it comes round again */
    double secs = (beats + 2) * 60.0 / t->bpm;
    t->frames = (int)(secs * RATE);
    t->pcm = calloc((size_t)t->frames, sizeof(float));
    if (!t->pcm) {
        t->frames = 0;
        return;
    }
    for (int v = 0; v < t->voice_count; v++) {
        render_voice(t, v, t->pcm, t->frames);
    }
    /* a tracker-style echo, folded round so the loop is seamless */
    int delay = (int)(0.27 * RATE);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < t->frames; i++) {
            int j = (i - delay + t->frames) % t->frames;
            t->pcm[i] += t->pcm[j] * (pass == 0 ? 0.22f : 0.08f);
        }
    }
    float peak = 0;
    for (int i = 0; i < t->frames; i++) {
        peak = fmaxf(peak, fabsf(t->pcm[i]));
    }
    /* every tune peaks at the same level, well under the sound effects */
    if (peak > 0) {
        for (int i = 0; i < t->frames; i++) {
            t->pcm[i] *= level / peak;
        }
    }
}

/* ---------------------------------------------------------------- loading */

bool music_load(const char *data_dir)
{
    if (tune_count > 0) {
        return true;
    }
    char *path = util_path_join(data_dir, "music.json");
    char *text = util_read_file(path, NULL);
    free(path);
    cJSON *root = text ? cJSON_Parse(text) : NULL;
    free(text);
    if (!root) {
        SDL_Log("No music: cannot read music.json");
        return false;
    }
    const cJSON *lv = cJSON_GetObjectItemCaseSensitive(root, "level");
    if (cJSON_IsNumber(lv) && lv->valuedouble > 0 && lv->valuedouble <= 1) {
        level = (float)lv->valuedouble;
    }
    const cJSON *tj;
    cJSON_ArrayForEach(tj, cJSON_GetObjectItemCaseSensitive(root, "tunes"))
    {
        if (tune_count >= MAX_TUNES) {
            break;
        }
        Tune *t = &tunes[tune_count];
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(tj, "id"), *bpm = cJSON_GetObjectItemCaseSensitive(tj, "bpm");
        if (!cJSON_IsString(id) || !cJSON_IsNumber(bpm) || bpm->valuedouble <= 0) {
            continue;
        }
        SDL_strlcpy(t->id, id->valuestring, sizeof t->id);
        t->bpm = (float)bpm->valuedouble;
        const cJSON *vj;
        cJSON_ArrayForEach(vj, cJSON_GetObjectItemCaseSensitive(tj, "voices"))
        {
            if (t->voice_count >= MAX_VOICES) {
                break;
            }
            int n = cJSON_GetArraySize(vj);
            Note *notes = calloc((size_t)(n > 0 ? n : 1), sizeof(Note));
            int k = 0;
            const cJSON *nj;
            cJSON_ArrayForEach(nj, vj)
            {
                if (cJSON_GetArraySize(nj) == 2) {
                    notes[k].midi = cJSON_GetArrayItem(nj, 0)->valueint;
                    notes[k].beats = (float)cJSON_GetArrayItem(nj, 1)->valuedouble;
                    k++;
                }
            }
            t->voices[t->voice_count] = notes;
            t->counts[t->voice_count++] = k;
        }
        if (t->voice_count > 0) {
            tune_count++;
        }
    }
    const cJSON *sj;
    cJSON_ArrayForEach(sj, cJSON_GetObjectItemCaseSensitive(root, "scenes"))
    {
        for (int i = 0; i < tune_count && scene_count < MAX_SCENES && cJSON_IsString(sj); i++) {
            if (strcmp(tunes[i].id, sj->valuestring) == 0) {
                SDL_strlcpy(scenes[scene_count].scene, sj->string, sizeof scenes[0].scene);
                scenes[scene_count++].tune = i;
            }
        }
    }
    cJSON_Delete(root);
    return tune_count > 0;
}

bool music_init(const char *data_dir)
{
    SDL_AudioDeviceID dev = sound_device();
    if (!dev || !music_load(data_dir)) {
        return false;
    }
    SDL_AudioSpec spec = {SDL_AUDIO_F32, 1, RATE};
    stream = SDL_CreateAudioStream(&spec, &spec);
    if (!stream || !SDL_BindAudioStream(dev, stream)) {
        SDL_Log("No music stream: %s", SDL_GetError());
        return false;
    }
    SDL_SetAudioStreamGain(stream, 0);
    return true;
}

void music_shutdown(void)
{
    if (stream) {
        SDL_DestroyAudioStream(stream);
        stream = NULL;
    }
    for (int i = 0; i < tune_count; i++) {
        for (int v = 0; v < tunes[i].voice_count; v++) {
            free(tunes[i].voices[v]);
        }
        free(tunes[i].pcm);
    }
    memset(tunes, 0, sizeof tunes);
    tune_count = scene_count = 0;
    playing = wanted = -1;
}

bool music_write_wav(const char *tune_id, const char *path)
{
    for (int i = 0; i < tune_count; i++) {
        Tune *t = &tunes[i];
        if (strcmp(t->id, tune_id) != 0) {
            continue;
        }
        if (!t->pcm) {
            render_tune(t);
        }
        SDL_IOStream *io = SDL_IOFromFile(path, "wb");
        if (!io) {
            return false;
        }
        Uint32 bytes = (Uint32)t->frames * 2;
        SDL_WriteIO(io, "RIFF", 4);
        SDL_WriteU32LE(io, 36 + bytes);
        SDL_WriteIO(io, "WAVEfmt ", 8);
        SDL_WriteU32LE(io, 16);
        SDL_WriteU16LE(io, 1); /* PCM */
        SDL_WriteU16LE(io, 1); /* mono */
        SDL_WriteU32LE(io, RATE);
        SDL_WriteU32LE(io, RATE * 2);
        SDL_WriteU16LE(io, 2);
        SDL_WriteU16LE(io, 16);
        SDL_WriteIO(io, "data", 4);
        SDL_WriteU32LE(io, bytes);
        for (int f = 0; f < t->frames; f++) {
            float v = fmaxf(-1.0f, fminf(1.0f, t->pcm[f]));
            SDL_WriteS16LE(io, (Sint16)(v * 32767));
        }
        return SDL_CloseIO(io);
    }
    return false;
}

/* ---------------------------------------------------------------- playing */

void music_set_volume(int percent)
{
    /* squared, since the ear hears loudness on a curve: 50% is a quarter of full, 10% a hundredth */
    float v = (percent < 0 ? 0 : percent > 100 ? 100 : percent) / 100.0f;
    volume = v * v;
}

void music_want(const char *scene)
{
    wanted = -1;
    for (int i = 0; scene && i < scene_count; i++) {
        if (strcmp(scenes[i].scene, scene) == 0) {
            wanted = scenes[i].tune;
        }
    }
}

void music_update(double dt)
{
    if (!stream) {
        return;
    }
    if (playing != wanted || volume <= 0) {
        /* fade the old tune out before the new one starts */
        fade -= (float)(dt / 0.8);
        if (fade <= 0) {
            fade = 0;
            SDL_ClearAudioStream(stream);
            playing = volume > 0 ? wanted : -1;
            pos = 0;
            if (playing >= 0 && !tunes[playing].pcm) {
                render_tune(&tunes[playing]);
            }
        }
    } else if (fade < 1) {
        fade = fminf(1, fade + (float)(dt / 1.5));
    }
    SDL_SetAudioStreamGain(stream, volume * fade);
    if (playing < 0 || !tunes[playing].pcm || tunes[playing].frames == 0) {
        return;
    }
    /* keep about half a second queued, looping the tune */
    Tune *t = &tunes[playing];
    while (SDL_GetAudioStreamQueued(stream) < (int)(RATE * 0.5 * sizeof(float))) {
        int n = t->frames - pos;
        if (n > RATE / 4) {
            n = RATE / 4;
        }
        SDL_PutAudioStreamData(stream, t->pcm + pos, n * (int)sizeof(float));
        pos = (pos + n) % t->frames;
    }
}
