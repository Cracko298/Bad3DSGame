static void draw_upgrade_icon(Surface *s, int item, int cx, int cy, bool owned) {
    Color faint = owned ? C_WHITE : rgb(185, 190, 195);
    switch (item) {
        case UPG_HP:
            draw_heart(s, cx - 4, cy - 4, C_RED);
            break;
        case UPG_ROPE:
            draw_line(s, cx - 16, cy + 8, cx + 14, cy - 9, C_GREEN);
            draw_glow_square(s, cx - 14, cy + 8, 3, C_CYAN);
            break;
        case UPG_COMBO:
            draw_text_center(s, cx, cy - 5, "X4", 2, C_PURPLE);
            break;
        case UPG_BULLETS:
            draw_glow_square(s, cx - 10, cy, 3, C_CYAN);
            for (int n = -2; n <= 2; ++n)
                draw_line(s,
                          cx - 2,
                          cy,
                          cx + 18,
                          cy + n * 5,
                          C_CYAN);
            break;
        case UPG_GREEN:
            draw_rect_outline(s, cx - 12, cy - 12, 24, 24, C_GREEN);
            draw_rect(s, cx - 5, cy - 5, 10, 10, C_BG);
            break;
        case UPG_MONEY:
            draw_rect_outline(s, cx - 12, cy - 12, 24, 24, C_MONEY);
            draw_text_center(s, cx, cy - 3, "$", 1, C_YELLOW);
            break;
        case UPG_THICC:
            draw_glow_square(s, cx - 4, cy, 8, C_CYAN);
            draw_glow_square(s, cx + 12, cy + 4, 3, C_RED);
            break;
        case UPG_LASER:
            draw_line(s, cx - 16, cy + 10, cx + 15, cy - 10, C_GREEN);
            draw_glow_square(s, cx - 16, cy + 10, 3, C_CYAN);
            draw_rect_outline(s, cx + 6, cy - 4, 6, 6, C_PURPLE);
            break;
        default:
            draw_glow_square(s, cx, cy, 5, faint);
            break;
    }
}

static float shop_selection_ease(const Game *g) {
    if (!g)
        return 1.0f;

    float t =
        clampf_local(
            g->shop_select_anim,
            0.0f,
            1.0f
        );

    float inv =
        1.0f -
        t;

    return
        1.0f -
        inv * inv * inv;
}

static Color preview_background_color(uint8_t style) {
    switch (style) {
        case 1: return rgb(2, 7, 20);
        case 2: return rgb(14, 3, 22);
        case 3: return rgb(1, 13, 8);
        case 4: return rgb(20, 7, 5);
        case 5: return rgb(1, 5, 16);
        case 6: return rgb(5, 7, 12);
        case 7: return rgb(2, 8, 24);
        case 8: return rgb(72, 142, 205);
        default: return C_BG;
    }
}

static Color preview_lava_color(uint8_t style) {
    switch (style) {
        case 1: return rgb(18, 210, 245);
        case 2: return rgb(190, 52, 235);
        case 3: return rgb(70, 235, 70);
        case 4: return rgb(245, 245, 250);
        case 5: return rgb(255, 65, 170);
        case 6: return rgb(35, 82, 255);
        case 7: return rgb(220, 80, 235);
        default: return C_ORANGE;
    }
}

