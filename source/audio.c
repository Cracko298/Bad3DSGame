#include <3ds.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"

/*
   audio_assets.s directly embeds each BCWAV in .rodata with .incbin.
   No generated DATA/bin2o object/header targets are needed.
*/
extern const u8 soundtrack_bcwav[];
extern const u8 soundtrack_bcwav_end[];

extern const u8 Explosion_bcwav[];
extern const u8 Explosion_bcwav_end[];

extern const u8 PlayerDied_bcwav[];
extern const u8 PlayerDied_bcwav_end[];

extern const u8 PlayerHit_bcwav[];
extern const u8 PlayerHit_bcwav_end[];

extern const u8 Coin_bcwav[];
extern const u8 Coin_bcwav_end[];

extern const u8 Coin1_bcwav[];
extern const u8 Coin1_bcwav_end[];

extern const u8 grapple_bcwav[];
extern const u8 grapple_bcwav_end[];

#define MUSIC_CHANNEL 0
#define SFX_CHANNEL_BASE 1

#define MUSIC_VOLUME 0.72f

#define MUSIC_BUFFER_COUNT 3
#define MUSIC_PCM_SAMPLES 4096

typedef struct {
    const u8 *file_data;
    u32 file_bytes;
    bool owns_file_data;

    const u8 *adpcm_data;
    u32 adpcm_bytes;

    u32 sample_rate;
    u32 sample_count;

    s16 coefs[16];

    s16 start_history0;
    s16 start_history1;

    s16 loop_history0;
    s16 loop_history1;
    u32 loop_start;

    bool valid;
    AudioSource source;
} CwavAsset;

typedef struct {
    const CwavAsset *asset;

    u32 sample_pos;
    u32 frame_index;
    u8 sample_in_frame;

    int scale;
    int coef1;
    int coef2;

    int yn1;
    int yn2;
} DspDecoder;

typedef struct {
    bool valid;

    s16 *pcm;
    u32 nsamples;
    u32 sample_rate;
    float volume;

    ndspWaveBuf wave;
} SfxSample;

typedef struct {
    bool ndsp_ready;
    bool available;
    bool playing;

    AudioBackend backend;
    AudioSource music_source;

    CwavAsset music_asset;
    CwavAsset sfx_asset[AUDIO_SFX_COUNT];

    DspDecoder music_decoder;

    s16 *music_pcm;
    ndspWaveBuf music_wave[MUSIC_BUFFER_COUNT];

    SfxSample sfx[AUDIO_SFX_COUNT];

    uint32_t coin_rng;
    const char *status;
} AudioState;

static AudioState s_audio;

/*
   These survive audio_init(), which clears AudioState. Saved settings can be
   applied during game_init() before NDSP is initialized.
*/
static bool s_sfx_enabled = true;
static bool s_music_enabled = true;

typedef struct {
    const char *name;
    const u8 *data;
    const u8 *end;
} LinkedAsset;

static const LinkedAsset LINKED_MUSIC = {
    "soundtrack.bcwav",
    soundtrack_bcwav,
    soundtrack_bcwav_end
};

static const LinkedAsset LINKED_SFX[AUDIO_SFX_COUNT] = {
    { "Explosion.bcwav",  Explosion_bcwav,  Explosion_bcwav_end  },
    { "PlayerDied.bcwav", PlayerDied_bcwav, PlayerDied_bcwav_end },
    { "PlayerHit.bcwav",  PlayerHit_bcwav,  PlayerHit_bcwav_end  },
    { "Coin.bcwav",       Coin_bcwav,       Coin_bcwav_end       },
    { "Coin1.bcwav",      Coin1_bcwav,      Coin1_bcwav_end      },
    { "grapple.bcwav",    grapple_bcwav,    grapple_bcwav_end    }
};

static const float SFX_VOLUMES[AUDIO_SFX_COUNT] = {
    0.78f,
    0.88f,
    0.72f,
    0.72f,
    0.72f,
    0.76f
};

