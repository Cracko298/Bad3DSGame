static void shop_message(Game *g, const char *msg) {
    strncpy(g->shop_message, msg, sizeof(g->shop_message) - 1);
    g->shop_message[sizeof(g->shop_message) - 1] = '\0';
    g->shop_message_timer = 1.4f;
}

static void buy_selected_upgrade(Game *g) {
    int i = g->shop_index;
    int level = g->progress.levels[i];
    if (level >= SHOP[i].max_level) {
        shop_message(g, "SOLD OUT");
        return;
    }

    uint32_t cost = shop_cost(g, i);
    if (g->progress.money < cost) {
        shop_message(g, "NOT ENOUGH MONEY");
        return;
    }

    g->progress.money -= cost;
    ++g->progress.levels[i];

    audio_play_purchase();

    save_progress(g);
    shop_message(g, "PURCHASED");
}

static void equip_cosmetic(Game *g, int item) {
    if (!g ||
        item < 0 ||
        item >= COSMETIC_COUNT) {
        return;
    }

    const CosmeticDef *d =
        &COSMETICS[item];

    switch (d->kind) {
        case COS_PLAYER_COLOR:
            g->progress.player_style =
                d->style;
            break;

        case COS_ROPE_COLOR:
            g->progress.rope_style =
                d->style;
            break;

        case COS_ROPE_ANIM:
            g->progress.pattern_style =
                d->style;
            break;

        case COS_SHAPE:
            g->progress.shape_style =
                d->style;
            break;

        case COS_PLAYER_ANIM:
            g->progress.player_anim_style =
                d->style;
            break;

        case COS_HAT:
            g->progress.hat_style =
                d->style;
            break;

        case COS_BLOCK_THEME:
            g->progress.block_theme =
                d->style;
            break;

        case COS_BACKGROUND:
            g->progress.background_style =
                d->style;
            break;

        case COS_LAVA_COLOR:
            g->progress.lava_color_style =
                d->style;
            break;

        case COS_LAVA_ANIM:
            g->progress.lava_anim_style =
                d->style;
            break;

        case COS_TITLE_THEME:
            g->progress.title_style =
                d->style;
            break;

        case COS_UI_THEME:
            g->progress.reserved_style1 =
                d->style;
            break;

        default:
            break;
    }
}

static int shop_page_item_count(const Game *g) {
    if (!g)
        return 0;

    if (g->shop_page == 0)
        return SHOP_ITEM_COUNT;

    int page =
        g->shop_page - 1;

    if (page < 0 ||
        page >= SHOP_PAGE_COUNT - 1) {
        return 0;
    }

    return
        COSMETIC_PAGES[page].count;
}

static int current_cosmetic_item(const Game *g) {
    if (!g ||
        g->shop_page <= 0 ||
        g->shop_page >= SHOP_PAGE_COUNT) {
        return -1;
    }

    const CosmeticPage *page =
        &COSMETIC_PAGES[
            g->shop_page - 1
        ];

    if (g->shop_index < 0 ||
        g->shop_index >= page->count) {
        return -1;
    }

    return
        page->items[
            g->shop_index
        ];
}

static void activate_selected_cosmetic(Game *g) {
    int item =
        current_cosmetic_item(g);

    if (item < 0 ||
        item >= COSMETIC_COUNT) {
        return;
    }

    const CosmeticDef *d =
        &COSMETICS[item];

    if (!cosmetic_owned(g, item)) {
        if (g->progress.money <
            d->cost) {
            shop_message(
                g,
                "NOT ENOUGH MONEY"
            );
            return;
        }

        g->progress.money -=
            d->cost;

        cosmetic_set_owned(
            g,
            item
        );

        equip_cosmetic(
            g,
            item
        );

        audio_play_purchase();
        save_progress(g);

        shop_message(
            g,
            "PURCHASED + EQUIPPED"
        );
        return;
    }

    equip_cosmetic(
        g,
        item
    );

    audio_play_purchase();
    save_progress(g);

    shop_message(
        g,
        "EQUIPPED"
    );
}


