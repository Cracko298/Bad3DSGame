static void draw_bottom_menu_help(const Game *g, Surface *bottom, const char *title, const char *line) {
    surface_clear(bottom, ui_bg_color(g));
    draw_ui_frame(g, bottom, 16, 18, 288, 196);
    draw_text_center(bottom, 160, 44, title, 3, C_WHITE);
    draw_text_center(bottom, 160, 102, "DPAD OR CIRCLE PAD", 1, ui_accent_color(g));
    draw_text_center(bottom, 160, 118, "MOVE SELECTION", 1, C_WHITE);
    draw_text_center(bottom, 160, 146, "A SELECT", 1, C_YELLOW);
    draw_text_center(bottom, 160, 161, "B BACK", 1, ui_dim_color(g));
    if (line) draw_text_center(bottom, 160, 196, line, 1, ui_dim_color(g));
}

static void draw_bottom_grapple(const Game *g, Surface *bottom, const GameInput *in) {
    (void)in;
    surface_clear(bottom, C_BG);

    const int cx = 160;
    const int cy = 120;

    draw_circle_outline(bottom, cx, cy, 34, rgb(42, 47, 52));
    draw_circle_outline(bottom, cx, cy, 72, rgb(22, 26, 30));
    draw_circle_outline(bottom, cx, cy, 108, rgb(12, 15, 18));
    draw_line(bottom, cx - 9, cy, cx + 9, cy, C_DIM);
    draw_line(bottom, cx, cy - 9, cx, cy + 9, C_DIM);
    draw_rect_outline(bottom, cx - 4, cy - 4, 9, 9, C_CYAN);

    if (g->aim_valid) {
        float top_sx = world_to_screen_x(g, g->aim_world.x);
        float top_sy = world_to_screen_y(g, g->aim_world.y);

        int bx = (int)(top_sx * (320.0f / 400.0f));
        int by = (int)top_sy;

        bx = clampi(bx, 0, 319);
        by = clampi(by, 0, 239);

        draw_line(bottom, cx, cy, bx, by,
                  g->progress.levels[UPG_LASER] ? C_GREEN : C_CYAN);
        draw_glow_square(bottom, bx, by, 3, C_CYAN);
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "ROPE %d/15",
             (int)g->progress.levels[UPG_ROPE]);
    draw_text(bottom, 8, 8, buf, 1, C_WHITE);

    if (g->rope.latched) {
        draw_text_center(bottom, 160, 42, "HOOKED", 2, C_GREEN);
        draw_text_center(bottom, 160, 202, "ANCHOR + GRAVITY + MOMENTUM", 1, C_WHITE);
        draw_text_center(bottom, 160, 216, "HOLD TO STAY ATTACHED", 1, C_DIM);
    } else {
        draw_text_center(bottom, 160, 42, "GRAPPLE", 2, C_CYAN);
        draw_text_center(bottom, 160, 202, "TAP NEAR A TOP SCREEN BLOCK", 1, C_WHITE);
        draw_text_center(bottom, 160, 216, "DRAGGING DOES NOT STEER", 1, C_DIM);
    }
}

static void draw_bottom_title(const Game *g, Surface *bottom) {
    surface_clear(
        bottom,
        ui_bg_color(g)
    );

    draw_ui_frame(g, bottom, 12, 10, 296, 220);

    draw_text_center(
        bottom,
        160,
        24,
        "BAD GAME 3DS",
        2,
        C_WHITE
    );

    char buf[64];

    snprintf(
        buf,
        sizeof(buf),
        "LEVEL %lu",
        (unsigned long)g->progress.player_level
    );

    draw_text(
        bottom,
        32,
        67,
        buf,
        1,
        C_CYAN
    );

    snprintf(
        buf,
        sizeof(buf),
        "$%lu",
        (unsigned long)g->progress.money
    );

    draw_text(
        bottom,
        210,
        67,
        buf,
        1,
        C_YELLOW
    );

    snprintf(
        buf,
        sizeof(buf),
        "BEST DIST %lu",
        (unsigned long)g->progress.best_distance
    );

    draw_text(
        bottom,
        32,
        91,
        buf,
        1,
        C_WHITE
    );

    snprintf(
        buf,
        sizeof(buf),
        "HIGH SCORE %lu",
        (unsigned long)g->progress.high_score
    );

    draw_text(
        bottom,
        32,
        113,
        buf,
        1,
        C_WHITE
    );

    int unlocked = 0;

    for (int i = 0;
         i < ACHIEVEMENT_COUNT;
         ++i) {
        if (achievement_unlocked(g, i))
            ++unlocked;
    }

    int ready = MISSION_SLOT_COUNT;

    snprintf(
        buf,
        sizeof(buf),
        "ACHIEVEMENTS %d/%d",
        unlocked,
        ACHIEVEMENT_COUNT
    );

    draw_text(
        bottom,
        32,
        143,
        buf,
        1,
        C_GREEN
    );

    snprintf(
        buf,
        sizeof(buf),
        "MISSIONS READY %d",
        ready
    );

    draw_text(
        bottom,
        32,
        165,
        buf,
        1,
        ready > 0
        ? C_YELLOW
        : C_DIM
    );

    draw_text_center(
        bottom,
        160,
        211,
        "UP/DOWN MOVE   A SELECT",
        1,
        C_DIM
    );
}