static u16 read_u16le(const u8 *p) {
    return (u16)p[0] |
           ((u16)p[1] << 8);
}

static s16 read_s16le(const u8 *p) {
    return (s16)read_u16le(p);
}

static u32 read_u32le(const u8 *p) {
    return (u32)p[0] |
           ((u32)p[1] << 8) |
           ((u32)p[2] << 16) |
           ((u32)p[3] << 24);
}

static bool range_ok(u32 total,
                     u32 offset,
                     u32 bytes) {
    return offset <= total &&
           bytes <= total - offset;
}

static int clamp_s16_int(int value) {
    if (value < -32768)
        return -32768;
    if (value > 32767)
        return 32767;
    return value;
}

static u32 dspadpcm_bytes_for_samples(u32 samples) {
    u64 frames =
        ((u64)samples + 13ull) /
        14ull;

    u64 bytes =
        frames * 8ull;

    if (bytes >
        0xFFFFFFFFull) {
        return 0;
    }

    return (u32)bytes;
}

/* =========================================================
   CWAV parser
   ========================================================= */

static bool parse_cwav_memory(
    CwavAsset *out,
    const u8 *file_data,
    u32 file_bytes,
    AudioSource source,
    bool owns_file_data) {

    if (!out ||
        !file_data ||
        file_bytes < 0x40) {
        return false;
    }

    if (memcmp(
            file_data,
            "CWAV",
            4) != 0) {
        return false;
    }

    if (read_u16le(file_data + 4) !=
        0xFEFF) {
        return false;
    }

    u16 header_size =
        read_u16le(
            file_data + 6
        );

    u32 stated_size =
        read_u32le(
            file_data + 0x0C
        );

    u16 block_count =
        read_u16le(
            file_data + 0x10
        );

    if (header_size < 0x14 ||
        header_size > file_bytes ||
        stated_size > file_bytes ||
        block_count < 2) {
        return false;
    }

    u32 info_off = 0;
    u32 data_off = 0;

    for (u32 i = 0;
         i < block_count;
         ++i) {

        u32 ref =
            0x14u +
            i * 12u;

        if (!range_ok(
                file_bytes,
                ref,
                12)) {
            return false;
        }

        u32 off =
            read_u32le(
                file_data +
                ref + 4
            );

        u32 size =
            read_u32le(
                file_data +
                ref + 8
            );

        if (size < 8 ||
            !range_ok(
                file_bytes,
                off,
                size)) {
            return false;
        }

        if (memcmp(
                file_data + off,
                "INFO",
                4) == 0) {
            info_off = off;
        } else if (memcmp(
                       file_data + off,
                       "DATA",
                       4) == 0) {
            data_off = off;
        }
    }

    if (!info_off ||
        !data_off ||
        !range_ok(
            file_bytes,
            info_off,
            0x28)) {
        return false;
    }

    u8 encoding =
        file_data[
            info_off + 8
        ];

    u32 sample_rate =
        read_u32le(
            file_data +
            info_off + 0x0C
        );

    u32 loop_start =
        read_u32le(
            file_data +
            info_off + 0x10
        );

    u32 sample_count =
        read_u32le(
            file_data +
            info_off + 0x14
        );

    u32 channel_count =
        read_u32le(
            file_data +
            info_off + 0x1C
        );

    if (encoding != 2 ||
        channel_count != 1 ||
        sample_rate < 8000 ||
        sample_rate > 96000 ||
        sample_count == 0) {
        return false;
    }

    u32 channel_table_base =
        info_off + 0x1C;

    if (!range_ok(
            file_bytes,
            channel_table_base,
            12)) {
        return false;
    }

    u32 channel_rel =
        read_u32le(
            file_data +
            channel_table_base +
            8
        );

    u64 channel_info64 =
        (u64)channel_table_base +
        (u64)channel_rel;

    if (channel_info64 >
        0xFFFFFFFFull) {
        return false;
    }

    u32 channel_info =
        (u32)channel_info64;

    if (!range_ok(
            file_bytes,
            channel_info,
            16)) {
        return false;
    }

    u32 sample_rel =
        read_u32le(
            file_data +
            channel_info + 4
        );

    u32 adpcm_rel =
        read_u32le(
            file_data +
            channel_info + 0x0C
        );

    u64 adpcm_info64 =
        (u64)channel_info +
        (u64)adpcm_rel;

    u64 sample_data64 =
        (u64)data_off +
        8ull +
        (u64)sample_rel;

    if (adpcm_info64 >
            0xFFFFFFFFull ||
        sample_data64 >
            0xFFFFFFFFull) {
        return false;
    }

    u32 adpcm_info =
        (u32)adpcm_info64;

    u32 sample_data =
        (u32)sample_data64;

    if (!range_ok(
            file_bytes,
            adpcm_info,
            0x2C)) {
        return false;
    }

    u32 adpcm_bytes =
        dspadpcm_bytes_for_samples(
            sample_count
        );

    if (!adpcm_bytes ||
        !range_ok(
            file_bytes,
            sample_data,
            adpcm_bytes)) {
        return false;
    }

    memset(
        out,
        0,
        sizeof(*out)
    );

    out->file_data =
        file_data;

    out->file_bytes =
        file_bytes;

    out->owns_file_data =
        owns_file_data;

    out->adpcm_data =
        file_data + sample_data;

    out->adpcm_bytes =
        adpcm_bytes;

    out->sample_rate =
        sample_rate;

    out->sample_count =
        sample_count;

    out->loop_start =
        loop_start;

    for (int i = 0;
         i < 16;
         ++i) {
        out->coefs[i] =
            read_s16le(
                file_data +
                adpcm_info +
                i * 2
            );
    }

    out->start_history0 =
        read_s16le(
            file_data +
            adpcm_info +
            0x22
        );

    out->start_history1 =
        read_s16le(
            file_data +
            adpcm_info +
            0x24
        );

    out->loop_history0 =
        read_s16le(
            file_data +
            adpcm_info +
            0x28
        );

    out->loop_history1 =
        read_s16le(
            file_data +
            adpcm_info +
            0x2A
        );

    out->source =
        source;

    out->valid = true;
    return true;
}

