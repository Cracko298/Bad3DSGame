static Color player_color(const Game *g) {
    int style =
        g
        ? (int)g->progress.player_style
        : 0;

    style =
        clampi(
            style,
            0,
            8
        );

    return PLAYER_COLORS[style];
}

static Color rope_color(const Game *g) {
    int style =
        g
        ? (int)g->progress.rope_style
        : 0;

    style =
        clampi(
            style,
            0,
            8
        );

    return ROPE_COLORS[style];
}

static uint32_t xp_required_for_level(uint32_t level) {
    if (level < 1) level = 1;

    uint64_t need =
        360ull +
        (uint64_t)level * 125ull +
        (uint64_t)level * (uint64_t)level * 8ull;

    if (need > 5000000ull) need = 5000000ull;
    return (uint32_t)need;
}

static uint32_t level_cash_base(uint32_t new_level) {
    uint64_t reward =
        700ull +
        (uint64_t)new_level * 300ull +
        (uint64_t)new_level * (uint64_t)new_level * 18ull;

    if (reward > 25000000ull) reward = 25000000ull;
    return (uint32_t)reward;
}

static uint32_t randomized_reward(uint32_t base, int low_percent, int high_percent) {
    if (high_percent <= low_percent) return base;

    int span = high_percent - low_percent + 1;
    int pct = low_percent + (rand() % span);
    uint64_t out = (uint64_t)base * (uint64_t)pct / 100ull;
    if (out > 0xFFFFFFFFull) out = 0xFFFFFFFFull;
    return (uint32_t)out;
}

static void add_cash(Game *g,
                     uint32_t amount,
                     Vec2 popup_pos,
                     bool popup,
                     bool play_reward_sound) {
    if (!g || amount == 0) return;

    uint64_t total = (uint64_t)g->progress.money + amount;
    g->progress.money = total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)total;

    uint64_t run_total = (uint64_t)g->run_cash_earned + amount;
    g->run_cash_earned =
        run_total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)run_total;

    











    if (play_reward_sound)
        audio_play_money_gain();

    if (popup) {
        add_popup(g, popup_pos,
                  (int)(amount > 0x7FFFFFFFu ? 0x7FFFFFFF : amount),
                  true, C_MONEY);
    }
}

static void grant_xp(Game *g, uint32_t amount, Vec2 popup_pos) {
    if (!g || amount == 0) return;

    uint64_t run_total = (uint64_t)g->run_xp_earned + amount;
    g->run_xp_earned =
        run_total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)run_total;

    uint64_t xp_total = (uint64_t)g->progress.xp + amount;
    g->progress.xp =
        xp_total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)xp_total;

    while (g->progress.player_level < 100000u) {
        uint32_t need = xp_required_for_level(g->progress.player_level);
        if (g->progress.xp < need)
            break;

        g->progress.xp -= need;
        ++g->progress.player_level;
        ++g->run_levelups;

        
        uint32_t prize = randomized_reward(
            level_cash_base(g->progress.player_level),
            88, 116
        );

        g->last_level_reward = prize;
        g->levelup_message_timer = 2.1f;
        add_cash(g, prize, popup_pos, true, true);

        spawn_particles(g, popup_pos, C_YELLOW, 18, 115.0f);
        spawn_particles(g, popup_pos, C_CYAN, 10, 82.0f);
    }
}

static uint32_t block_xp_reward(BlockType type, int combo) {
    uint32_t xp = 14u + (uint32_t)(rand() % 15);
    if (type == BLOCK_RED) xp += 8u;
    if (type == BLOCK_GREEN) xp += 5u;
    if (combo > 1) xp += (uint32_t)(combo * 2);
    return xp;
}

static uint64_t cosmetic_bit(int item) {
    if (item < 0 ||
        item >= COSMETIC_COUNT) {
        return 0ull;
    }

    return
        1ull <<
        ((unsigned)item & 63u);
}

static bool cosmetic_owned(const Game *g, int item) {
    if (!g ||
        item < 0 ||
        item >= COSMETIC_COUNT) {
        return false;
    }

    uint64_t bit =
        cosmetic_bit(item);

    if (item < 64)
        return (g->progress.cosmetic_owned & bit) != 0;

    return
        (g->progress.cosmetic_owned2 & bit) != 0;
}

