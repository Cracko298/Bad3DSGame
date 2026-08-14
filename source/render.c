#include "render.h"

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int s_bloom_level = 3;

void render_set_bloom_level(int level) {
    if (level < 0) level = 0;
    if (level > 4) level = 4;
    s_bloom_level = level;
}

int render_get_bloom_level(void) {
    return s_bloom_level;
}

static inline uint8_t dim_channel(uint8_t v, int div) {
    return (uint8_t)(v / div);
}


bool surface_init(Surface *s, int logical_width, int logical_height) {
    if (!s || logical_width <= 0 || logical_height != 240)
        return false;

    memset(s, 0, sizeof(*s));

    s->width = logical_width;
    s->height = logical_height;
    s->fb_width = 240;
    s->fb_height = logical_width;
    s->byte_size = (size_t)s->fb_width * (size_t)s->fb_height * 3u;

    s->pixels = (uint8_t *)malloc(s->byte_size);
    if (!s->pixels) {
        memset(s, 0, sizeof(*s));
        return false;
    }

    memset(s->pixels, 0, s->byte_size);
    return true;
}

void surface_destroy(Surface *s) {
    if (!s) return;
    free(s->pixels);
    memset(s, 0, sizeof(*s));
}

void surface_present_shifted(const Surface *s,
                             gfxScreen_t screen,
                             gfx3dSide_t side,
                             int shift_x) {
    if (!s || !s->pixels)
        return;

    u16 physical_w = 0;
    u16 physical_h = 0;
    uint8_t *fb = gfxGetFramebuffer(screen, side, &physical_w, &physical_h);
    if (!fb)
        return;

    size_t live_size = (size_t)physical_w * (size_t)physical_h * 3u;

    if (shift_x == 0) {
        size_t copy_size = s->byte_size < live_size ? s->byte_size : live_size;
        memcpy(fb, s->pixels, copy_size);
        return;
    }

    memset(fb, 0, live_size);

    int src_x0 = 0;
    int dst_x0 = 0;
    int columns = s->width;

    if (shift_x > 0) {
        dst_x0 = shift_x;
        columns -= shift_x;
    } else {
        src_x0 = -shift_x;
        columns += shift_x;
    }

    if (dst_x0 >= s->width || src_x0 >= s->width || columns <= 0)
        return;

    if (dst_x0 + columns > s->width)
        columns = s->width - dst_x0;

    if (src_x0 + columns > s->width)
        columns = s->width - src_x0;

    if (columns <= 0)
        return;

    size_t stride = (size_t)physical_w * 3u;

    for (int x = 0; x < columns; ++x) {
        int src_x = src_x0 + x;
        int dst_x = dst_x0 + x;

        memcpy(fb + (size_t)dst_x * stride,
               s->pixels + (size_t)src_x * stride,
               stride);
    }
}

void surface_present(const Surface *s, gfxScreen_t screen) {
    surface_present_shifted(s, screen, GFX_LEFT, 0);
}

void draw_pixel(Surface *s, int x, int y, Color c) {
    if (!s || !s->pixels) return;
    if (x < 0 || y < 0 || x >= s->width || y >= s->height) return;

    size_t pixel = (size_t)x * (size_t)s->fb_width
                 + (size_t)(s->fb_width - 1 - y);
    size_t i = pixel * 3u;

    s->pixels[i + 0] = c.b;
    s->pixels[i + 1] = c.g;
    s->pixels[i + 2] = c.r;
}

static inline uint8_t sat_add_u8(uint8_t a, uint8_t b) {
    unsigned v = (unsigned)a + (unsigned)b;
    return (uint8_t)(v > 255u ? 255u : v);
}

static void draw_pixel_add(Surface *s, int x, int y, Color c) {
    if (!s || !s->pixels) return;
    if (x < 0 || y < 0 || x >= s->width || y >= s->height) return;

    size_t pixel = (size_t)x * (size_t)s->fb_width
                 + (size_t)(s->fb_width - 1 - y);
    size_t i = pixel * 3u;

    s->pixels[i + 0] = sat_add_u8(s->pixels[i + 0], c.b);
    s->pixels[i + 1] = sat_add_u8(s->pixels[i + 1], c.g);
    s->pixels[i + 2] = sat_add_u8(s->pixels[i + 2], c.r);
}