static void update_title(Game *g, const GameInput *in, float dt) {
    g->title_select_anim +=
        dt * 7.5f;

    if (g->title_select_anim > 1.0f)
        g->title_select_anim = 1.0f;
    if (in->nav_y != 0) {
        g->title_index +=
            in->nav_y;

        if (g->title_index < 0)
            g->title_index = 5;

        if (g->title_index > 5)
            g->title_index = 0;

        g->title_select_anim = 0.0f;
        audio_play_menu_move();
    }

    if (!(in->keys_down & KEY_A))
        return;

    switch (g->title_index) {
        case 0:
            start_run(g);
            break;

        case 1:
            g->shop_return_mode =
                MODE_TITLE;

            g->shop_page = 0;
            g->shop_index = 0;
            g->shop_select_anim = 0.0f;
            g->shop_touch_pending = -1;
            g->mode = MODE_SHOP;
            break;

        case 2:
            g->missions_index = 0;
            g->mode = MODE_MISSIONS;
            break;

        case 3:
            g->achievements_index = 0;
            g->mode = MODE_ACHIEVEMENTS;
            break;

        case 4:
            g->settings_page = 0;
            g->settings_index = 0;
            g->settings_scroll = 0;
            g->mode = MODE_SETTINGS;
            break;

        case 5:
        default:
            g->request_exit = true;
            break;
    }
}

static void settings_save_and_feedback(Game *g) {
    if (!g)
        return;

    apply_runtime_settings(g);
    save_progress(g);
    audio_play_purchase();
}

static void settings_set_flag(Game *g,
                              uint8_t flag,
                              bool enabled) {
    if (!g)
        return;

    bool was_on =
        setting_enabled(
            g,
            flag
        );

    if (was_on == enabled)
        return;

    
    if (flag == SETTING_SFX &&
        was_on &&
        !enabled) {
        audio_play_purchase();
    }

    set_setting_flag(
        g,
        flag,
        enabled
    );

    apply_runtime_settings(g);
    save_progress(g);

    if (!(flag == SETTING_SFX &&
          was_on &&
          !enabled)) {
        audio_play_purchase();
    }
}

static void settings_change_flag(Game *g,
                                 uint8_t flag,
                                 int horizontal,
                                 bool activate) {
    bool current =
        setting_enabled(
            g,
            flag
        );

    bool desired =
        current;

    if (activate)
        desired = !current;
    else if (horizontal < 0)
        desired = false;
    else if (horizontal > 0)
        desired = true;

    settings_set_flag(
        g,
        flag,
        desired
    );
}

#define SETTINGS_VISIBLE_ROWS 5

static int settings_items_on_page(int page) {
    return page == 0 ? 9 : 6;
}

static void settings_ensure_visible(Game *g) {
    if (!g)
        return;

    int count = settings_items_on_page(g->settings_page);
    int max_scroll = count - SETTINGS_VISIBLE_ROWS;
    if (max_scroll < 0) max_scroll = 0;

    if (g->settings_index < g->settings_scroll)
        g->settings_scroll = g->settings_index;

    if (g->settings_index >= g->settings_scroll + SETTINGS_VISIBLE_ROWS)
        g->settings_scroll =
            g->settings_index - SETTINGS_VISIBLE_ROWS + 1;

    if (g->settings_scroll < 0) g->settings_scroll = 0;
    if (g->settings_scroll > max_scroll) g->settings_scroll = max_scroll;
}

static void settings_switch_page(Game *g, int direction) {
    if (!g ||
        direction == 0) {
        return;
    }

    g->settings_page +=
        direction;

    if (g->settings_page < 0)
        g->settings_page = 1;

    if (g->settings_page > 1)
        g->settings_page = 0;

    if (g->settings_index < 0)
        g->settings_index = 0;

    int max_index =
        settings_items_on_page(g->settings_page) - 1;

    if (g->settings_index > max_index)
        g->settings_index = max_index;

    g->settings_scroll = 0;
    settings_ensure_visible(g);
    audio_play_menu_move();
}

