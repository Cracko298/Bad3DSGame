static void rope_clear(Rope *r) {
    r->active = false;
    r->latched = false;
    r->user_owned = false;
    r->target_block = -1;
    r->direction = vec2(0.0f, -1.0f);
    r->hook_pos = vec2(0.0f, 0.0f);
    r->length = 0.0f;
    r->desired_length = 0.0f;
}

static void add_popup(Game *g, Vec2 pos, int value, bool money, Color c) {
    for (int i = 0; i < MAX_POPUPS; ++i) {
        if (!g->popups[i].active) {
            g->popups[i].active = true;
            g->popups[i].pos = pos;
            g->popups[i].life = 0.85f;
            g->popups[i].value = value;
            g->popups[i].money = money;
            g->popups[i].color = c;
            return;
        }
    }
}

static void spawn_particles(Game *g, Vec2 pos, Color c, int count, float speed) {
    if (!g ||
        !setting_enabled(g, SETTING_PARTICLES)) {
        return;
    }

    for (int n = 0; n < count; ++n) {
        int slot = -1;
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            if (!g->particles[i].active) { slot = i; break; }
        }
        if (slot < 0) return;

        float a = frand_range(0.0f, 6.2831853f);
        float s = frand_range(speed * 0.25f, speed);
        Particle *p = &g->particles[slot];
        p->active = true;
        p->pos = pos;
        p->vel = vec2(cosf(a) * s, sinf(a) * s);
        p->life = frand_range(0.22f, 0.72f);
        p->color = c;
    }
}

static void spawn_bullet(Game *g, Vec2 pos, Vec2 dir, Color c, float speed) {
    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (!g->bullets[i].active) {
            g->bullets[i].active = true;
            g->bullets[i].pos = pos;
            g->bullets[i].vel = vmul(vnormalize(dir), speed);
            g->bullets[i].life = 3.0f;
            g->bullets[i].color = c;
            g->bullets[i].power = 1;
            return;
        }
    }
}

static void spawn_green_burst(Game *g, Vec2 pos) {
    const int count = 10;
    for (int i = 0; i < count; ++i) {
        float a = ((float)i / (float)count) * 6.2831853f;
        spawn_bullet(g, pos, vec2(cosf(a), sinf(a)), C_GREEN, 150.0f);
    }
}

static void setup_anchor(Block *b, float x, float y, float half) {
    memset(b, 0, sizeof(*b));
    b->active = true;
    b->type = BLOCK_ANCHOR;
    b->half = half;
    b->body.pos = vec2(x, y);
    b->body.vel = vec2(0.0f, 0.0f);
    b->body.inv_mass = 0.0f;
    b->body.damping = 0.0f;
    b->body.restitution = 0.75f;
    b->home = b->body.pos;
    b->base_half = half;
    b->anim_phase = frand_range(0.0f, 6.2831853f);
    b->angle = sinf(b->anim_phase) * 0.175f;
    b->angular_velocity = frand_range(0.85f, 1.45f);
    b->section_x = 0;
    b->section_y = 0;
    b->streamed = false;
}

static void ensure_start_anchor(Game *g) {
    



    
    setup_anchor(&g->blocks[0], 200.0f, 138.0f, 14.0f);
    g->blocks[0].value = 0;
    g->starter_platform_active = true;
}

static void setup_initial_target(Block *b, BlockType type,
                                 float x, float y, float half, int value) {
    setup_anchor(b, x, y, half);
    b->type = type;
    b->value = value;
}

static int world_to_section(float v, float section_size) {
    return (int)floorf(v / section_size);
}

static bool section_stamp_exists(const Game *g, int sx, int sy) {
    for (int i = 0; i < MAX_SECTION_STAMPS; ++i) {
        if (g->sections[i].valid &&
            g->sections[i].x == sx &&
            g->sections[i].y == sy)
            return true;
    }
    return false;
}

static int find_free_section_stamp(const Game *g) {
    for (int i = 0; i < MAX_SECTION_STAMPS; ++i)
        if (!g->sections[i].valid) return i;
    return -1;
}

static int find_free_stream_block(const Game *g) {
    for (int i = STREAM_BLOCK_START; i < MAX_BLOCKS; ++i)
        if (!g->blocks[i].active) return i;
    return -1;
}

static void clear_streamed_section(Game *g, int sx, int sy) {
    for (int i = STREAM_BLOCK_START; i < MAX_BLOCKS; ++i) {
        Block *b = &g->blocks[i];
        if (!b->streamed || b->section_x != sx || b->section_y != sy)
            continue;

        if (g->rope.target_block == i)
            rope_clear(&g->rope);

        b->active = false;
        b->streamed = false;
    }
}

static uint32_t map_mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static uint32_t section_seed(const Game *g, int sx, int sy) {
    uint32_t x = (uint32_t)sx * 0x9E3779B9u;
    uint32_t y = (uint32_t)sy * 0x85EBCA6Bu;
    return map_mix32(g->world_seed ^ x ^ rol32(y, 13) ^ 0xA511E9B3u);
}