static Color preview_block_theme_color(uint8_t theme,
                                       BlockType t) {
    switch (theme) {
        case 1:
            if (t == BLOCK_RED) return rgb(74, 150, 255);
            if (t == BLOCK_PURPLE) return rgb(120, 210, 255);
            return rgb(110, 255, 220);

        case 2:
            if (t == BLOCK_RED) return rgb(255, 72, 62);
            if (t == BLOCK_PURPLE) return rgb(255, 86, 175);
            return rgb(255, 154, 58);

        case 3:
            if (t == BLOCK_RED) return rgb(205, 205, 205);
            if (t == BLOCK_PURPLE) return rgb(150, 150, 160);
            return rgb(235, 235, 235);

        case 4:
            if (t == BLOCK_RED) return rgb(255, 45, 125);
            if (t == BLOCK_PURPLE) return rgb(156, 68, 255);
            return rgb(25, 255, 125);

        case 5:
            if (t == BLOCK_RED) return rgb(42, 78, 160);
            if (t == BLOCK_PURPLE) return rgb(78, 54, 170);
            return rgb(32, 145, 142);

        case 6:
            if (t == BLOCK_RED) return rgb(255, 86, 138);
            if (t == BLOCK_PURPLE) return rgb(202, 95, 255);
            return rgb(88, 255, 176);

        default:
            if (t == BLOCK_RED) return C_RED;
            if (t == BLOCK_PURPLE) return C_PURPLE;
            return C_GREEN;
    }
}

static void draw_preview_shape(Surface *s,
                               uint8_t style,
                               int cx,
                               int cy,
                               float size,
                               float angle,
                               Color c) {
    float half =
        7.0f *
        size;

    switch (style) {
        case 1:
            draw_bloom_rotated_square(
                s,
                (float)cx,
                (float)cy,
                half,
                angle + 0.78539816f,
                c
            );
            break;

        case 2:
            draw_bloom_circle(
                s,
                cx,
                cy,
                (int)fmaxf(2.0f, half),
                c
            );
            break;

        case 3: {
            float fx = cosf(angle);
            float fy = sinf(angle);
            float rx = -fy;
            float ry = fx;

            int x0 =
                (int)(
                    (float)cx +
                    fx * half * 1.35f
                );

            int y0 =
                (int)(
                    (float)cy +
                    fy * half * 1.35f
                );

            int x1 =
                (int)(
                    (float)cx -
                    fx * half * 0.7f +
                    rx * half * 0.8f
                );

            int y1 =
                (int)(
                    (float)cy -
                    fy * half * 0.7f +
                    ry * half * 0.8f
                );

            int x2 =
                (int)(
                    (float)cx -
                    fx * half * 0.7f -
                    rx * half * 0.8f
                );

            int y2 =
                (int)(
                    (float)cy -
                    fy * half * 0.7f -
                    ry * half * 0.8f
                );

            draw_bloom_triangle(
                s,
                x0, y0,
                x1, y1,
                x2, y2,
                c
            );
            break;
        }

        case 4:
            draw_bloom_rotated_rect(
                s,
                (float)cx,
                (float)cy,
                half * 1.2f,
                half * 0.7f,
                angle,
                c
            );

            draw_bloom_circle(
                s,
                (int)(
                    (float)cx -
                    cosf(angle) *
                    half * 0.7f
                ),
                (int)(
                    (float)cy -
                    sinf(angle) *
                    half * 0.7f
                ),
                (int)fmaxf(2.0f, half * 0.28f),
                C_WHITE
            );
            break;

        case 5: {
            float outer =
                half *
                1.15f;

            float inner =
                outer *
                0.45f;

            int px[10];
            int py[10];

            for (int i = 0;
                 i < 10;
                 ++i) {
                float radius =
                    (i & 1)
                    ? inner
                    : outer;

                float a =
                    angle -
                    1.57079633f +
                    (float)i *
                    0.31415927f;

                px[i] =
                    (int)lroundf(
                        (float)cx +
                        cosf(a) *
                        radius
                    );

                py[i] =
                    (int)lroundf(
                        (float)cy +
                        sinf(a) *
                        radius
                    );
            }

            for (int i = 0;
                 i < 10;
                 ++i) {
                int j =
                    (i + 1) %
                    10;

                draw_bloom_line(
                    s,
                    px[i], py[i],
                    px[j], py[j],
                    c
                );
            }

            draw_circle_filled(
                s,
                cx,
                cy,
                (int)fmaxf(2.0f, inner * 0.45f),
                c
            );
            break;
        }

        case 6: {
            float radius =
                half *
                1.10f;

            int px[6];
            int py[6];

            for (int i = 0;
                 i < 6;
                 ++i) {
                float a =
                    angle +
                    (float)i *
                    1.04719755f;

                px[i] =
                    (int)lroundf(
                        (float)cx +
                        cosf(a) *
                        radius
                    );

                py[i] =
                    (int)lroundf(
                        (float)cy +
                        sinf(a) *
                        radius
                    );
            }

            for (int i = 0;
                 i < 6;
                 ++i) {
                int j =
                    (i + 1) %
                    6;

                draw_bloom_line(
                    s,
                    px[i], py[i],
                    px[j], py[j],
                    c
                );
            }

            draw_rotated_rect(
                s,
                (float)cx,
                (float)cy,
                radius * 0.58f,
                radius * 0.42f,
                angle,
                c
            );
            break;
        }

        case 7: {
            Color peel = rgb(250, 232, 72);
            Color tip = rgb(114, 82, 24);
            float radius = half * 1.08f;
            int prev_x = 0;
            int prev_y = 0;
            bool have_prev = false;

            for (int i = 0; i < 7; ++i) {
                float t = -0.78f + (float)i * 0.26f;
                float a = angle + t;
                int px = (int)lroundf((float)cx + cosf(a) * radius);
                int py = (int)lroundf((float)cy + sinf(a) * radius * 0.72f);
                draw_bloom_circle(s, px, py,
                                  (int)fmaxf(2.0f, half * 0.22f), peel);
                if (have_prev)
                    draw_bloom_line(s, prev_x, prev_y, px, py, peel);
                prev_x = px;
                prev_y = py;
                have_prev = true;
            }

            draw_bloom_circle(s,
                              (int)lroundf((float)cx + cosf(angle - 0.90f) * radius * 1.02f),
                              (int)lroundf((float)cy + sinf(angle - 0.90f) * radius * 0.74f),
                              1,
                              tip);
            draw_bloom_circle(s,
                              (int)lroundf((float)cx + cosf(angle + 0.92f) * radius * 1.00f),
                              (int)lroundf((float)cy + sinf(angle + 0.92f) * radius * 0.74f),
                              1,
                              tip);
            break;
        }

        case 0:
        default:
            draw_bloom_rotated_rect(
                s,
                (float)cx,
                (float)cy,
                half * 1.2f,
                half * 0.75f,
                angle,
                c
            );
            break;
    }
}