static void cosmetic_set_owned(Game *g, int item) {
    if (!g ||
        item < 0 ||
        item >= COSMETIC_COUNT) {
        return;
    }

    uint64_t bit =
        cosmetic_bit(item);

    if (item < 64)
        g->progress.cosmetic_owned |= bit;
    else
        g->progress.cosmetic_owned2 |= bit;
}

static bool cosmetic_equipped(const Game *g, int item) {
    if (!g ||
        item < 0 ||
        item >= COSMETIC_COUNT) {
        return false;
    }

    const CosmeticDef *d =
        &COSMETICS[item];

    switch (d->kind) {
        case COS_PLAYER_COLOR:
            return g->progress.player_style == d->style;

        case COS_ROPE_COLOR:
            return g->progress.rope_style == d->style;

        case COS_ROPE_ANIM:
            return g->progress.pattern_style == d->style;

        case COS_SHAPE:
            return g->progress.shape_style == d->style;

        case COS_PLAYER_ANIM:
            return g->progress.player_anim_style == d->style;

        case COS_HAT:
            return g->progress.hat_style == d->style;

        case COS_BLOCK_THEME:
            return g->progress.block_theme == d->style;

        case COS_BACKGROUND:
            return g->progress.background_style == d->style;

        case COS_LAVA_COLOR:
            return g->progress.lava_color_style == d->style;

        case COS_LAVA_ANIM:
            return g->progress.lava_anim_style == d->style;

        case COS_TITLE_THEME:
            return g->progress.title_style == d->style;

        case COS_UI_THEME:
            return g->progress.reserved_style1 == d->style;

        default:
            return false;
    }
}

static int cosmetic_owned_count(const Game *g) {
    if (!g)
        return 0;

    uint64_t masks[2] = {
        g->progress.cosmetic_owned,
        g->progress.cosmetic_owned2
    };

    int count = 0;

    for (int m = 0;
         m < 2;
         ++m) {
        uint64_t bits =
            masks[m];

        while (bits) {
            count +=
                (int)(bits & 1ull);
            bits >>= 1;
        }
    }

    return count;
}

static uint32_t goal_value(const Game *g, GoalKind kind) {
    if (!g)
        return 0;

    switch (kind) {
        case GOAL_DISTANCE:
            return g->progress.best_distance;

        case GOAL_BLOCKS:
            return g->progress.lifetime_destroyed;

        case GOAL_LEVEL:
            return g->progress.player_level;

        case GOAL_ROPE:
            return g->progress.levels[UPG_ROPE];

        case GOAL_SPEED:
            return g->progress.best_speed;

        case GOAL_COMBO:
            return g->progress.best_combo;

        case GOAL_COSMETICS:
            return (uint32_t)cosmetic_owned_count(g);

        case GOAL_SCORE:
            return g->progress.high_score;

        default:
            return 0;
    }
}

static bool achievement_unlocked(const Game *g, int index) {
    if (!g ||
        index < 0 ||
        index >= ACHIEVEMENT_COUNT) {
        return false;
    }

    const AchievementDef *a =
        &ACHIEVEMENTS[index];

    return
        goal_value(g, a->kind) >=
        a->target;
}

static const GeneratedMission *mission_slot_const(const Game *g,
                                                  int index) {
    if (!g ||
        index < 0 ||
        index >= MISSION_SLOT_COUNT) {
        return NULL;
    }

    return &g->missions[index];
}

static GeneratedMission *mission_slot(Game *g,
                                      int index) {
    if (!g ||
        index < 0 ||
        index >= MISSION_SLOT_COUNT) {
        return NULL;
    }

    return &g->missions[index];
}

static uint32_t mission_reward_cash(const Game *g,
                                    GoalKind kind,
                                    uint32_t tier) {
    (void)g;

    uint32_t base = 500u + tier * 140u;

    switch (kind) {
        case GOAL_DISTANCE: return base + 260u;
        case GOAL_SPEED:    return base + 220u;
        case GOAL_SCORE:    return base + 320u;
        case GOAL_COMBO:    return base + 280u;
        case GOAL_LEVEL:    return base + 420u;
        case GOAL_BLOCKS:
        default:            return base + 120u;
    }
}

static uint32_t mission_reward_xp(const Game *g,
                                  GoalKind kind,
                                  uint32_t tier) {
    (void)g;

    uint32_t base = 110u + tier * 32u;

    switch (kind) {
        case GOAL_DISTANCE: return base + 28u;
        case GOAL_SPEED:    return base + 34u;
        case GOAL_SCORE:    return base + 44u;
        case GOAL_COMBO:    return base + 38u;
        case GOAL_LEVEL:    return base + 56u;
        case GOAL_BLOCKS:
        default:            return base + 20u;
    }
}