static uint32_t section_rng_next(uint32_t *state) {
    uint32_t x = *state ? *state : 0x6D2B79F5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float section_rand01(uint32_t *state) {
    return (float)(section_rng_next(state) & 0x00FFFFFFu) / 16777216.0f;
}

static float section_rand_range(uint32_t *state, float lo, float hi) {
    return lo + (hi - lo) * section_rand01(state);
}

static BlockType random_section_block_type(const Game *g,
                                           int ordinal,
                                           uint32_t *rng) {
    if (ordinal == 0) return BLOCK_ANCHOR;
    if (ordinal == 1) return BLOCK_PURPLE;

    float roll = section_rand01(rng);
    if (roll < 0.29f) return BLOCK_ANCHOR;
    if (roll < 0.58f) return BLOCK_PURPLE;
    if (roll < 0.74f) return BLOCK_RED;
    if (roll < 0.88f) return BLOCK_MONEY;
    if (g->progress.levels[UPG_GREEN] > 0) return BLOCK_GREEN;
    return BLOCK_PURPLE;
}

static float section_distance_from_start(int sx, int sy) {
    float x = (float)sx * SECTION_W;
    float y = (float)sy * SECTION_H;
    return sqrtf(x * x + y * y);
}

static int section_block_count(int sx, int sy) {
    
    float d = section_distance_from_start(sx, sy);
    int count =
        d < 1200.0f ? 1 :
        d < 2800.0f ? 2 :
        d < 5200.0f ? 3 :
                      4;

    
    float height = fmaxf(0.0f, -((float)sy * SECTION_H));
    if (height >= 900.0f)
        --count;
    if (height >= 2100.0f)
        --count;

    if (count < 1)
        count = 1;

    return count;
}

static float section_clump_amount(int sx, int sy) {
    



    float d = section_distance_from_start(sx, sy);
    return clampf_local((d - 3600.0f) / 4200.0f, 0.0f, 0.55f);
}

static uint32_t block_anim_hash(const Game *g, int sx, int sy, int ordinal) {
    uint32_t x = (uint32_t)sx * 73856093u;
    uint32_t y = (uint32_t)sy * 19349663u;
    uint32_t o = (uint32_t)(ordinal + 1) * 83492791u;
    uint32_t h = x ^ y ^ o ^ g->world_seed ^ 0xA511E9B3u;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    return h;
}

static bool section_position_is_clear(const Game *g,
                                      int sx, int sy,
                                      Vec2 world_pos,
                                      float min_sep) {
    float min_sep2 = min_sep * min_sep;

    for (int i = STREAM_BLOCK_START; i < MAX_BLOCKS; ++i) {
        const Block *other = &g->blocks[i];

        if (!other->active || !other->streamed)
            continue;

        if (other->section_x != sx || other->section_y != sy)
            continue;

        Vec2 d = vsub(world_pos, other->body.pos);
        if (vdot(d, d) < min_sep2)
            return false;
    }

    return true;
}

static bool find_section_partner(const Game *g,
                                 int sx, int sy,
                                 Vec2 *out) {
    int matches[8];
    int count = 0;

    for (int i = STREAM_BLOCK_START;
         i < MAX_BLOCKS && count < (int)(sizeof(matches) / sizeof(matches[0]));
         ++i) {
        const Block *b = &g->blocks[i];

        if (!b->active || !b->streamed)
            continue;

        if (b->section_x == sx && b->section_y == sy)
            matches[count++] = i;
    }

    if (count <= 0)
        return false;

    int chosen = matches[rand() % count];
    *out = g->blocks[chosen].body.pos;
    return true;
}

static void setup_streamed_block(Game *g, int slot,
                                 int sx, int sy,
                                 int ordinal, int count,
                                 uint32_t *rng) {
    (void)count;

    Block *b = &g->blocks[slot];
    memset(b, 0, sizeof(*b));

    BlockType type = random_section_block_type(g, ordinal, rng);

    b->active = true;
    b->streamed = true;
    b->section_x = sx;
    b->section_y = sy;
    b->type = type;

    float half = 7.0f;
    int value = 80;

    switch (type) {
        case BLOCK_RED:
            half = section_rand_range(rng, 6.0f, 9.0f);
            value = 160;
            break;

        case BLOCK_PURPLE:
            half = section_rand_range(rng, 6.0f, 9.0f);
            value = 100;
            break;

        case BLOCK_GREEN:
            half = section_rand_range(rng, 6.0f, 8.0f);
            value = 175;
            break;

        case BLOCK_MONEY:
            half = 7.0f;
            value = 120;
            break;

        case BLOCK_ANCHOR:
        default:
            half = section_rand_range(rng, 6.0f, 9.0f);
            value = 80;
            break;
    }

    float d = section_distance_from_start(sx, sy);
    float clump = section_clump_amount(sx, sy);

    



    float min_sep =
        d < 1200.0f ? 125.0f :
        d < 2800.0f ? 100.0f :
        d < 5200.0f ? 78.0f :
                      60.0f;

    float height = fmaxf(0.0f, -((float)sy * SECTION_H));
    min_sep += clampf_local(height / 260.0f, 0.0f, 36.0f);

    float base_x = (float)sx * SECTION_W;
    float base_y = (float)sy * SECTION_H;

    Vec2 chosen = vec2(base_x + SECTION_W * 0.5f,
                       base_y + SECTION_H * 0.5f);

    bool placed = false;

    for (int attempt = 0; attempt < 28; ++attempt) {
        float local_x;
        float local_y;

        



        bool try_cluster =
            ordinal >= 1 &&
            clump > 0.05f &&
            section_rand01(rng) < clump * 0.55f;

        if (try_cluster) {
            Vec2 partner;

            if (find_section_partner(g, sx, sy, &partner)) {
                float a = section_rand_range(rng, 0.0f, 6.2831853f);
                float gap = section_rand_range(rng, 48.0f, 88.0f);

                local_x = (partner.x - base_x) + cosf(a) * gap;
                local_y = (partner.y - base_y) + sinf(a) * gap;
            } else {
                local_x = section_rand_range(rng, 32.0f, SECTION_W - 32.0f);
                local_y = section_rand_range(rng, 32.0f, SECTION_H - 32.0f);
            }
        } else {
            local_x = section_rand_range(rng, 32.0f, SECTION_W - 32.0f);
            local_y = section_rand_range(rng, 32.0f, SECTION_H - 32.0f);
        }

        local_x = clampf_local(local_x, 28.0f, SECTION_W - 28.0f);
        local_y = clampf_local(local_y, 28.0f, SECTION_H - 28.0f);

        Vec2 world_pos = vec2(base_x + local_x, base_y + local_y);

        if (section_position_is_clear(g, sx, sy, world_pos, min_sep)) {
            chosen = world_pos;
            placed = true;
            break;
        }
    }

    



    if (!placed) {
        chosen = vec2(
            base_x + section_rand_range(rng, 32.0f, SECTION_W - 32.0f),
            base_y + section_rand_range(rng, 32.0f, SECTION_H - 32.0f)
        );
    }

    b->half = half;
    b->base_half = half;
    b->value = value;
    b->body.pos = chosen;
    b->home = chosen;
    b->body.vel = vec2(0.0f, 0.0f);
    b->body.inv_mass = 0.0f;
    b->body.damping = 0.0f;
    b->body.restitution = 0.0f;

    uint32_t ah = block_anim_hash(g, sx, sy, ordinal);

    b->anim_phase =
        ((float)(ah % 6283u) / 1000.0f) +
        (float)ordinal * 0.41f;

    while (b->anim_phase >= 6.2831853f)
        b->anim_phase -= 6.2831853f;

    b->angular_velocity =
        0.85f + (float)((ah >> 8) % 61u) / 100.0f;

    b->angle = 0.0f;
}

static void generate_section(Game *g, int sx, int sy) {
    if (section_stamp_exists(g, sx, sy))
        return;

    int stamp = find_free_section_stamp(g);
    if (stamp < 0)
        return;

    g->sections[stamp].valid = true;
    g->sections[stamp].x = sx;
    g->sections[stamp].y = sy;

    



    if ((float)sy * SECTION_H >= LAVA_Y)
        return;

    
    if (sx == 0 && sy == 0)
        return;

    int count = section_block_count(sx, sy);
    uint32_t rng = section_seed(g, sx, sy);

    for (int n = 0; n < count; ++n) {
        int slot = find_free_stream_block(g);
        if (slot < 0)
            return;
        setup_streamed_block(g, slot, sx, sy, n, count, &rng);
    }
}

static bool section_needed(int sx, int sy,
                           int cx, int cy,
                           int radius_x, int radius_y) {
    return sx >= cx - radius_x &&
           sx <= cx + radius_x &&
           sy >= cy - radius_y &&
           sy <= cy + radius_y;
}

static void desired_section_radii(const Game *g,
                                  int *radius_x,
                                  int *radius_y) {
    float half_world_w = TOP_W * 0.5f * g->camera_zoom;
    float half_world_h = TOP_H * 0.5f * g->camera_zoom;

    int rx = (int)ceilf(half_world_w / SECTION_W);
    int ry = (int)ceilf(half_world_h / SECTION_H);

    



    if (g->camera_zoom > 1.80f) {
        ++rx;
        ++ry;
    }

    *radius_x = clampi(rx, 1, 3);
    *radius_y = clampi(ry, 1, 3);
}

static void refresh_section_cache(Game *g, bool force) {
    float view_center_x =
        g->camera_x + TOP_W * 0.5f * g->camera_zoom;
    float view_center_y =
        g->camera_y + TOP_H * 0.5f * g->camera_zoom;

    int cx = world_to_section(view_center_x, SECTION_W);
    int cy = world_to_section(view_center_y, SECTION_H);

    int radius_x;
    int radius_y;
    desired_section_radii(g, &radius_x, &radius_y);

    if (!force &&
        g->section_cache_ready &&
        cx == g->section_center_x &&
        cy == g->section_center_y &&
        radius_x == g->section_radius_x &&
        radius_y == g->section_radius_y)
        return;

    g->section_center_x = cx;
    g->section_center_y = cy;
    g->section_radius_x = radius_x;
    g->section_radius_y = radius_y;
    g->section_cache_ready = true;

    




    for (int i = 0; i < MAX_SECTION_STAMPS; ++i) {
        SectionStamp *s = &g->sections[i];
        if (!s->valid) continue;

        if (!section_needed(s->x, s->y,
                            cx, cy,
                            radius_x, radius_y)) {
            clear_streamed_section(g, s->x, s->y);
            s->valid = false;
        }
    }

    for (int sy = cy - radius_y; sy <= cy + radius_y; ++sy) {
        for (int sx = cx - radius_x; sx <= cx + radius_x; ++sx) {
            if (!section_stamp_exists(g, sx, sy))
                generate_section(g, sx, sy);
        }
    }
}


static void reset_player_to_anchor(Game *g) {
    ensure_start_anchor(g);

    g->player_half = g->progress.levels[UPG_THICC] ? 10.0f : 6.0f;

    
    g->player.pos = vec2(200.0f,
                         g->blocks[0].body.pos.y -
                         g->blocks[0].half -
                         g->player_half);

    g->player.vel = vec2(0.0f, 0.0f);
    g->player.inv_mass = 1.0f;
    g->player.damping = PLAYER_AIR_DRAG;
    g->player.restitution = 0.35f;
    g->player_angle = 0.0f;
    g->player_spin = 0.0f;
    g->player_stretch = 0.0f;

    rope_clear(&g->rope);
    g->aim_world = g->player.pos;
    g->aim_valid = false;
    g->last_distance_sample = g->player.pos;
}

static void start_run(Game *g) {
    memset(g->blocks, 0, sizeof(g->blocks));
    memset(g->particles, 0, sizeof(g->particles));
    memset(g->bullets, 0, sizeof(g->bullets));
    memset(g->popups, 0, sizeof(g->popups));

    g->score = 0;
    g->combo = 1;
    g->run_best_combo = 1;
    g->max_combo = 1 + (int)g->progress.levels[UPG_COMBO];
    g->destroyed = 0;
    g->combo_timer = 0.0f;
    g->spawn_timer = 0.35f;
    g->difficulty_time = 0.0f;
    g->bullet_timer = BULLET_TIMER_BASE_SECONDS;
    g->screen_shake = 0.0f;
    g->flash_timer = 0.0f;
    g->invuln_timer = 0.0f;

    g->max_health = 3 + (int)g->progress.levels[UPG_HP];
    g->health = g->max_health;
    g->player_half = g->progress.levels[UPG_THICC] ? 10.0f : 6.0f;

    g->camera_x = 0.0f;
    g->camera_y = 0.0f;
    g->camera_zoom = CAMERA_ZOOM_STARTING;
    g->highest_y = 118.0f;

    reset_player_to_anchor(g);

    



    g->camera_x =
        g->player.pos.x - TOP_W * 0.5f * g->camera_zoom;
    g->camera_y =
        g->player.pos.y - TOP_H * 0.5f * g->camera_zoom;

    



    g->run_start_pos = g->player.pos;
    g->run_distance = 0.0f;
    g->run_path_distance = 0.0f;
    g->last_distance_sample = g->player.pos;

    g->run_xp_earned = 0;
    g->run_cash_earned = 0;
    g->normal_cash_kills = 0;
    g->run_end_xp_bonus = 0;
    g->run_end_cash_bonus = 0;
    g->run_levelups = 0;
    g->last_level_reward = 0;
    g->levelup_message_timer = 0.0f;

    
    g->world_seed =
        map_mix32((uint32_t)osGetTime() ^
                  (uint32_t)rand() ^
                  (uint32_t)(g->frame_counter * 0x9E3779B9u));

    
    uint32_t opening_rng = map_mix32(g->world_seed ^ 0x51ED270Bu);

    float left_x =
        58.0f + section_rand01(&opening_rng) * 118.0f;
    float right_x =
        224.0f + section_rand01(&opening_rng) * 118.0f;

    float left_y =
        g->player.pos.y -
        (48.0f + section_rand01(&opening_rng) * 72.0f);

    float right_y =
        g->player.pos.y -
        (48.0f + section_rand01(&opening_rng) * 72.0f);

    left_y = clampf_local(left_y, -28.0f, 70.0f);
    right_y = clampf_local(right_y, -28.0f, 70.0f);

    BlockType first_type =
        section_rand01(&opening_rng) < 0.5f
        ? BLOCK_ANCHOR
        : BLOCK_PURPLE;

    BlockType second_type =
        first_type == BLOCK_ANCHOR
        ? BLOCK_PURPLE
        : BLOCK_ANCHOR;

    setup_initial_target(
        &g->blocks[1],
        first_type,
        left_x,
        left_y,
        section_rand_range(&opening_rng, 7.0f, 9.0f),
        first_type == BLOCK_PURPLE ? 100 : 80
    );

    setup_initial_target(
        &g->blocks[2],
        second_type,
        right_x,
        right_y,
        section_rand_range(&opening_rng, 7.0f, 9.0f),
        second_type == BLOCK_PURPLE ? 100 : 80
    );

    



    g->blocks[0].anim_phase =
        section_rand_range(&opening_rng, 0.0f, 6.2831853f);
    g->blocks[1].anim_phase =
        section_rand_range(&opening_rng, 0.0f, 6.2831853f);
    g->blocks[2].anim_phase =
        section_rand_range(&opening_rng, 0.0f, 6.2831853f);

    g->blocks[0].angular_velocity =
        section_rand_range(&opening_rng, 0.85f, 1.45f);
    g->blocks[1].angular_velocity =
        section_rand_range(&opening_rng, 0.85f, 1.45f);
    g->blocks[2].angular_velocity =
        section_rand_range(&opening_rng, 0.85f, 1.45f);

    memset(g->sections, 0, sizeof(g->sections));
    g->section_radius_x = 0;
    g->section_radius_y = 0;
    g->section_cache_ready = false;

    




    refresh_section_cache(g, true);

    g->mode = MODE_PLAYING;
}

static uint32_t current_run_distance(const Game *g) {
    if (!g)
        return 0;

    Vec2 delta =
        vsub(
            g->player.pos,
            g->run_start_pos
        );

    float distance =
        vlen(
            delta
        );

    if (!isfinite(distance) ||
        distance <= 0.0f) {
        return 0;
    }

    if (distance >= 4294967040.0f)
        return 0xFFFFFFFFu;

    return (uint32_t)(distance + 0.5f);
}








static uint32_t run_end_xp_base(const Game *g,
                                uint32_t distance) {
    if (!g)
        return 0;

    uint64_t score =
        g->score > 0
        ? (uint64_t)g->score
        : 0ull;

    uint64_t destroyed =
        g->destroyed > 0
        ? (uint64_t)g->destroyed
        : 0ull;

    uint64_t reward =
        (uint64_t)distance / 18ull +
        destroyed * 4ull +
        score / 120ull;

    if (reward > 2500000ull)
        reward = 2500000ull;

    return (uint32_t)reward;
}

static uint32_t run_end_cash_base(const Game *g,
                                  uint32_t distance) {
    if (!g)
        return 0;

    uint64_t score =
        g->score > 0
        ? (uint64_t)g->score
        : 0ull;

    uint64_t destroyed =
        g->destroyed > 0
        ? (uint64_t)g->destroyed
        : 0ull;

    uint64_t reward =
        (uint64_t)distance / 7ull +
        destroyed * 3ull +
        score / 80ull;

    if (reward > 5000000ull)
        reward = 5000000ull;

    return (uint32_t)reward;
}

static void grant_end_of_run_rewards(Game *g) {
    if (!g)
        return;

    uint32_t distance =
        current_run_distance(g);

    uint32_t xp_base =
        run_end_xp_base(
            g,
            distance
        );

    uint32_t cash_base =
        run_end_cash_base(
            g,
            distance
        );

    uint32_t xp_bonus =
        xp_base
        ? randomized_reward(
              xp_base,
              92,
              108
          )
        : 0u;

    uint32_t cash_bonus =
        cash_base
        ? randomized_reward(
              cash_base,
              90,
              115
          )
        : 0u;

    g->run_end_xp_bonus =
        xp_bonus;

    g->run_end_cash_bonus =
        cash_bonus;

    int levelups_before =
        g->run_levelups;

    if (xp_bonus) {
        grant_xp(
            g,
            xp_bonus,
            g->player.pos
        );
    }

    



    bool levelup_chimed =
        g->run_levelups >
        levelups_before;

    if (cash_bonus) {
        add_cash(
            g,
            cash_bonus,
            g->player.pos,
            false,
            !levelup_chimed
        );
    }
}

static void commit_run_records(Game *g) {
    if (!g) return;

    if ((uint32_t)g->score > g->progress.high_score)
        g->progress.high_score = (uint32_t)g->score;

    uint32_t distance = current_run_distance(g);
    if (distance > g->progress.best_distance)
        g->progress.best_distance = distance;

    if ((uint32_t)g->run_best_combo >
        g->progress.best_combo) {
        g->progress.best_combo =
            (uint32_t)g->run_best_combo;
    }
}

static void spawn_player_destroy_burst(Game *g) {
    Color pc = player_color(g);
    Color rc = rope_color(g);

    spawn_particles(g, g->player.pos, pc, 30, 180.0f);
    spawn_particles(g, g->player.pos, rc, 14, 145.0f);
    spawn_particles(g, g->player.pos, C_WHITE, 10, 110.0f);

    g->screen_shake = fmaxf(g->screen_shake, 0.36f);
    g->flash_timer = fmaxf(g->flash_timer, 0.16f);
}

static void set_game_over(Game *g) {
    audio_play_sfx(AUDIO_SFX_PLAYER_DIED);
    spawn_player_destroy_burst(g);

    grant_end_of_run_rewards(g);

    if (g->run_path_distance > 0.0f) {
        uint64_t total =
            (uint64_t)g->progress.total_distance_traveled +
            (uint64_t)(g->run_path_distance + 0.5f);

        if (total > 0xFFFFFFFFull)
            total = 0xFFFFFFFFull;

        g->progress.total_distance_traveled = (uint32_t)total;
    }

    commit_run_records(g);
    save_progress(g);
    rope_clear(&g->rope);
    g->gameover_index = 0;
    g->mode = MODE_GAMEOVER;
}

static void damage_player(Game *g, Vec2 source, bool force_respawn) {
    if (g->invuln_timer > 0.0f || g->mode != MODE_PLAYING) return;

    --g->health;
    g->combo = 1;
    g->combo_timer = 0.0f;
    g->screen_shake = 0.22f;
    g->flash_timer = 0.09f;
    g->invuln_timer = 0.95f;
    spawn_particles(g, g->player.pos, player_color(g), 18, 140.0f);
    spawn_particles(g, g->player.pos, C_RED, 8, 105.0f);

    if (g->health <= 0) {
        g->health = 0;
        set_game_over(g);
        return;
    }

    audio_play_sfx(AUDIO_SFX_PLAYER_HIT);

    if (force_respawn) {
        reset_player_to_anchor(g);
        g->invuln_timer = 1.15f;
    } else {
        Vec2 away = vnormalize(vsub(g->player.pos, source));
        if (vlen(away) < 0.01f) away = vec2(0.0f, -1.0f);
        g->player.vel = vadd(g->player.vel, vmul(away, 120.0f));
        if (g->rope.target_block >= 0) rope_clear(&g->rope);
    }
}


static uint32_t normal_kill_height_bonus(const Game *g) {
    if (!g)
        return 0;

    float height =
        g->run_start_pos.y -
        g->player.pos.y;

    if (!isfinite(height) ||
        height <= 0.0f) {
        return 0;
    }

    



    uint32_t bonus =
        (uint32_t)(
            height /
            500.0f
        );

    if (bonus > 12u)
        bonus = 12u;

    return bonus;
}

static uint32_t normal_kill_base_cash(const Game *g) {
    return 1u +
        normal_kill_height_bonus(g);
}

static uint32_t fifth_kill_bonus(const Game *g) {
    if (!g)
        return 0;

    float speed =
        vlen(
            g->player.vel
        );

    if (!isfinite(speed) ||
        speed < 0.0f) {
        speed = 0.0f;
    }

    float height =
        g->run_start_pos.y -
        g->player.pos.y;

    if (!isfinite(height) ||
        height < 0.0f) {
        height = 0.0f;
    }

    
    uint32_t combo_bonus =
        g->combo > 1
        ? (uint32_t)(g->combo - 1)
        : 0u;

    if (combo_bonus > 8u)
        combo_bonus = 8u;

    uint32_t speed_bonus =
        (uint32_t)(
            speed /
            120.0f
        );

    if (speed_bonus > 6u)
        speed_bonus = 6u;

    uint32_t height_bonus =
        (uint32_t)(
            height /
            350.0f
        );

    if (height_bonus > 12u)
        height_bonus = 12u;

    return 1u +
           combo_bonus +
           speed_bonus +
           height_bonus;
}

static void destroy_block(Game *g, int index, bool award) {
    if (index < 0 || index >= MAX_BLOCKS) return;
    Block *b = &g->blocks[index];
    if (!b->active) return;

    BlockType type = b->type;
    Vec2 pos = b->body.pos;
    Color c = block_color(g, type);
    int base = b->value > 0 ? b->value : 80;

    if (g->rope.target_block == index) rope_clear(&g->rope);
    b->active = false;

    
    audio_play_sfx(AUDIO_SFX_EXPLOSION);

    spawn_particles(g, pos, c, type == BLOCK_RED ? 26 : 22, 155.0f);
    spawn_particles(g, pos, C_WHITE, 8, 105.0f);
    g->screen_shake = fmaxf(g->screen_shake, 0.13f);

    if (!award) return;

    if (g->combo_timer > 0.0f) {
        if (g->combo < g->max_combo) ++g->combo;
    } else {
        g->combo = 1;
    }
    g->combo_timer = 2.35f;

    if (g->combo > g->run_best_combo)
        g->run_best_combo = g->combo;

    int points = base * g->combo;
    g->score += points;
    ++g->destroyed;
    ++g->progress.lifetime_destroyed;
    add_popup(g, pos, points, false, C_YELLOW);

    
    uint32_t block_xp =
        block_xp_reward(
            type,
            g->combo
        );

    block_xp =
        (uint32_t)(
            (float)block_xp *
            cosmetic_block_xp_mult(g) +
            0.5f
        );

    if (type == BLOCK_MONEY) {
        uint32_t money_bonus =
            g->progress.levels[UPG_MONEY]
            ? randomized_reward(
                  260u,
                  80,
                  145
              )
            : randomized_reward(
                  120u,
                  75,
                  140
              );

        add_cash(
            g,
            money_bonus,
            vadd(
                pos,
                vec2(0.0f, 12.0f)
            ),
            true,
            true
        );
    } else {
        ++g->normal_cash_kills;

        




        uint32_t kill_cash =
            normal_kill_base_cash(g) +
            cosmetic_kill_cash_bonus(g);

        bool fifth =
            (g->normal_cash_kills %
             5u) == 0u;

        if (fifth) {
            



            kill_cash +=
                fifth_kill_bonus(g);
        }

        add_cash(
            g,
            kill_cash,
            vadd(
                pos,
                vec2(0.0f, 12.0f)
            ),
            true,
            fifth
        );
    }

    grant_xp(
        g,
        block_xp,
        vadd(
            pos,
            vec2(0.0f, -10.0f)
        )
    );

    if (type == BLOCK_GREEN) {
        spawn_green_burst(g, pos);
    }
}

static void update_particles(Game *g, float dt) {
    if (setting_enabled(g, SETTING_PARTICLES)) {
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            Particle *p = &g->particles[i];
            if (!p->active) continue;
            p->life -= dt;
            if (p->life <= 0.0f) { p->active = false; continue; }
            p->vel = vmul(p->vel, clampf_local(1.0f - 1.5f * dt, 0.0f, 1.0f));
            p->pos = vadd(p->pos, vmul(p->vel, dt));
        }
    }

    for (int i = 0; i < MAX_POPUPS; ++i) {
        ScorePopup *p = &g->popups[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.0f) { p->active = false; continue; }
        p->pos.y -= 22.0f * dt;
    }
}