static void draw_line_add(Surface *s, int x0, int y0, int x1, int y1, Color c) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        draw_pixel_add(s, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;

        int e2 = err * 2;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void draw_rect_outline_add(Surface *s, int x, int y, int w, int h, Color c) {
    if (w <= 0 || h <= 0) return;
    draw_line_add(s, x, y, x + w - 1, y, c);
    draw_line_add(s, x, y + h - 1, x + w - 1, y + h - 1, c);
    draw_line_add(s, x, y, x, y + h - 1, c);
    draw_line_add(s, x + w - 1, y, x + w - 1, y + h - 1, c);
}

static void draw_rotated_outline_add(Surface *s, float cx, float cy,
                                     float half, float radians, Color c) {
    float sn = sinf(radians);
    float cs = cosf(radians);

    float lx[4] = { -half,  half,  half, -half };
    float ly[4] = { -half, -half,  half,  half };
    int px[4];
    int py[4];

    for (int i = 0; i < 4; ++i) {
        float rx = lx[i] * cs - ly[i] * sn;
        float ry = lx[i] * sn + ly[i] * cs;
        px[i] = (int)lroundf(cx + rx);
        py[i] = (int)lroundf(cy + ry);
    }

    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) & 3;
        draw_line_add(s, px[i], py[i], px[j], py[j], c);
    }
}

static void draw_rotated_rect_outline_add(Surface *s,
                                          float cx,
                                          float cy,
                                          float half_w,
                                          float half_h,
                                          float radians,
                                          Color c) {
    float sn = sinf(radians);
    float cs = cosf(radians);

    float lx[4] = { -half_w, half_w, half_w, -half_w };
    float ly[4] = { -half_h, -half_h, half_h, half_h };
    int px[4];
    int py[4];

    for (int i = 0; i < 4; ++i) {
        float rx =
            lx[i] * cs -
            ly[i] * sn;

        float ry =
            lx[i] * sn +
            ly[i] * cs;

        px[i] =
            (int)lroundf(
                cx + rx
            );

        py[i] =
            (int)lroundf(
                cy + ry
            );
    }

    for (int i = 0; i < 4; ++i) {
        int j =
            (i + 1) &
            3;

        draw_line_add(
            s,
            px[i],
            py[i],
            px[j],
            py[j],
            c
        );
    }
}

void surface_clear(Surface *s, Color c) {
    if (!s || !s->pixels) return;

    /*
       Most screens in Bad3DSGame are black. Let libc use its optimized
       memset path instead of touching three bytes manually per pixel.
    */
    if (c.r == 0 && c.g == 0 && c.b == 0) {
        memset(s->pixels, 0, s->byte_size);
        return;
    }

    size_t pixels = (size_t)s->fb_width * (size_t)s->fb_height;
    uint8_t *p = s->pixels;

    for (size_t i = 0; i < pixels; ++i) {
        *p++ = c.b;
        *p++ = c.g;
        *p++ = c.r;
    }
}

void draw_rect(Surface *s, int x, int y, int w, int h, Color c) {
    if (!s || !s->pixels || w <= 0 || h <= 0) return;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;

    if (x1 > s->width) x1 = s->width;
    if (y1 > s->height) y1 = s->height;
    if (x0 >= x1 || y0 >= y1) return;

    /*
       Native 3DS framebuffer memory is rotated. For one logical X column,
       logical Y pixels are contiguous in memory (in reverse visual order).
       Start at y1-1 and walk forward through RAM.
    */
    int column_pixels = y1 - y0;

    for (int px = x0; px < x1; ++px) {
        size_t first_pixel = (size_t)px * (size_t)s->fb_width
                           + (size_t)(s->fb_width - y1);
        uint8_t *p = s->pixels + first_pixel * 3u;

        for (int n = 0; n < column_pixels; ++n) {
            p[0] = c.b;
            p[1] = c.g;
            p[2] = c.r;
            p += 3;
        }
    }
}

