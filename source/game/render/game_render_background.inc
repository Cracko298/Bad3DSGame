static void draw_heart(Surface *s, int x, int y, Color c) {
    draw_rect(s, x, y + 2, 3, 4, c);
    draw_rect(s, x + 5, y + 2, 3, 4, c);
    draw_rect(s, x + 1, y, 6, 7, c);
    draw_rect(s, x + 2, y + 6, 4, 3, c);
}

static void draw_hearts(const Game *g, Surface *s) {
    int shown = g->max_health;
    if (shown > 12) shown = 12;
    for (int i = 0; i < shown; ++i) {
        Color c = i < g->health ? C_RED : rgb(58, 16, 18);
        draw_heart(s, 14 + i * 13, 14, c);
    }
}

static void draw_filled_circle_cheap(Surface *s,
                                     int cx,
                                     int cy,
                                     int radius,
                                     Color c) {
    if (!s ||
        radius <= 0) {
        return;
    }

    int rr =
        radius *
        radius;

    for (int y = -radius;
         y <= radius;
         ++y) {
        int inside =
            rr -
            y * y;

        if (inside < 0)
            continue;

        int half =
            (int)sqrtf(
                (float)inside
            );

        draw_rect(
            s,
            cx - half,
            cy + y,
            half * 2 + 1,
            1,
            c
        );
    }
}

static void draw_cloud_cluster(Surface *s,
                               int cx,
                               int cy,
                               int scale,
                               Color c) {
    if (!s)
        return;

    if (scale < 1)
        scale = 1;

    draw_filled_circle_cheap(s, cx - 10 * scale, cy + 2 * scale, 6 * scale, c);
    draw_filled_circle_cheap(s, cx, cy - 2 * scale, 8 * scale, c);
    draw_filled_circle_cheap(s, cx + 11 * scale, cy + 2 * scale, 6 * scale, c);

    draw_rect(
        s,
        cx - 15 * scale,
        cy + 1 * scale,
        31 * scale,
        7 * scale,
        c
    );
}

static void draw_city_skyline_layer(const Game *g,
                                    Surface *s,
                                    int lod,
                                    bool title_scene) {
    if (!g || !s)
        return;

    int scroll =
        title_scene
        ? 0
        : ((int)(
              -g->camera_x *
              0.035f /
              g->camera_zoom
          )) % 52;

    if (scroll < 0)
        scroll += 52;

    Color building = rgb(4, 9, 22);
    Color edge = rgb(9, 28, 48);
    Color window_a = rgb(28, 150, 210);
    Color window_b = rgb(235, 188, 52);

    int base_y =
        title_scene
        ? 236
        : 218;

    for (int i = -1;
         i < 9;
         ++i) {
        int x =
            i * 52 +
            scroll;

        int seed =
            i + 17;

        int w =
            30 +
            ((seed * 13) & 15);

        int h =
            46 +
            ((seed * 29) & 63);

        
        if (title_scene &&
            x > 100 &&
            x < 270) {
            h =
                20 +
                ((seed * 7) & 15);
        }

        int y =
            base_y -
            h;

        draw_rect(s, x, y, w, h, building);
        draw_rect_outline(s, x, y, w, h, edge);

        if (lod < 2) {
            int step_x =
                lod == 0
                ? 9
                : 13;

            int step_y =
                lod == 0
                ? 11
                : 16;

            for (int wy = y + 8;
                 wy < base_y - 5;
                 wy += step_y) {
                for (int wx = x + 6;
                     wx < x + w - 5;
                     wx += step_x) {
                    unsigned hash =
                        (unsigned)(
                            wx * 17 +
                            wy * 31 +
                            i * 13
                        );

                    if ((hash & 3u) == 0u)
                        continue;

                    draw_rect(
                        s,
                        wx,
                        wy,
                        2,
                        3,
                        (hash & 4u)
                        ? window_a
                        : window_b
                    );
                }
            }
        }

        if (((unsigned)seed & 1u) == 0u) {
            int ax =
                x +
                w / 2;

            draw_line(s, ax, y, ax, y - 9, edge);
            draw_pixel(s, ax, y - 10, C_CYAN);
        }
    }

    


    int tower_x =
        title_scene
        ? 330
        : 334 + scroll / 3;

    int tower_base =
        base_y;

    draw_rect(
        s,
        tower_x,
        tower_base - 128,
        28,
        128,
        rgb(3, 8, 20)
    );

    draw_rect_outline(
        s,
        tower_x,
        tower_base - 128,
        28,
        128,
        rgb(16, 54, 78)
    );

    draw_line(
        s,
        tower_x + 14,
        tower_base - 128,
        tower_x + 14,
        tower_base - 158,
        ui_accent_color(g)
    );

    if (lod < 2) {
        for (int y = tower_base - 114;
             y < tower_base - 8;
             y += 13) {
            draw_rect(
                s,
                tower_x + 7,
                y,
                4,
                4,
                C_YELLOW
            );

            draw_rect(
                s,
                tower_x + 17,
                y + 4,
                4,
                4,
                C_CYAN
            );
        }
    }
}