static void draw_cosmetic_preview_item(const Game *g,
                                       Surface *s,
                                       int item,
                                       int cx,
                                       int cy,
                                       float scale,
                                       float twist) {
    if (!g ||
        !s ||
        item < 0 ||
        item >= COSMETIC_COUNT) {
        return;
    }

    const CosmeticDef *d =
        &COSMETICS[item];

    Color pc =
        player_color(g);

    Color rc =
        rope_color(g);

    switch (d->kind) {
        case COS_PLAYER_COLOR: {
            Color c =
                PLAYER_COLORS[
                    clampi(
                        d->style,
                        0,
                        8
                    )
                ];

            draw_preview_shape(
                s,
                g->progress.shape_style,
                cx,
                cy,
                scale,
                twist,
                c
            );
            break;
        }

        case COS_ROPE_COLOR: {
            Color c =
                ROPE_COLORS[
                    clampi(
                        d->style,
                        0,
                        8
                    )
                ];

            float len =
                18.0f *
                scale;

            float cs =
                cosf(twist);

            float sn =
                sinf(twist);

            draw_bloom_line(
                s,
                (int)(
                    (float)cx -
                    cs * len
                ),
                (int)(
                    (float)cy -
                    sn * len
                ),
                (int)(
                    (float)cx +
                    cs * len
                ),
                (int)(
                    (float)cy +
                    sn * len
                ),
                c
            );
            break;
        }

        case COS_ROPE_ANIM: {
            int segs = 6;
            int left =
                cx -
                (int)(20.0f * scale);

            int right =
                cx +
                (int)(20.0f * scale);

            int width =
                right -
                left;

            for (int i = 0;
                 i < segs;
                 ++i) {
                if (d->style == 1 &&
                    (i & 1)) {
                    continue;
                }

                int x0 =
                    left +
                    width * i /
                    segs;

                int x1 =
                    left +
                    width * (i + 1) /
                    segs;

                int y0 = cy;
                int y1 = cy;

                Color c = rc;

                if (d->style == 2 &&
                    (i & 1)) {
                    c = pc;
                }

                if (d->style == 3 &&
                    ((i + (int)(g->frame_counter / 5u)) & 1)) {
                    c = C_WHITE;
                }

                if (d->style == 4) {
                    y0 +=
                        (int)lroundf(
                            sinf(
                                (float)i +
                                (float)g->frame_counter *
                                0.08f
                            ) *
                            4.0f
                        );

                    y1 +=
                        (int)lroundf(
                            sinf(
                                (float)(i + 1) +
                                (float)g->frame_counter *
                                0.08f
                            ) *
                            4.0f
                        );
                }

                if (d->style == 5 &&
                    (i +
                     (int)(g->frame_counter / 3u)) %
                    4 == 0) {
                    c = C_WHITE;
                }

                if (d->style == 6) {
                    c =
                        ((i +
                          (int)(g->frame_counter / 2u)) &
                         1)
                        ? C_WHITE
                        : C_CYAN;

                    y0 +=
                        ((i * 5 +
                          (int)(g->frame_counter / 2u)) %
                         5) -
                        2;

                    y1 +=
                        (((i + 1) * 5 +
                          (int)(g->frame_counter / 2u)) %
                         5) -
                        2;
                }

                if (d->style == 7) {
                    float rp =
                        (float)i *
                        0.85f +
                        (float)g->frame_counter *
                        0.09f;

                    c =
                        rgb(
                            (uint8_t)(
                                128.0f +
                                127.0f *
                                sinf(rp)
                            ),
                            (uint8_t)(
                                128.0f +
                                127.0f *
                                sinf(rp + 2.094f)
                            ),
                            (uint8_t)(
                                128.0f +
                                127.0f *
                                sinf(rp + 4.188f)
                            )
                        );
                }

                draw_bloom_line(
                    s,
                    x0,
                    y0,
                    x1,
                    y1,
                    c
                );
            }
            break;
        }

        case COS_SHAPE:
            draw_preview_shape(
                s,
                d->style,
                cx,
                cy,
                scale,
                twist,
                pc
            );
            break;

        case COS_PLAYER_ANIM: {
            float phase =
                (float)g->frame_counter *
                0.10f;

            float anim_scale =
                scale;

            float angle =
                twist;

            if (d->style == 1)
                anim_scale *=
                    1.0f +
                    sinf(phase) *
                    0.10f;
            else if (d->style == 2)
                angle +=
                    sinf(phase) *
                    0.28f;
            else if (d->style == 3)
                anim_scale *=
                    1.0f +
                    sinf(phase * 1.6f) *
                    0.12f;
            else if (d->style == 5)
                angle +=
                    phase *
                    0.85f;
            else if (d->style == 6) {
                int jitter =
                    (int)(g->frame_counter % 5u) -
                    2;

                angle +=
                    (float)jitter *
                    0.08f;

                anim_scale *=
                    1.0f +
                    (float)jitter *
                    0.025f;
            }

            draw_preview_shape(
                s,
                g->progress.shape_style,
                cx,
                cy,
                anim_scale,
                angle,
                d->style == 4
                ? rgb(
                      (uint8_t)(pc.r / 2),
                      (uint8_t)(pc.g / 2),
                      (uint8_t)(pc.b / 2)
                  )
                : pc
            );
            break;
        }

        case COS_HAT: {
            draw_preview_shape(
                s,
                g->progress.shape_style,
                cx,
                cy + 7,
                scale * 0.85f,
                twist,
                pc
            );

            draw_hat_style(
                s,
                d->style,
                (float)cx,
                (float)(cy + 7),
                8.0f * scale,
                twist,
                pc,
                g->frame_counter
            );
            break;
        }

        case COS_BLOCK_THEME: {
            Color a =
                preview_block_theme_color(
                    d->style,
                    BLOCK_RED
                );

            Color b =
                preview_block_theme_color(
                    d->style,
                    BLOCK_PURPLE
                );

            Color c =
                preview_block_theme_color(
                    d->style,
                    BLOCK_GREEN
                );

            int hs =
                (int)fmaxf(
                    3.0f,
                    5.0f * scale
                );

            draw_glow_square(s, cx - 14, cy, hs, a);
            draw_glow_square(s, cx, cy - 5, hs, b);
            draw_glow_square(s, cx + 14, cy, hs, c);
            break;
        }

        case COS_BACKGROUND: {
            Color bg =
                preview_background_color(
                    d->style
                );

            int hw =
                (int)(22.0f * scale);

            int hh =
                (int)(12.0f * scale);

            draw_rect(
                s,
                cx - hw,
                cy - hh,
                hw * 2,
                hh * 2,
                bg
            );

            draw_rect_outline(
                s,
                cx - hw,
                cy - hh,
                hw * 2,
                hh * 2,
                C_DIM
            );

            for (int i = -2;
                 i <= 2;
                 ++i) {
                draw_pixel(
                    s,
                    cx + i * 7,
                    cy + (i & 1 ? -5 : 4),
                    C_WHITE
                );
            }

            if (d->style == 5) {
                for (int i = -3;
                     i <= 3;
                     ++i) {
                    int bh =
                        4 +
                        ((i * i * 3 + 7) & 7);

                    draw_rect(
                        s,
                        cx + i * 7 - 3,
                        cy + hh - bh,
                        6,
                        bh,
                        rgb(7, 20, 35)
                    );
                }

                draw_line(
                    s,
                    cx + 14,
                    cy + hh - 11,
                    cx + 14,
                    cy + hh - 18,
                    C_CYAN
                );
            } else if (d->style == 6) {
                draw_cloud_cluster(
                    s,
                    cx,
                    cy - 6,
                    1,
                    rgb(30, 36, 48)
                );

                draw_line(
                    s,
                    cx + 7,
                    cy,
                    cx + 1,
                    cy + 8,
                    C_CYAN
                );
            } else if (d->style == 7) {
                draw_bloom_line(
                    s,
                    cx - hw + 2,
                    cy - 5,
                    cx + hw - 2,
                    cy - 1,
                    rgb(35, 220, 160)
                );

                draw_bloom_line(
                    s,
                    cx - hw + 5,
                    cy + 1,
                    cx + hw - 4,
                    cy + 5,
                    rgb(180, 70, 235)
                );
            } else if (d->style == 8) {
                draw_cloud_cluster(
                    s,
                    cx - 8,
                    cy - 4,
                    1,
                    rgb(245, 250, 255)
                );

                draw_cloud_cluster(
                    s,
                    cx + 12,
                    cy + 5,
                    1,
                    rgb(225, 238, 248)
                );
            }

            break;
        }

        case COS_LAVA_COLOR: {
            Color lc =
                preview_lava_color(
                    d->style
                );

            int half =
                (int)(
                    22.0f *
                    scale
                );

            draw_rect(
                s,
                cx - half,
                cy,
                half * 2,
                9,
                lc
            );

            draw_bloom_line(
                s,
                cx - half,
                cy,
                cx + half,
                cy,
                lc
            );
            break;
        }

        case COS_LAVA_ANIM: {
            int half =
                (int)(
                    22.0f *
                    scale
                );

            int prev_x =
                cx - half;

            int prev_y =
                cy;

            for (int i = 1;
                 i <= 8;
                 ++i) {
                int x =
                    cx -
                    half +
                    half * 2 * i /
                    8;

                float phase =
                    (float)i * 0.8f +
                    (float)g->frame_counter *
                    0.08f;

                float amp =
                    d->style == 0 ? 1.0f :
                    d->style == 1 ? 2.0f :
                    d->style == 2 ? 4.0f :
                    d->style == 3 ? 3.0f :
                    d->style == 4 ? 5.0f :
                    d->style == 5 ? 2.0f :
                                    3.0f;

                int y =
                    cy +
                    (int)lroundf(
                        sinf(phase) *
                        amp
                    );

                draw_bloom_line(
                    s,
                    prev_x,
                    prev_y,
                    x,
                    y,
                    preview_lava_color(
                        g->progress.lava_color_style
                    )
                );

                prev_x = x;
                prev_y = y;
            }

            if (d->style == 5) {
                for (int i = 0;
                     i < 5;
                     ++i) {
                    int bx =
                        cx -
                        18 +
                        i * 9;

                    int by =
                        cy -
                        4 -
                        ((int)(g->frame_counter / 3u) +
                          i * 7) %
                        14;

                    draw_circle_outline(
                        s,
                        bx,
                        by,
                        1 + (i & 1),
                        preview_lava_color(
                            g->progress.lava_color_style
                        )
                    );
                }
            } else if (d->style == 6) {
                int jet =
                    (int)(
                        12.0f *
                        fabsf(
                            sinf(
                                (float)g->frame_counter *
                                0.08f
                            )
                        )
                    );

                draw_bloom_line(
                    s,
                    cx,
                    cy,
                    cx,
                    cy - jet,
                    preview_lava_color(
                        g->progress.lava_color_style
                    )
                );
            }

            break;
        }

        case COS_TITLE_THEME: {
            int hw =
                (int)(
                    23.0f *
                    scale
                );

            int hh =
                (int)(
                    13.0f *
                    scale
                );

            Color bg =
                d->style == 1 ? rgb(5, 7, 12) :
                d->style == 2 ? rgb(1, 5, 16) :
                d->style == 3 ? rgb(16, 2, 2) :
                d->style == 4 ? C_BG :
                d->style == 5 ? rgb(1, 13, 8) :
                d->style == 6 ? rgb(13, 2, 21) :
                                rgb(1, 5, 16);

            draw_rect(
                s,
                cx - hw,
                cy - hh,
                hw * 2,
                hh * 2,
                bg
            );

            draw_rect_outline(
                s,
                cx - hw,
                cy - hh,
                hw * 2,
                hh * 2,
                C_DIM
            );

            if (d->style == 0) {
                for (int i = -3;
                     i <= 3;
                     ++i) {
                    int bh =
                        4 +
                        ((i * i + 5) & 8);

                    draw_rect(
                        s,
                        cx + i * 7 - 3,
                        cy + hh - bh,
                        6,
                        bh,
                        rgb(7, 20, 35)
                    );
                }
            } else if (d->style == 1) {
                draw_cloud_cluster(
                    s,
                    cx,
                    cy - 6,
                    1,
                    rgb(30, 36, 48)
                );

                draw_line(
                    s,
                    cx + 5,
                    cy - 1,
                    cx,
                    cy + 9,
                    C_CYAN
                );
            } else if (d->style == 2) {
                draw_glow_square(
                    s,
                    cx - 10,
                    cy - 1,
                    3,
                    C_PURPLE
                );

                draw_glow_square(
                    s,
                    cx + 11,
                    cy - 7,
                    3,
                    C_GREEN
                );

                draw_bloom_line(
                    s,
                    cx,
                    cy + 3,
                    cx + 11,
                    cy - 7,
                    C_CYAN
                );

                draw_rect(
                    s,
                    cx - hw,
                    cy + hh - 5,
                    hw * 2,
                    5,
                    C_ORANGE
                );
            } else if (d->style == 3) {
                draw_rect(
                    s,
                    cx - hw,
                    cy + 2,
                    hw * 2,
                    hh - 2,
                    C_ORANGE
                );

                for (int i = -2;
                     i <= 2;
                     ++i) {
                    draw_circle_outline(
                        s,
                        cx + i * 8,
                        cy - (i & 1 ? 2 : 6),
                        2,
                        C_YELLOW
                    );
                }
            } else if (d->style == 4) {
                for (int i = -2;
                     i <= 2;
                     ++i) {
                    draw_pixel(
                        s,
                        cx + i * 8,
                        cy + (i & 1 ? -5 : 4),
                        C_WHITE
                    );
                }
            } else if (d->style == 5) {
                for (int i = -2;
                     i <= 2;
                     ++i) {
                    draw_line(
                        s,
                        cx + i * 8,
                        cy - hh,
                        cx + i * 8,
                        cy + hh,
                        rgb(25, 120, 60)
                    );
                }

                draw_line(
                    s,
                    cx - hw,
                    cy,
                    cx + hw,
                    cy,
                    rgb(25, 120, 60)
                );
            } else {
                draw_bloom_line(
                    s,
                    cx - hw + 2,
                    cy + hh - 3,
                    cx - 3,
                    cy - 2,
                    C_CYAN
                );

                draw_bloom_line(
                    s,
                    cx + hw - 2,
                    cy + hh - 3,
                    cx + 3,
                    cy - 2,
                    rgb(255, 65, 170)
                );
            }

            break;
        }


        case COS_UI_THEME: {
            Color panel =
                d->style == 1 ? rgb(9, 28, 44) :
                d->style == 2 ? rgb(40, 18, 10) :
                d->style == 3 ? rgb(22, 10, 32) :
                                rgb(7, 10, 14);
            Color accent =
                d->style == 1 ? rgb(80, 195, 255) :
                d->style == 2 ? rgb(255, 170, 70) :
                d->style == 3 ? rgb(195, 110, 255) :
                                C_CYAN;
            draw_rect(s, cx - 26, cy - 16, 52, 32, panel);
            draw_rect_outline(s, cx - 26, cy - 16, 52, 32, accent);
            draw_rect(s, cx - 20, cy - 8, 40, 6, rgb(16, 20, 24));
            draw_rect(s, cx - 20, cy + 3, 28, 5, rgb(16, 20, 24));
            draw_bloom_line(s, cx - 18, cy - 11, cx + 18, cy - 11, accent);
            draw_text_center(s, cx, cy + 9, "UI", 1, accent);
            break;
        }

        default:
            break;
    }
}

