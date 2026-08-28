static uint32_t rol32(uint32_t v, unsigned n) {
    n &= 31u;
    return (v << n) | (v >> ((32u - n) & 31u));
}

static uint32_t ror32(uint32_t v, unsigned n) {
    n &= 31u;
    return (v >> n) | (v << ((32u - n) & 31u));
}

static uint64_t rol64(uint64_t v, unsigned n) {
    n &= 63u;
    return (v << n) | (v >> ((64u - n) & 63u));
}

static uint64_t ror64(uint64_t v, unsigned n) {
    n &= 63u;
    return (v >> n) | (v << ((64u - n) & 63u));
}

static uint8_t rol8(uint8_t v, unsigned n) {
    n &= 7u;
    return (uint8_t)((v << n) | (v >> ((8u - n) & 7u)));
}

static uint8_t ror8(uint8_t v, unsigned n) {
    n &= 7u;
    return (uint8_t)((v >> n) | (v << ((8u - n) & 7u)));
}



static uint8_t rot13_byte(uint8_t b) {
    if (b >= (uint8_t)'A' && b <= (uint8_t)'Z')
        return (uint8_t)('A' + ((b - (uint8_t)'A' + 13u) % 26u));
    if (b >= (uint8_t)'a' && b <= (uint8_t)'z')
        return (uint8_t)('a' + ((b - (uint8_t)'a' + 13u) % 26u));
    return b;
}

static void put_u16le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 0);
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 0);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_u64le(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; ++i)
        p[i] = (uint8_t)(v >> (i * 8));
}