static bool storm_flash_active(const Game *g) {
    if (!g)
        return false;

    unsigned phase =
        g->frame_counter %
        420u;

    return
        phase == 31u ||
        phase == 32u ||
        phase == 38u ||
        phase == 39u;
}

static void draw_storm_layer(const Game *g,
                             Surface *s,
                             int lod,
                             bool title_scene) {
    if (!g || !s)
        return;

    float drift =
        title_scene
        ? (float)(g->frame_counter % 800u) * 0.05f
        : -g->camera_x * 0.018f +
          (float)(g->frame_counter % 1200u) * 0.03f;

    Color cloud = rgb(18, 23, 33);
    Color cloud_hi = rgb(27, 34, 46);

    for (int i = -1;
         i < 6;
         ++i) {
        int x =
            (int)(
                (float)(i * 92) +
                drift
            ) % 520;

        if (x < -80)
            x += 520;

        int y =
            7 +
            ((i * 19) & 15);

        draw_cloud_cluster(
            s,
            x,
            y,
            1,
            (i & 1)
            ? cloud
            : cloud_hi
        );
    }

    if (lod < 2) {
        int rain_step =
            lod == 0
            ? 17
            : 29;

        int rain_phase =
            (int)(
                g->frame_counter *
                3u
            ) %
            rain_step;

        for (int x = 5;
             x < 400;
             x += rain_step) {
            int y =
                42 +
                ((x * 11 +
                  rain_phase * 7) %
                 156);

            draw_line(
                s,
                x,
                y,
                x - 2,
                y + 7,
                rgb(20, 58, 82)
            );
        }
    }

    if (storm_flash_active(g)) {
        draw_line(
            s,
            0,
            1,
            399,
            1,
            rgb(130, 160, 185)
        );

        int bx =
            title_scene
            ? 306
            : 290;

        int by = 33;

        draw_line(s, bx, by, bx - 11, by + 22, C_WHITE);
        draw_line(s, bx - 11, by + 22, bx - 4, by + 22, C_WHITE);
        draw_line(s, bx - 4, by + 22, bx - 17, by + 47, C_WHITE);
        draw_line(s, bx - 17, by + 47, bx - 10, by + 45, C_CYAN);
    }
}

static void draw_title_decals(const Game *g,
                              Surface *s) {
    if (!g || !s)
        return;

    int lod =
        effective_lod_level(g);

    
    int star_stride =
        lod >= 2
        ? 4
        : (lod >= 1 ? 3 : 2);

    for (int i = 0;
         i < MAX_STARS;
         i += star_stride) {
        int sx =
            g->stars[i].x;

        int sy =
            g->stars[i].y %
            190;

        draw_pixel(
            s,
            sx,
            sy,
            (g->stars[i].brightness & 1)
            ? rgb(80, 128, 180)
            : rgb(190, 190, 150)
        );
    }

    



    draw_city_skyline_layer(
        g,
        s,
        lod,
        true
    );

    Color cloud =
        rgb(19, 25, 34);

    draw_cloud_cluster(s, 28, 9, 1, cloud);
    draw_cloud_cluster(s, 92, 4, 1, rgb(14, 20, 29));
    draw_cloud_cluster(s, 314, 5, 1, rgb(14, 20, 29));
    draw_cloud_cluster(s, 378, 11, 1, cloud);

    if (storm_flash_active(g)) {
        draw_line(s, 319, 27, 309, 43, C_WHITE);
        draw_line(s, 309, 43, 316, 43, C_WHITE);
        draw_line(s, 316, 43, 305, 60, C_CYAN);
    }

    
    draw_bloom_line(s, 76, 30, 101, 30, C_CYAN);
    draw_bloom_line(s, 299, 30, 324, 30, C_CYAN);

    draw_line(s, 68, 21, 75, 28, C_YELLOW);
    draw_line(s, 68, 31, 75, 24, C_YELLOW);
    draw_line(s, 325, 24, 332, 31, C_YELLOW);
    draw_line(s, 325, 28, 332, 21, C_YELLOW);

    draw_line(s, 42, 54, 42, 68, C_DIM);
    draw_circle_outline(s, 42, 72, 4, C_CYAN);
    draw_line(s, 42, 76, 47, 80, C_CYAN);
}

static void draw_aurora_layer(const Game *g,
                              Surface *s,
                              int lod) {
    int step = lod >= 2 ? 24 : 12;

    for (int band = 0; band < 3; ++band) {
        Color c =
            band == 0 ? rgb(35, 220, 160) :
            band == 1 ? rgb(45, 145, 255) :
                        rgb(180, 70, 235);

        int prev_x = 0;
        int prev_y = 28 + band * 17;

        for (int x = step; x <= 400; x += step) {
            float phase =
                (float)x * 0.025f +
                (float)g->frame_counter *
                (0.012f + (float)band * 0.004f);

            int y =
                28 +
                band * 17 +
                (int)lroundf(
                    sinf(phase) *
                    (8.0f + (float)band * 2.0f)
                );

            if (lod < 2)
                draw_bloom_line(s, prev_x, prev_y, x, y, c);
            else
                draw_line(s, prev_x, prev_y, x, y, c);

            prev_x = x;
            prev_y = y;
        }
    }
}