static void reset_save_game(Game *g) {
    if (!g)
        return;

    

    memset(&g->progress, 0, sizeof(g->progress));
    ensure_progress_defaults(&g->progress);

    
    g->score = 0;
    g->destroyed = 0;
    g->combo = 1;
    g->max_combo = 1;
    g->run_best_combo = 1;
    g->run_distance = 0.0f;
    g->run_path_distance = 0.0f;
    g->run_xp_earned = 0;
    g->run_cash_earned = 0;
    g->normal_cash_kills = 0;
    g->run_end_xp_bonus = 0;
    g->run_end_cash_bonus = 0;
    g->run_levelups = 0;
    g->last_level_reward = 0;
    g->run_start_pos = g->player.pos;
    g->last_distance_sample = g->player.pos;

    generate_all_missions(g);
    apply_runtime_settings(g);
    save_progress(g);
    audio_play_purchase();
}

static void update_settings(Game *g, const GameInput *in) {
    if (!g || !in)
        return;

    if (g->reset_save_confirm) {
        if ((in->keys_down & KEY_B) ||
            (in->keys_down & KEY_START)) {
            g->reset_save_confirm = false;
            g->reset_save_choice = 0;
            audio_play_menu_move();
            return;
        }

        if (in->nav_x != 0 || in->nav_y != 0) {
            g->reset_save_choice =
                g->reset_save_choice == 0 ? 1 : 0;
            audio_play_menu_move();
        }

        if (in->keys_down & KEY_A) {
            if (g->reset_save_choice == 1)
                reset_save_game(g);
            else
                audio_play_menu_move();

            g->reset_save_confirm = false;
            g->reset_save_choice = 0;
        }
        return;
    }

    if (in->keys_down & KEY_L)
        settings_switch_page(g, -1);

    if (in->keys_down & KEY_R)
        settings_switch_page(g, 1);

    if (in->nav_y != 0) {
        int count =
            settings_items_on_page(g->settings_page);

        g->settings_index +=
            in->nav_y;

        if (g->settings_index < 0)
            g->settings_index = count - 1;

        if (g->settings_index >= count)
            g->settings_index = 0;

        settings_ensure_visible(g);
        audio_play_menu_move();
    }

    if ((in->keys_down & KEY_B) ||
        (in->keys_down & KEY_START)) {
        g->mode = MODE_TITLE;
        return;
    }

    int horizontal =
        in->nav_x;

    bool activate =
        (in->keys_down &
         KEY_A) != 0;

    if (horizontal == 0 &&
        !activate) {
        return;
    }

    if (g->settings_page == 0) {
        switch (g->settings_index) {
            case 0:
                settings_change_flag(g, SETTING_SFX, horizontal, activate);
                break;

            case 1:
                settings_change_flag(g, SETTING_MUSIC, horizontal, activate);
                break;

            case 2: {
                bool current = g->progress.bloom_enabled != 0;
                bool desired = current;

                if (activate)
                    desired = !current;
                else if (horizontal < 0)
                    desired = false;
                else if (horizontal > 0)
                    desired = true;

                if (desired != current) {
                    g->progress.bloom_enabled = desired ? 1u : 0u;
                    settings_save_and_feedback(g);
                }
                break;
            }

            case 3: {
                int dir = horizontal != 0 ? horizontal : 1;
                int value = clampi(
                    (int)g->progress.bloom_radius + dir,
                    (int)SETTINGS_BLOOM_RADIUS_MIN,
                    (int)SETTINGS_BLOOM_RADIUS_MAX
                );
                if (value != (int)g->progress.bloom_radius) {
                    g->progress.bloom_radius = (uint8_t)value;
                    settings_save_and_feedback(g);
                }
                break;
            }

            case 4: {
                int dir = horizontal != 0 ? horizontal : 1;
                int value = clampi(
                    (int)g->progress.bloom_intensity + dir,
                    (int)SETTINGS_BLOOM_INTENSITY_MIN,
                    (int)SETTINGS_BLOOM_INTENSITY_MAX
                );
                if (value != (int)g->progress.bloom_intensity) {
                    g->progress.bloom_intensity = (uint8_t)value;
                    settings_save_and_feedback(g);
                }
                break;
            }

            case 5: {
                int dir = horizontal != 0 ? horizontal : 1;
                int value = clampi(
                    (int)g->progress.bloom_quads + dir,
                    (int)SETTINGS_BLOOM_QUADS_MIN,
                    (int)SETTINGS_BLOOM_QUADS_MAX
                );
                if (value != (int)g->progress.bloom_quads) {
                    g->progress.bloom_quads = (uint8_t)value;
                    settings_save_and_feedback(g);
                }
                break;
            }

            case 6: {
                
                static const uint8_t FPS_ORDER[5] = {
                    SETTINGS_FPS_15,
                    SETTINGS_FPS_30,
                    SETTINGS_FPS_45,
                    SETTINGS_FPS_60,
                    SETTINGS_FPS_OFF
                };

                int current_index = 3;
                for (int i = 0; i < 5; ++i) {
                    if (FPS_ORDER[i] == g->progress.fps_limit) {
                        current_index = i;
                        break;
                    }
                }

                int dir = horizontal != 0 ? horizontal : 1;
                int next = current_index + dir;
                if (next < 0) next = 4;
                if (next > 4) next = 0;

                uint8_t value = FPS_ORDER[next];
                if (value != g->progress.fps_limit) {
                    g->progress.fps_limit = value;
                    settings_save_and_feedback(g);
                }
                break;
            }

            case 7: {
                int dir = horizontal != 0 ? horizontal : 1;
                int lod = (int)g->progress.force_lod + dir;

                if (lod < 0) lod = SETTINGS_FORCE_LOD_MAX;
                if (lod > (int)SETTINGS_FORCE_LOD_MAX) lod = 0;

                if (lod != (int)g->progress.force_lod) {
                    g->progress.force_lod = (uint8_t)lod;
                    settings_save_and_feedback(g);
                }
                break;
            }

            case 8:
                settings_change_flag(
                    g,
                    SETTING_STEREO_3D,
                    horizontal,
                    activate
                );
                break;

            default:
                break;
        }
    } else {
        switch (g->settings_index) {
            case 0:
                settings_change_flag(
                    g,
                    SETTING_BLOCK_ANIM,
                    horizontal,
                    activate
                );
                break;

            case 1:
                settings_change_flag(
                    g,
                    SETTING_BLOCK_OUTLINES,
                    horizontal,
                    activate
                );
                break;

            case 2:
                settings_change_flag(
                    g,
                    SETTING_LAVA_ANIM,
                    horizontal,
                    activate
                );
                break;

            case 3: {
                bool current =
                    setting_enabled(
                        g,
                        SETTING_PARTICLES
                    );

                bool enable =
                    current;

                if (activate)
                    enable = !current;
                else if (horizontal < 0)
                    enable = false;
                else if (horizontal > 0)
                    enable = true;

                if (enable != current) {
                    settings_set_flag(
                        g,
                        SETTING_PARTICLES,
                        enable
                    );

                    if (!enable) {
                        memset(
                            g->particles,
                            0,
                            sizeof(g->particles)
                        );
                    }
                }
                break;
            }

            case 4:
                settings_change_flag(
                    g,
                    SETTING_SCREENSHAKE,
                    horizontal,
                    activate
                );
                break;

            case 5:
                if (activate) {
                    g->reset_save_confirm = true;
                    g->reset_save_choice = 0;
                    audio_play_menu_move();
                }
                break;

            default:
                break;
        }
    }
}