static void draw_bottom_shop(const Game *g, Surface *bottom) {
    surface_clear(
        bottom,
        ui_bg_color(g)
    );

    draw_ui_frame(g, bottom, 12, 10, 296, 220);

    const char *page_name =
        g->shop_page == 0
        ? "UPGRADES"
        : COSMETIC_PAGES[
              g->shop_page - 1
          ].name;

    draw_text_center(
        bottom,
        160,
        20,
        page_name,
        2,
        C_WHITE
    );

    if (g->shop_page == 0) {
        int item =
            clampi(
                g->shop_index,
                0,
                SHOP_ITEM_COUNT - 1
            );

        draw_upgrade_icon(
            bottom,
            item,
            160,
            72,
            g->progress.levels[item] > 0
        );

        draw_text_center(
            bottom,
            160,
            105,
            SHOP[item].name,
            2,
            C_CYAN
        );

        draw_text_center(
            bottom,
            160,
            132,
            SHOP[item].desc,
            1,
            C_WHITE
        );

        char state[64];

        int level =
            g->progress.levels[item];

        int max =
            SHOP[item].max_level;

        if (level >= max) {
            snprintf(
                state,
                sizeof(state),
                "LEVEL %d/%d   MAXED",
                level,
                max
            );
        } else {
            snprintf(
                state,
                sizeof(state),
                "LEVEL %d/%d   NEXT $%lu",
                level,
                max,
                (unsigned long)shop_cost(g, item)
            );
        }

        draw_text_center(
            bottom,
            160,
            157,
            state,
            1,
            level >= max
            ? C_GREEN
            : C_YELLOW
        );
    } else {
        int item =
            current_cosmetic_item(g);

        if (item >= 0) {
            const CosmeticDef *d =
                &COSMETICS[item];

            float ease =
                shop_selection_ease(g);

            float scale =
                1.65f +
                0.18f * ease;

            float twist =
                (1.0f - ease) *
                0.38f +
                sinf(
                    (float)g->frame_counter *
                    0.055f
                ) *
                0.035f;

            draw_cosmetic_preview_item(
                g,
                bottom,
                item,
                160,
                80,
                scale,
                twist
            );

            draw_text_center(
                bottom,
                160,
                116,
                d->name,
                2,
                cosmetic_equipped(g, item)
                ? C_GREEN
                : C_CYAN
            );

            draw_text_center(
                bottom,
                160,
                144,
                d->desc,
                1,
                C_WHITE
            );

            if (d->bonus &&
                d->bonus[0] != '\0') {
                draw_text_center(
                    bottom,
                    160,
                    160,
                    d->bonus,
                    1,
                    C_GREEN
                );
            }

            char state[48];

            if (cosmetic_equipped(g, item)) {
                strcpy(
                    state,
                    "EQUIPPED"
                );
            } else if (cosmetic_owned(g, item)) {
                strcpy(
                    state,
                    "OWNED - A TO EQUIP"
                );
            } else {
                snprintf(
                    state,
                    sizeof(state),
                    "$%lu - A TO BUY",
                    (unsigned long)d->cost
                );
            }

            draw_text_center(
                bottom,
                160,
                181,
                state,
                1,
                cosmetic_equipped(g, item)
                ? C_GREEN
                : C_YELLOW
            );
        }
    }

    draw_text_center(
        bottom,
        160,
        202,
        "D-PAD SELECT   L/R PAGE",
        1,
        C_DIM
    );

    draw_text_center(
        bottom,
        160,
        216,
        "A BUY/EQUIP   B BACK",
        1,
        C_DIM
    );
}