void draw_rect_outline(Surface *s, int x, int y, int w, int h, Color c) {
    if (w <= 0 || h <= 0) return;
    draw_line(s, x, y, x + w - 1, y, c);
    draw_line(s, x, y + h - 1, x + w - 1, y + h - 1, c);
    draw_line(s, x, y, x, y + h - 1, c);
    draw_line(s, x + w - 1, y, x + w - 1, y + h - 1, c);
}

void draw_line(Surface *s, int x0, int y0, int x1, int y1, Color c) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        draw_pixel(s, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_circle_outline(Surface *s, int cx, int cy, int r, Color c) {
    int x = r, y = 0, err = 0;
    while (x >= y) {
        draw_pixel(s, cx + x, cy + y, c);
        draw_pixel(s, cx + y, cy + x, c);
        draw_pixel(s, cx - y, cy + x, c);
        draw_pixel(s, cx - x, cy + y, c);
        draw_pixel(s, cx - x, cy - y, c);
        draw_pixel(s, cx - y, cy - x, c);
        draw_pixel(s, cx + y, cy - x, c);
        draw_pixel(s, cx + x, cy - y, c);
        ++y;
        if (err <= 0) err += 2 * y + 1;
        if (err > 0) { --x; err -= 2 * x + 1; }
    }
}

void draw_circle_filled(Surface *s, int cx, int cy, int r, Color c) {
    if (!s || r < 0)
        return;

    int rr =
        r * r;

    for (int y = -r; y <= r; ++y) {
        int yy =
            y * y;

        for (int x = -r; x <= r; ++x) {
            if (x * x + yy <= rr)
                draw_pixel(s, cx + x, cy + y, c);
        }
    }
}

void draw_bloom_circle(Surface *s, int cx, int cy, int r, Color core) {
    if (r < 1)
        r = 1;

    if (s_bloom_level <= 0) {
        draw_circle_filled(s, cx, cy, r, core);
        return;
    }

    Color far =
        rgb(
            dim_channel(core.r, 7),
            dim_channel(core.g, 7),
            dim_channel(core.b, 7)
        );

    Color near =
        rgb(
            dim_channel(core.r, 2),
            dim_channel(core.g, 2),
            dim_channel(core.b, 2)
        );

    if (s_bloom_level >= 3)
        draw_circle_outline(s, cx, cy, r + 7, far);

    if (s_bloom_level >= 2)
        draw_circle_outline(s, cx, cy, r + 4, near);

    draw_circle_outline(s, cx, cy, r + 2, near);

    if (s_bloom_level >= 4)
        draw_circle_outline(s, cx, cy, r + 11, far);

    draw_circle_filled(s, cx, cy, r, core);
}

static int edge2d(int ax, int ay,
                  int bx, int by,
                  int px, int py) {
    return
        (px - ax) * (by - ay) -
        (py - ay) * (bx - ax);
}

void draw_triangle_filled(Surface *s,
                          int x0, int y0,
                          int x1, int y1,
                          int x2, int y2,
                          Color c) {
    if (!s)
        return;

    int min_x = x0;
    int max_x = x0;
    int min_y = y0;
    int max_y = y0;

    if (x1 < min_x) min_x = x1;
    if (x2 < min_x) min_x = x2;
    if (x1 > max_x) max_x = x1;
    if (x2 > max_x) max_x = x2;

    if (y1 < min_y) min_y = y1;
    if (y2 < min_y) min_y = y2;
    if (y1 > max_y) max_y = y1;
    if (y2 > max_y) max_y = y2;

    int area =
        edge2d(
            x0, y0,
            x1, y1,
            x2, y2
        );

    if (area == 0) {
        draw_triangle_outline(
            s,
            x0, y0,
            x1, y1,
            x2, y2,
            c
        );
        return;
    }

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            int e0 =
                edge2d(
                    x0, y0,
                    x1, y1,
                    x, y
                );

            int e1 =
                edge2d(
                    x1, y1,
                    x2, y2,
                    x, y
                );

            int e2 =
                edge2d(
                    x2, y2,
                    x0, y0,
                    x, y
                );

            if ((area > 0 &&
                 e0 >= 0 &&
                 e1 >= 0 &&
                 e2 >= 0) ||
                (area < 0 &&
                 e0 <= 0 &&
                 e1 <= 0 &&
                 e2 <= 0)) {
                draw_pixel(s, x, y, c);
            }
        }
    }
}