static void update_pause(Game *g, const GameInput *in) {
    if ((in->keys_down & KEY_START) ||
        (in->keys_down & KEY_B)) {
        g->mode = MODE_PLAYING;
        return;
    }

    if (in->nav_y != 0) {
        g->pause_index +=
            in->nav_y;

        if (g->pause_index < 0)
            g->pause_index = 2;

        if (g->pause_index > 2)
            g->pause_index = 0;

        audio_play_menu_move();
    }

    if (in->keys_down & KEY_A) {
        if (g->pause_index == 0) {
            g->mode = MODE_PLAYING;
        } else if (g->pause_index == 1) {
            commit_run_records(g);
            save_progress(g);
            start_run(g);
        } else {
            commit_run_records(g);
            save_progress(g);
            g->mode = MODE_TITLE;
        }
    }
}

static void update_gameover(Game *g, const GameInput *in) {
    if (in->nav_y != 0) {
        g->gameover_index +=
            in->nav_y;

        if (g->gameover_index < 0)
            g->gameover_index = 2;

        if (g->gameover_index > 2)
            g->gameover_index = 0;

        audio_play_menu_move();
    }

    if (in->keys_down & KEY_A) {
        if (g->gameover_index == 0) {
            start_run(g);
        } else if (g->gameover_index == 1) {
            g->shop_return_mode =
                MODE_GAMEOVER;

            g->shop_page = 0;
            g->shop_index = 0;
            g->shop_select_anim = 0.0f;
            g->shop_touch_pending = -1;
            g->mode = MODE_SHOP;
        } else {
            g->mode = MODE_TITLE;
        }
    }

    if (in->keys_down & KEY_B)
        g->mode = MODE_TITLE;
}

