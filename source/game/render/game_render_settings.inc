static void draw_settings_row(Surface *top,
                              int y,
                              const char *label,
                              const char *value,
                              bool selected) {
    Color label_color =
        selected
        ? C_CYAN
        : C_WHITE;

    Color value_color =
        selected
        ? C_YELLOW
        : C_DIM;

    if (selected)
        draw_text(
            top,
            52,
            y,
            ">",
            1,
            C_CYAN
        );

    draw_text(
        top,
        72,
        y,
        label,
        1,
        label_color
    );

    if (value) {
        draw_text(
            top,
            280,
            y,
            value,
            1,
            value_color
        );
    }
}

static void draw_settings_slider(Surface *top,
                                 int y,
                                 const char *label,
                                 int value,
                                 int min_value,
                                 int max_value,
                                 const char *value_text,
                                 bool selected) {
    Color label_color = selected ? C_CYAN : C_WHITE;
    Color value_color = selected ? C_YELLOW : C_DIM;

    if (selected)
        draw_text(top, 52, y, ">", 1, C_CYAN);

    draw_text(top, 72, y, label, 1, label_color);

    int count = max_value - min_value + 1;
    if (count < 1) count = 1;

    const int track_x = 266;
    const int track_w = 68;
    const int gap = 2;
    int seg_w = (track_w - gap * (count - 1)) / count;
    if (seg_w < 3) seg_w = 3;

    for (int i = 0; i < count; ++i) {
        int x = track_x + i * (seg_w + gap);
        bool filled = i <= value - min_value;
        Color edge = selected ? rgb(72, 82, 90) : rgb(42, 47, 52);
        Color fill = selected ? C_YELLOW : rgb(145, 132, 42);

        draw_rect_outline(top, x, y + 1, seg_w, 8, edge);
        if (filled && seg_w > 2)
            draw_rect(top, x + 1, y + 2, seg_w - 2, 6, fill);
    }

    if (value_text)
        draw_text(top, 342, y, value_text, 1, value_color);
}