void draw_triangle_outline(Surface *s,
                           int x0, int y0,
                           int x1, int y1,
                           int x2, int y2,
                           Color c) {
    draw_line(s, x0, y0, x1, y1, c);
    draw_line(s, x1, y1, x2, y2, c);
    draw_line(s, x2, y2, x0, y0, c);
}

void draw_bloom_triangle(Surface *s,
                         int x0, int y0,
                         int x1, int y1,
                         int x2, int y2,
                         Color core) {
    if (s_bloom_level > 0) {
        draw_bloom_line(s, x0, y0, x1, y1, core);
        draw_bloom_line(s, x1, y1, x2, y2, core);
        draw_bloom_line(s, x2, y2, x0, y0, core);
    }

    draw_triangle_filled(
        s,
        x0, y0,
        x1, y1,
        x2, y2,
        core
    );
}


void draw_glow_square(Surface *s, int cx, int cy, int half, Color core) {
    if (s_bloom_level <= 0) {
        draw_rect(s, cx - half, cy - half, half * 2 + 1, half * 2 + 1, core);
        return;
    }

    Color far_glow = rgb(dim_channel(core.r, 8), dim_channel(core.g, 8), dim_channel(core.b, 8));
    Color near_glow = rgb(dim_channel(core.r, 3), dim_channel(core.g, 3), dim_channel(core.b, 3));

    if (s_bloom_level >= 2)
        draw_rect_outline(s, cx - half - 5, cy - half - 5, (half + 5) * 2 + 1, (half + 5) * 2 + 1, far_glow);

    draw_rect_outline(s, cx - half - 2, cy - half - 2, (half + 2) * 2 + 1, (half + 2) * 2 + 1, near_glow);

    if (s_bloom_level >= 4)
        draw_rect_outline(s, cx - half - 8, cy - half - 8, (half + 8) * 2 + 1, (half + 8) * 2 + 1, far_glow);

    draw_rect(s, cx - half, cy - half, half * 2 + 1, half * 2 + 1, core);
}

void draw_bloom_line(Surface *s, int x0, int y0, int x1, int y1, Color core) {
    if (s_bloom_level <= 0) {
        draw_line(s, x0, y0, x1, y1, core);
        return;
    }

    Color far = rgb(dim_channel(core.r, 8),
                    dim_channel(core.g, 8),
                    dim_channel(core.b, 8));
    Color mid = rgb(dim_channel(core.r, 4),
                    dim_channel(core.g, 4),
                    dim_channel(core.b, 4));
    Color near = rgb(dim_channel(core.r, 2),
                     dim_channel(core.g, 2),
                     dim_channel(core.b, 2));

    if (s_bloom_level >= 3) {
        for (int d = -3; d <= 3; d += 3) {
            if (d == 0) continue;
            draw_line_add(s, x0 + d, y0, x1 + d, y1, far);
            draw_line_add(s, x0, y0 + d, x1, y1 + d, far);
        }
    }

    if (s_bloom_level >= 2) {
        draw_line_add(s, x0 - 2, y0, x1 - 2, y1, mid);
        draw_line_add(s, x0 + 2, y0, x1 + 2, y1, mid);
        draw_line_add(s, x0, y0 - 2, x1, y1 - 2, mid);
        draw_line_add(s, x0, y0 + 2, x1, y1 + 2, mid);
    }

    draw_line_add(s, x0 - 1, y0, x1 - 1, y1, near);
    draw_line_add(s, x0 + 1, y0, x1 + 1, y1, near);
    draw_line_add(s, x0, y0 - 1, x1, y1 - 1, near);
    draw_line_add(s, x0, y0 + 1, x1, y1 + 1, near);

    if (s_bloom_level >= 4) {
        draw_line_add(s, x0 - 5, y0, x1 - 5, y1, far);
        draw_line_add(s, x0 + 5, y0, x1 + 5, y1, far);
        draw_line_add(s, x0, y0 - 5, x1, y1 - 5, far);
        draw_line_add(s, x0, y0 + 5, x1, y1 + 5, far);
    }

    draw_line(s, x0, y0, x1, y1, core);
}