static void update_missions(Game *g, const GameInput *in) {
    if (!g || !in)
        return;

    if (in->nav_y != 0) {
        g->missions_index +=
            in->nav_y;

        if (g->missions_index < 0)
            g->missions_index =
                MISSION_SLOT_COUNT - 1;

        if (g->missions_index >= MISSION_SLOT_COUNT)
            g->missions_index = 0;

        audio_play_menu_move();
    }

    if ((in->keys_down & KEY_B) ||
        (in->keys_down & KEY_START)) {
        g->mode = MODE_TITLE;
    }
}

static void update_achievements(Game *g, const GameInput *in) {
    if (!g || !in)
        return;

    if (in->nav_y != 0) {
        g->achievements_index +=
            in->nav_y;

        if (g->achievements_index < 0)
            g->achievements_index =
                ACHIEVEMENT_COUNT - 1;

        if (g->achievements_index >=
            ACHIEVEMENT_COUNT) {
            g->achievements_index = 0;
        }

        audio_play_menu_move();
    }

    if ((in->keys_down & KEY_B) ||
        (in->keys_down & KEY_START) ||
        (in->keys_down & KEY_A)) {
        g->mode = MODE_TITLE;
    }
}

static void shop_change_page(Game *g, int direction) {
    if (!g ||
        direction == 0) {
        return;
    }

    g->shop_page +=
        direction;

    if (g->shop_page < 0)
        g->shop_page =
            SHOP_PAGE_COUNT - 1;

    if (g->shop_page >=
        SHOP_PAGE_COUNT) {
        g->shop_page = 0;
    }

    g->shop_index = 0;
    g->shop_select_anim = 0.0f;

    audio_play_menu_move();
}