static void draw_upgrade_shop_card(const Game *g,
                                   Surface *s,
                                   int item,
                                   int x,
                                   int y,
                                   bool selected) {
    int level =
        g->progress.levels[item];

    int max =
        SHOP[item].max_level;

    int grow =
        selected
        ? (int)lroundf(
              2.0f *
              shop_selection_ease(g)
          )
        : 0;

    Color border =
        selected
        ? C_CYAN
        : rgb(100, 106, 112);

    draw_rect_outline(
        s,
        x - grow,
        y - grow,
        185 + grow * 2,
        39 + grow * 2,
        border
    );

    draw_upgrade_icon(
        s,
        item,
        x + 24,
        y + 19,
        level > 0
    );

    draw_text(
        s,
        x + 48,
        y + 7,
        SHOP[item].name,
        1,
        C_WHITE
    );

    char state[40];

    if (level >= max) {
        snprintf(
            state,
            sizeof(state),
            "MAX %d",
            max
        );
    } else {
        snprintf(
            state,
            sizeof(state),
            "LV %d/%d  $%lu",
            level,
            max,
            (unsigned long)shop_cost(g, item)
        );
    }

    draw_text(
        s,
        x + 48,
        y + 22,
        state,
        1,
        level >= max
        ? C_GREEN
        : C_YELLOW
    );
}

