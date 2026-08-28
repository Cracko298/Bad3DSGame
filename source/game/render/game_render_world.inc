static void shake_offset(const Game *g, int *ox, int *oy) {
    *ox = 0; *oy = 0;

    if (!setting_enabled(g, SETTING_SCREENSHAKE) ||
        g->screen_shake <= 0.0f) {
        return;
    }
    unsigned f = g->frame_counter;
    *ox = (int)((f * 17u) % 5u) - 2;
    *oy = (int)((f * 29u) % 5u) - 2;
}

static Vec2 rope_end(const Game *g) {
    if (g->rope.latched && g->rope.target_block >= 0 &&
        g->rope.target_block < MAX_BLOCKS && g->blocks[g->rope.target_block].active)
        return g->blocks[g->rope.target_block].body.pos;
    return g->rope.hook_pos;
}

static void draw_hat_style(Surface *top,
                           uint8_t style,
                           float cx,
                           float cy,
                           float half_h,
                           float angle,
                           Color c,
                           unsigned frame_counter) {
    if (!top ||
        style == 0) {
        return;
    }

    float up_x = sinf(angle);
    float up_y = -cosf(angle);
    float right_x = cosf(angle);
    float right_y = sinf(angle);

    float base_dist =
        half_h +
        5.0f;

    float hx =
        cx +
        up_x *
        base_dist;

    float hy =
        cy +
        up_y *
        base_dist;

    switch (style) {
        case 1: { 
            draw_rotated_rect(top, hx, hy, 5.0f, 3.0f, angle, c);

            draw_line(
                top,
                (int)(hx - right_x * 8.0f),
                (int)(hy - right_y * 8.0f),
                (int)(hx + right_x * 8.0f),
                (int)(hy + right_y * 8.0f),
                c
            );
            break;
        }

        case 2: { 
            float bx = hx - up_x * 2.0f;
            float by = hy - up_y * 2.0f;

            for (int i = -1; i <= 1; ++i) {
                float px =
                    bx +
                    right_x *
                    (float)i *
                    5.0f;

                float py =
                    by +
                    right_y *
                    (float)i *
                    5.0f;

                draw_line(
                    top,
                    (int)px,
                    (int)py,
                    (int)(px + up_x * (i == 0 ? 8.0f : 6.0f)),
                    (int)(py + up_y * (i == 0 ? 8.0f : 6.0f)),
                    C_YELLOW
                );
            }

            draw_line(
                top,
                (int)(bx - right_x * 7.0f),
                (int)(by - right_y * 7.0f),
                (int)(bx + right_x * 7.0f),
                (int)(by + right_y * 7.0f),
                C_YELLOW
            );
            break;
        }

        case 3: 
            draw_circle_outline(
                top,
                (int)(hx + up_x * 4.0f),
                (int)(hy + up_y * 4.0f),
                6,
                C_YELLOW
            );
            break;

        case 4: { 
            float ex = hx + up_x * 10.0f;
            float ey = hy + up_y * 10.0f;

            draw_line(
                top,
                (int)hx,
                (int)hy,
                (int)ex,
                (int)ey,
                c
            );

            draw_bloom_circle(
                top,
                (int)ex,
                (int)ey,
                2,
                C_CYAN
            );
            break;
        }

        case 5: { 
            draw_rotated_rect(
                top,
                hx,
                hy,
                6.0f,
                2.2f,
                angle,
                c
            );

            draw_line(
                top,
                (int)(hx + right_x * 2.0f),
                (int)(hy + right_y * 2.0f),
                (int)(hx + right_x * 10.0f),
                (int)(hy + right_y * 10.0f),
                C_CYAN
            );
            break;
        }

        case 6: { 
            float tip_x =
                hx +
                up_x * 13.0f +
                right_x * 4.0f;

            float tip_y =
                hy +
                up_y * 13.0f +
                right_y * 4.0f;

            draw_line(
                top,
                (int)(hx - right_x * 7.0f),
                (int)(hy - right_y * 7.0f),
                (int)(hx + right_x * 7.0f),
                (int)(hy + right_y * 7.0f),
                C_PURPLE
            );

            draw_line(
                top,
                (int)(hx - right_x * 5.0f),
                (int)(hy - right_y * 5.0f),
                (int)tip_x,
                (int)tip_y,
                C_PURPLE
            );

            draw_line(
                top,
                (int)(hx + right_x * 5.0f),
                (int)(hy + right_y * 5.0f),
                (int)tip_x,
                (int)tip_y,
                C_PURPLE
            );
            break;
        }

        case 7: { 
            for (int side = -1; side <= 1; side += 2) {
                float bx =
                    hx +
                    right_x *
                    (float)side *
                    5.0f;

                float by =
                    hy +
                    right_y *
                    (float)side *
                    5.0f;

                float mx =
                    bx +
                    up_x * 6.0f +
                    right_x *
                    (float)side *
                    2.0f;

                float my =
                    by +
                    up_y * 6.0f +
                    right_y *
                    (float)side *
                    2.0f;

                float tx =
                    bx +
                    up_x * 10.0f -
                    right_x *
                    (float)side;

                float ty =
                    by +
                    up_y * 10.0f -
                    right_y *
                    (float)side;

                draw_bloom_line(
                    top,
                    (int)bx,
                    (int)by,
                    (int)mx,
                    (int)my,
                    C_RED
                );

                draw_line(
                    top,
                    (int)mx,
                    (int)my,
                    (int)tx,
                    (int)ty,
                    C_YELLOW
                );
            }
            break;
        }

        case 8: { 
            for (int side = -1; side <= 1; side += 2) {
                float bx =
                    hx +
                    right_x *
                    (float)side *
                    5.0f;

                float by =
                    hy +
                    right_y *
                    (float)side *
                    5.0f;

                float tip_x =
                    bx +
                    up_x * 8.0f +
                    right_x *
                    (float)side *
                    2.0f;

                float tip_y =
                    by +
                    up_y * 8.0f +
                    right_y *
                    (float)side *
                    2.0f;

                float inner_x =
                    hx +
                    right_x *
                    (float)side *
                    1.5f;

                float inner_y =
                    hy +
                    right_y *
                    (float)side *
                    1.5f;

                draw_line(top, (int)bx, (int)by,
                          (int)tip_x, (int)tip_y, c);
                draw_line(top, (int)tip_x, (int)tip_y,
                          (int)inner_x, (int)inner_y, c);
            }
            break;
        }

        case 9: { 
            float tx = hx + up_x * 12.0f;
            float ty = hy + up_y * 12.0f;

            draw_line(
                top,
                (int)(hx - right_x * 6.0f),
                (int)(hy - right_y * 6.0f),
                (int)tx,
                (int)ty,
                C_YELLOW
            );

            draw_line(
                top,
                (int)(hx + right_x * 6.0f),
                (int)(hy + right_y * 6.0f),
                (int)tx,
                (int)ty,
                C_CYAN
            );

            draw_bloom_circle(
                top,
                (int)tx,
                (int)ty,
                2,
                rgb(255, 65, 170)
            );
            break;
        }

        case 10: { 
            draw_line(
                top,
                (int)(hx - right_x * 10.0f),
                (int)(hy - right_y * 10.0f),
                (int)(hx + right_x * 10.0f),
                (int)(hy + right_y * 10.0f),
                C_YELLOW
            );

            draw_rotated_rect(
                top,
                hx + up_x * 3.0f,
                hy + up_y * 3.0f,
                5.5f,
                3.0f,
                angle,
                rgb(190, 128, 42)
            );
            break;
        }

        case 11: { 
            draw_rotated_rect(
                top,
                hx,
                hy,
                5.0f,
                2.0f,
                angle,
                c
            );

            float mast_x = hx + up_x * 7.0f;
            float mast_y = hy + up_y * 7.0f;

            draw_line(
                top,
                (int)hx,
                (int)hy,
                (int)mast_x,
                (int)mast_y,
                c
            );

            float spin =
                (float)frame_counter *
                0.20f;

            float ps = sinf(spin);
            float pc = cosf(spin);

            float blade_x =
                right_x * pc * 8.0f +
                up_x * ps * 3.0f;

            float blade_y =
                right_y * pc * 8.0f +
                up_y * ps * 3.0f;

            draw_bloom_line(
                top,
                (int)(mast_x - blade_x),
                (int)(mast_y - blade_y),
                (int)(mast_x + blade_x),
                (int)(mast_y + blade_y),
                C_CYAN
            );
            break;
        }

        case 12: { 
            float bob =
                sinf(
                    (float)frame_counter *
                    0.09f
                ) *
                2.0f;

            float ux =
                hx +
                up_x *
                (9.0f + bob);

            float uy =
                hy +
                up_y *
                (9.0f + bob);

            draw_rotated_rect(
                top,
                ux,
                uy,
                8.0f,
                2.0f,
                angle,
                rgb(125, 135, 150)
            );

            draw_circle_outline(
                top,
                (int)(ux + up_x),
                (int)(uy + up_y),
                3,
                C_CYAN
            );

            draw_bloom_line(
                top,
                (int)(ux - right_x * 6.0f),
                (int)(uy - right_y * 6.0f),
                (int)(ux + right_x * 6.0f),
                (int)(uy + right_y * 6.0f),
                C_PURPLE
            );
            break;
        }

        case 13: { 
            float bx =
                hx +
                up_x *
                2.0f;

            float by =
                hy +
                up_y *
                2.0f;

            draw_circle_filled(
                top,
                (int)bx,
                (int)by,
                6,
                c
            );

            draw_line(
                top,
                (int)(hx - right_x * 7.0f),
                (int)(hy - right_y * 7.0f),
                (int)(hx + right_x * 7.0f),
                (int)(hy + right_y * 7.0f),
                C_WHITE
            );

            draw_bloom_circle(
                top,
                (int)(bx + up_x * 6.0f),
                (int)(by + up_y * 6.0f),
                2,
                C_CYAN
            );
            break;
        }

        case 14: { 
            float center_x =
                hx +
                up_x *
                1.0f;

            float center_y =
                hy +
                up_y *
                1.0f;

            draw_circle_outline(
                top,
                (int)center_x,
                (int)center_y,
                8,
                C_CYAN
            );

            for (int side = -1;
                 side <= 1;
                 side += 2) {
                float ex =
                    center_x +
                    right_x *
                    (float)side *
                    8.0f;

                float ey =
                    center_y +
                    right_y *
                    (float)side *
                    8.0f;

                draw_rotated_rect(
                    top,
                    ex,
                    ey,
                    2.5f,
                    4.0f,
                    angle,
                    rgb(255, 65, 170)
                );

                draw_bloom_circle(
                    top,
                    (int)ex,
                    (int)ey,
                    1,
                    C_CYAN
                );
            }
            break;
        }

        case 15: { 
            float mx =
                hx +
                up_x *
                2.0f;

            float my =
                hy +
                up_y *
                2.0f;

            draw_rotated_rect(
                top,
                mx,
                my,
                5.0f,
                7.0f,
                angle,
                C_WHITE
            );

            draw_line(
                top,
                (int)(mx - right_x * 5.0f + up_x * 1.5f),
                (int)(my - right_y * 5.0f + up_y * 1.5f),
                (int)(mx + right_x * 5.0f + up_x * 1.5f),
                (int)(my + right_y * 5.0f + up_y * 1.5f),
                rgb(76, 168, 255)
            );

            draw_line(
                top,
                (int)(mx - right_x * 5.0f + up_x * 1.5f),
                (int)(my - right_y * 5.0f + up_y * 1.5f),
                (int)(mx + up_x * 7.0f),
                (int)(my + up_y * 7.0f),
                rgb(76, 168, 255)
            );

            draw_line(
                top,
                (int)(mx + right_x * 5.0f + up_x * 1.5f),
                (int)(my + right_y * 5.0f + up_y * 1.5f),
                (int)(mx + up_x * 7.0f),
                (int)(my + up_y * 7.0f),
                rgb(76, 168, 255)
            );

            draw_text_center(
                top,
                (int)mx,
                (int)(my - 2.0f),
                "M",
                1,
                rgb(76, 168, 255)
            );
            break;
        }

        default:
            break;
    }
}