static void update_shop(Game *g, const GameInput *in, float dt) {
    if (!g || !in)
        return;

    if (g->shop_message_timer > 0.0f)
        g->shop_message_timer -= dt;

    g->shop_select_anim +=
        dt * 5.5f;

    if (g->shop_select_anim > 1.0f)
        g->shop_select_anim = 1.0f;

    
    if (in->keys_down & KEY_L) {
        shop_change_page(g, -1);
    } else if (in->keys_down & KEY_R) {
        shop_change_page(g, 1);
    } else {
        int count =
            shop_page_item_count(g);

        if (count <= 0)
            count = 1;

        int columns =
            g->shop_page == 0
            ? 2
            : 3;

        bool moved = false;

        if (in->nav_x != 0) {
            int col =
                g->shop_index %
                columns;

            int row =
                g->shop_index /
                columns;

            int new_col =
                col +
                in->nav_x;

            if (new_col < 0) {
                shop_change_page(g, -1);
            } else if (new_col >= columns) {
                shop_change_page(g, 1);
            } else {
                int next =
                    row * columns +
                    new_col;

                if (next >= 0 &&
                    next < count) {
                    g->shop_index =
                        next;

                    moved = true;
                }
            }
        }

        if (in->nav_y != 0) {
            int next =
                g->shop_index +
                in->nav_y *
                columns;

            if (next >= 0 &&
                next < count) {
                g->shop_index =
                    next;

                moved = true;
            }
        }

        if (moved) {
            g->shop_select_anim = 0.0f;
            audio_play_menu_move();
        }
    }

    if (in->keys_down & KEY_A) {
        if (g->shop_page == 0)
            buy_selected_upgrade(g);
        else
            activate_selected_cosmetic(g);
    }

    if ((in->keys_down & KEY_B) ||
        (in->keys_down & KEY_START)) {
        g->mode =
            g->shop_return_mode;
    }
}

void game_init(Game *g) {
    memset(g, 0, sizeof(*g));
    load_progress(g);
    apply_runtime_settings(g);

    for (int i = 0; i < MAX_STARS; ++i) {
        g->stars[i].x = rand() % 400;
        g->stars[i].y = rand() % 218;
        g->stars[i].brightness = 1 + rand() % 3;
    }

    g->mode = MODE_TITLE;
    g->shop_return_mode = MODE_TITLE;
    g->title_index = 0;
    g->title_select_anim = 1.0f;

    
    g->camera_zoom = CAMERA_ZOOM_MIN;
    g->camera_x = 0.0f;
    g->camera_y = 0.0f;

    g->settings_page = 0;
    g->settings_index = 0;
    g->settings_scroll = 0;
    g->reset_save_confirm = false;
    g->reset_save_choice = 0;

    g->shop_page = 0;
    g->shop_index = 0;
    g->shop_select_anim = 1.0f;
    g->shop_touch_pending = -1;

    g->missions_index = 0;
    g->achievements_index = 0;
    generate_all_missions(g);
    g->mission_notice_timer = 0.0f;
    g->mission_notice[0] = '\0';
    g->run_best_combo = 1;

    rope_clear(&g->rope);
}

void game_shutdown(Game *g) {
    if (!g) return;
    commit_run_records(g);
    save_progress(g);
}

void game_update(Game *g, const GameInput *in, float dt) {
    ++g->frame_counter;
    update_particles(g, dt);

    if (g->mission_notice_timer > 0.0f) {
        g->mission_notice_timer -= dt;
        if (g->mission_notice_timer < 0.0f)
            g->mission_notice_timer = 0.0f;
    }

    switch (g->mode) {
        case MODE_TITLE: update_title(g, in, dt); break;
        case MODE_PLAYING: update_playing(g, in, dt); break;
        case MODE_PAUSED: update_pause(g, in); break;
        case MODE_SHOP: update_shop(g, in, dt); break;
        case MODE_SETTINGS: update_settings(g, in); break;
        case MODE_MISSIONS: update_missions(g, in); break;
        case MODE_ACHIEVEMENTS: update_achievements(g, in); break;
        case MODE_GAMEOVER: update_gameover(g, in); break;
        default: g->mode = MODE_TITLE; break;
    }

    update_auto_missions(g);
}