static bool load_sdmc_cwav(
    const char *name,
    CwavAsset *out) {

    if (!name || !out)
        return false;

    char path[160];

    int n = snprintf(
        path,
        sizeof(path),
        "/3ds/Bad3DSGame/%s",
        name
    );

    if (n <= 0 ||
        n >= (int)sizeof(path)) {
        return false;
    }

    Handle file = 0;

    FS_Path archive_path =
        fsMakePath(
            PATH_EMPTY,
            ""
        );

    FS_Path file_path =
        fsMakePath(
            PATH_ASCII,
            path
        );

    Result rc =
        FSUSER_OpenFileDirectly(
            &file,
            ARCHIVE_SDMC,
            archive_path,
            file_path,
            FS_OPEN_READ,
            0
        );

    if (R_FAILED(rc))
        return false;

    u64 size64 = 0;

    rc = FSFILE_GetSize(
        file,
        &size64
    );

    if (R_FAILED(rc) ||
        size64 == 0 ||
        size64 > 0xFFFFFFFFull) {
        FSFILE_Close(file);
        return false;
    }

    u32 size =
        (u32)size64;

    u8 *copy =
        (u8 *)malloc(size);

    if (!copy) {
        FSFILE_Close(file);
        return false;
    }

    u32 got = 0;

    rc = FSFILE_Read(
        file,
        &got,
        0,
        copy,
        size
    );

    FSFILE_Close(file);

    if (R_FAILED(rc) ||
        got != size) {
        free(copy);
        return false;
    }

    if (!parse_cwav_memory(
            out,
            copy,
            size,
            AUDIO_SOURCE_SDMC,
            true)) {
        free(copy);
        return false;
    }

    return true;
}