static void draw_player_hat(const Game *g,
                            Surface *top,
                            float cx,
                            float cy,
                            float half_h,
                            float angle,
                            Color c) {
    if (!g ||
        !top) {
        return;
    }

    draw_hat_style(
        top,
        g->progress.hat_style,
        cx,
        cy,
        half_h,
        angle,
        c,
        g->frame_counter
    );
}

static void draw_player_outline_shape(const Game *g,
                                      Surface *top,
                                      float cx,
                                      float cy,
                                      float half_w,
                                      float half_h,
                                      float angle,
                                      Color near_color,
                                      Color far_color) {
    if (!g || !top)
        return;

    switch (g->progress.shape_style) {
        case 1: { 
            float hw = half_w * 0.90f;
            float hh = half_h * 1.10f;
            draw_rotated_rect_outline(top, cx, cy,
                                      hw + 2.0f, hh + 2.0f,
                                      angle + 0.78539816f,
                                      near_color);
            draw_rotated_rect_outline(top, cx, cy,
                                      hw + 5.0f, hh + 5.0f,
                                      angle + 0.78539816f,
                                      far_color);
            break;
        }

        case 2: { 
            int radius =
                (int)fmaxf(2.0f, (half_w + half_h) * 0.52f);
            draw_circle_outline(top, (int)lroundf(cx), (int)lroundf(cy),
                                radius + 2, near_color);
            draw_circle_outline(top, (int)lroundf(cx), (int)lroundf(cy),
                                radius + 5, far_color);
            break;
        }

        case 3: { 
            float fx = cosf(angle);
            float fy = sinf(angle);
            float rx = -fy;
            float ry = fx;

            float grows[2] = { 2.0f, 5.0f };
            Color cols[2] = { near_color, far_color };

            for (int pass = 0; pass < 2; ++pass) {
                float grow = grows[pass];
                float out_w = half_w + grow;
                float out_h = half_h + grow * 0.85f;

                int x0 = (int)lroundf(cx + fx * out_w * 1.35f);
                int y0 = (int)lroundf(cy + fy * out_w * 1.35f);
                int x1 = (int)lroundf(cx - fx * out_w * 0.75f + rx * out_h);
                int y1 = (int)lroundf(cy - fy * out_w * 0.75f + ry * out_h);
                int x2 = (int)lroundf(cx - fx * out_w * 0.75f - rx * out_h);
                int y2 = (int)lroundf(cy - fy * out_w * 0.75f - ry * out_h);

                draw_triangle_outline(top, x0, y0, x1, y1, x2, y2, cols[pass]);
            }
            break;
        }

        case 4: 
        case 0:
        default:
            draw_rotated_rect_outline(top, cx, cy,
                                      half_w + 2.0f, half_h + 2.0f,
                                      angle, near_color);
            draw_rotated_rect_outline(top, cx, cy,
                                      half_w + 5.0f, half_h + 5.0f,
                                      angle, far_color);
            break;

        case 5: { 
            float outer = fmaxf(half_w, half_h) * 1.25f;
            float inner = outer * 0.45f;
            float extra[2] = { 2.0f, 5.0f };
            Color cols[2] = { near_color, far_color };

            for (int pass = 0; pass < 2; ++pass) {
                int px[10];
                int py[10];
                float o = outer + extra[pass];
                float i = inner + extra[pass] * 0.60f;

                for (int k = 0; k < 10; ++k) {
                    float radius = (k & 1) ? i : o;
                    float a = angle - 1.57079633f + (float)k * 0.31415927f;
                    px[k] = (int)lroundf(cx + cosf(a) * radius);
                    py[k] = (int)lroundf(cy + sinf(a) * radius);
                }

                for (int k = 0; k < 10; ++k) {
                    int j = (k + 1) % 10;
                    draw_line(top, px[k], py[k], px[j], py[j], cols[pass]);
                }
            }
            break;
        }

        case 6: { 
            float radius = fmaxf(half_w, half_h) * 1.15f;
            float extra[2] = { 2.0f, 5.0f };
            Color cols[2] = { near_color, far_color };

            for (int pass = 0; pass < 2; ++pass) {
                int px[6];
                int py[6];
                float r = radius + extra[pass];

                for (int k = 0; k < 6; ++k) {
                    float a = angle + (float)k * 1.04719755f;
                    px[k] = (int)lroundf(cx + cosf(a) * r);
                    py[k] = (int)lroundf(cy + sinf(a) * r);
                }

                for (int k = 0; k < 6; ++k) {
                    int j = (k + 1) % 6;
                    draw_line(top, px[k], py[k], px[j], py[j], cols[pass]);
                }
            }
            break;
        }
    }
}

