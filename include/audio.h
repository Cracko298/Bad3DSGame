#pragma once

#include <stdbool.h>

typedef enum {
    AUDIO_BACKEND_NONE = 0,
    AUDIO_BACKEND_NDSP,
    AUDIO_BACKEND_CSND
} AudioBackend;

typedef enum {
    AUDIO_SOURCE_NONE = 0,
    AUDIO_SOURCE_LINKED,
    AUDIO_SOURCE_SDMC
} AudioSource;

typedef enum {
    AUDIO_SFX_EXPLOSION = 0,
    AUDIO_SFX_PLAYER_DIED,
    AUDIO_SFX_PLAYER_HIT,
    AUDIO_SFX_COIN,
    AUDIO_SFX_COIN1,
    AUDIO_SFX_GRAPPLE,
    AUDIO_SFX_COUNT
} AudioSfx;

bool audio_init(void);
void audio_update(void);

void audio_restart_music(void);
void audio_stop_music(void);

void audio_play_sfx(AudioSfx sfx);

void audio_play_menu_move(void);
void audio_play_purchase(void);
void audio_play_money_gain(void);

void audio_set_sfx_enabled(bool enabled);
void audio_set_music_enabled(bool enabled);
bool audio_sfx_enabled(void);
bool audio_music_enabled(void);

bool audio_music_available(void);
AudioBackend audio_backend(void);
AudioSource audio_music_source(void);
const char *audio_status_text(void);

void audio_shutdown(void);
