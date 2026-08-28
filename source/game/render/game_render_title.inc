static void draw_menu_choice(Surface *s,
                             int y,
                             const char *label,
                             bool selected) {
    Color c =
        selected
        ? C_CYAN
        : C_WHITE;

    if (selected)
        draw_text(
            s,
            116,
            y,
            ">",
            2,
            c
        );

    draw_text_center(
        s,
        200,
        y,
        label,
        2,
        c
    );
}

static void draw_title_starfield(const Game *g,
                                 Surface *top,
                                 Color a,
                                 Color b) {
    int stride =
        effective_lod_level(g) >= 2
        ? 4
        : 2;

    for (int i = 0;
         i < MAX_STARS;
         i += stride) {
        draw_pixel(
            top,
            g->stars[i].x,
            g->stars[i].y % 220,
            (g->stars[i].brightness & 1)
            ? a
            : b
        );
    }
}

static void draw_title_lava_bubbles(Surface *top,
                                    unsigned frame,
                                    int surface_y,
                                    Color lava,
                                    Color hi,
                                    int count) {
    draw_rect(
        top,
        0,
        surface_y,
        400,
        240 - surface_y,
        lava
    );

    draw_bloom_line(
        top,
        0,
        surface_y,
        399,
        surface_y,
        hi
    );

    for (int i = 0;
         i < count;
         ++i) {
        unsigned period =
            80u +
            (unsigned)(i * 11);

        unsigned phase =
            (frame +
             (unsigned)i * 31u) %
            period;

        float t =
            (float)phase /
            (float)period;

        int x =
            18 +
            ((i * 73) % 370);

        int y =
            surface_y -
            (int)(t * 28.0f);

        int r =
            2 +
            (i % 3);

        draw_circle_outline(
            top,
            x,
            y,
            r,
            hi
        );
    }
}

static void draw_title_fake_gameplay(const Game *g,
                                     Surface *top) {
    surface_clear(
        top,
        rgb(1, 5, 16)
    );

    draw_title_starfield(
        g,
        top,
        rgb(70, 120, 170),
        rgb(210, 205, 150)
    );

    draw_city_skyline_layer(
        g,
        top,
        effective_lod_level(g),
        true
    );

    Color lc =
        lava_color(g);

    Color lh =
        lava_highlight_color(g);

    draw_title_lava_bubbles(
        top,
        g->frame_counter,
        216,
        lc,
        lh,
        5
    );

    Color bc =
        block_color(
            g,
            BLOCK_PURPLE
        );

    Color bc2 =
        block_color(
            g,
            BLOCK_GREEN
        );

    draw_glow_square(
        top,
        88,
        145,
        10,
        bc
    );

    draw_glow_square(
        top,
        310,
        116,
        9,
        bc2
    );

    float swing =
        sinf(
            (float)g->frame_counter *
            0.025f
        );

    int px =
        200 +
        (int)(swing * 32.0f);

    int py =
        154 -
        (int)(
            fabsf(swing) *
            18.0f
        );

    draw_bloom_line(
        top,
        px,
        py,
        310,
        116,
        rope_color(g)
    );

    draw_bloom_rotated_rect(
        top,
        (float)px,
        (float)py,
        8.0f,
        5.0f,
        swing * 0.35f,
        player_color(g)
    );
}

static void draw_title_theme(const Game *g,
                             Surface *top) {
    if (!g || !top)
        return;

    switch (g->progress.title_style) {
        case 1: 
            surface_clear(
                top,
                rgb(5, 7, 12)
            );

            draw_storm_layer(
                g,
                top,
                effective_lod_level(g),
                true
            );

            draw_city_skyline_layer(
                g,
                top,
                2,
                true
            );
            break;

        case 2: 
            draw_title_fake_gameplay(
                g,
                top
            );
            break;

        case 3: { 
            surface_clear(
                top,
                rgb(16, 2, 2)
            );

            draw_title_starfield(
                g,
                top,
                rgb(90, 30, 20),
                rgb(170, 75, 24)
            );

            draw_title_lava_bubbles(
                top,
                g->frame_counter,
                214,
                lava_color(g),
                lava_highlight_color(g),
                12
            );
            break;
        }

        case 4: 
            surface_clear(
                top,
                C_BG
            );

            draw_title_starfield(
                g,
                top,
                rgb(90, 120, 160),
                C_WHITE
            );
            break;

        case 5: { 
            surface_clear(
                top,
                rgb(1, 13, 8)
            );

            int shift =
                (int)(
                    g->frame_counter %
                    32u
                );

            for (int x = -32 + shift;
                 x < 432;
                 x += 32) {
                draw_line(
                    top,
                    x,
                    0,
                    x,
                    239,
                    rgb(4, 38, 18)
                );
            }

            for (int y = 0;
                 y < 240;
                 y += 24) {
                draw_line(
                    top,
                    0,
                    y,
                    399,
                    y,
                    rgb(3, 29, 14)
                );
            }
            break;
        }

        case 6: { 
            surface_clear(
                top,
                rgb(13, 2, 21)
            );

            draw_title_starfield(
                g,
                top,
                rgb(255, 65, 170),
                C_CYAN
            );

            int pulse =
                12 +
                (int)(
                    5.0f *
                    sinf(
                        (float)g->frame_counter *
                        0.08f
                    )
                );

            draw_bloom_line(
                top,
                20,
                220,
                140,
                170,
                C_CYAN
            );

            draw_bloom_line(
                top,
                380,
                220,
                260,
                170,
                rgb(255, 65, 170)
            );

            for (int x = 40;
                 x < 390;
                 x += 55) {
                draw_rect_outline(
                    top,
                    x,
                    180 - (x % pulse),
                    22,
                    42 + (x % 35),
                    (x & 1)
                    ? C_CYAN
                    : rgb(255, 65, 170)
                );
            }
            break;
        }

        case 0:
        default:
            surface_clear(
                top,
                rgb(1, 5, 16)
            );

            draw_title_decals(
                g,
                top
            );
            break;
    }
}