static void draw_player_shape(const Game *g,
                              Surface *top,
                              float cx,
                              float cy,
                              float half_w,
                              float half_h,
                              float angle,
                              Color core,
                              int lod) {
    if (!g || !top)
        return;

    switch (g->progress.shape_style) {
        case 1: { 
            float hw =
                half_w *
                0.90f;

            float hh =
                half_h *
                1.10f;

            if (lod <= 2) {
                draw_bloom_rotated_rect(
                    top,
                    cx,
                    cy,
                    hw,
                    hh,
                    angle +
                    0.78539816f,
                    core
                );
            } else {
                draw_rotated_rect(
                    top,
                    cx,
                    cy,
                    hw,
                    hh,
                    angle +
                    0.78539816f,
                    core
                );
            }
            break;
        }

        case 2: { 
            int radius =
                (int)fmaxf(
                    2.0f,
                    (half_w +
                     half_h) *
                    0.52f
                );

            if (lod <= 2)
                draw_bloom_circle(
                    top,
                    (int)cx,
                    (int)cy,
                    radius,
                    core
                );
            else
                draw_circle_filled(
                    top,
                    (int)cx,
                    (int)cy,
                    radius,
                    core
                );
            break;
        }

        case 3: { 
            float fx =
                cosf(angle);

            float fy =
                sinf(angle);

            float rx =
                -fy;

            float ry =
                fx;

            float tip_x =
                cx +
                fx *
                half_w *
                1.35f;

            float tip_y =
                cy +
                fy *
                half_w *
                1.35f;

            float back_x =
                cx -
                fx *
                half_w *
                0.75f;

            float back_y =
                cy -
                fy *
                half_w *
                0.75f;

            int x0 = (int)tip_x;
            int y0 = (int)tip_y;
            int x1 = (int)(back_x + rx * half_h);
            int y1 = (int)(back_y + ry * half_h);
            int x2 = (int)(back_x - rx * half_h);
            int y2 = (int)(back_y - ry * half_h);

            if (lod <= 2)
                draw_bloom_triangle(
                    top,
                    x0, y0,
                    x1, y1,
                    x2, y2,
                    core
                );
            else
                draw_triangle_filled(
                    top,
                    x0, y0,
                    x1, y1,
                    x2, y2,
                    core
                );
            break;
        }

        case 4: { 
            if (lod <= 2) {
                draw_bloom_rotated_rect(
                    top,
                    cx,
                    cy,
                    half_w,
                    half_h,
                    angle,
                    core
                );

                float fx =
                    cosf(angle);

                float fy =
                    sinf(angle);

                draw_bloom_circle(
                    top,
                    (int)(
                        cx -
                        fx *
                        half_w *
                        0.70f
                    ),
                    (int)(
                        cy -
                        fy *
                        half_w *
                        0.70f
                    ),
                    3,
                    C_WHITE
                );
            } else {
                draw_rotated_rect(
                    top,
                    cx,
                    cy,
                    half_w,
                    half_h,
                    angle,
                    core
                );
            }
            break;
        }

        case 5: { 
            float outer =
                fmaxf(
                    half_w,
                    half_h
                ) *
                1.25f;

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
                        cx +
                        cosf(a) *
                        radius
                    );

                py[i] =
                    (int)lroundf(
                        cy +
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

                if (lod <= 2)
                    draw_bloom_line(
                        top,
                        px[i], py[i],
                        px[j], py[j],
                        core
                    );
                else
                    draw_line(
                        top,
                        px[i], py[i],
                        px[j], py[j],
                        core
                    );
            }

            draw_circle_filled(
                top,
                (int)cx,
                (int)cy,
                (int)fmaxf(
                    2.0f,
                    inner * 0.45f
                ),
                core
            );
            break;
        }

        case 6: { 
            float radius =
                fmaxf(
                    half_w,
                    half_h
                ) *
                1.15f;

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
                        cx +
                        cosf(a) *
                        radius
                    );

                py[i] =
                    (int)lroundf(
                        cy +
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

                if (lod <= 2)
                    draw_bloom_line(
                        top,
                        px[i], py[i],
                        px[j], py[j],
                        core
                    );
                else
                    draw_line(
                        top,
                        px[i], py[i],
                        px[j], py[j],
                        core
                    );
            }

            draw_rotated_rect(
                top,
                cx,
                cy,
                radius * 0.58f,
                radius * 0.42f,
                angle,
                core
            );
            break;
        }


        case 8: { 
            Color shell = rgb(42, 46, 58);
            Color bezel = rgb(190, 210, 230);
            float body_w = half_w * 1.18f;
            float body_h = half_h * 1.10f;

            if (lod <= 2) {
                draw_bloom_rotated_rect(
                    top,
                    cx,
                    cy,
                    body_w,
                    body_h,
                    angle,
                    shell
                );
            } else {
                draw_rotated_rect(
                    top,
                    cx,
                    cy,
                    body_w,
                    body_h,
                    angle,
                    shell
                );
            }

            draw_rotated_rect_outline(
                top,
                cx,
                cy,
                body_w + 1.0f,
                body_h + 1.0f,
                angle,
                bezel
            );

            float pulse = (float)g->frame_counter * 0.16f;
            Color screen = rgb(
                (uint8_t)(90 + 70 * (0.5f + 0.5f * sinf(pulse))),
                (uint8_t)(120 + 80 * (0.5f + 0.5f * sinf(pulse + 2.1f))),
                (uint8_t)(140 + 70 * (0.5f + 0.5f * sinf(pulse + 4.2f)))
            );

            draw_rotated_rect(
                top,
                cx,
                cy,
                body_w * 0.70f,
                body_h * 0.58f,
                angle,
                screen
            );

            float fx = cosf(angle);
            float fy = sinf(angle);
            float rx = -fy;
            float ry = fx;
            for (int i = -1; i <= 1; ++i) {
                float offs = (float)i * body_h * 0.28f;
                int x0 = (int)lroundf(cx - fx * body_w * 0.56f + rx * offs);
                int y0 = (int)lroundf(cy - fy * body_w * 0.56f + ry * offs);
                int x1 = (int)lroundf(cx + fx * body_w * 0.56f + rx * offs);
                int y1 = (int)lroundf(cy + fy * body_w * 0.56f + ry * offs);
                if (lod <= 2)
                    draw_bloom_line(top, x0, y0, x1, y1, core);
                else
                    draw_line(top, x0, y0, x1, y1, core);
            }

            float ant_x = cx - rx * body_w * 0.15f;
            float ant_y = cy - ry * body_w * 0.15f - body_h * 1.25f;
            draw_line(top,
                      (int)lroundf(cx - rx * body_w * 0.15f),
                      (int)lroundf(cy - ry * body_w * 0.15f - body_h * 0.95f),
                      (int)lroundf(ant_x),
                      (int)lroundf(ant_y),
                      bezel);
            draw_line(top,
                      (int)lroundf(cx + rx * body_w * 0.15f),
                      (int)lroundf(cy + ry * body_w * 0.15f - body_h * 0.95f),
                      (int)lroundf(cx + rx * body_w * 0.30f),
                      (int)lroundf(cy + ry * body_w * 0.30f - body_h * 1.25f),
                      bezel);
            draw_circle_filled(top,
                               (int)lroundf(ant_x),
                               (int)lroundf(ant_y),
                               2,
                               C_RED);
            draw_circle_filled(top,
                               (int)lroundf(cx + rx * body_w * 0.30f),
                               (int)lroundf(cy + ry * body_w * 0.30f - body_h * 1.25f),
                               2,
                               C_CYAN);
            break;
        }

        case 7: { 
            Color peel = rgb(250, 232, 72);
            Color tip = rgb(114, 82, 24);
            float radius = fmaxf(half_w, half_h) * 1.10f;
            float prev_x = 0.0f;
            float prev_y = 0.0f;
            bool have_prev = false;

            for (int i = 0; i < 7; ++i) {
                float t = -0.78f + (float)i * 0.26f;
                float a = angle + t;
                float r = radius * (1.0f + 0.18f * cosf(t * 2.0f));
                float px = cx + cosf(a) * r;
                float py = cy + sinf(a) * (radius * 0.72f);
                if (lod <= 2)
                    draw_bloom_circle(top, (int)px, (int)py,
                                      (int)fmaxf(2.0f, radius * 0.22f), peel);
                else
                    draw_circle_filled(top, (int)px, (int)py,
                                       (int)fmaxf(2.0f, radius * 0.22f), peel);
                if (have_prev) {
                    if (lod <= 2)
                        draw_bloom_line(top, (int)prev_x, (int)prev_y,
                                        (int)px, (int)py, peel);
                    else
                        draw_line(top, (int)prev_x, (int)prev_y,
                                  (int)px, (int)py, peel);
                }
                prev_x = px;
                prev_y = py;
                have_prev = true;
            }

            draw_bloom_circle(top,
                              (int)(cx + cosf(angle - 0.90f) * radius * 1.02f),
                              (int)(cy + sinf(angle - 0.90f) * radius * 0.74f),
                              2,
                              tip);
            draw_bloom_circle(top,
                              (int)(cx + cosf(angle + 0.92f) * radius * 1.00f),
                              (int)(cy + sinf(angle + 0.92f) * radius * 0.74f),
                              2,
                              tip);
            break;
        }

        case 0:
        default:
            if (lod <= 2) {
                draw_bloom_rotated_rect(
                    top,
                    cx,
                    cy,
                    half_w,
                    half_h,
                    angle,
                    core
                );
            } else {
                draw_rotated_rect(
                    top,
                    cx,
                    cy,
                    half_w,
                    half_h,
                    angle,
                    core
                );

                draw_rotated_rect_outline(
                    top,
                    cx,
                    cy,
                    half_w + 1.0f,
                    half_h + 1.0f,
                    angle,
                    C_WHITE
                );
            }
            break;
    }
}