void draw_bloom_rect(Surface *s, int cx, int cy, int half_w, int half_h, Color core) {
    if (half_w < 1) half_w = 1;
    if (half_h < 1) half_h = 1;

    if (s_bloom_level <= 0) {
        draw_rect(s, cx - half_w, cy - half_h,
                  half_w * 2 + 1, half_h * 2 + 1, core);
        return;
    }

    Color far = rgb(dim_channel(core.r, 8),
                    dim_channel(core.g, 8),
                    dim_channel(core.b, 8));
    Color mid = rgb(dim_channel(core.r, 4),
                    dim_channel(core.g, 4),
                    dim_channel(core.b, 4));
    Color near = rgb(dim_channel(core.r, 2),
                     dim_channel(core.g, 2),
                     dim_channel(core.b, 2));

    if (s_bloom_level >= 3) {
        draw_rect_outline_add(s,
                              cx - half_w - 10, cy - half_h - 10,
                              (half_w + 10) * 2 + 1, (half_h + 10) * 2 + 1,
                              far);
        draw_rect_outline_add(s,
                              cx - half_w - 7, cy - half_h - 7,
                              (half_w + 7) * 2 + 1, (half_h + 7) * 2 + 1,
                              far);
    }

    if (s_bloom_level >= 2) {
        draw_rect_outline_add(s,
                              cx - half_w - 4, cy - half_h - 4,
                              (half_w + 4) * 2 + 1, (half_h + 4) * 2 + 1,
                              mid);
        draw_rect_outline_add(s,
                              cx - half_w - 2, cy - half_h - 2,
                              (half_w + 2) * 2 + 1, (half_h + 2) * 2 + 1,
                              near);
    }

    draw_rect_outline_add(s,
                          cx - half_w - 1, cy - half_h - 1,
                          (half_w + 1) * 2 + 1, (half_h + 1) * 2 + 1,
                          near);

    if (s_bloom_level >= 3) {
        draw_line_add(s, cx - half_w - 12, cy, cx + half_w + 12, cy, mid);
        draw_line_add(s, cx, cy - half_h - 12, cx, cy + half_h + 12, mid);
    }

    if (s_bloom_level >= 4) {
        draw_rect_outline_add(s,
                              cx - half_w - 14, cy - half_h - 14,
                              (half_w + 14) * 2 + 1, (half_h + 14) * 2 + 1,
                              far);
    }

    draw_rect(s, cx - half_w, cy - half_h,
              half_w * 2 + 1, half_h * 2 + 1, core);
}

void draw_bloom_rotated_square(Surface *s, float cx, float cy, float half,
                               float radians, Color core) {
    if (s_bloom_level <= 0) {
        draw_rotated_square(s, cx, cy, half, radians, core);
        return;
    }

    Color far = rgb(dim_channel(core.r, 7),
                    dim_channel(core.g, 7),
                    dim_channel(core.b, 7));
    Color mid = rgb(dim_channel(core.r, 3),
                    dim_channel(core.g, 3),
                    dim_channel(core.b, 3));
    Color near = rgb(dim_channel(core.r, 2),
                     dim_channel(core.g, 2),
                     dim_channel(core.b, 2));

    if (s_bloom_level >= 3) {
        draw_rotated_outline_add(s, cx, cy, half + 10.0f, radians, far);
        draw_rotated_outline_add(s, cx, cy, half + 7.0f, radians, far);
    }

    if (s_bloom_level >= 2) {
        draw_rotated_outline_add(s, cx, cy, half + 4.0f, radians, mid);
        draw_rotated_outline_add(s, cx, cy, half + 2.0f, radians, near);
    }

    draw_rotated_outline_add(s, cx, cy, half + 1.0f, radians, near);

    if (s_bloom_level >= 4)
        draw_rotated_outline_add(s, cx, cy, half + 14.0f, radians, far);

    draw_rotated_square(s, cx, cy, half, radians, core);
}