static uint32_t mission_target_for_kind(const Game *g,
                                        GoalKind kind,
                                        uint32_t tier) {
    uint32_t current = goal_value(g, kind);

    switch (kind) {
        case GOAL_BLOCKS:
            return current + 18u + tier * 6u;

        case GOAL_DISTANCE:
            return current + 300u + tier * 130u;

        case GOAL_SPEED:
            return current + 28u + tier * 7u;

        case GOAL_SCORE:
            return current + 900u + tier * 240u;

        case GOAL_COMBO:
            return current + 1u + tier / 3u;

        case GOAL_LEVEL:
            if (current < 1u)
                current = 1u;
            return current + 1u + tier / 4u;

        default:
            return current + 1u;
    }
}

static void fill_generated_mission_text(GeneratedMission *m) {
    if (!m)
        return;

    GoalKind kind =
        (GoalKind)m->kind;

    switch (kind) {
        case GOAL_BLOCKS:
            snprintf(m->name, sizeof(m->name), "BLOCK BASH");
            snprintf(m->desc, sizeof(m->desc),
                     "BREAK %lu TOTAL BLOCKS",
                     (unsigned long)m->target);
            break;

        case GOAL_DISTANCE:
            snprintf(m->name, sizeof(m->name), "DISTANCE RUN");
            snprintf(m->desc, sizeof(m->desc),
                     "REACH %lu BEST DIST",
                     (unsigned long)m->target);
            break;

        case GOAL_SPEED:
            snprintf(m->name, sizeof(m->name), "FULL SEND");
            snprintf(m->desc, sizeof(m->desc),
                     "REACH SPEED %lu",
                     (unsigned long)m->target);
            break;

        case GOAL_SCORE:
            snprintf(m->name, sizeof(m->name), "STACK SCORE");
            snprintf(m->desc, sizeof(m->desc),
                     "REACH SCORE %lu",
                     (unsigned long)m->target);
            break;

        case GOAL_COMBO:
            snprintf(m->name, sizeof(m->name), "CHAIN BREAK");
            snprintf(m->desc, sizeof(m->desc),
                     "REACH COMBO X%lu",
                     (unsigned long)m->target);
            break;

        case GOAL_LEVEL:
            snprintf(m->name, sizeof(m->name), "LEVEL UP");
            snprintf(m->desc, sizeof(m->desc),
                     "REACH LEVEL %lu",
                     (unsigned long)m->target);
            break;

        default:
            snprintf(m->name, sizeof(m->name), "MISSION");
            snprintf(m->desc, sizeof(m->desc), "KEEP GOING");
            break;
    }
}

static void generate_mission_slot(Game *g,
                                  int index) {
    GeneratedMission *m =
        mission_slot(g, index);

    if (!m)
        return;

    uint32_t tier =
        g->progress.missions_claimed /
        (uint32_t)MISSION_SLOT_COUNT;

    uint32_t serial =
        g->progress.missions_claimed +
        (uint32_t)index;

    GoalKind kind =
        MISSION_KIND_ROTATION[
            (serial +
             (uint32_t)index * 2u) %
            MISSION_KIND_COUNT
        ];

    m->kind =
        (uint8_t)kind;

    m->target =
        mission_target_for_kind(
            g,
            kind,
            tier +
            (uint32_t)index
        );

    m->cash_reward =
        mission_reward_cash(
            g,
            kind,
            tier +
            (uint32_t)index
        );

    m->xp_reward =
        mission_reward_xp(
            g,
            kind,
            tier +
            (uint32_t)index
        );

    fill_generated_mission_text(m);
}

static void generate_all_missions(Game *g) {
    if (!g)
        return;

    for (int i = 0;
         i < MISSION_SLOT_COUNT;
         ++i) {
        generate_mission_slot(g, i);
    }
}

static bool mission_complete(const Game *g, int index) {
    const GeneratedMission *m =
        mission_slot_const(g, index);

    if (!m)
        return false;

    return goal_value(
               g,
               (GoalKind)m->kind
           ) >=
           m->target;
}