static void draw_settings(const Game *g, Surface *top) {
    draw_background(g, top);

    draw_rect(top, 34, 12, 332, 216, rgb(7, 9, 11));
    draw_rect_outline(top, 34, 12, 332, 216, C_DIM);

    draw_text_center(top, 200, 20, "SETTINGS", 3, C_WHITE);
    draw_text_center(
        top,
        200,
        52,
        g->settings_page == 0 ? "AUDIO + DISPLAY" : "EFFECTS",
        1,
        C_CYAN
    );

    int count = settings_items_on_page(g->settings_page);
    int first = g->settings_scroll;
    if (first < 0) first = 0;
    if (first > count - 1) first = count - 1;
    if (first < 0) first = 0;

    
    for (int slot = 0; slot < SETTINGS_VISIBLE_ROWS; ++slot) {
        int index = first + slot;
        if (index >= count)
            break;

        int y = 76 + slot * 24;
        bool selected = g->settings_index == index;

        if (g->settings_page == 0) {
            switch (index) {
                case 0:
                    draw_settings_row(
                        top, y, "SFX",
                        setting_enabled(g, SETTING_SFX) ? "ON" : "OFF",
                        selected
                    );
                    break;

                case 1:
                    draw_settings_row(
                        top, y, "MUSIC",
                        setting_enabled(g, SETTING_MUSIC) ? "ON" : "OFF",
                        selected
                    );
                    break;

                case 2:
                    draw_settings_row(
                        top, y, "BLOOM",
                        g->progress.bloom_enabled ? "ON" : "OFF",
                        selected
                    );
                    break;

                case 3: {
                    char value[8];
                    snprintf(value, sizeof(value), "%u",
                             (unsigned)g->progress.bloom_radius);
                    draw_settings_slider(
                        top, y, "BLOOM RADIUS",
                        (int)g->progress.bloom_radius,
                        SETTINGS_BLOOM_RADIUS_MIN,
                        SETTINGS_BLOOM_RADIUS_MAX,
                        value,
                        selected
                    );
                    break;
                }

                case 4: {
                    char value[8];
                    snprintf(value, sizeof(value), "%u",
                             (unsigned)g->progress.bloom_intensity);
                    draw_settings_slider(
                        top, y, "BLOOM INTENSITY",
                        (int)g->progress.bloom_intensity,
                        SETTINGS_BLOOM_INTENSITY_MIN,
                        SETTINGS_BLOOM_INTENSITY_MAX,
                        value,
                        selected
                    );
                    break;
                }

                case 5: {
                    int actual_quads =
                        ((int)g->progress.bloom_radius * 2 + 1) *
                        2 *
                        (int)g->progress.bloom_quads;
                    char value[8];
                    snprintf(value, sizeof(value), "%dQ", actual_quads);
                    draw_settings_slider(
                        top, y, "BLOOM QUADS",
                        (int)g->progress.bloom_quads,
                        SETTINGS_BLOOM_QUADS_MIN,
                        SETTINGS_BLOOM_QUADS_MAX,
                        value,
                        selected
                    );
                    break;
                }

                case 6: {
                    const char *fps = "60";
                    if (g->progress.fps_limit == SETTINGS_FPS_15) fps = "15";
                    else if (g->progress.fps_limit == SETTINGS_FPS_30) fps = "30";
                    else if (g->progress.fps_limit == SETTINGS_FPS_45) fps = "45";
                    else if (g->progress.fps_limit == SETTINGS_FPS_OFF) fps = "OFF";
                    draw_settings_row(top, y, "FPS LIMIT", fps, selected);
                    break;
                }

                case 7: {
                    const char *lod = "AUTO";
                    if (g->progress.force_lod == 1) lod = "0";
                    else if (g->progress.force_lod == 2) lod = "1";
                    else if (g->progress.force_lod == 3) lod = "2";
                    draw_settings_row(top, y, "FORCE LOD", lod, selected);
                    break;
                }

                case 8:
                    draw_settings_row(
                        top, y, "STEREO 3D",
                        setting_enabled(g, SETTING_STEREO_3D) ? "ON" : "OFF",
                        selected
                    );
                    break;
            }
        } else {
            switch (index) {
                case 0:
                    draw_settings_row(
                        top, y, "BLOCK ANIM",
                        setting_enabled(g, SETTING_BLOCK_ANIM) ? "ON" : "OFF",
                        selected
                    );
                    break;
                case 1:
                    draw_settings_row(
                        top, y, "BLOCK OUTLINES",
                        setting_enabled(g, SETTING_BLOCK_OUTLINES) ? "ON" : "OFF",
                        selected
                    );
                    break;
                case 2:
                    draw_settings_row(
                        top, y, "LAVA ANIM",
                        setting_enabled(g, SETTING_LAVA_ANIM) ? "ON" : "OFF",
                        selected
                    );
                    break;
                case 3:
                    draw_settings_row(
                        top, y, "PARTICLES",
                        setting_enabled(g, SETTING_PARTICLES) ? "ON" : "OFF",
                        selected
                    );
                    break;
                case 4:
                    draw_settings_row(
                        top, y, "SCREEN SHAKE",
                        setting_enabled(g, SETTING_SCREENSHAKE) ? "ON" : "OFF",
                        selected
                    );
                    break;
                case 5:
                    draw_settings_row(
                        top, y, "RESET SAVE GAME", "A", selected
                    );
                    break;
            }
        }
    }

    if (first > 0)
        draw_text_center(top, 200, 66, "^", 1, C_CYAN);

    if (first + SETTINGS_VISIBLE_ROWS < count)
        draw_text_center(top, 200, 190, "v", 1, C_CYAN);

    
    char dist_buf[32];
    snprintf(dist_buf, sizeof(dist_buf), "%lu",
             (unsigned long)g->progress.total_distance_traveled);
    draw_text(top, 58, 199, "TOTAL DIST", 1, C_DIM);
    draw_text(top, 276, 199, dist_buf, 1, C_YELLOW);

    char page[16];
    snprintf(page, sizeof(page), "%d / 2", g->settings_page + 1);
    draw_text(top, 58, 214, "L PAGE", 1, C_DIM);
    draw_text_center(top, 200, 214, page, 1, C_DIM);
    draw_text(top, 306, 214, "R PAGE", 1, C_DIM);

    if (g->reset_save_confirm) {
        const int box_x = 72;
        const int box_y = 66;
        const int box_w = 256;
        const int box_h = 112;

        draw_rect(top, box_x, box_y, box_w, box_h, rgb(3, 4, 6));
        draw_rect_outline(top, box_x, box_y, box_w, box_h, C_RED);
        draw_rect_outline(top, box_x + 3, box_y + 3, box_w - 6, box_h - 6, C_DIM);

        draw_text_center(top, 200, 79, "RESET SAVE GAME?", 2, C_RED);
        draw_text_center(top, 200, 105, "ALL PROGRESS WILL BE LOST", 1, C_WHITE);
        draw_text_center(top, 200, 120, "THIS CANNOT BE UNDONE", 1, C_DIM);

        Color no_color = g->reset_save_choice == 0 ? C_CYAN : C_DIM;
        Color yes_color = g->reset_save_choice == 1 ? C_RED : C_DIM;

        if (g->reset_save_choice == 0)
            draw_rect_outline(top, 111, 143, 72, 22, C_CYAN);
        else
            draw_rect_outline(top, 217, 143, 72, 22, C_RED);

        draw_text_center(top, 147, 150, "NO", 1, no_color);
        draw_text_center(top, 253, 150, "YES", 1, yes_color);
    }
}