static void update_blocks(Game *g, float dt) {
    






    (void)g;
    (void)dt;
}

static Vec2 touch_to_world(const Game *g, const GameInput *in) {
    



    float screen_x = (float)in->touch_x * (400.0f / 320.0f);
    float screen_y = (float)in->touch_y;

    screen_x = clampf_local(screen_x, 0.0f, 399.0f);
    screen_y = clampf_local(screen_y, 0.0f, 239.0f);

    return vec2(
        g->camera_x + screen_x * g->camera_zoom,
        g->camera_y + screen_y * g->camera_zoom
    );
}

static int nearest_block_to_point(const Game *g, Vec2 world_point) {
    int best = -1;
    float pick_radius = 125.0f * g->camera_zoom;
    float best_d2 = pick_radius * pick_radius;
    float max_rope = rope_max_length(g);
    float max_rope2 = max_rope * max_rope;

    for (int i = 0; i < MAX_BLOCKS; ++i) {
        const Block *b = &g->blocks[i];
        if (!b->active) continue;
        if (i == 0 && g->starter_platform_active) continue;

        Vec2 d = vsub(b->body.pos, world_point);
        float d2 = vdot(d, d);
        if (d2 >= best_d2) continue;

        Vec2 from_player = vsub(b->body.pos, g->player.pos);
        if (vdot(from_player, from_player) > max_rope2) continue;

        best_d2 = d2;
        best = i;
    }

    return best;
}