static bool load_asset(
    const LinkedAsset *linked,
    CwavAsset *out) {

    if (!linked || !out)
        return false;

    size_t linked_size = 0;

    if (linked->data &&
        linked->end &&
        linked->end > linked->data) {
        linked_size =
            (size_t)(
                linked->end -
                linked->data
            );
    }

    if (linked_size > 0 &&
        linked_size <= 0xFFFFFFFFu &&
        parse_cwav_memory(
            out,
            linked->data,
            (u32)linked_size,
            AUDIO_SOURCE_LINKED,
            false)) {
        return true;
    }

    return load_sdmc_cwav(
        linked->name,
        out
    );
}

static void free_asset(
    CwavAsset *asset) {

    if (!asset)
        return;

    if (asset->owns_file_data &&
        asset->file_data) {
        free(
            (void *)asset->file_data
        );
    }

    memset(
        asset,
        0,
        sizeof(*asset)
    );
}

/* =========================================================
   Software Nintendo GC/DSP-ADPCM decoder
   ========================================================= */

static void decoder_reset(
    DspDecoder *decoder,
    const CwavAsset *asset) {

    memset(
        decoder,
        0,
        sizeof(*decoder)
    );

    decoder->asset =
        asset;

    decoder->sample_in_frame =
        14;

    decoder->yn1 =
        asset->start_history0;

    decoder->yn2 =
        asset->start_history1;
}

static bool decoder_load_frame(
    DspDecoder *decoder) {

    const CwavAsset *asset =
        decoder->asset;

    if (!asset ||
        decoder->frame_index >=
            (asset->adpcm_bytes /
             8u)) {
        return false;
    }

    const u8 *frame =
        asset->adpcm_data +
        decoder->frame_index *
        8u;

    int header =
        frame[0];

    int predictor =
        (header >> 4) &
        0x07;

    int shift =
        header &
        0x0F;

    decoder->scale =
        1 << shift;

    decoder->coef1 =
        asset->coefs[
            predictor * 2
        ];

    decoder->coef2 =
        asset->coefs[
            predictor * 2 + 1
        ];

    decoder->sample_in_frame = 0;
    return true;
}

static bool decoder_next(
    DspDecoder *decoder,
    s16 *out_sample) {

    if (!decoder ||
        !out_sample ||
        !decoder->asset ||
        !decoder->asset->valid) {
        return false;
    }

    const CwavAsset *asset =
        decoder->asset;

    if (decoder->sample_pos >=
        asset->sample_count) {
        decoder_reset(
            decoder,
            asset
        );
    }

    if (decoder->sample_in_frame >=
        14) {
        if (!decoder_load_frame(
                decoder)) {
            return false;
        }
    }

    const u8 *frame =
        asset->adpcm_data +
        decoder->frame_index *
        8u;

    u8 packed =
        frame[
            1u +
            decoder->sample_in_frame /
            2u
        ];

    int nibble =
        ((decoder->sample_in_frame &
          1u) == 0)
        ? ((packed >> 4) & 0x0F)
        : (packed & 0x0F);

    if (nibble >= 8)
        nibble -= 16;

    int xn =
        nibble *
        decoder->scale;

    int64_t accum =
        (int64_t)xn *
            2048ll +
        0x400ll +
        (int64_t)decoder->coef1 *
            decoder->yn1 +
        (int64_t)decoder->coef2 *
            decoder->yn2;

    int value =
        (int)(accum >> 11);

    value =
        clamp_s16_int(value);

    decoder->yn2 =
        decoder->yn1;

    decoder->yn1 =
        value;

    *out_sample =
        (s16)value;

    ++decoder->sample_pos;
    ++decoder->sample_in_frame;

    if (decoder->sample_in_frame >=
        14) {
        ++decoder->frame_index;
    }

    return true;
}