static uint16_t get_u16le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32le(const uint8_t *p) {
    return ((uint32_t)p[0] << 0) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t get_u64le(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= (uint64_t)p[i] << (i * 8);
    return v;
}

static uint64_t interleave_u32(uint32_t a, uint32_t b) {
    uint64_t v = 0;
    for (unsigned i = 0; i < 32; ++i) {
        v |= ((uint64_t)((a >> i) & 1u)) << (i * 2u);
        v |= ((uint64_t)((b >> i) & 1u)) << (i * 2u + 1u);
    }
    return v;
}

static uint32_t deinterleave_even(uint64_t v) {
    uint32_t out = 0;
    for (unsigned i = 0; i < 32; ++i)
        out |= (uint32_t)((v >> (i * 2u)) & 1u) << i;
    return out;
}

static uint32_t deinterleave_odd(uint64_t v) {
    uint32_t out = 0;
    for (unsigned i = 0; i < 32; ++i)
        out |= (uint32_t)((v >> (i * 2u + 1u)) & 1u) << i;
    return out;
}

static uint32_t pack_levels(const Progress *p) {
    uint32_t packed = 0;
    for (int i = 0; i < SHOP_ITEM_COUNT; ++i)
        packed |= ((uint32_t)p->levels[i] & 0xFu) << (i * 4);
    return packed;
}

static void unpack_levels(Progress *p, uint32_t packed) {
    for (int i = 0; i < SHOP_ITEM_COUNT; ++i) {
        uint8_t level = (uint8_t)((packed >> (i * 4)) & 0xFu);
        if (level > (uint8_t)SHOP[i].max_level)
            level = (uint8_t)SHOP[i].max_level;
        p->levels[i] = level;
    }
}

static uint32_t pack_settings_legacy(const Progress *p) {
    uint8_t display_byte =
        (uint8_t)((p->force_lod & 0x0Fu) |
                  ((p->fps_limit & 0x0Fu) << 4));

    return ((uint32_t)p->settings_flags << 0) |
           ((uint32_t)p->bloom_level_legacy << 8) |
           ((uint32_t)display_byte << 16) |
           ((uint32_t)p->settings_reserved << 24);
}

static void migrate_legacy_bloom(Progress *p, uint8_t old_level) {
    p->bloom_level_legacy = old_level;

    switch (old_level) {
        case 0:
            p->bloom_enabled = 0;
            p->bloom_radius = SETTINGS_BLOOM_RADIUS_DEFAULT;
            p->bloom_intensity = SETTINGS_BLOOM_INTENSITY_DEFAULT;
            p->bloom_quads = SETTINGS_BLOOM_QUADS_DEFAULT;
            break;
        case 1:
            p->bloom_enabled = 1;
            p->bloom_radius = 1;
            p->bloom_intensity = 4;
            p->bloom_quads = 1;
            break;
        case 2:
            p->bloom_enabled = 1;
            p->bloom_radius = 2;
            p->bloom_intensity = 5;
            p->bloom_quads = 1;
            break;
        case 4:
            p->bloom_enabled = 1;
            p->bloom_radius = 2;
            p->bloom_intensity = 9;
            p->bloom_quads = 3;
            break;
        case 3:
        default:
            p->bloom_enabled = 1;
            p->bloom_radius = SETTINGS_BLOOM_RADIUS_DEFAULT;
            p->bloom_intensity = SETTINGS_BLOOM_INTENSITY_DEFAULT;
            p->bloom_quads = SETTINGS_BLOOM_QUADS_DEFAULT;
            break;
    }
}

static uint32_t pack_settings(const Progress *p) {
    






    uint8_t bloom_byte =
        (uint8_t)((p->bloom_radius & 0x0Fu) |
                  ((p->bloom_intensity & 0x0Fu) << 4));

    uint8_t display_byte =
        (uint8_t)((p->force_lod & 0x03u) |
                  ((p->fps_limit & 0x07u) << 2) |
                  ((p->bloom_enabled ? 1u : 0u) << 5) |
                  (((p->bloom_quads - 1u) & 0x03u) << 6));

    return ((uint32_t)p->settings_flags << 0) |
           ((uint32_t)bloom_byte << 8) |
           ((uint32_t)display_byte << 16) |
           ((uint32_t)p->settings_reserved << 24);
}

static void unpack_settings_legacy(Progress *p, uint32_t packed) {
    p->settings_flags = (uint8_t)((packed >> 0) & 0xFFu);

    uint8_t old_level = (uint8_t)((packed >> 8) & 0xFFu);
    uint8_t display_byte = (uint8_t)((packed >> 16) & 0xFFu);

    p->force_lod = (uint8_t)(display_byte & 0x0Fu);
    p->fps_limit = (uint8_t)((display_byte >> 4) & 0x0Fu);
    p->settings_reserved = (uint8_t)((packed >> 24) & 0xFFu);

    migrate_legacy_bloom(p, old_level);
}

static void unpack_settings(Progress *p, uint32_t packed) {
    p->settings_flags = (uint8_t)((packed >> 0) & 0xFFu);

    uint8_t bloom_byte = (uint8_t)((packed >> 8) & 0xFFu);
    uint8_t display_byte = (uint8_t)((packed >> 16) & 0xFFu);

    p->bloom_radius = (uint8_t)(bloom_byte & 0x0Fu);
    p->bloom_intensity = (uint8_t)((bloom_byte >> 4) & 0x0Fu);
    p->force_lod = (uint8_t)(display_byte & 0x03u);
    p->fps_limit = (uint8_t)((display_byte >> 2) & 0x07u);
    p->bloom_enabled = (uint8_t)((display_byte >> 5) & 0x01u);
    p->bloom_quads = (uint8_t)(((display_byte >> 6) & 0x03u) + 1u);
    p->bloom_level_legacy = 3;
    p->settings_reserved = (uint8_t)((packed >> 24) & 0xFFu);
}


static uint32_t checksum_u32(uint32_t h, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        h ^= (uint8_t)(v >> (i * 8));
        h *= 16777619u;
    }
    return h;
}