static void begin_rope_shot(Game *g, const GameInput *in) {
    Rope *r = &g->rope;
    Vec2 cursor = touch_to_world(g, in);

    rope_clear(r);
    r->active = true;
    r->user_owned = true;
    r->hook_pos = cursor;

    g->aim_world = cursor;
    g->aim_valid = true;

    int hit = nearest_block_to_point(g, cursor);
    if (hit < 0) {
        r->direction = vnormalize(vsub(cursor, g->player.pos));
        return;
    }

    Block *target = &g->blocks[hit];
    r->latched = true;
    r->target_block = hit;
    r->hook_pos = target->body.pos;

    Vec2 delta = vsub(target->body.pos, g->player.pos);
    Vec2 dir = vnormalize(delta);
    r->direction = dir;

    
    float spin = vcross(delta, g->player.vel);
    float sign = 0.0f;

    if (fabsf(spin) > 0.01f)
        sign = (spin > 0.0f) ? 1.0f : -1.0f;
    else if (fabsf(g->player.vel.x) > 0.5f)
        sign = (g->player.vel.x > 0.0f) ? 1.0f : -1.0f;
    else if (fabsf(g->player.vel.y) > 0.5f)
        sign = (g->player.vel.y > 0.0f) ? 1.0f : -1.0f;
    else
        sign = (dir.x >= 0.0f) ? 1.0f : -1.0f;

    Vec2 tangent = tangent_from_dir(dir, sign);

    
    g->player.vel = vadd(
        g->player.vel,
        vmul(tangent, grapple_launch_speed(g))
    );

    
    g->player.pos = vadd(g->player.pos, vmul(dir, grapple_catch_step(g)));

    r->length = vlen(vsub(target->body.pos, g->player.pos));
    r->desired_length = r->length;

    audio_play_sfx(AUDIO_SFX_GRAPPLE);

    



    if (g->starter_platform_active) {
        g->starter_platform_active = false;
        if (g->blocks[0].active)
            destroy_block(g, 0, false);
    }

    spawn_particles(g, target->body.pos, C_CYAN, 4, 34.0f);
}