static float title_selection_ease(const Game *g) {
    float t =
        clampf_local(
            g
            ? g->title_select_anim
            : 1.0f,
            0.0f,
            1.0f
        );

    return
        t * t *
        (3.0f - 2.0f * t);
}

static void draw_title_menu_choice(const Game *g,
                                   Surface *s,
                                   int y,
                                   const char *label,
                                   bool selected) {
    if (!selected) {
        draw_text_center(
            s,
            200,
            y,
            label,
            2,
            C_WHITE
        );
        return;
    }

    float ease =
        title_selection_ease(g);

    float pop =
        sinf(
            ease *
            3.14159265f
        ) *
        5.0f;

    float idle =
        ease > 0.98f
        ? sinf(
              (float)g->frame_counter *
              0.11f
          ) *
          1.5f
        : 0.0f;

    int yy =
        y -
        (int)lroundf(
            pop +
            idle
        );

    int label_half =
        (int)strlen(label) * 8 +
        10 +
        (int)lroundf(ease * 2.0f);

    const int text_h = 14;
    const int pad_y = 5;
    const int box_y = yy - pad_y;
    const int box_h = text_h + pad_y * 2;

    draw_rect_outline(
        s,
        200 - label_half,
        box_y,
        label_half * 2,
        box_h,
        C_CYAN
    );

    draw_rect_outline(
        s,
        200 - label_half - 3,
        box_y - 3,
        label_half * 2 + 6,
        box_h + 6,
        rgb(20, 80, 100)
    );

    int accent_gap =
        16 +
        (int)lroundf(ease * 4.0f);

    int accent_len =
        24 +
        (int)lroundf(ease * 8.0f);

    int left_edge = 200 - label_half - 3;
    int right_edge = 200 + label_half + 3;
    int line_y = yy + text_h / 2;

    draw_bloom_line(
        s,
        left_edge - accent_gap - accent_len,
        line_y,
        left_edge - accent_gap,
        line_y,
        C_CYAN
    );

    draw_bloom_line(
        s,
        right_edge + accent_gap,
        line_y,
        right_edge + accent_gap + accent_len,
        line_y,
        C_CYAN
    );

    draw_text_center(
        s,
        200,
        yy + 1,
        label,
        2,
        rgb(20, 80, 100)
    );

    draw_text_center(
        s,
        200,
        yy,
        label,
        2,
        C_CYAN
    );
}

static void draw_title(const Game *g, Surface *top) {
    draw_title_theme(
        g,
        top
    );

    float logo_t =
        (float)g->frame_counter *
        0.004f;

    const float title_angle_limit =
        0.06981317f; 

    float title_angle =
        sinf(logo_t) *
        title_angle_limit;

    float logo_zoom =
        1.0f +
        sinf(logo_t * 0.72f + 0.45f) *
        0.022f;

    draw_bloom_text_center_animated(
        top,
        200.0f,
        12.0f,
        "BAD GAME",
        3,
        logo_zoom,
        title_angle,
        0.0f,
        0.0f,
        1,
        48u,
        ui_accent_color(g)
    );

    float sub_zoom =
        1.0f +
        sinf(logo_t * 0.72f + 0.45f) *
        0.012f;

    draw_bloom_text_center_animated(
        top,
        200.0f,
        43.0f,
        "3DS EDITION - VERSION 0.2.0",
        1,
        sub_zoom,
        title_angle,
        0.0f,
        0.0f,
        0,
        28u,
        C_CYAN
    );

    static const char *ITEMS[6] = {
        "PLAY",
        "SHOP",
        "MISSIONS",
        "ACHIEVEMENTS",
        "SETTINGS",
        "QUIT"
    };

    for (int i = 0;
         i < 6;
         ++i) {
        draw_title_menu_choice(
            g,
            top,
            66 + i * 27,
            ITEMS[i],
            g->title_index == i
        );
    }
}