static void draw_cosmetic_shop_card(const Game *g,
                                    Surface *s,
                                    int item,
                                    int x,
                                    int y,
                                    bool selected) {
    const CosmeticDef *d =
        &COSMETICS[item];

    bool owned =
        cosmetic_owned(
            g,
            item
        );

    bool equipped =
        cosmetic_equipped(
            g,
            item
        );

    float ease =
        selected
        ? shop_selection_ease(g)
        : 0.0f;

    int grow =
        selected
        ? (int)lroundf(
              3.0f *
              ease
          )
        : 0;

    float scale =
        1.0f +
        0.10f *
        ease;

    float twist =
        selected
        ? (1.0f - ease) * 0.38f +
          sinf(
              (float)g->frame_counter *
              0.055f
          ) *
          0.035f
        : 0.0f;

    Color border =
        selected
        ? C_CYAN
        : rgb(90, 96, 102);

    if (equipped)
        border = C_GREEN;

    draw_rect_outline(
        s,
        x - grow,
        y - grow,
        120 + grow * 2,
        70 + grow * 2,
        border
    );

    draw_cosmetic_preview_item(
        g,
        s,
        item,
        x + 60,
        y + 25,
        scale,
        twist
    );

    draw_text_center(
        s,
        x + 60,
        y + 47,
        d->name,
        1,
        C_WHITE
    );

    char state[28];

    if (equipped)
        strcpy(state, "EQUIPPED");
    else if (owned)
        strcpy(state, "OWNED");
    else
        snprintf(
            state,
            sizeof(state),
            "$%lu",
            (unsigned long)d->cost
        );

    draw_text_center(
        s,
        x + 60,
        y + 59,
        state,
        1,
        equipped
        ? C_GREEN
        : (owned
           ? C_CYAN
           : C_YELLOW)
    );
}