static void apply_laser_rope(Game *g) {
    if (!g->progress.levels[UPG_LASER] || !g->rope.active) return;

    Vec2 end = g->rope.hook_pos;
    if (g->rope.latched && g->rope.target_block >= 0 &&
        g->rope.target_block < MAX_BLOCKS && g->blocks[g->rope.target_block].active)
        end = g->blocks[g->rope.target_block].body.pos;

    for (int i = 0; i < MAX_BLOCKS; ++i) {
        if (!g->blocks[i].active || i == g->rope.target_block) continue;
        if (segment_aabb(g->player.pos, end, g->blocks[i].body.pos,
                         g->blocks[i].half + 1.0f, NULL))
            destroy_block(g, i, true);
    }
}

static void update_rope(Game *g, const GameInput *in, float dt) {
    Rope *r = &g->rope;

    if (in->touch_up) {
        rope_clear(r);
        g->aim_valid = false;
        return;
    }

    



    if (in->touch_down)
        begin_rope_shot(g, in);

    if (!r->active || !r->latched) return;

    if (r->target_block < 0 || r->target_block >= MAX_BLOCKS ||
        !g->blocks[r->target_block].active) {
        rope_clear(r);
        return;
    }

    Block *target = &g->blocks[r->target_block];
    r->hook_pos = target->body.pos;

    
    (void)dt;
}


