static void draw_progress_bar(Surface *s,
                              int x,
                              int y,
                              int w,
                              uint32_t value,
                              uint32_t target,
                              Color c) {
    draw_rect_outline(
        s,
        x,
        y,
        w,
        8,
        C_DIM
    );

    float pct =
        target > 0
        ? (float)value /
          (float)target
        : 1.0f;

    pct =
        clampf_local(
            pct,
            0.0f,
            1.0f
        );

    int fill =
        (int)(
            (float)(w - 4) *
            pct
        );

    if (fill > 0) {
        draw_rect(
            s,
            x + 2,
            y + 2,
            fill,
            4,
            c
        );
    }
}

static void draw_missions(const Game *g, Surface *top) {
    draw_background(g, top);

    draw_rect(
        top,
        24,
        10,
        352,
        220,
        rgb(7, 9, 11)
    );

    draw_rect_outline(
        top,
        24,
        10,
        352,
        220,
        C_DIM
    );

    draw_text_center(
        top,
        200,
        18,
        "AUTO MISSIONS",
        3,
        C_WHITE
    );

    char completed[40];
    snprintf(
        completed,
        sizeof(completed),
        "COMPLETED %lu",
        (unsigned long)g->progress.missions_claimed
    );
    draw_text_center(top, 200, 40, completed, 1, C_YELLOW);

    int first =
        g->missions_index - 2;

    if (first < 0)
        first = 0;

    if (first >
        MISSION_SLOT_COUNT - 5) {
        first =
            MISSION_SLOT_COUNT - 5;
    }

    if (first < 0)
        first = 0;

    for (int slot = 0;
         slot < 5;
         ++slot) {
        int i =
            first +
            slot;

        if (i >= MISSION_SLOT_COUNT)
            break;

        const GeneratedMission *m =
            mission_slot_const(g, i);

        if (!m)
            continue;

        bool selected =
            i ==
            g->missions_index;

        int y =
            62 +
            slot * 31;

        if (selected) {
            draw_rect_outline(
                top,
                42,
                y - 5,
                316,
                27,
                C_CYAN
            );
        }

        draw_text(
            top,
            54,
            y,
            m->name,
            1,
            C_WHITE
        );

        char progress[32];

        snprintf(
            progress,
            sizeof(progress),
            "%lu/%lu",
            (unsigned long)goal_value(g, (GoalKind)m->kind),
            (unsigned long)m->target
        );

        draw_text(
            top,
            277,
            y,
            progress,
            1,
            C_WHITE
        );

        draw_progress_bar(
            top,
            54,
            y + 13,
            200,
            goal_value(g, (GoalKind)m->kind),
            m->target,
            C_CYAN
        );
    }
}

static void draw_achievements(const Game *g, Surface *top) {
    draw_background(g, top);

    draw_rect(
        top,
        24,
        10,
        352,
        220,
        rgb(7, 9, 11)
    );

    draw_rect_outline(
        top,
        24,
        10,
        352,
        220,
        C_DIM
    );

    int unlocked = 0;

    for (int i = 0;
         i < ACHIEVEMENT_COUNT;
         ++i) {
        if (achievement_unlocked(g, i))
            ++unlocked;
    }

    char heading[48];

    snprintf(
        heading,
        sizeof(heading),
        "ACHIEVEMENTS %d/%d",
        unlocked,
        ACHIEVEMENT_COUNT
    );

    draw_text_center(
        top,
        200,
        20,
        heading,
        2,
        C_WHITE
    );

    int first =
        g->achievements_index - 2;

    if (first < 0)
        first = 0;

    if (first >
        ACHIEVEMENT_COUNT - 5) {
        first =
            ACHIEVEMENT_COUNT - 5;
    }

    if (first < 0)
        first = 0;

    for (int slot = 0;
         slot < 5;
         ++slot) {
        int i =
            first +
            slot;

        if (i >= ACHIEVEMENT_COUNT)
            break;

        const AchievementDef *a =
            &ACHIEVEMENTS[i];

        bool selected =
            i ==
            g->achievements_index;

        bool unlocked_now =
            achievement_unlocked(
                g,
                i
            );

        int y =
            62 +
            slot * 31;

        if (selected) {
            draw_rect_outline(
                top,
                42,
                y - 5,
                316,
                27,
                C_CYAN
            );
        }

        Color state =
            unlocked_now
            ? C_GREEN
            : C_DIM;

        draw_text(
            top,
            54,
            y,
            a->name,
            1,
            state
        );

        draw_text(
            top,
            292,
            y,
            unlocked_now
            ? "DONE"
            : "LOCKED",
            1,
            state
        );

        draw_progress_bar(
            top,
            54,
            y + 13,
            200,
            goal_value(g, a->kind),
            a->target,
            state
        );
    }
}