static void set_mission_notice(Game *g,
                               const char *text) {
    if (!g || !text)
        return;

    strncpy(
        g->mission_notice,
        text,
        sizeof(g->mission_notice) - 1
    );

    g->mission_notice[
        sizeof(g->mission_notice) - 1
    ] = '\0';

    g->mission_notice_timer = 2.0f;
}

static void complete_generated_mission(Game *g,
                                       int index) {
    GeneratedMission completed;
    GeneratedMission *m =
        mission_slot(g, index);

    if (!g || !m)
        return;

    completed = *m;

    add_cash(
        g,
        completed.cash_reward,
        g->player.pos,
        false,
        false
    );

    grant_xp(
        g,
        completed.xp_reward,
        g->player.pos
    );

    ++g->progress.missions_claimed;

    char notice[64];
    snprintf(
        notice,
        sizeof(notice),
        "%s +$%lu +%luXP",
        completed.name,
        (unsigned long)completed.cash_reward,
        (unsigned long)completed.xp_reward
    );
    set_mission_notice(g, notice);

    audio_play_purchase();
    generate_mission_slot(g, index);
    save_progress(g);
}

static void update_auto_missions(Game *g) {
    if (!g)
        return;

    for (int pass = 0;
         pass < 2;
         ++pass) {
        for (int i = 0;
             i < MISSION_SLOT_COUNT;
             ++i) {
            if (mission_complete(g, i))
                complete_generated_mission(g, i);
        }
    }
}

static float shape_rope_reach_mult(const Game *g) {
    if (!g)
        return 1.0f;

    switch (g->progress.shape_style) {
        case 2: return 1.08f; 
        case 4: return 1.03f; 
        default: return 1.0f;
    }
}

static float shape_launch_mult(const Game *g) {
    if (!g)
        return 1.0f;

    switch (g->progress.shape_style) {
        case 1: return 1.08f; 
        case 4: return 1.03f; 
        case 5: return 1.07f; 
        default: return 1.0f;
    }
}

static float shape_catch_mult(const Game *g) {
    if (!g)
        return 1.0f;

    switch (g->progress.shape_style) {
        case 4: return 1.03f; 
        case 7: return 1.06f; 
        default: return 1.0f;
    }
}

static float shape_air_drag_mult(const Game *g) {
    if (!g)
        return 1.0f;

    switch (g->progress.shape_style) {
        case 3: return 0.90f; 
        case 4: return 0.97f; 
        case 6: return 0.93f; 
        default: return 1.0f;
    }
}

static float shape_combo_timer_mult(const Game *g) {
    if (!g)
        return 1.0f;

    switch (g->progress.shape_style) {
        case 8: return 1.04f; 
        default: return 1.0f;
    }
}

static uint32_t cosmetic_kill_cash_bonus(const Game *g) {
    if (!g)
        return 0;

    uint32_t bonus = 0;

    if (g->progress.player_style == 7)
        ++bonus; 

    if (g->progress.block_theme == 6)
        ++bonus; 

    return bonus;
}

static float cosmetic_block_xp_mult(const Game *g) {
    if (!g)
        return 1.0f;

    return
        g->progress.lava_color_style == 7
        ? 1.05f
        : 1.0f;
}

static int cosmetic_bullet_count(const Game *g) {
    if (!g)
        return BULLET_SPRAY_COUNT;

    return
        g->progress.pattern_style == 6
        ? 12
        : BULLET_SPRAY_COUNT;
}

static float cosmetic_bullet_timer_mult(const Game *g) {
    if (!g)
        return 1.0f;

    return
        g->progress.hat_style == 14
        ? 1.08f
        : 1.0f;
}

static float cosmetic_bullet_size_mult(const Game *g) {
    if (!g)
        return 1.0f;

    return
        g->progress.pattern_style == 7
        ? 1.75f
        : 1.0f;
}

static float cosmetic_bullet_hit_bonus(const Game *g) {
    if (!g)
        return 0.0f;

    return
        g->progress.pattern_style == 7
        ? 2.5f
        : 0.0f;
}


static uint32_t shop_cost(const Game *g, int item) {
    int level = g->progress.levels[item];
    uint32_t cost = SHOP[item].base_cost;
    for (int i = 0; i < level; ++i) {
        uint64_t next = (uint64_t)cost * (uint64_t)SHOP[item].cost_scale_percent;
        cost = (uint32_t)(next / 100u);
        if (cost > 99999999u) return 99999999u;
    }
    return cost;
}