static bool decode_entire_asset(
    const CwavAsset *asset,
    s16 *output,
    u32 output_samples) {

    if (!asset ||
        !asset->valid ||
        !output ||
        output_samples <
            asset->sample_count) {
        return false;
    }

    DspDecoder decoder;

    decoder_reset(
        &decoder,
        asset
    );

    for (u32 i = 0;
         i < asset->sample_count;
         ++i) {
        if (!decoder_next(
                &decoder,
                &output[i])) {
            return false;
        }
    }

    return true;
}

/* =========================================================
   NDSP PCM16 playback
   ========================================================= */

static void configure_pcm_channel(
    int channel,
    u32 sample_rate,
    float volume) {

    ndspChnReset(channel);

    ndspChnSetInterp(
        channel,
        NDSP_INTERP_LINEAR
    );

    ndspChnSetRate(
        channel,
        (float)sample_rate
    );

    ndspChnSetFormat(
        channel,
        NDSP_FORMAT_MONO_PCM16
    );

    float mix[12];

    memset(
        mix,
        0,
        sizeof(mix)
    );

    mix[0] = volume;
    mix[1] = volume;

    ndspChnSetMix(
        channel,
        mix
    );
}

static bool fill_music_buffer(
    int index) {

    if (index < 0 ||
        index >= MUSIC_BUFFER_COUNT ||
        !s_audio.music_pcm ||
        !s_audio.music_asset.valid) {
        return false;
    }

    s16 *dst =
        s_audio.music_pcm +
        index *
        MUSIC_PCM_SAMPLES;

    for (u32 i = 0;
         i < MUSIC_PCM_SAMPLES;
         ++i) {
        if (!decoder_next(
                &s_audio.music_decoder,
                &dst[i])) {
            return false;
        }
    }

    if (R_FAILED(
            DSP_FlushDataCache(
                dst,
                MUSIC_PCM_SAMPLES *
                sizeof(s16)))) {
        return false;
    }

    ndspWaveBuf *wave =
        &s_audio.music_wave[index];

    memset(
        wave,
        0,
        sizeof(*wave)
    );

    wave->data_pcm16 =
        dst;

    wave->nsamples =
        MUSIC_PCM_SAMPLES;

    wave->looping = false;

    return true;
}

static bool start_music(void) {
    if (!s_audio.ndsp_ready ||
        !s_audio.music_asset.valid ||
        !s_audio.music_pcm) {
        return false;
    }

    ndspChnWaveBufClear(
        MUSIC_CHANNEL
    );

    configure_pcm_channel(
        MUSIC_CHANNEL,
        s_audio.music_asset.sample_rate,
        MUSIC_VOLUME
    );

    decoder_reset(
        &s_audio.music_decoder,
        &s_audio.music_asset
    );

    memset(
        s_audio.music_wave,
        0,
        sizeof(s_audio.music_wave)
    );

    for (int i = 0;
         i < MUSIC_BUFFER_COUNT;
         ++i) {
        if (!fill_music_buffer(i)) {
            ndspChnWaveBufClear(
                MUSIC_CHANNEL
            );

            s_audio.playing = false;
            return false;
        }

        ndspChnWaveBufAdd(
            MUSIC_CHANNEL,
            &s_audio.music_wave[i]
        );
    }

    s_audio.playing = true;
    return true;
}