static void draw_pause(const Game *g, Surface *top) {
    draw_rect(top, 88, 61, 224, 128, rgb(7, 9, 11));
    draw_rect_outline(top, 88, 61, 224, 128, C_DIM);
    draw_text_center(top, 200, 73, "PAUSED", 3, C_WHITE);
    draw_menu_choice(top, 105, "RESUME", g->pause_index == 0);
    draw_menu_choice(top, 133, "RESTART", g->pause_index == 1);
    draw_menu_choice(top, 161, "TITLE", g->pause_index == 2);
}

static void draw_gameover(const Game *g, Surface *top) {
    
    draw_rect(top, 48, 10, 304, 224, rgb(7, 9, 11));
    draw_rect_outline(top, 48, 10, 304, 224, C_DIM);

    draw_text_center(top, 200, 20, "GAME OVER", 3, C_RED);

    char buf[64];

    snprintf(buf, sizeof(buf), "SCORE %d", g->score);
    draw_text_center(top, 200, 54, buf, 1, C_YELLOW);

    snprintf(buf, sizeof(buf), "DIST %lu   BLOCKS %d",
             (unsigned long)current_run_distance(g),
             g->destroyed);
    draw_text_center(top, 200, 66, buf, 1, C_WHITE);

    snprintf(buf, sizeof(buf), "+XP %lu   +$%lu",
             (unsigned long)g->run_xp_earned,
             (unsigned long)g->run_cash_earned);
    draw_text_center(top, 200, 78, buf, 1, C_GREEN);

    snprintf(buf, sizeof(buf), "RUN BONUS +%luXP +$%lu",
             (unsigned long)g->run_end_xp_bonus,
             (unsigned long)g->run_end_cash_bonus);
    draw_text_center(top, 200, 90, buf, 1, C_YELLOW);

    snprintf(buf, sizeof(buf), "LEVEL %lu",
             (unsigned long)g->progress.player_level);
    draw_text_center(top, 200, 105, buf, 2, C_CYAN);

    uint32_t need =
        xp_required_for_level(
            g->progress.player_level
        );

    float pct =
        clampf_local(
            (float)g->progress.xp /
            (float)need,
            0.0f,
            1.0f
        );

    draw_rect_outline(top, 120, 128, 160, 10, C_DIM);
    draw_rect(top, 122, 130, (int)(156.0f * pct), 6, C_GREEN);

    snprintf(buf, sizeof(buf), "%lu / %lu XP",
             (unsigned long)g->progress.xp,
             (unsigned long)need);
    draw_text_center(top, 200, 142, buf, 1, C_DIM);

    if (g->run_levelups > 0) {
        snprintf(buf, sizeof(buf), "LEVEL UPS +%d", g->run_levelups);
        draw_text_center(top, 200, 153, buf, 1, C_YELLOW);
    }

    draw_menu_choice(top, 171, "RETRY", g->gameover_index == 0);
    draw_menu_choice(top, 193, "SHOP", g->gameover_index == 1);
    draw_menu_choice(top, 215, "TITLE", g->gameover_index == 2);
}