void draw_rotated_rect(Surface *s,
                       float cx,
                       float cy,
                       float half_w,
                       float half_h,
                       float radians,
                       Color c) {
    if (!s ||
        !s->pixels ||
        half_w <= 0.0f ||
        half_h <= 0.0f) {
        return;
    }

    float sn = sinf(radians);
    float cs = cosf(radians);

    int radius =
        (int)ceilf(
            sqrtf(
                half_w * half_w +
                half_h * half_h
            )
        ) + 1;

    int min_x = (int)cx - radius;
    int max_x = (int)cx + radius;
    int min_y = (int)cy - radius;
    int max_y = (int)cy + radius;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float dx =
                ((float)x + 0.5f) -
                cx;

            float dy =
                ((float)y + 0.5f) -
                cy;

            float lx =
                 dx * cs +
                 dy * sn;

            float ly =
                -dx * sn +
                 dy * cs;

            if (fabsf(lx) <= half_w &&
                fabsf(ly) <= half_h) {
                draw_pixel(
                    s,
                    x,
                    y,
                    c
                );
            }
        }
    }
}

void draw_rotated_rect_outline(Surface *s,
                               float cx,
                               float cy,
                               float half_w,
                               float half_h,
                               float radians,
                               Color c) {
    if (!s ||
        half_w <= 0.0f ||
        half_h <= 0.0f) {
        return;
    }

    float sn = sinf(radians);
    float cs = cosf(radians);

    float lx[4] = { -half_w, half_w, half_w, -half_w };
    float ly[4] = { -half_h, -half_h, half_h, half_h };
    int px[4];
    int py[4];

    for (int i = 0; i < 4; ++i) {
        float rx =
            lx[i] * cs -
            ly[i] * sn;

        float ry =
            lx[i] * sn +
            ly[i] * cs;

        px[i] =
            (int)lroundf(
                cx + rx
            );

        py[i] =
            (int)lroundf(
                cy + ry
            );
    }

    for (int i = 0; i < 4; ++i) {
        int j =
            (i + 1) &
            3;

        draw_line(
            s,
            px[i],
            py[i],
            px[j],
            py[j],
            c
        );
    }
}

void draw_bloom_rotated_rect(Surface *s,
                             float cx,
                             float cy,
                             float half_w,
                             float half_h,
                             float radians,
                             Color core) {
    if (s_bloom_level <= 0) {
        draw_rotated_rect(s, cx, cy, half_w, half_h, radians, core);
        return;
    }

    Color far = rgb(dim_channel(core.r, 7),
                    dim_channel(core.g, 7),
                    dim_channel(core.b, 7));
    Color mid = rgb(dim_channel(core.r, 3),
                    dim_channel(core.g, 3),
                    dim_channel(core.b, 3));
    Color near = rgb(dim_channel(core.r, 2),
                     dim_channel(core.g, 2),
                     dim_channel(core.b, 2));

    if (s_bloom_level >= 3) {
        draw_rotated_rect_outline_add(
            s, cx, cy, half_w + 8.0f, half_h + 8.0f, radians, far);
    }

    if (s_bloom_level >= 2) {
        draw_rotated_rect_outline_add(
            s, cx, cy, half_w + 5.0f, half_h + 5.0f, radians, mid);
        draw_rotated_rect_outline_add(
            s, cx, cy, half_w + 2.0f, half_h + 2.0f, radians, near);
    }

    draw_rotated_rect_outline_add(
        s, cx, cy, half_w + 1.0f, half_h + 1.0f, radians, near);

    if (s_bloom_level >= 4) {
        draw_rotated_rect_outline_add(
            s, cx, cy, half_w + 12.0f, half_h + 12.0f, radians, far);
    }

    draw_rotated_rect(
        s, cx, cy, half_w, half_h, radians, core);
}