static bool prepare_sfx(AudioSfx id) {
    if (id < 0 ||
        id >= AUDIO_SFX_COUNT) {
        return false;
    }

    CwavAsset *asset =
        &s_audio.sfx_asset[id];

    if (!asset->valid ||
        asset->sample_count == 0) {
        return false;
    }

    u64 bytes64 =
        (u64)asset->sample_count *
        sizeof(s16);

    if (bytes64 >
        0xFFFFFFFFull) {
        return false;
    }

    SfxSample *s =
        &s_audio.sfx[id];

    memset(
        s,
        0,
        sizeof(*s)
    );

    s->pcm =
        (s16 *)linearAlloc(
            (u32)bytes64
        );

    if (!s->pcm)
        return false;

    if (!decode_entire_asset(
            asset,
            s->pcm,
            asset->sample_count)) {
        linearFree(s->pcm);
        memset(s, 0, sizeof(*s));
        return false;
    }

    if (R_FAILED(
            DSP_FlushDataCache(
                s->pcm,
                (u32)bytes64))) {
        linearFree(s->pcm);
        memset(s, 0, sizeof(*s));
        return false;
    }

    s->nsamples =
        asset->sample_count;

    s->sample_rate =
        asset->sample_rate;

    s->volume =
        SFX_VOLUMES[id];

    s->valid = true;

    return true;
}

static uint32_t coin_rng_next(void) {
    uint32_t x =
        s_audio.coin_rng;

    if (x == 0)
        x = 0xA341316Cu;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    s_audio.coin_rng = x;
    return x;
}

/* =========================================================
   Public API
   ========================================================= */

bool audio_init(void) {
    memset(
        &s_audio,
        0,
        sizeof(s_audio)
    );

    s_audio.status =
        "AUDIO: INIT PCM";

    s_audio.coin_rng =
        (uint32_t)osGetTime() ^
        0xC01C01u;

    Result rc =
        ndspInit();

    if (R_FAILED(rc)) {
        s_audio.status =
            "AUDIO: NDSP REQUIRED";
        return false;
    }

    s_audio.ndsp_ready = true;
    s_audio.backend =
        AUDIO_BACKEND_NDSP;

    ndspSetOutputMode(
        NDSP_OUTPUT_STEREO
    );

    if (!load_asset(
            &LINKED_MUSIC,
            &s_audio.music_asset)) {
        s_audio.status =
            "AUDIO: MUSIC CWAV BAD";
        return false;
    }

    for (int i = 0;
         i < AUDIO_SFX_COUNT;
         ++i) {
        load_asset(
            &LINKED_SFX[i],
            &s_audio.sfx_asset[i]
        );
    }

    s_audio.music_pcm =
        (s16 *)linearAlloc(
            MUSIC_BUFFER_COUNT *
            MUSIC_PCM_SAMPLES *
            sizeof(s16)
        );

    if (!s_audio.music_pcm) {
        s_audio.status =
            "AUDIO: PCM ALLOC FAIL";
        return false;
    }

    memset(
        s_audio.music_pcm,
        0,
        MUSIC_BUFFER_COUNT *
        MUSIC_PCM_SAMPLES *
        sizeof(s16)
    );

    for (int i = 0;
         i < AUDIO_SFX_COUNT;
         ++i) {
        prepare_sfx(
            (AudioSfx)i
        );
    }

    s_audio.music_source =
        s_audio.music_asset.source;

    s_audio.available = true;

    if (s_music_enabled) {
        if (!start_music()) {
            s_audio.status =
                "AUDIO: PCM QUEUE FAIL";
            s_audio.available = false;
            return false;
        }
    } else {
        s_audio.playing = false;
    }

    s_audio.status =
        s_audio.music_source ==
            AUDIO_SOURCE_LINKED
        ? "AUDIO: PCM LINKED"
        : "AUDIO: PCM SDMC";

    return true;
}

void audio_update(void) {
    if (!s_music_enabled ||
        !s_audio.available ||
        !s_audio.ndsp_ready ||
        !s_audio.playing) {
        return;
    }

    for (int i = 0;
         i < MUSIC_BUFFER_COUNT;
         ++i) {

        ndspWaveBuf *wave =
            &s_audio.music_wave[i];

        if (wave->status !=
            NDSP_WBUF_DONE) {
            continue;
        }

        if (!fill_music_buffer(i)) {
            audio_stop_music();
            s_audio.status =
                "AUDIO: PCM REFILL FAIL";
            return;
        }

        ndspChnWaveBufAdd(
            MUSIC_CHANNEL,
            wave
        );
    }
}