static void draw_cloud9_layer(const Game *g,
                              Surface *s,
                              int lod) {
    float drift =
        (float)(g->frame_counter % 1800u) *
        0.025f -
        g->camera_x *
        0.018f;

    float y_scroll =
        -g->camera_y * 0.020f;

    int count = lod >= 2 ? 4 : 7;

    for (int i = 0; i < count; ++i) {
        int x =
            (int)((float)(i * 78) + drift) %
            500;

        if (x < -60)
            x += 500;

        int y =
            18 +
            (i % 3) * 40 +
            (int)lroundf(y_scroll * (0.55f + 0.15f * (float)(i % 3)));

        Color c =
            (i & 1)
            ? rgb(225, 238, 248)
            : rgb(245, 250, 255);

        draw_cloud_cluster(s, x, y, 1, c);
    }

    if (lod < 2) {
        int prev_y =
            181 + (int)lroundf(y_scroll);
        for (int x = 8; x <= 400; x += 8) {
            float phase =
                (float)x * 0.035f +
                (float)g->frame_counter * 0.030f;
            int y =
                181 +
                (int)lroundf(y_scroll) +
                (int)lroundf(sinf(phase) * 2.0f);
            draw_bloom_line(
                s,
                x - 8,
                prev_y,
                x,
                y,
                rgb(180, 220, 255)
            );
            prev_y = y;
        }
    }
}

static void draw_background(const Game *g, Surface *s) {
    surface_clear(
        s,
        background_clear_color(g)
    );

    float px_scroll =
        g->camera_x *
        0.13f /
        g->camera_zoom;

    float py_scroll =
        g->camera_y *
        0.18f /
        g->camera_zoom;

    int lod =
        effective_lod_level(g);

    int star_stride =
        lod >= 2 ? 3 :
        lod >= 1 ? 2 : 1;

    if (g->progress.background_style != 8) {
    for (int i = 0;
         i < MAX_STARS;
         i += star_stride) {

        float xf =
            (float)g->stars[i].x -
            px_scroll;

        float yf =
            (float)g->stars[i].y -
            py_scroll;

        while (xf < 0.0f) xf += 400.0f;
        while (xf >= 400.0f) xf -= 400.0f;
        while (yf < 0.0f) yf += 218.0f;
        while (yf >= 218.0f) yf -= 218.0f;

        draw_pixel(
            s,
            (int)xf,
            (int)yf,
            background_star_color(
                g,
                g->stars[i].brightness
            )
        );
    }
    }

    if (g->progress.background_style == 5) {
        draw_city_skyline_layer(g, s, lod, false);
        return;
    }

    if (g->progress.background_style == 6) {
        draw_storm_layer(g, s, lod, false);
        return;
    }

    if (g->progress.background_style == 7) {
        draw_aurora_layer(g, s, lod);
        return;
    }

    if (g->progress.background_style == 8) {
        draw_cloud9_layer(g, s, lod);
        return;
    }

    if (lod >= 2)
        return;

    int scroll_x =
        ((int)(
            -g->camera_x *
            0.06f /
            g->camera_zoom
        )) %
        64;

    int scroll_y =
        ((int)(
            -g->camera_y *
            0.06f /
            g->camera_zoom
        )) %
        48;

    if (scroll_x < 0) scroll_x += 64;
    if (scroll_y < 0) scroll_y += 48;

    Color grid =
        rgb(6, 8, 10);

    switch (g->progress.background_style) {
        case 1:
            grid = rgb(5, 12, 28);
            break;

        case 2:
            grid = rgb(23, 7, 31);
            break;

        case 3:
            grid = rgb(4, 28, 14);
            break;

        case 4:
            grid = rgb(34, 12, 8);
            break;

        case 5:
            grid = rgb(7, 24, 45);
            break;

        case 6:
            grid = rgb(17, 24, 32);
            break;

        default:
            break;
    }

    int column_step =
        lod >= 1
        ? 96
        : 64;

    if (g->progress.background_style == 3) {
        


        for (int x = -64 + scroll_x;
             x < 432;
             x += column_step) {
            draw_line(
                s,
                x,
                0,
                x,
                239,
                grid
            );
        }

        for (int y = -48 + scroll_y;
             y < 260;
             y += 48) {
            draw_line(
                s,
                0,
                y,
                399,
                y,
                grid
            );
        }
    } else {
        for (int x = -40 + scroll_x;
             x < 420;
             x += column_step) {
            draw_rect_outline(
                s,
                x,
                -48 + scroll_y,
                34,
                288,
                grid
            );
        }
    }
}