static void draw_shop(const Game *g, Surface *top) {
    draw_background(g, top);

    draw_rect(
        top,
        5,
        5,
        390,
        230,
        rgb(5, 7, 9)
    );

    draw_rect_outline(
        top,
        5,
        5,
        390,
        230,
        C_DIM
    );

    char money[32];

    snprintf(
        money,
        sizeof(money),
        "$%lu",
        (unsigned long)g->progress.money
    );

    draw_text(
        top,
        326,
        14,
        money,
        1,
        C_YELLOW
    );

    const char *page_name =
        g->shop_page == 0
        ? "UPGRADES"
        : COSMETIC_PAGES[
              g->shop_page - 1
          ].name;

    char heading[64];

    snprintf(
        heading,
        sizeof(heading),
        "< %s  %d/%d >",
        page_name,
        g->shop_page + 1,
        SHOP_PAGE_COUNT
    );

    draw_text_center(
        top,
        200,
        14,
        heading,
        1,
        C_WHITE
    );

    if (g->shop_message_timer > 0.0f) {
        draw_text_center(
            top,
            200,
            34,
            g->shop_message,
            1,
            C_CYAN
        );
    } else {
        draw_text_center(
            top,
            200,
            34,
            "L/R BUTTONS CHANGE PAGE",
            1,
            C_DIM
        );
    }

    if (g->shop_page == 0) {
        for (int i = 0;
             i < SHOP_ITEM_COUNT;
             ++i) {
            int col =
                i & 1;

            int row =
                i >> 1;

            int x =
                10 +
                col * 195;

            int y =
                55 +
                row * 43;

            draw_upgrade_shop_card(
                g,
                top,
                i,
                x,
                y,
                g->shop_index == i
            );
        }
    } else {
        const CosmeticPage *page =
            &COSMETIC_PAGES[
                g->shop_page - 1
            ];

        const int xs[3] = {
            12,
            140,
            268
        };

        const int ys[2] = {
            61,
            145
        };

        int window_start =
            (g->shop_index / 6) *
            6;

        int window_end =
            window_start +
            6;

        if (window_end >
            page->count) {
            window_end =
                page->count;
        }

        for (int i = window_start;
             i < window_end;
             ++i) {
            int local =
                i -
                window_start;

            int item =
                page->items[i];

            draw_cosmetic_shop_card(
                g,
                top,
                item,
                xs[local % 3],
                ys[local / 3],
                g->shop_index == i
            );
        }

        if (page->count > 6) {
            char subpage[24];

            snprintf(
                subpage,
                sizeof(subpage),
                "%d/%d",
                g->shop_index / 6 + 1,
                (page->count + 5) / 6
            );

            draw_text(
                top,
                354,
                34,
                subpage,
                1,
                C_CYAN
            );
        }
    }
}