static void apply_grapple_anchor_constraint(Game *g) {
    Rope *r = &g->rope;

    if (!r->active || !r->latched ||
        r->target_block < 0 || r->target_block >= MAX_BLOCKS ||
        !g->blocks[r->target_block].active)
        return;

    Vec2 anchor = g->blocks[r->target_block].body.pos;
    r->hook_pos = anchor;

    float rope_len = r->desired_length;
    if (rope_len < 8.0f)
        rope_len = 8.0f;

    Vec2 delta = vsub(g->player.pos, anchor);
    float dist = vlen(delta);

    
    if (dist < 0.0001f) {
        Vec2 fallback = vmul(r->direction, -1.0f);
        if (vlen(fallback) < 0.0001f)
            fallback = vec2(0.0f, 1.0f);

        fallback = vnormalize(fallback);
        g->player.pos = vadd(anchor, vmul(fallback, rope_len));
        delta = vsub(g->player.pos, anchor);
        dist = vlen(delta);
    }

    Vec2 n = vmul(delta, 1.0f / dist);

    
    g->player.pos = vadd(anchor, vmul(n, rope_len));

    float radial = vdot(g->player.vel, n);
    g->player.vel = vsub(g->player.vel, vmul(n, radial));

}

static void confine_player(Game *g) {
    


    (void)g;
}