static void draw_bottom_missions(const Game *g, Surface *bottom) {
    surface_clear(
        bottom,
        ui_bg_color(g)
    );

    draw_ui_frame(g, bottom, 12, 10, 296, 220);

    int i =
        clampi(
            g->missions_index,
            0,
            MISSION_SLOT_COUNT - 1
        );

    const GeneratedMission *m =
        mission_slot_const(g, i);

    if (!m)
        return;

    draw_text_center(
        bottom,
        160,
        22,
        "MISSION GOAL",
        2,
        C_WHITE
    );

    draw_text_center(
        bottom,
        160,
        63,
        m->name,
        2,
        C_CYAN
    );

    draw_text_center(
        bottom,
        160,
        96,
        m->desc,
        1,
        C_WHITE
    );

    char progress[48];

    snprintf(
        progress,
        sizeof(progress),
        "%lu / %lu",
        (unsigned long)goal_value(g, (GoalKind)m->kind),
        (unsigned long)m->target
    );

    draw_text_center(
        bottom,
        160,
        119,
        progress,
        1,
        C_YELLOW
    );

    draw_progress_bar(
        bottom,
        45,
        137,
        230,
        goal_value(g, (GoalKind)m->kind),
        m->target,
        C_GREEN
    );

    char reward[64];

    snprintf(
        reward,
        sizeof(reward),
        "AUTO REWARD $%lu + %lu XP",
        (unsigned long)m->cash_reward,
        (unsigned long)m->xp_reward
    );

    draw_text_center(
        bottom,
        160,
        160,
        reward,
        1,
        C_GREEN
    );

    draw_text_center(
        bottom,
        160,
        188,
        "COMPLETES + REFRESHES",
        2,
        C_YELLOW
    );

    char total[48];
    snprintf(
        total,
        sizeof(total),
        "TOTAL COMPLETED %lu",
        (unsigned long)g->progress.missions_claimed
    );
    draw_text_center(
        bottom,
        160,
        206,
        total,
        1,
        C_WHITE
    );

    draw_text_center(
        bottom,
        160,
        224,
        "UP/DOWN GOAL   AUTO   B BACK",
        1,
        C_DIM
    );
}

static void draw_bottom_achievements(const Game *g, Surface *bottom) {
    surface_clear(
        bottom,
        ui_bg_color(g)
    );

    draw_ui_frame(g, bottom, 12, 10, 296, 220);

    int i =
        clampi(
            g->achievements_index,
            0,
            ACHIEVEMENT_COUNT - 1
        );

    const AchievementDef *a =
        &ACHIEVEMENTS[i];

    bool unlocked =
        achievement_unlocked(
            g,
            i
        );

    draw_text_center(
        bottom,
        160,
        25,
        unlocked
        ? "ACHIEVEMENT UNLOCKED"
        : "ACHIEVEMENT",
        2,
        unlocked
        ? C_GREEN
        : C_WHITE
    );

    draw_text_center(
        bottom,
        160,
        72,
        a->name,
        2,
        unlocked
        ? C_GREEN
        : C_CYAN
    );

    draw_text_center(
        bottom,
        160,
        106,
        a->desc,
        1,
        C_WHITE
    );

    char progress[48];

    snprintf(
        progress,
        sizeof(progress),
        "%lu / %lu",
        (unsigned long)goal_value(g, a->kind),
        (unsigned long)a->target
    );

    draw_text_center(
        bottom,
        160,
        132,
        progress,
        1,
        unlocked
        ? C_GREEN
        : C_YELLOW
    );

    draw_progress_bar(
        bottom,
        45,
        151,
        230,
        goal_value(g, a->kind),
        a->target,
        unlocked
        ? C_GREEN
        : C_CYAN
    );

    draw_text_center(
        bottom,
        160,
        190,
        unlocked
        ? "COMPLETE"
        : "KEEP WRECKING",
        2,
        unlocked
        ? C_GREEN
        : C_DIM
    );

    draw_text_center(
        bottom,
        160,
        224,
        "UP/DOWN VIEW   A/B BACK",
        1,
        C_DIM
    );
}

void game_render_bottom(const Game *g,
                        Surface *bottom,
                        const GameInput *in) {
    switch (g->mode) {
        case MODE_PLAYING:
            draw_bottom_grapple(
                g,
                bottom,
                in
            );
            break;

        case MODE_TITLE:
            draw_bottom_title(
                g,
                bottom
            );
            break;

        case MODE_SHOP:
            draw_bottom_shop(
                g,
                bottom
            );
            break;

        case MODE_SETTINGS:
            if (g->reset_save_confirm) {
                draw_bottom_menu_help(
                    g,
                    bottom,
                    "RESET SAVE?",
                    "LEFT/RIGHT CHOOSE   A CONFIRM"
                );
            } else {
                draw_bottom_menu_help(
                    g,
                    bottom,
                    g->settings_page == 0
                    ? "AUDIO + DISPLAY"
                    : "EFFECTS",
                    "UP/DOWN SCROLL  L/R PAGE  LEFT/RIGHT CHANGE"
                );
            }
            break;

        case MODE_MISSIONS:
            draw_bottom_missions(
                g,
                bottom
            );
            break;

        case MODE_ACHIEVEMENTS:
            draw_bottom_achievements(
                g,
                bottom
            );
            break;

        case MODE_PAUSED:
            draw_bottom_menu_help(
                g,
                bottom,
                "PAUSED",
                "START OR B RESUMES"
            );
            break;

        case MODE_GAMEOVER:
            draw_bottom_menu_help(
                g,
                bottom,
                "GAME OVER",
                "SPEND MONEY IN SHOP"
            );
            break;

        default:
            surface_clear(
                bottom,
                C_BG
            );
            break;
    }
}
