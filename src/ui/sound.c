#include "sound.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>

#define RATE 22050
#define CHANNELS 4
#define PI 3.14159265358979

typedef struct {
    float *data;
    int frames;
} Sample;

static SDL_AudioDeviceID device;
static SDL_AudioStream *streams[CHANNELS];
static Sample samples[SFX_COUNT];
static int next_channel;
static bool enabled = true;

/* A small private noise generator; sound is not part of the simulation, so it needs no seed. */
static uint32_t noise_state = 0x12345678u;
static float noise(void)
{
    noise_state ^= noise_state << 13;
    noise_state ^= noise_state >> 17;
    noise_state ^= noise_state << 5;
    return (float)(noise_state & 0xffff) / 32768.0f - 1.0f;
}

static Sample make(double seconds)
{
    Sample s;
    s.frames = (int)(seconds * RATE);
    s.data = calloc((size_t)s.frames, sizeof(float));
    return s;
}

static void synth_all(void)
{
    /* click: a tiny tick for choosing */
    Sample s = make(0.03);
    for (int i = 0; i < s.frames; i++) {
        s.data[i] = noise() * 0.25f * expf(-i / 60.0f);
    }
    samples[SFX_CLICK] = s;

    /* gunshot: a sharp crack and a low boom fading out */
    s = make(0.9);
    float lp = 0;
    for (int i = 0; i < s.frames; i++) {
        float t = (float)i / RATE;
        lp += 0.15f * (noise() - lp);
        float crack = noise() * expf(-t * 60.0f);
        float boom = lp * 2.5f * expf(-t * 6.0f);
        s.data[i] = (crack * 0.7f + boom * 0.6f) * 0.8f;
    }
    samples[SFX_GUNSHOT] = s;

    /* misfire: the flint's snap and a little fizz in the pan */
    s = make(0.25);
    for (int i = 0; i < s.frames; i++) {
        float t = (float)i / RATE;
        s.data[i] = noise() * (0.6f * expf(-t * 300.0f) + 0.08f * expf(-t * 12.0f));
    }
    samples[SFX_MISFIRE] = s;

    /* whip crack */
    s = make(0.12);
    for (int i = 0; i < s.frames; i++) {
        float t = (float)i / RATE;
        s.data[i] = noise() * expf(-t * 90.0f) * 0.8f;
    }
    samples[SFX_WHIP] = s;

    /* river: low, rolling filtered noise */
    s = make(1.6);
    lp = 0;
    float lp2 = 0;
    for (int i = 0; i < s.frames; i++) {
        float t = (float)i / RATE;
        lp += 0.05f * (noise() - lp);
        lp2 += 0.3f * (noise() - lp2);
        float env = sinf((float)PI * t / 1.6f);
        s.data[i] = (lp * 1.6f + lp2 * 0.15f) * env * 0.6f;
    }
    samples[SFX_WATER] = s;

    /* an ox lowing: a slow falling tone with harmonics */
    s = make(1.1);
    double phase = 0;
    for (int i = 0; i < s.frames; i++) {
        double t = (double)i / RATE;
        double f = 150.0 - 40.0 * t + 3.0 * sin(2 * PI * 5 * t);
        phase += 2 * PI * f / RATE;
        double env = sin(PI * t / 1.1);
        double v = sin(phase) + 0.5 * sin(2 * phase) + 0.3 * sin(3 * phase) + 0.15 * sin(4 * phase);
        s.data[i] = (float)(v * env * 0.18);
    }
    samples[SFX_LOWING] = s;

    /* a bell, for a grave */
    s = make(2.5);
    for (int i = 0; i < s.frames; i++) {
        double t = (double)i / RATE;
        double v = sin(2 * PI * 220 * t) + 0.5 * sin(2 * PI * 440.7 * t) + 0.25 * sin(2 * PI * 661.3 * t);
        s.data[i] = (float)(v * exp(-t * 1.6) * 0.22);
    }
    samples[SFX_BELL] = s;
}

bool sound_init(void)
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("No audio: %s", SDL_GetError());
        return false;
    }
    SDL_AudioSpec spec = {SDL_AUDIO_F32, 1, RATE};
    device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!device) {
        SDL_Log("No audio device: %s", SDL_GetError());
        return false;
    }
    for (int i = 0; i < CHANNELS; i++) {
        streams[i] = SDL_CreateAudioStream(&spec, &spec);
        if (streams[i]) {
            SDL_BindAudioStream(device, streams[i]);
        }
    }
    synth_all();
    return true;
}

void sound_shutdown(void)
{
    for (int i = 0; i < CHANNELS; i++) {
        if (streams[i]) {
            SDL_DestroyAudioStream(streams[i]);
            streams[i] = NULL;
        }
    }
    if (device) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
    for (int i = 0; i < SFX_COUNT; i++) {
        free(samples[i].data);
        samples[i].data = NULL;
    }
}

void sound_set_enabled(bool on)
{
    enabled = on;
}

void sound_play(Sfx s)
{
    if (!enabled || !device || s < 0 || s >= SFX_COUNT || !samples[s].data) {
        return;
    }
    SDL_AudioStream *st = streams[next_channel];
    next_channel = (next_channel + 1) % CHANNELS;
    if (!st) {
        return;
    }
    SDL_ClearAudioStream(st);
    SDL_PutAudioStreamData(st, samples[s].data, samples[s].frames * (int)sizeof(float));
}