static void update_player_collisions(Game *g) {
    for (int i = 0; i < MAX_BLOCKS; ++i) {
        Block *b = &g->blocks[i];
        if (!b->active) continue;

        



        if (i == 0 && g->starter_platform_active) {
            float platform_top = b->body.pos.y - b->half;
            float px = fabsf(g->player.pos.x - b->body.pos.x);
            float player_bottom = g->player.pos.y + g->player_half;

            if (px <= b->half + g->player_half &&
                player_bottom >= platform_top &&
                player_bottom <= platform_top + 8.0f &&
                g->player.vel.y >= 0.0f) {
                g->player.pos.y = platform_top - g->player_half;
                g->player.vel.y = 0.0f;
                g->player.vel.x *= 0.985f;
            }
            continue;
        }

        if (!phys_aabb_overlap(g->player.pos, g->player_half, g->player_half,
                               b->body.pos, b->half, b->half))
            continue;

        bool hooked = (i == g->rope.target_block);

        
        if (b->type == BLOCK_RED &&
            !hooked &&
            !g->progress.levels[UPG_THICC]) {
            Vec2 p = b->body.pos;
            destroy_block(g, i, false);
            damage_player(g, p, false);
            continue;
        }

        destroy_block(g, i, true);
        g->player.vel.x *= 0.985f;
        g->player.vel.y *= 0.96f;
    }
}

static float bullet_level_factor(const Game *g) {
    






    int level = clampi((int)g->progress.levels[UPG_BULLETS], 1, 9);
    return 1.0f + 0.075f * (float)(level - 1);
}

static float bullet_combo_factor(const Game *g) {
    











    int combo = clampi(g->combo, 1, 6);
    return 1.0f + 0.12f * (float)(combo - 1);
}

static float bullet_speed_factor(const Game *g) {
    






    float horizontal_speed = fabsf(g->player.vel.x);

    float t =
        (horizontal_speed - BULLET_SPEED_BONUS_START) /
        (BULLET_SPEED_BONUS_FULL - BULLET_SPEED_BONUS_START);

    t = clampf_local(t, 0.0f, 1.0f);

    
    t = t * t * (3.0f - 2.0f * t);

    return 1.0f + t;
}

static float bullet_timer_rate(const Game *g) {
    


















    float rate =
        bullet_level_factor(g) *
        bullet_combo_factor(g) *
        bullet_speed_factor(g) *
        cosmetic_bullet_timer_mult(g);

    return clampf_local(rate, 1.0f, BULLET_TIMER_MAX_RATE);
}


static void fire_horizontal_bullet_spray(Game *g) {
    float horizontal_speed = fabsf(g->player.vel.x);

    if (horizontal_speed < BULLET_FIRE_MIN_HORIZONTAL_SPEED)
        return;

    








    float sign = g->player.vel.x >= 0.0f ? 1.0f : -1.0f;
    Vec2 forward = vec2(sign, 0.0f);

    float bullet_speed =
        fmaxf(280.0f, horizontal_speed + 150.0f);

    if (bullet_speed > 850.0f)
        bullet_speed = 850.0f;

    int spray_count =
        cosmetic_bullet_count(g);

    if (spray_count < 2)
        spray_count = 2;

    for (int i = 0; i < spray_count; ++i) {
        float t =
            (float)i /
            (float)(spray_count - 1);

        float degrees =
            -BULLET_SPRAY_HALF_ANGLE_DEG +
            (BULLET_SPRAY_HALF_ANGLE_DEG * 2.0f) * t;

        float radians =
            degrees * (3.14159265358979323846f / 180.0f);

        



        Vec2 dir = vec2(
            sign * cosf(radians),
            sinf(radians)
        );

        float vertical_offset =
            ((float)i -
             ((float)spray_count - 1.0f) * 0.5f) *
            0.75f;

        Vec2 pos = vadd(
            g->player.pos,
            vadd(
                vmul(forward, g->player_half + 5.0f),
                vec2(0.0f, vertical_offset)
            )
        );

        spawn_bullet(
            g,
            pos,
            dir,
            player_color(g),
            bullet_speed
        );
    }

    spawn_particles(
        g,
        vadd(g->player.pos, vmul(forward, g->player_half + 5.0f)),
        player_color(g),
        16,
        72.0f
    );
}

static void update_auto_bullets(Game *g, float dt) {
    int level =
        clampi((int)g->progress.levels[UPG_BULLETS], 0, 9);

    if (level <= 0) {
        g->bullet_timer = BULLET_TIMER_BASE_SECONDS;
        return;
    }

    



    float rate = bullet_timer_rate(g);
    g->bullet_timer -= dt * rate;

    if (g->bullet_timer > 0.0f)
        return;

    
    if (fabsf(g->player.vel.x) < BULLET_FIRE_MIN_HORIZONTAL_SPEED) {
        g->bullet_timer = 0.0f;
        return;
    }

    fire_horizontal_bullet_spray(g);
    g->bullet_timer = BULLET_TIMER_BASE_SECONDS;
}

static void update_bullets(Game *g, float dt) {
    for (int i = 0; i < MAX_BULLETS; ++i) {
        Bullet *b = &g->bullets[i];
        if (!b->active) continue;

        Vec2 old = b->pos;
        b->pos = vadd(b->pos, vmul(b->vel, dt));
        b->life -= dt;
        float screen_x = world_to_screen_x(g, b->pos.x);
        float screen_y = world_to_screen_y(g, b->pos.y);
        if (b->life <= 0.0f ||
            screen_x < -30.0f || screen_x > 430.0f ||
            screen_y < -30.0f || screen_y > 270.0f) {
            b->active = false;
            continue;
        }

        for (int j = 1; j < MAX_BLOCKS; ++j) {
            Block *target = &g->blocks[j];
            if (!target->active) continue;
            if (segment_aabb(
                    old,
                    b->pos,
                    target->body.pos,
                    target->half +
                        1.0f +
                        cosmetic_bullet_hit_bonus(g),
                    NULL)) {
                destroy_block(g, j, true);
                b->active = false;
                break;
            }
        }
    }
}