void draw_rotated_square(Surface *s, float cx, float cy, float half, float radians, Color c) {
    float sn = sinf(radians);
    float cs = cosf(radians);
    int radius = (int)ceilf(half * 1.45f) + 1;
    int min_x = (int)cx - radius;
    int max_x = (int)cx + radius;
    int min_y = (int)cy - radius;
    int max_y = (int)cy + radius;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float dx = ((float)x + 0.5f) - cx;
            float dy = ((float)y + 0.5f) - cy;
            float lx =  dx * cs + dy * sn;
            float ly = -dx * sn + dy * cs;
            if (fabsf(lx) <= half && fabsf(ly) <= half) draw_pixel(s, x, y, c);
        }
    }
}

void draw_glow_rotated_square(Surface *s, float cx, float cy, float half, float radians, Color c) {
    if (s_bloom_level <= 0) {
        draw_rotated_square(s, cx, cy, half, radians, c);
        return;
    }

    Color far_glow = rgb(dim_channel(c.r, 9), dim_channel(c.g, 9), dim_channel(c.b, 9));
    Color near_glow = rgb(dim_channel(c.r, 4), dim_channel(c.g, 4), dim_channel(c.b, 4));

    if (s_bloom_level >= 2)
        draw_rotated_square(s, cx, cy, half + 4.0f, radians, far_glow);

    draw_rotated_square(s, cx, cy, half + 2.0f, radians, near_glow);

    if (s_bloom_level >= 4)
        draw_rotated_square(s, cx, cy, half + 7.0f, radians, far_glow);

    draw_rotated_square(s, cx, cy, half, radians, c);
}

static const uint8_t DIGITS[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,2,12}
};

static const uint8_t LETTERS[26][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,14}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {1,1,1,1,17,17,14}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
};

static void glyph_for(char ch, uint8_t rows[7]) {
    memset(rows, 0, 7);
    if (ch >= 'a' && ch <= 'z') ch = (char)toupper((unsigned char)ch);
    if (ch >= '0' && ch <= '9') { memcpy(rows, DIGITS[ch - '0'], 7); return; }
    if (ch >= 'A' && ch <= 'Z') { memcpy(rows, LETTERS[ch - 'A'], 7); return; }

    switch (ch) {
        case ':': rows[1]=4; rows[2]=4; rows[4]=4; rows[5]=4; break;
        case '.': rows[6]=4; break;
        case '!': rows[0]=4; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=4; rows[6]=4; break;
        case '-': rows[3]=14; break;
        case '+': rows[1]=4; rows[2]=4; rows[3]=31; rows[4]=4; rows[5]=4; break;
        case '/': rows[0]=1; rows[1]=2; rows[2]=2; rows[3]=4; rows[4]=8; rows[5]=8; rows[6]=16; break;
        case '$': rows[0]=4; rows[1]=15; rows[2]=20; rows[3]=14; rows[4]=5; rows[5]=30; rows[6]=4; break;
        case '?': rows[0]=14; rows[1]=17; rows[2]=2; rows[3]=4; rows[5]=4; break;
        case '=': rows[2]=31; rows[4]=31; break;
        default: break;
    }
}

int text_width(const char *text, int scale) {
    if (!text || scale <= 0) return 0;
    int n = (int)strlen(text);
    return n > 0 ? n * 6 * scale - scale : 0;
}

void draw_text(Surface *s, int x, int y, const char *text, int scale, Color c) {
    if (!text || scale <= 0) return;
    int cursor = x;
    for (const char *p = text; *p; ++p) {
        if (*p == ' ') { cursor += 6 * scale; continue; }
        uint8_t rows[7];
        glyph_for(*p, rows);
        for (int gy = 0; gy < 7; ++gy) {
            for (int gx = 0; gx < 5; ++gx) {
                if (rows[gy] & (1u << (4 - gx))) {
                    draw_rect(s, cursor + gx * scale, y + gy * scale, scale, scale, c);
                }
            }
        }
        cursor += 6 * scale;
    }
}

void draw_text_center(Surface *s, int center_x, int y, const char *text, int scale, Color c) {
    draw_text(s, center_x - text_width(text, scale) / 2, y, text, scale, c);
}