void audio_restart_music(void) {
    if (!s_music_enabled ||
        !s_audio.available ||
        !s_audio.ndsp_ready) {
        return;
    }

    start_music();
}

void audio_stop_music(void) {
    if (!s_audio.ndsp_ready)
        return;

    ndspChnWaveBufClear(
        MUSIC_CHANNEL
    );

    s_audio.playing = false;
}

void audio_play_sfx(AudioSfx id) {
    if (!s_sfx_enabled ||
        !s_audio.available ||
        !s_audio.ndsp_ready ||
        id < 0 ||
        id >= AUDIO_SFX_COUNT) {
        return;
    }

    SfxSample *s =
        &s_audio.sfx[id];

    if (!s->valid ||
        !s->pcm ||
        s->nsamples == 0) {
        return;
    }

    int channel =
        SFX_CHANNEL_BASE +
        (int)id;

    ndspChnWaveBufClear(
        channel
    );

    configure_pcm_channel(
        channel,
        s->sample_rate,
        s->volume
    );

    memset(
        &s->wave,
        0,
        sizeof(s->wave)
    );

    s->wave.data_pcm16 =
        s->pcm;

    s->wave.nsamples =
        s->nsamples;

    s->wave.looping = false;

    ndspChnWaveBufAdd(
        channel,
        &s->wave
    );
}

void audio_play_menu_move(void) {
    audio_play_sfx(
        AUDIO_SFX_COIN1
    );
}

void audio_play_purchase(void) {
    audio_play_sfx(
        AUDIO_SFX_COIN
    );
}

void audio_play_money_gain(void) {
    AudioSfx sfx =
        (coin_rng_next() & 1u)
        ? AUDIO_SFX_COIN
        : AUDIO_SFX_COIN1;

    audio_play_sfx(sfx);
}

void audio_set_sfx_enabled(bool enabled) {
    s_sfx_enabled = enabled;
}

void audio_set_music_enabled(bool enabled) {
    if (s_music_enabled == enabled)
        return;

    s_music_enabled = enabled;

    if (!s_audio.ndsp_ready ||
        !s_audio.available) {
        return;
    }

    if (enabled) {
        if (!s_audio.playing)
            start_music();
    } else {
        audio_stop_music();
    }
}

bool audio_sfx_enabled(void) {
    return s_sfx_enabled;
}

bool audio_music_enabled(void) {
    return s_music_enabled;
}

bool audio_music_available(void) {
    return s_audio.available;
}

AudioBackend audio_backend(void) {
    return s_audio.backend;
}

AudioSource audio_music_source(void) {
    return s_audio.music_source;
}

const char *audio_status_text(void) {
    return s_audio.status
        ? s_audio.status
        : "AUDIO: NOT INIT";
}

void audio_shutdown(void) {
    if (s_audio.ndsp_ready) {
        ndspChnWaveBufClear(
            MUSIC_CHANNEL
        );

        for (int i = 0;
             i < AUDIO_SFX_COUNT;
             ++i) {
            ndspChnWaveBufClear(
                SFX_CHANNEL_BASE + i
            );
        }
    }

    if (s_audio.music_pcm)
        linearFree(
            s_audio.music_pcm
        );

    for (int i = 0;
         i < AUDIO_SFX_COUNT;
         ++i) {
        if (s_audio.sfx[i].pcm)
            linearFree(
                s_audio.sfx[i].pcm
            );

        free_asset(
            &s_audio.sfx_asset[i]
        );
    }

    free_asset(
        &s_audio.music_asset
    );

    if (s_audio.ndsp_ready)
        ndspExit();

    memset(
        &s_audio,
        0,
        sizeof(s_audio)
    );
}