static float rope_max_length(const Game *g) {
    
    float level_bonus =
        1.0f + 0.025f * (float)g->progress.levels[UPG_ROPE];

    return 500.0f * g->camera_zoom * level_bonus * shape_rope_reach_mult(g);
}

static float grapple_launch_speed(const Game *g) {
    



    return (62.0f + 8.0f * (float)g->progress.levels[UPG_ROPE]) * shape_launch_mult(g);
}

static float grapple_catch_step(const Game *g) {
    



    return (2.75f + 0.35f * (float)g->progress.levels[UPG_ROPE]) * shape_catch_mult(g);
}

static float vcross(Vec2 a, Vec2 b) {
    return a.x * b.y - a.y * b.x;
}

static Vec2 tangent_from_dir(Vec2 dir, float sign) {
    return (sign >= 0.0f)
        ? vec2(-dir.y, dir.x)
        : vec2(dir.y, -dir.x);
}

static Color block_color(const Game *g, BlockType t) {
    uint8_t theme =
        g
        ? g->progress.block_theme
        : 0;

    switch (theme) {
        case 1: 
            switch (t) {
                case BLOCK_RED:    return rgb(74, 150, 255);
                case BLOCK_PURPLE: return rgb(120, 210, 255);
                case BLOCK_GREEN:  return rgb(110, 255, 220);
                case BLOCK_MONEY:  return rgb(220, 245, 255);
                default:           return rgb(240, 250, 255);
            }

        case 2: 
            switch (t) {
                case BLOCK_RED:    return rgb(255, 72, 62);
                case BLOCK_PURPLE: return rgb(255, 86, 175);
                case BLOCK_GREEN:  return rgb(255, 154, 58);
                case BLOCK_MONEY:  return rgb(255, 220, 82);
                default:           return rgb(255, 238, 205);
            }

        case 3: 
            switch (t) {
                case BLOCK_RED:    return rgb(205, 205, 205);
                case BLOCK_PURPLE: return rgb(150, 150, 160);
                case BLOCK_GREEN:  return rgb(235, 235, 235);
                case BLOCK_MONEY:  return rgb(190, 190, 190);
                default:           return C_WHITE;
            }

        case 4: 
            switch (t) {
                case BLOCK_RED:    return rgb(255, 45, 125);
                case BLOCK_PURPLE: return rgb(156, 68, 255);
                case BLOCK_GREEN:  return rgb(25, 255, 125);
                case BLOCK_MONEY:  return rgb(255, 246, 45);
                default:           return rgb(45, 235, 255);
            }

        case 5: 
            switch (t) {
                case BLOCK_RED:    return rgb(42, 78, 160);
                case BLOCK_PURPLE: return rgb(78, 54, 170);
                case BLOCK_GREEN:  return rgb(32, 145, 142);
                case BLOCK_MONEY:  return rgb(175, 160, 72);
                default:           return rgb(90, 125, 185);
            }

        case 6: 
            switch (t) {
                case BLOCK_RED:    return rgb(255, 86, 138);
                case BLOCK_PURPLE: return rgb(202, 95, 255);
                case BLOCK_GREEN:  return rgb(88, 255, 176);
                case BLOCK_MONEY:  return rgb(255, 230, 100);
                default:           return rgb(94, 220, 255);
            }

        case 0:
        default:
            switch (t) {
                case BLOCK_RED: return C_RED;
                case BLOCK_PURPLE: return C_PURPLE;
                case BLOCK_GREEN: return C_GREEN;
                case BLOCK_MONEY: return C_MONEY;
                case BLOCK_ANCHOR:
                default: return C_WHITE;
            }
    }
}

static Color background_clear_color(const Game *g) {
    if (!g)
        return C_BG;

    switch (g->progress.background_style) {
        case 1: return rgb(2, 7, 20);   
        case 2: return rgb(14, 3, 22);  
        case 3: return rgb(1, 13, 8);   
        case 4: return rgb(20, 7, 5);   
        case 5: return rgb(1, 5, 16);   
        case 6: return rgb(5, 7, 12);   
        case 7: return rgb(2, 8, 24);   
        case 8: return rgb(72, 142, 205); 
        case 0:
        default: return C_BG;
    }
}

static int ui_style(const Game *g) {
    return clampi(g ? (int)g->progress.reserved_style1 : 0, 0, 3);
}

static Color ui_bg_color(const Game *g) {
    switch (ui_style(g)) {
        case 1: return rgb(4, 12, 20);
        case 2: return rgb(18, 10, 6);
        case 3: return rgb(11, 6, 18);
        default: return C_BG;
    }
}