static uint32_t progress_checksum_version(const Progress *p,
                                          uint32_t salt,
                                          uint16_t version) {
    uint32_t h = 2166136261u ^ salt ^ 0x91E10DA5u;

    h = checksum_u32(h, p->money);
    h = checksum_u32(h, p->high_score);

    if (version >= SAVE_VERSION_V7)
        h = checksum_u32(h, p->best_distance);

    if (version >= SAVE_VERSION_V8) {
        h = checksum_u32(h, p->xp);
        h = checksum_u32(h, p->player_level);

        
        h = checksum_u32(
            h,
            (uint32_t)(
                p->cosmetic_owned &
                0xFFFFFFFFull
            )
        );

        h = checksum_u32(h, p->lifetime_destroyed);

        uint32_t equipped =
            ((uint32_t)p->player_style << 0) |
            ((uint32_t)p->rope_style << 8) |
            ((uint32_t)p->pattern_style << 16);

        h = checksum_u32(h, equipped);
    }

    if (version >= SAVE_VERSION_V9) {
        h = checksum_u32(
            h,
            version >= SAVE_VERSION
                ? pack_settings(p)
                : pack_settings_legacy(p)
        );
    }

    if (version >= SAVE_VERSION_V10) {
        h = checksum_u32(
            h,
            (uint32_t)(
                p->cosmetic_owned >>
                32
            )
        );

        h = checksum_u32(h, p->best_speed);
        h = checksum_u32(h, p->best_combo);
        h = checksum_u32(h, p->missions_claimed);

        uint32_t equip2 =
            ((uint32_t)p->shape_style << 0) |
            ((uint32_t)p->player_anim_style << 8) |
            ((uint32_t)p->hat_style << 16) |
            ((uint32_t)p->block_theme << 24);

        uint32_t equip3 =
            ((uint32_t)p->background_style << 0) |
            ((uint32_t)p->lava_color_style << 8) |
            ((uint32_t)p->lava_anim_style << 16) |
            ((uint32_t)p->reserved_style1 << 24);

        h = checksum_u32(h, equip2);
        h = checksum_u32(h, equip3);
    }

    if (version >= SAVE_VERSION_V11) {
        h = checksum_u32(
            h,
            (uint32_t)(p->cosmetic_owned2 & 0xFFFFFFFFull)
        );

        h = checksum_u32(
            h,
            (uint32_t)(p->cosmetic_owned2 >> 32)
        );

        h = checksum_u32(
            h,
            (uint32_t)p->title_style
        );
    }

    if (version >= SAVE_VERSION_V12)
        h = checksum_u32(h, p->total_distance_traveled);

    for (int i = 0; i < SHOP_ITEM_COUNT; ++i) {
        h ^= p->levels[i];
        h *= 16777619u;
    }

    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

static uint32_t progress_checksum(const Progress *p, uint32_t salt) {
    return progress_checksum_version(p, salt, SAVE_VERSION);
}

static uint32_t save_stream_step(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void obfuscate_blob(uint8_t blob[SAVE_BYTES], unsigned bytes) {
    uint32_t stream = 0xC13FA9A9u;

    for (unsigned i = 0; i < bytes; ++i) {
        uint8_t key = (uint8_t)(save_stream_step(&stream) >> 24);
        uint8_t b = (uint8_t)(blob[i] ^ key ^ (uint8_t)(i * 37u + 0x5Bu));
        b = rol8(b, (i % 7u) + 1u);
        blob[i] = rot13_byte(b);
    }
}

static void deobfuscate_blob(uint8_t blob[SAVE_BYTES], unsigned bytes) {
    uint32_t stream = 0xC13FA9A9u;

    for (unsigned i = 0; i < bytes; ++i) {
        uint8_t key = (uint8_t)(save_stream_step(&stream) >> 24);
        uint8_t b = rot13_byte(blob[i]);
        b = ror8(b, (i % 7u) + 1u);
        blob[i] = (uint8_t)(b ^ key ^ (uint8_t)(i * 37u + 0x5Bu));
    }
}

static void encode_progress_blob(const Progress *p, uint8_t out[SAVE_BYTES]) {
    memset(out, 0, SAVE_BYTES);

    uint32_t owned_low =
        (uint32_t)(
            p->cosmetic_owned &
            0xFFFFFFFFull
        );

    uint32_t owned_high =
        (uint32_t)(
            p->cosmetic_owned >>
            32
        );

    uint32_t salt = (uint32_t)osGetTime();
    salt ^= p->money * 0x9E3779B9u;
    salt ^= rol32(p->high_score, 11);
    salt ^= rol32(p->best_distance, 19);
    salt ^= rol32(p->xp, 7);
    salt ^= rol32(p->player_level, 23);
    salt ^= owned_low * 0x85EBCA6Bu;
    salt ^= rol32(owned_high, 13);
    salt ^= rol32(p->missions_claimed, 5);
    salt ^= (uint32_t)(p->cosmetic_owned2 & 0xFFFFFFFFull) * 0xC2B2AE35u;
    salt ^= rol32((uint32_t)(p->cosmetic_owned2 >> 32), 9);
    salt ^= (uint32_t)p->title_style * 0x27D4EB2Fu;
    salt ^= rol32(p->total_distance_traveled, 3);
    salt ^= 0xA5D3C771u;

    if (salt == 0)
        salt = 0x6C8E9CF5u;

    uint64_t salt64 =
        ((uint64_t)salt << 32) |
        (uint64_t)(salt ^ 0x6D2B79F5u);

    uint64_t mixed =
        interleave_u32(
            p->high_score,
            p->money
        );

    mixed =
        rol64(
            mixed ^
            0xD6E8FEB86659FD93ULL ^
            salt64,
            13
        );

    uint32_t levels =
        rol32(
            pack_levels(p) ^
            salt ^
            0x51ED270Bu,
            13
        );

    uint32_t distance =
        rol32(
            p->best_distance ^
            salt ^
            0xC2B2AE35u,
            17
        );

    uint32_t equipped =
        ((uint32_t)p->player_style << 0) |
        ((uint32_t)p->rope_style << 8) |
        ((uint32_t)p->pattern_style << 16);

    uint32_t equip2 =
        ((uint32_t)p->shape_style << 0) |
        ((uint32_t)p->player_anim_style << 8) |
        ((uint32_t)p->hat_style << 16) |
        ((uint32_t)p->block_theme << 24);

    uint32_t equip3 =
        ((uint32_t)p->background_style << 0) |
        ((uint32_t)p->lava_color_style << 8) |
        ((uint32_t)p->lava_anim_style << 16);

    uint32_t xp_enc =
        rol32(
            p->xp ^
            salt ^
            0x27D4EB2Fu,
            9
        );

    uint32_t level_enc =
        rol32(
            p->player_level ^
            salt ^
            0x165667B1u,
            21
        );

    uint32_t owned_low_enc =
        rol32(
            owned_low ^
            salt ^
            0xD3A2646Cu,
            11
        );

    uint32_t equip_enc =
        rol32(
            equipped ^
            salt ^
            0x9E3779B9u,
            5
        );

    uint32_t blocks_enc =
        rol32(
            p->lifetime_destroyed ^
            salt ^
            0x7F4A7C15u,
            15
        );

    uint32_t settings_enc =
        rol32(
            pack_settings(p) ^
            salt ^
            0x4CF5AD43u,
            3
        );

    uint32_t owned_high_enc =
        rol32(
            owned_high ^
            salt ^
            0xB7E15163u,
            27
        );

    uint32_t speed_enc =
        rol32(
            p->best_speed ^
            salt ^
            0x1BF5A17Du,
            7
        );

    uint32_t combo_enc =
        rol32(
            p->best_combo ^
            salt ^
            0x94D049BBu,
            19
        );

    uint32_t missions_enc =
        rol32(
            p->missions_claimed ^
            salt ^
            0xDEADBEEFu,
            9
        );

    uint32_t equip2_enc =
        rol32(
            equip2 ^
            salt ^
            0x6A09E667u,
            11
        );

    uint32_t equip3_enc =
        rol32(
            equip3 ^
            salt ^
            0xBB67AE85u,
            17
        );

    uint32_t owned2_low =
        (uint32_t)(p->cosmetic_owned2 & 0xFFFFFFFFull);

    uint32_t owned2_high =
        (uint32_t)(p->cosmetic_owned2 >> 32);

    uint32_t owned2_low_enc =
        rol32(
            owned2_low ^
            salt ^
            0x3C6EF372u,
            13
        );

    uint32_t owned2_high_enc =
        rol32(
            owned2_high ^
            salt ^
            0xA54FF53Au,
            23
        );

    uint32_t title_enc =
        rol32(
            (uint32_t)p->title_style ^
            salt ^
            0x510E527Fu,
            3
        );

    uint32_t total_distance_enc =
        rol32(
            p->total_distance_traveled ^
            salt ^
            0x1F83D9ABu,
            29
        );

    uint32_t check =
        progress_checksum(
            p,
            salt
        );

    check =
        rol32(
            check ^
            salt ^
            0xB5297A4Du,
            7
        );

    put_u32le(out + 0, SAVE_MAGIC);
    put_u16le(out + 4, SAVE_VERSION);
    put_u16le(out + 6, (uint16_t)SAVE_BYTES);
    put_u32le(out + 8, salt);
    put_u64le(out + 12, mixed);
    put_u32le(out + 20, levels);
    put_u32le(out + 24, check);
    put_u32le(out + 28, distance);
    put_u32le(out + 32, xp_enc);
    put_u32le(out + 36, level_enc);
    put_u32le(out + 40, owned_low_enc);
    put_u32le(out + 44, equip_enc);
    put_u32le(out + 48, blocks_enc);
    put_u32le(out + 52, settings_enc);

    put_u32le(out + 56, owned_high_enc);
    put_u32le(out + 60, speed_enc);
    put_u32le(out + 64, combo_enc);
    put_u32le(out + 68, missions_enc);
    put_u32le(out + 72, equip2_enc);
    put_u32le(out + 76, equip3_enc);

    put_u32le(out + 80, owned2_low_enc);
    put_u32le(out + 84, owned2_high_enc);
    put_u32le(out + 88, title_enc);
    put_u32le(out + 92, total_distance_enc);

    uint32_t filler =
        salt ^
        0x243F6A88u;

    for (unsigned i = 96;
         i < SAVE_BYTES;
         ++i) {
        out[i] =
            (uint8_t)(
                save_stream_step(
                    &filler
                ) >>
                24
            );
    }

    obfuscate_blob(
        out,
        SAVE_BYTES
    );
}

static bool decode_progress_blob(Progress *p,
                                 const uint8_t input[SAVE_BYTES],
                                 unsigned file_bytes) {
    if (!p ||
        !input ||
        (file_bytes != SAVE_BYTES &&
         file_bytes != SAVE_LEGACY_BYTES)) {
        return false;
    }

    uint8_t blob[SAVE_BYTES];
    memset(blob, 0, sizeof(blob));
    memcpy(blob, input, file_bytes);

    deobfuscate_blob(
        blob,
        file_bytes
    );

    if (get_u32le(blob + 0) != SAVE_MAGIC)
        return false;

    uint16_t version =
        get_u16le(
            blob + 4
        );

    if (version != SAVE_VERSION &&
        version != SAVE_VERSION_V13 &&
        version != SAVE_VERSION_V12 &&
        version != SAVE_VERSION_V11 &&
        version != SAVE_VERSION_V10 &&
        version != SAVE_VERSION_V9 &&
        version != SAVE_VERSION_V8 &&
        version != SAVE_VERSION_V7 &&
        version != SAVE_VERSION_LEGACY) {
        return false;
    }

    uint16_t stored_bytes =
        get_u16le(
            blob + 6
        );

    unsigned expected_bytes =
        version >= SAVE_VERSION_V10
        ? SAVE_BYTES
        : SAVE_LEGACY_BYTES;

    if (stored_bytes != expected_bytes ||
        file_bytes != expected_bytes) {
        return false;
    }

    uint32_t salt =
        get_u32le(
            blob + 8
        );

    uint64_t salt64 =
        ((uint64_t)salt << 32) |
        (uint64_t)(salt ^ 0x6D2B79F5u);

    uint64_t mixed =
        get_u64le(
            blob + 12
        );

    mixed =
        ror64(
            mixed,
            13
        ) ^
        0xD6E8FEB86659FD93ULL ^
        salt64;

    Progress decoded;
    memset(
        &decoded,
        0,
        sizeof(decoded)
    );

    decoded.high_score =
        deinterleave_even(
            mixed
        );

    decoded.money =
        deinterleave_odd(
            mixed
        );

    uint32_t levels =
        get_u32le(
            blob + 20
        );

    levels =
        ror32(
            levels,
            13
        ) ^
        salt ^
        0x51ED270Bu;

    unpack_levels(
        &decoded,
        levels
    );

    if (version >= SAVE_VERSION_V7) {
        uint32_t distance =
            get_u32le(
                blob + 28
            );

        decoded.best_distance =
            ror32(
                distance,
                17
            ) ^
            salt ^
            0xC2B2AE35u;
    }

    if (version >= SAVE_VERSION_V8) {
        decoded.xp =
            ror32(
                get_u32le(blob + 32),
                9
            ) ^
            salt ^
            0x27D4EB2Fu;

        decoded.player_level =
            ror32(
                get_u32le(blob + 36),
                21
            ) ^
            salt ^
            0x165667B1u;

        uint32_t owned_low =
            ror32(
                get_u32le(blob + 40),
                11
            ) ^
            salt ^
            0xD3A2646Cu;

        decoded.cosmetic_owned =
            (uint64_t)owned_low;

        uint32_t equipped =
            ror32(
                get_u32le(blob + 44),
                5
            ) ^
            salt ^
            0x9E3779B9u;

        decoded.player_style =
            (uint8_t)(
                (equipped >> 0) &
                0xFFu
            );

        decoded.rope_style =
            (uint8_t)(
                (equipped >> 8) &
                0xFFu
            );

        decoded.pattern_style =
            (uint8_t)(
                (equipped >> 16) &
                0xFFu
            );

        decoded.lifetime_destroyed =
            ror32(
                get_u32le(blob + 48),
                15
            ) ^
            salt ^
            0x7F4A7C15u;
    } else {
        decoded.player_level = 1;
    }

    if (version >= SAVE_VERSION_V9) {
        uint32_t settings =
            ror32(
                get_u32le(blob + 52),
                3
            ) ^
            salt ^
            0x4CF5AD43u;

        if (version >= SAVE_VERSION)
            unpack_settings(&decoded, settings);
        else
            unpack_settings_legacy(&decoded, settings);
    } else {
        decoded.settings_flags = SETTINGS_DEFAULT_FLAGS;
        decoded.bloom_level_legacy = 3;
        decoded.bloom_enabled = SETTINGS_BLOOM_ENABLED_DEFAULT;
        decoded.bloom_radius = SETTINGS_BLOOM_RADIUS_DEFAULT;
        decoded.bloom_intensity = SETTINGS_BLOOM_INTENSITY_DEFAULT;
        decoded.bloom_quads = SETTINGS_BLOOM_QUADS_DEFAULT;
        decoded.force_lod = SETTINGS_FORCE_LOD_AUTO;
        decoded.fps_limit = SETTINGS_FPS_DEFAULT;
        decoded.settings_reserved = SETTINGS_VALID_MARKER;
    }

    if (version >= SAVE_VERSION_V10) {
        uint32_t owned_high =
            ror32(
                get_u32le(blob + 56),
                27
            ) ^
            salt ^
            0xB7E15163u;

        decoded.cosmetic_owned |=
            (uint64_t)owned_high <<
            32;

        decoded.best_speed =
            ror32(
                get_u32le(blob + 60),
                7
            ) ^
            salt ^
            0x1BF5A17Du;

        decoded.best_combo =
            ror32(
                get_u32le(blob + 64),
                19
            ) ^
            salt ^
            0x94D049BBu;

        decoded.missions_claimed =
            ror32(
                get_u32le(blob + 68),
                9
            ) ^
            salt ^
            0xDEADBEEFu;

        uint32_t equip2 =
            ror32(
                get_u32le(blob + 72),
                11
            ) ^
            salt ^
            0x6A09E667u;

        uint32_t equip3 =
            ror32(
                get_u32le(blob + 76),
                17
            ) ^
            salt ^
            0xBB67AE85u;

        decoded.shape_style =
            (uint8_t)((equip2 >> 0) & 0xFFu);

        decoded.player_anim_style =
            (uint8_t)((equip2 >> 8) & 0xFFu);

        decoded.hat_style =
            (uint8_t)((equip2 >> 16) & 0xFFu);

        decoded.block_theme =
            (uint8_t)((equip2 >> 24) & 0xFFu);

        decoded.background_style =
            (uint8_t)((equip3 >> 0) & 0xFFu);

        decoded.lava_color_style =
            (uint8_t)((equip3 >> 8) & 0xFFu);

        decoded.lava_anim_style =
            (uint8_t)((equip3 >> 16) & 0xFFu);

        decoded.reserved_style1 =
            (uint8_t)((equip3 >> 24) & 0xFFu);
    } else {
        decoded.reserved_style1 = 0;
    }

    if (version >= SAVE_VERSION_V11) {
        uint32_t owned2_low =
            ror32(
                get_u32le(blob + 80),
                13
            ) ^
            salt ^
            0x3C6EF372u;

        uint32_t owned2_high =
            ror32(
                get_u32le(blob + 84),
                23
            ) ^
            salt ^
            0xA54FF53Au;

        decoded.cosmetic_owned2 =
            (uint64_t)owned2_low |
            ((uint64_t)owned2_high << 32);

        decoded.title_style =
            (uint8_t)(
                ror32(
                    get_u32le(blob + 88),
                    3
                ) ^
                salt ^
                0x510E527Fu
            );
    } else {
        decoded.cosmetic_owned2 = 0;
        decoded.title_style = 0;
    }

    if (version >= SAVE_VERSION_V12) {
        decoded.total_distance_traveled =
            ror32(
                get_u32le(blob + 92),
                29
            ) ^
            salt ^
            0x1F83D9ABu;
    } else {
        decoded.total_distance_traveled = 0;
    }

    uint32_t stored_check =
        get_u32le(
            blob + 24
        );

    stored_check =
        ror32(
            stored_check,
            7
        ) ^
        salt ^
        0xB5297A4Du;

    uint32_t expected =
        progress_checksum_version(
            &decoded,
            salt,
            version
        );

    if (stored_check != expected)
        return false;

    *p = decoded;
    return true;
}

static bool save_progress_native(const Progress *progress) {
    if (!progress) return false;

    uint8_t blob[SAVE_BYTES];
    encode_progress_blob(progress, blob);

    Handle file = 0;
    FS_Path archive_path = fsMakePath(PATH_EMPTY, "");
    FS_Path file_path = fsMakePath(PATH_ASCII, SAVE_FS_PATH);

    Result rc = FSUSER_OpenFileDirectly(
        &file,
        ARCHIVE_SDMC,
        archive_path,
        file_path,
        FS_OPEN_WRITE | FS_OPEN_CREATE,
        0
    );

    if (R_FAILED(rc))
        return false;

    rc = FSFILE_SetSize(file, SAVE_BYTES);
    if (R_FAILED(rc)) {
        FSFILE_Close(file);
        return false;
    }

    u32 written = 0;
    rc = FSFILE_Write(
        file,
        &written,
        0,
        blob,
        SAVE_BYTES,
        FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME
    );

    Result close_rc = FSFILE_Close(file);

    return R_SUCCEEDED(rc) &&
           R_SUCCEEDED(close_rc) &&
           written == SAVE_BYTES;
}

static bool load_progress_native(Progress *progress) {
    if (!progress)
        return false;

    memset(
        progress,
        0,
        sizeof(*progress)
    );

    Handle file = 0;

    FS_Path archive_path =
        fsMakePath(
            PATH_EMPTY,
            ""
        );

    FS_Path file_path =
        fsMakePath(
            PATH_ASCII,
            SAVE_FS_PATH
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

    u64 file_size = 0;

    rc =
        FSFILE_GetSize(
            file,
            &file_size
        );

    if (R_FAILED(rc) ||
        (file_size != SAVE_BYTES &&
         file_size != SAVE_LEGACY_BYTES)) {
        FSFILE_Close(file);
        return false;
    }

    uint8_t blob[SAVE_BYTES];
    memset(blob, 0, sizeof(blob));

    u32 bytes_read = 0;

    rc =
        FSFILE_Read(
            file,
            &bytes_read,
            0,
            blob,
            (u32)file_size
        );

    Result close_rc =
        FSFILE_Close(
            file
        );

    if (R_FAILED(rc) ||
        R_FAILED(close_rc) ||
        bytes_read != (u32)file_size) {
        return false;
    }

    Progress decoded;
    memset(
        &decoded,
        0,
        sizeof(decoded)
    );

    if (!decode_progress_blob(
            &decoded,
            blob,
            (unsigned)file_size)) {
        return false;
    }

    *progress = decoded;
    return true;
}

static void save_progress(const Game *g) {
    if (!g) return;
    (void)save_progress_native(&g->progress);
}

static void load_progress(Game *g) {
    if (!g) return;

    memset(&g->progress, 0, sizeof(g->progress));
    (void)load_progress_native(&g->progress);
    ensure_progress_defaults(&g->progress);
}

static void apply_runtime_settings(Game *g) {
    if (!g)
        return;

    


    int fallback_bloom = 0;
    if (g->progress.bloom_enabled) {
        fallback_bloom =
            clampi(((int)g->progress.bloom_intensity + 1) / 3, 1, 4);
    }
    render_set_bloom_level(fallback_bloom);

    audio_set_sfx_enabled(
        setting_enabled(
            g,
            SETTING_SFX
        )
    );

    audio_set_music_enabled(
        setting_enabled(
            g,
            SETTING_MUSIC
        )
    );
}



static void ensure_progress_defaults(Progress *p) {
    if (!p)
        return;

    if (p->player_level == 0)
        p->player_level = 1;

    



    p->cosmetic_owned |=
        (1ull << 0)  |  
        (1ull << 4)  |  
        (1ull << 8)  |  
        (1ull << 18) |  
        (1ull << 23) |  
        (1ull << 28) |  
        (1ull << 33) |  
        (1ull << 38) |  
        (1ull << 43) |  
        (1ull << 49);   

    p->cosmetic_owned2 |=
        (1ull << (84 - 64)) | 
        (1ull << (98 - 64));   

    if (p->player_style > 8) p->player_style = 0;
    if (p->rope_style > 8) p->rope_style = 0;
    if (p->pattern_style > 7) p->pattern_style = 0;
    if (p->shape_style > 8) p->shape_style = 0;
    if (p->player_anim_style > 8) p->player_anim_style = 0;
    if (p->hat_style > 15) p->hat_style = 0;
    if (p->block_theme > 6) p->block_theme = 0;
    if (p->background_style > 8) p->background_style = 0;
    if (p->lava_color_style > 7) p->lava_color_style = 0;
    if (p->lava_anim_style > 6) p->lava_anim_style = 0;
    if (p->title_style > 6) p->title_style = 0;
    if (p->reserved_style1 > 3) p->reserved_style1 = 0;

    if (p->levels[UPG_BULLETS] > 9)
        p->levels[UPG_BULLETS] = 9;

    if (p->levels[UPG_ROPE] > 15)
        p->levels[UPG_ROPE] = 15;

    if (p->settings_reserved != SETTINGS_VALID_MARKER) {
        p->settings_flags = SETTINGS_DEFAULT_FLAGS;
        p->bloom_level_legacy = 3;
        p->bloom_enabled = SETTINGS_BLOOM_ENABLED_DEFAULT;
        p->bloom_radius = SETTINGS_BLOOM_RADIUS_DEFAULT;
        p->bloom_intensity = SETTINGS_BLOOM_INTENSITY_DEFAULT;
        p->bloom_quads = SETTINGS_BLOOM_QUADS_DEFAULT;
        p->force_lod = SETTINGS_FORCE_LOD_AUTO;
        p->fps_limit = SETTINGS_FPS_DEFAULT;
        p->settings_reserved = SETTINGS_VALID_MARKER;
    }

    if (p->bloom_enabled > 1u)
        p->bloom_enabled = SETTINGS_BLOOM_ENABLED_DEFAULT;

    if (p->bloom_radius < SETTINGS_BLOOM_RADIUS_MIN ||
        p->bloom_radius > SETTINGS_BLOOM_RADIUS_MAX)
        p->bloom_radius = SETTINGS_BLOOM_RADIUS_DEFAULT;

    if (p->bloom_intensity < SETTINGS_BLOOM_INTENSITY_MIN ||
        p->bloom_intensity > SETTINGS_BLOOM_INTENSITY_MAX)
        p->bloom_intensity = SETTINGS_BLOOM_INTENSITY_DEFAULT;

    if (p->bloom_quads < SETTINGS_BLOOM_QUADS_MIN ||
        p->bloom_quads > SETTINGS_BLOOM_QUADS_MAX)
        p->bloom_quads = SETTINGS_BLOOM_QUADS_DEFAULT;

    if (p->force_lod > SETTINGS_FORCE_LOD_MAX)
        p->force_lod = SETTINGS_FORCE_LOD_AUTO;

    if (p->fps_limit > SETTINGS_FPS_MAX)
        p->fps_limit = SETTINGS_FPS_DEFAULT;
}