static void draw_world(const Game *g, Surface *top) {
    draw_background(g, top);

    int lod =
        effective_lod_level(g);

    int ox, oy;
    shake_offset(g, &ox, &oy);

    float lava_screen = world_to_screen_y(g, LAVA_Y);

    if (lava_screen < 240.0f) {
        int ly =
            (int)lava_screen +
            oy;

        if (ly < 0)
            ly = 0;

        Color lava =
            lava_color(g);

        Color lava_hi =
            lava_highlight_color(g);

        draw_rect(
            top,
            0,
            ly,
            400,
            240 - ly,
            lava
        );

        bool lava_moves =
            setting_enabled(
                g,
                SETTING_LAVA_ANIM
            );

        int style =
            lava_moves
            ? (int)g->progress.lava_anim_style
            : 0;

        int step =
            lod >= 2
            ? 16
            : 8;

        int prev_x = 0;
        int prev_y = ly;

        for (int x = 0;
             x <= 400;
             x += step) {

            float phase =
                (float)x * 0.055f +
                (float)g->frame_counter *
                0.055f;

            int wobble = 0;

            if (lava_moves) {
                switch (style) {
                    case 1: 
                        wobble =
                            (int)lroundf(
                                sinf(phase * 0.65f) *
                                2.0f
                            );
                        break;

                    case 2: 
                        wobble =
                            (int)lroundf(
                                sinf(phase * 0.42f) *
                                4.0f
                            );
                        break;

                    case 3: 
                        wobble =
                            ((x / step +
                              (int)(g->frame_counter / 3u)) %
                             5 == 0)
                            ? -4
                            : (int)lroundf(
                                  sinf(phase) *
                                  1.5f
                              );
                        break;

                    case 4: 
                        wobble =
                            (int)lroundf(
                                sinf(phase * 1.31f) * 3.0f +
                                sinf(phase * 0.47f + 1.2f) * 2.0f
                            );
                        break;

                    case 5: 
                        wobble =
                            (int)lroundf(
                                sinf(phase * 0.74f) *
                                2.0f
                            );
                        break;

                    case 6: 
                        wobble =
                            (int)lroundf(
                                sinf(phase * 0.90f) *
                                2.5f
                            );
                        break;

                    case 0:
                    default: 
                        wobble =
                            (int)lroundf(
                                sinf(phase) *
                                1.5f
                            );
                        break;
                }
            }

            int y =
                ly +
                wobble;

            if (x > 0) {
                if (lod < 2)
                    draw_bloom_line(
                        top,
                        prev_x,
                        prev_y,
                        x,
                        y,
                        lava_hi
                    );
                else
                    draw_line(
                        top,
                        prev_x,
                        prev_y,
                        x,
                        y,
                        lava_hi
                    );
            }

            if (lava_moves &&
                style == 3 &&
                ((x / step +
                  (int)(g->frame_counter / 2u)) %
                 7 == 0)) {
                draw_pixel(
                    top,
                    x,
                    y - 3,
                    C_WHITE
                );
            }

            prev_x = x;
            prev_y = y;
        }

        if (lava_moves &&
            lod < 2) {
            int bubble_count =
                style == 5
                ? 10
                : 5;

            for (int i = 0;
                 i < bubble_count;
                 ++i) {
                unsigned period =
                    78u +
                    (unsigned)i * 9u;

                unsigned bubble_phase =
                    (g->frame_counter +
                     (unsigned)i * 37u) %
                    period;

                float bt =
                    (float)bubble_phase /
                    (float)period;

                int bx =
                    18 +
                    ((i * 83 +
                      (int)(g->frame_counter / 5u)) %
                     370);

                int by =
                    ly -
                    2 -
                    (int)(
                        bt *
                        (style == 5 ? 34.0f : 20.0f)
                    );

                int br =
                    style == 5
                    ? 2 + (i % 3)
                    : 1 + (i & 1);

                draw_circle_outline(
                    top,
                    bx,
                    by,
                    br,
                    lava_hi
                );
            }

            if (style == 6) {
                unsigned geyser_phase =
                    g->frame_counter %
                    180u;

                if (geyser_phase < 42u) {
                    float gt =
                        (float)geyser_phase /
                        42.0f;

                    int gx =
                        80 +
                        (int)(
                            (g->frame_counter / 180u * 137u) %
                            240u
                        );

                    int top_y =
                        ly -
                        (int)(
                            sinf(gt * 3.14159265f) *
                            48.0f
                        );

                    draw_bloom_line(
                        top,
                        gx,
                        ly,
                        gx,
                        top_y,
                        lava_hi
                    );

                    draw_circle_outline(
                        top,
                        gx,
                        top_y,
                        4,
                        lava_hi
                    );
                }
            }
        }
    }

    float player_sx = world_to_screen_x(g, g->player.pos.x);
    float player_sy = world_to_screen_y(g, g->player.pos.y);

    if (g->rope.active) {
        Vec2 end = rope_end(g);
        float end_sx = world_to_screen_x(g, end.x);
        float end_sy = world_to_screen_y(g, end.y);

        Color rope_core = rope_color(g);
        Color player_core = player_color(g);

        int rx0 = (int)player_sx + ox;
        int ry0 = (int)player_sy + oy;
        int rx1 = (int)end_sx + ox;
        int ry1 = (int)end_sy + oy;

        int pattern =
            (int)g->progress.pattern_style;

        if (pattern == 0) {
            if (lod < 1)
                draw_bloom_line(
                    top,
                    rx0,
                    ry0,
                    rx1,
                    ry1,
                    rope_core
                );
            else
                draw_line(
                    top,
                    rx0,
                    ry0,
                    rx1,
                    ry1,
                    rope_core
                );
        } else {
            float dx =
                (float)(rx1 - rx0);

            float dy =
                (float)(ry1 - ry0);

            float len =
                sqrtf(
                    dx * dx +
                    dy * dy
                );

            int segs =
                clampi(
                    (int)(len / 13.0f),
                    1,
                    38
                );

            float inv_len =
                len > 0.001f
                ? 1.0f / len
                : 0.0f;

            float nx =
                -dy * inv_len;

            float ny =
                dx * inv_len;

            for (int s = 0;
                 s < segs;
                 ++s) {

                float t0 =
                    (float)s /
                    (float)segs;

                float t1 =
                    (float)(s + 1) /
                    (float)segs;

                if (pattern == 1 &&
                    (s & 1)) {
                    continue; 
                }

                Color sc =
                    rope_core;

                if (pattern == 2 &&
                    (s & 1)) {
                    sc =
                        player_core; 
                } else if (pattern == 3) {
                    float pulse =
                        0.5f +
                        0.5f *
                        sinf(
                            (float)g->frame_counter *
                            0.12f +
                            (float)s
                        );

                    sc =
                        pulse > 0.45f
                        ? C_WHITE
                        : rope_core;
                } else if (pattern == 5) {
                    int spark =
                        (s +
                         (int)(
                             g->frame_counter /
                             3u
                         )) %
                        5;

                    if (spark == 3)
                        continue;

                    if (spark == 0)
                        sc = C_WHITE;
                } else if (pattern == 6) {
                    sc =
                        ((s +
                          (int)(g->frame_counter / 2u)) &
                         1)
                        ? C_WHITE
                        : C_CYAN;
                } else if (pattern == 7) {
                    float rp =
                        (float)s * 0.65f +
                        (float)g->frame_counter *
                        0.08f;

                    int rr =
                        (int)(
                            128.0f +
                            127.0f *
                            sinf(rp)
                        );

                    int rg =
                        (int)(
                            128.0f +
                            127.0f *
                            sinf(rp + 2.094f)
                        );

                    int rb =
                        (int)(
                            128.0f +
                            127.0f *
                            sinf(rp + 4.188f)
                        );

                    sc =
                        rgb(
                            (uint8_t)rr,
                            (uint8_t)rg,
                            (uint8_t)rb
                        );
                }

                float wave0 = 0.0f;
                float wave1 = 0.0f;

                if (pattern == 4) {
                    float phase0 =
                        t0 * 18.0f -
                        (float)g->frame_counter *
                        0.11f;

                    float phase1 =
                        t1 * 18.0f -
                        (float)g->frame_counter *
                        0.11f;

                    wave0 =
                        sinf(phase0) *
                        4.0f;

                    wave1 =
                        sinf(phase1) *
                        4.0f;
                } else if (pattern == 6) {
                    int jitter0 =
                        ((s * 13 +
                          (int)(g->frame_counter / 2u)) %
                         5) -
                        2;

                    int jitter1 =
                        (((s + 1) * 13 +
                          (int)(g->frame_counter / 2u)) %
                         5) -
                        2;

                    wave0 =
                        (float)jitter0 *
                        1.8f;

                    wave1 =
                        (float)jitter1 *
                        1.8f;
                }

                int sx0 =
                    rx0 +
                    (int)(dx * t0) +
                    (int)(nx * wave0);

                int sy0 =
                    ry0 +
                    (int)(dy * t0) +
                    (int)(ny * wave0);

                int sx1 =
                    rx0 +
                    (int)(dx * t1) +
                    (int)(nx * wave1);

                int sy1 =
                    ry0 +
                    (int)(dy * t1) +
                    (int)(ny * wave1);

                

                draw_bloom_line(
                    top,
                    sx0,
                    sy0,
                    sx1,
                    sy1,
                    sc
                );
            }
        }
    }

    



    if (g->aim_valid) {
        int ax = (int)world_to_screen_x(g, g->aim_world.x) + ox;
        int ay = (int)world_to_screen_y(g, g->aim_world.y) + oy;
        draw_rect_outline(top, ax - 5, ay - 5, 11, 11, C_CYAN);
        draw_pixel(top, ax, ay, C_WHITE);
    }

    for (int i = 0; i < MAX_BLOCKS; ++i) {
        const Block *b = &g->blocks[i];
        if (!b->active) continue;

        float sx = world_to_screen_x(g, b->body.pos.x);
        float sy = world_to_screen_y(g, b->body.pos.y);
        if (sx < -24.0f || sx > 424.0f ||
            sy < -24.0f || sy > 264.0f) continue;

        Color c = block_color(g, b->type);

        float pulse = 1.0f;
        float angle = 0.0f;

        if (setting_enabled(g, SETTING_BLOCK_ANIM)) {
            


            float phase =
                b->anim_phase +
                g->difficulty_time *
                b->angular_velocity;

            float wave =
                sinf(phase);

            if (lod < 1) {
                pulse =
                    1.0f +
                    wave * 0.135f +
                    sinf(phase * 0.47f + 1.1f) * 0.030f;

                angle =
                    sinf(phase * 0.79f) * 0.175f +
                    sinf(phase * 0.31f + 0.7f) * 0.025f;
            } else {
                pulse =
                    1.0f +
                    wave * 0.155f;

                angle =
                    wave * 0.12f;
            }
        }

        float visual_half_world = b->base_half * pulse;
        float visual_half =
            fmaxf(1.0f, world_size_to_screen(g, visual_half_world));

        if (lod < 1) {
            
            draw_bloom_rotated_square(top,
                                      sx + (float)ox,
                                      sy + (float)oy,
                                      visual_half,
                                      angle,
                                      c);
        } else if (lod < 2) {
            



            draw_glow_square(top,
                             (int)sx + ox,
                             (int)sy + oy,
                             (int)fmaxf(1.0f, visual_half),
                             c);
        } else {
            

            int half = (int)fmaxf(1.0f, visual_half);
            draw_bloom_rect(top,
                            (int)sx + ox,
                            (int)sy + oy,
                            half,
                            half,
                            c);

            draw_rect_outline(top,
                              (int)sx - half - 1 + ox,
                              (int)sy - half - 1 + oy,
                              half * 2 + 3,
                              half * 2 + 3,
                              c);
        }

        





        if (setting_enabled(g, SETTING_BLOCK_OUTLINES)) {
            Color outline_near = rgb(
                (uint8_t)(c.r / 3u),
                (uint8_t)(c.g / 3u),
                (uint8_t)(c.b / 3u)
            );
            Color outline_far = rgb(
                (uint8_t)(c.r / 8u),
                (uint8_t)(c.g / 8u),
                (uint8_t)(c.b / 8u)
            );

            if (lod < 1) {
                draw_rotated_rect_outline(
                    top,
                    sx + (float)ox,
                    sy + (float)oy,
                    visual_half + 2.0f,
                    visual_half + 2.0f,
                    angle,
                    outline_near
                );
                draw_rotated_rect_outline(
                    top,
                    sx + (float)ox,
                    sy + (float)oy,
                    visual_half + 5.0f,
                    visual_half + 5.0f,
                    angle,
                    outline_far
                );
            } else {
                int half = (int)fmaxf(1.0f, visual_half);
                draw_rect_outline(
                    top,
                    (int)sx - half - 2 + ox,
                    (int)sy - half - 2 + oy,
                    half * 2 + 5,
                    half * 2 + 5,
                    outline_near
                );

                if (lod < 2) {
                    draw_rect_outline(
                        top,
                        (int)sx - half - 5 + ox,
                        (int)sy - half - 5 + oy,
                        half * 2 + 11,
                        half * 2 + 11,
                        outline_far
                    );
                }
            }
        }

        



        if (lod < 1) {
            if (b->type == BLOCK_MONEY) {
                draw_text_center(top,
                                 (int)sx + ox,
                                 (int)sy - 3 + oy,
                                 "$", 1, rgb(30, 36, 6));
            } else if (b->type == BLOCK_RED) {
                draw_line(top,
                          (int)sx - 3 + ox, (int)sy - 3 + oy,
                          (int)sx + 3 + ox, (int)sy + 3 + oy,
                          rgb(90, 0, 0));
                draw_line(top,
                          (int)sx + 3 + ox, (int)sy - 3 + oy,
                          (int)sx - 3 + ox, (int)sy + 3 + oy,
                          rgb(90, 0, 0));
            }
        }

        if (g->rope.latched && g->rope.target_block == i)
            draw_rect_outline(top,
                              (int)sx - (int)visual_half - 4 + ox,
                              (int)sy - (int)visual_half - 4 + oy,
                              (int)visual_half * 2 + 9,
                              (int)visual_half * 2 + 9,
                              C_CYAN);
    }

    for (int i = 0; i < MAX_BULLETS; ++i) {
        const Bullet *b = &g->bullets[i];
        if (!b->active) continue;

        float sx = world_to_screen_x(g, b->pos.x);
        float sy = world_to_screen_y(g, b->pos.y);
        if (sx < -5.0f || sx > 405.0f ||
            sy < -5.0f || sy > 245.0f) continue;

        int bullet_size =
            clampi(
                (int)lroundf(
                    3.0f *
                    cosmetic_bullet_size_mult(g) /
                    g->camera_zoom
                ),
                1,
                6
            );

        draw_bloom_rect(top,
                        (int)sx + ox,
                        (int)sy + oy,
                        bullet_size > 1 ? bullet_size / 2 : 1,
                        bullet_size > 1 ? bullet_size / 2 : 1,
                        b->color);
    }

    if (setting_enabled(g, SETTING_PARTICLES)) {
        int particle_stride =
            lod >= 2 ? 3 :
            lod >= 1 ? 2 : 1;

        for (int i = 0; i < MAX_PARTICLES; i += particle_stride) {
            const Particle *p = &g->particles[i];
            if (!p->active) continue;

            float sx = world_to_screen_x(g, p->pos.x);
            float sy = world_to_screen_y(g, p->pos.y);
            if (sx < -5.0f || sx > 405.0f ||
                sy < -5.0f || sy > 245.0f) continue;

            draw_bloom_rect(top,
                            (int)sx + ox,
                            (int)sy + oy,
                            1,
                            1,
                            p->color);
        }
    }

    bool blink_off =
        g->invuln_timer > 0.0f &&
        ((g->frame_counter / 4u) & 1u);

    
    if (g->progress.player_anim_style == 4 &&
        ((g->frame_counter / 5u) % 7u) == 0u) {
        blink_off = true;
    }

    if (g->progress.player_anim_style == 6 &&
        (g->frame_counter % 19u) == 0u) {
        blink_off = true;
    }

    if (!blink_off) {
        float t =
            g->player_stretch;

        float half_w_world =
            fmaxf(
                2.0f,
                g->player_half *
                (1.0f + 1.15f * t)
            );

        float half_h_world =
            fmaxf(
                2.0f,
                g->player_half *
                (1.0f - 0.38f * t)
            );

        float render_angle =
            g->player_angle;

        float anim_phase =
            (float)g->frame_counter *
            0.10f;

        switch (g->progress.player_anim_style) {
            case 1: { 
                float pulse =
                    1.0f +
                    sinf(anim_phase) *
                    0.08f;

                half_w_world *= pulse;
                half_h_world *= pulse;
                break;
            }

            case 2: 
                render_angle +=
                    sinf(
                        anim_phase *
                        1.35f
                    ) *
                    0.18f;
                break;

            case 3: { 
                float jelly =
                    sinf(
                        anim_phase *
                        1.55f
                    ) *
                    0.12f;

                half_w_world *=
                    1.0f +
                    jelly;

                half_h_world *=
                    1.0f -
                    jelly;
                break;
            }

            case 5: 
                render_angle +=
                    (float)g->frame_counter *
                    0.075f;
                break;

            case 6: { 
                int q =
                    (int)(g->frame_counter % 7u) -
                    3;

                render_angle +=
                    (float)q *
                    0.035f;

                float glitch =
                    1.0f +
                    (float)((int)(g->frame_counter % 3u) - 1) *
                    0.055f;

                half_w_world *= glitch;
                half_h_world *=
                    2.0f -
                    glitch;
                break;
            }

            case 7: 
                render_angle +=
                    sinf(anim_phase * 1.15f) *
                    0.12f;
                break;

            case 8: { 
                float bounce =
                    fabsf(sinf(anim_phase * 1.65f));
                half_w_world *= 1.0f + bounce * 0.10f;
                half_h_world *= 1.0f - bounce * 0.08f;
                break;
            }

            case 4:
            case 0:
            default:
                break;
        }

        float half_w =
            fmaxf(
                2.0f,
                world_size_to_screen(
                    g,
                    half_w_world
                )
            );

        float half_h =
            fmaxf(
                2.0f,
                world_size_to_screen(
                    g,
                    half_h_world
                )
            );

        if (t > 0.18f) {
            Vec2 back =
                vnormalize(
                    g->player.vel
                );

            int streak =
                (int)(
                    10.0f +
                    22.0f * t
                );

            int x0 =
                (int)player_sx +
                ox;

            int y0 =
                (int)player_sy +
                oy;

            int x1 =
                x0 -
                (int)(
                    back.x *
                    (float)streak
                );

            int y1 =
                y0 -
                (int)(
                    back.y *
                    (float)streak
                );

            draw_bloom_line(
                top,
                x1, y1,
                x0, y0,
                player_color(g)
            );
        }

        Color pc =
            player_color(g);

        if (g->progress.player_anim_style == 7) {
            float pulse = (float)g->frame_counter * 0.16f;
            pc = rgb(
                (uint8_t)(110 + 110 * (0.5f + 0.5f * sinf(pulse))),
                (uint8_t)(110 + 110 * (0.5f + 0.5f * sinf(pulse + 2.1f))),
                (uint8_t)(110 + 110 * (0.5f + 0.5f * sinf(pulse + 4.2f)))
            );
        }

        int px =
            (int)player_sx +
            ox;

        int py =
            (int)player_sy +
            oy;

        if (g->progress.player_anim_style == 4 &&
            lod < 2) {
            Vec2 back =
                vnormalize(
                    g->player.vel
                );

            Color ghost =
                rgb(
                    (uint8_t)(pc.r / 4),
                    (uint8_t)(pc.g / 4),
                    (uint8_t)(pc.b / 4)
                );

            draw_rotated_rect_outline(
                top,
                (float)px -
                    back.x * 5.0f,
                (float)py -
                    back.y * 5.0f,
                half_w,
                half_h,
                render_angle,
                ghost
            );
        }

        if (g->progress.player_anim_style == 6 &&
            lod < 2) {
            int jitter =
                (int)(g->frame_counter % 5u) -
                2;

            draw_rotated_rect_outline(
                top,
                (float)px + (float)jitter * 2.0f,
                (float)py - (float)jitter,
                half_w,
                half_h,
                render_angle,
                C_CYAN
            );

            draw_rotated_rect_outline(
                top,
                (float)px - (float)jitter * 2.0f,
                (float)py + (float)jitter,
                half_w,
                half_h,
                render_angle,
                rgb(255, 65, 170)
            );
        }

        if (g->progress.player_anim_style == 7 &&
            lod < 2) {
            draw_rotated_rect_outline(
                top,
                (float)px,
                (float)py,
                half_w + 2.0f,
                half_h + 2.0f,
                render_angle,
                C_WHITE
            );
        }

        if (g->progress.player_anim_style == 8 &&
            lod < 2) {
            draw_rotated_rect_outline(
                top,
                (float)px,
                (float)py + 4.0f,
                half_w,
                half_h,
                render_angle,
                rgb(110, 110, 130)
            );
        }

        if (setting_enabled(g, SETTING_BLOCK_OUTLINES)) {
            Color outline_near = rgb(
                (uint8_t)(pc.r / 3u),
                (uint8_t)(pc.g / 3u),
                (uint8_t)(pc.b / 3u)
            );
            Color outline_far = rgb(
                (uint8_t)(pc.r / 8u),
                (uint8_t)(pc.g / 8u),
                (uint8_t)(pc.b / 8u)
            );

            draw_player_outline_shape(
                g,
                top,
                (float)px,
                (float)py,
                half_w,
                half_h,
                render_angle,
                outline_near,
                outline_far
            );
        }

        draw_player_shape(
            g,
            top,
            (float)px,
            (float)py,
            half_w,
            half_h,
            render_angle,
            pc,
            lod
        );

        draw_player_hat(
            g,
            top,
            (float)px,
            (float)py,
            half_h,
            render_angle,
            pc
        );
    }

    for (int i = 0; i < MAX_POPUPS; ++i) {
        const ScorePopup *p = &g->popups[i];
        if (!p->active) continue;

        float sx = world_to_screen_x(g, p->pos.x);
        float sy = world_to_screen_y(g, p->pos.y);
        if (sx < -40.0f || sx > 440.0f ||
            sy < -16.0f || sy > 250.0f) continue;

        char pbuf[32];
        if (p->money) snprintf(pbuf, sizeof(pbuf), "$%d", p->value);
        else snprintf(pbuf, sizeof(pbuf), "%d", p->value);

        draw_text_center(top,
                         (int)sx + ox,
                         (int)sy + oy,
                         pbuf, 1, p->color);
    }

    draw_hearts(g, top);

    char buf[64];
    snprintf(buf, sizeof(buf), "SCORE %d", g->score);
    draw_text_center(top, 220, 10, buf, 2, C_WHITE);

    float combo_pct = g->combo_timer / 2.35f;
    combo_pct = clampf_local(combo_pct, 0.0f, 1.0f);

    draw_rect_outline(top, 166, 29, 108, 8, rgb(37, 42, 50));
    int bar = (int)(104.0f * combo_pct);
    int left = bar / 2;
    draw_rect(top, 168, 31, left, 4, C_CYAN);
    draw_rect(top, 168 + left, 31, bar - left, 4, C_PURPLE);

    snprintf(buf, sizeof(buf), "X%d", g->combo);
    draw_text(top, 342, 13, buf, 2, C_PURPLE);

    snprintf(buf, sizeof(buf), "$%lu", (unsigned long)g->progress.money);
    draw_text(top, 300, 40, buf, 1, C_YELLOW);

    uint32_t need_xp = xp_required_for_level(g->progress.player_level);
    snprintf(buf, sizeof(buf), "LV %lu",
             (unsigned long)g->progress.player_level);
    draw_text(top, 278, 54, buf, 1, C_CYAN);

    draw_rect_outline(top, 320, 54, 68, 7, rgb(58, 63, 68));
    int xp_bar = (int)(64.0f *
        clampf_local((float)g->progress.xp / (float)need_xp, 0.0f, 1.0f));
    draw_rect(top, 322, 56, xp_bar, 3, C_GREEN);

    snprintf(buf, sizeof(buf), "DIST %lu",
             (unsigned long)current_run_distance(g));
    draw_text(top, 14, 39, buf, 1, C_WHITE);

    if (g->levelup_message_timer > 0.0f) {
        draw_text_center(top, 200, 74, "LEVEL UP!", 2, C_YELLOW);

        snprintf(buf, sizeof(buf), "LV %lu  +$%lu",
                 (unsigned long)g->progress.player_level,
                 (unsigned long)g->last_level_reward);
        draw_text_center(top, 200, 94, buf, 1, C_WHITE);
    }

    if (g->flash_timer > 0.0f)
        draw_rect_outline(top, 2, 2, 396, 236, C_RED);
}