static Color ui_panel_color(const Game *g) {
    switch (ui_style(g)) {
        case 1: return rgb(8, 24, 36);
        case 2: return rgb(34, 16, 10);
        case 3: return rgb(22, 10, 32);
        default: return rgb(7, 10, 14);
    }
}

static Color ui_panel_alt_color(const Game *g) {
    switch (ui_style(g)) {
        case 1: return rgb(11, 34, 50);
        case 2: return rgb(48, 22, 12);
        case 3: return rgb(30, 14, 42);
        default: return rgb(11, 15, 20);
    }
}

static Color ui_accent_color(const Game *g) {
    switch (ui_style(g)) {
        case 1: return rgb(90, 200, 255);
        case 2: return rgb(255, 170, 68);
        case 3: return rgb(190, 110, 255);
        default: return C_CYAN;
    }
}


static Color ui_dim_color(const Game *g) {
    switch (ui_style(g)) {
        case 1: return rgb(130, 170, 190);
        case 2: return rgb(185, 155, 130);
        case 3: return rgb(160, 135, 205);
        default: return C_DIM;
    }
}

static void draw_ui_frame(const Game *g,
                          Surface *s,
                          int x,
                          int y,
                          int w,
                          int h) {
    draw_rect(s, x, y, w, h, ui_panel_color(g));
    draw_rect_outline(s, x, y, w, h, ui_accent_color(g));
    draw_rect_outline(s, x + 3, y + 3, w - 6, h - 6, ui_panel_alt_color(g));
}

static Color background_star_color(const Game *g, int brightness) {
    int v = 15 + brightness * 7;

    if (!g)
        return rgb((uint8_t)(v + 20), (uint8_t)(v / 2), 3);

    switch (g->progress.background_style) {
        case 1:
            return rgb((uint8_t)(v / 2),
                       (uint8_t)(v + 24),
                       (uint8_t)(v + 46));

        case 2:
            return rgb((uint8_t)(v + 34),
                       (uint8_t)(v / 2),
                       (uint8_t)(v + 40));

        case 3:
            return rgb((uint8_t)(v / 3),
                       (uint8_t)(v + 40),
                       (uint8_t)(v / 2));

        case 4:
            return rgb((uint8_t)(v + 48),
                       (uint8_t)(v + 12),
                       (uint8_t)(v / 3));

        case 5:
            return rgb((uint8_t)(v / 2),
                       (uint8_t)(v + 28),
                       (uint8_t)(v + 58));

        case 6:
            return rgb((uint8_t)(v + 18),
                       (uint8_t)(v + 20),
                       (uint8_t)(v + 28));

        case 7:
            return rgb((uint8_t)(v / 2),
                       (uint8_t)(v + 48),
                       (uint8_t)(v + 30));

        case 8:
            return rgb(245, 250, 255);

        case 0:
        default:
            return rgb((uint8_t)(v + 20),
                       (uint8_t)(v / 2),
                       3);
    }
}

static Color lava_color(const Game *g) {
    if (!g)
        return C_ORANGE;

    switch (g->progress.lava_color_style) {
        case 1: return rgb(18, 210, 245);
        case 2: return rgb(190, 52, 235);
        case 3: return rgb(70, 235, 70);
        case 4: return rgb(245, 245, 250);
        case 5: return rgb(255, 65, 170);
        case 6: return rgb(35, 82, 255);
        case 7: {
            float p =
                g
                ? (float)g->frame_counter * 0.025f
                : 0.0f;

            int r =
                (int)(128.0f + 127.0f * sinf(p));

            int gg =
                (int)(128.0f + 127.0f * sinf(p + 2.094f));

            int b =
                (int)(128.0f + 127.0f * sinf(p + 4.188f));

            return rgb((uint8_t)r, (uint8_t)gg, (uint8_t)b);
        }
        case 0:
        default: return C_ORANGE;
    }
}

static Color lava_highlight_color(const Game *g) {
    Color c =
        lava_color(g);

    int r = c.r + 72;
    int gg = c.g + 72;
    int b = c.b + 72;

    if (r > 255) r = 255;
    if (gg > 255) gg = 255;
    if (b > 255) b = 255;

    return rgb(
        (uint8_t)r,
        (uint8_t)gg,
        (uint8_t)b
    );
}