static void update_camera(Game *g, float dt) {
    





    float speed = vlen(g->player.vel);
    float target_zoom = camera_zoom_for_speed(speed);

    
    if (g->starter_platform_active &&
        target_zoom < CAMERA_ZOOM_STARTING) {
        target_zoom = CAMERA_ZOOM_STARTING;
    }

    float old_zoom = g->camera_zoom;
    float old_center_x = g->camera_x + TOP_W * 0.5f * old_zoom;
    float old_center_y = g->camera_y + TOP_H * 0.5f * old_zoom;

    float zoom_rate =
        target_zoom > old_zoom
        ? CAMERA_ZOOM_OUT_RATE
        : CAMERA_ZOOM_IN_RATE;

    float zt = clampf_local(zoom_rate * dt, 0.0f, 1.0f);
    g->camera_zoom += (target_zoom - g->camera_zoom) * zt;
    g->camera_zoom =
        clampf_local(g->camera_zoom, CAMERA_ZOOM_MIN, CAMERA_ZOOM_MAX);

    


    g->camera_x = old_center_x - TOP_W * 0.5f * g->camera_zoom;
    g->camera_y = old_center_y - TOP_H * 0.5f * g->camera_zoom;

    
    float screen_x = world_to_screen_x(g, g->player.pos.x);
    float screen_y = world_to_screen_y(g, g->player.pos.y);

    float desired_x = g->camera_x;
    float desired_y = g->camera_y;

    if (screen_x < 118.0f)
        desired_x = g->player.pos.x - 118.0f * g->camera_zoom;
    if (screen_x > 282.0f)
        desired_x = g->player.pos.x - 282.0f * g->camera_zoom;

    if (screen_y < 82.0f)
        desired_y = g->player.pos.y - 82.0f * g->camera_zoom;
    if (screen_y > 158.0f)
        desired_y = g->player.pos.y - 158.0f * g->camera_zoom;

    




    float max_camera_y =
        -TOP_H * 0.5f * (g->camera_zoom - CAMERA_ZOOM_MIN);
    if (desired_y > max_camera_y)
        desired_y = max_camera_y;

    float tx = clampf_local(8.5f * dt, 0.0f, 1.0f);
    float ty = clampf_local(6.5f * dt, 0.0f, 1.0f);

    g->camera_x += (desired_x - g->camera_x) * tx;
    g->camera_y += (desired_y - g->camera_y) * ty;

    if (g->player.pos.y < g->highest_y)
        g->highest_y = g->player.pos.y;
}

static void update_playing(Game *g, const GameInput *in, float dt) {
    if (in->keys_down & KEY_START) {
        g->pause_index = 0;
        g->mode = MODE_PAUSED;
        return;
    }

    g->difficulty_time += dt;
    if (g->invuln_timer > 0.0f) g->invuln_timer -= dt;

    if (g->combo_timer > 0.0f) {
        g->combo_timer -= dt / shape_combo_timer_mult(g);
        if (g->combo_timer <= 0.0f) {
            g->combo_timer = 0.0f;
            g->combo = 1;
        }
    }

    if (g->screen_shake > 0.0f) g->screen_shake -= dt;
    if (g->flash_timer > 0.0f) g->flash_timer -= dt;
    if (g->levelup_message_timer > 0.0f)
        g->levelup_message_timer -= dt;

    update_blocks(g, dt);

    
    update_rope(g, in, dt);

    bool grappled =
        g->rope.active &&
        g->rope.latched &&
        g->rope.target_block >= 0 &&
        g->rope.target_block < MAX_BLOCKS &&
        g->blocks[g->rope.target_block].active;

    
    g->player.damping =
        PLAYER_AIR_DRAG *
        shape_air_drag_mult(g);

    phys_integrate(
        &g->player,
        vec2(0.0f, PLAYER_GRAVITY),
        dt
    );

    if (grappled)
        apply_grapple_anchor_constraint(g);

    confine_player(g);

    {
        Vec2 step_delta =
            vsub(
                g->player.pos,
                g->last_distance_sample
            );

        float step_distance =
            vlen(
                step_delta
            );

        if (isfinite(step_distance) &&
            step_distance >= 0.0f &&
            step_distance < 800.0f) {
            g->run_path_distance += step_distance;
        }

        g->last_distance_sample = g->player.pos;
    }

    



    {
        Vec2 distance_delta =
            vsub(
                g->player.pos,
                g->run_start_pos
            );

        float displacement =
            vlen(
                distance_delta
            );

        if (isfinite(displacement) &&
            displacement >= 0.0f) {
            g->run_distance =
                displacement;
        }
    }

    update_camera(g, dt);

    
    float player_speed = vlen(g->player.vel);

    if (isfinite(player_speed) &&
        player_speed > 0.0f) {
        uint32_t speed_record =
            (uint32_t)(
                player_speed +
                0.5f
            );

        if (speed_record >
            g->progress.best_speed) {
            g->progress.best_speed =
                speed_record;
        }
    }

    float stretch_target = clampf_local((player_speed - 35.0f) / 285.0f,
                                        0.0f, 1.0f);
    float stretch_lerp = clampf_local(10.0f * dt, 0.0f, 1.0f);
    g->player_stretch += (stretch_target - g->player_stretch) * stretch_lerp;

    update_player_visual_angle(
        g,
        dt
    );

    apply_laser_rope(g);
    update_player_collisions(g);
    if (g->mode != MODE_PLAYING) return;

    




    if (g->player.pos.y + g->player_half >= LAVA_Y) {
        spawn_particles(g, g->player.pos, lava_color(g), 14, 125.0f);
        set_game_over(g);
        return;
    }

    update_auto_bullets(g, dt);
    update_bullets(g, dt);

    



    refresh_section_cache(g, false);
}

