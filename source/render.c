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

    s->bloom_width = (logical_width + 1) / 2;
    s->bloom_height = (logical_height + 1) / 2;
    s->bloom_byte_size =
        (size_t)s->bloom_width *
        (size_t)s->bloom_height *
        3u;

    s->bloom_pixels =
        (uint8_t *)malloc(s->bloom_byte_size);
    s->bloom_tmp =
        (uint8_t *)malloc(s->bloom_byte_size);

    if (!s->pixels ||
        !s->bloom_pixels ||
        !s->bloom_tmp) {
        free(s->pixels);
        free(s->bloom_pixels);
        free(s->bloom_tmp);
        memset(s, 0, sizeof(*s));
        return false;
    }

    memset(s->pixels, 0, s->byte_size);
    memset(s->bloom_pixels, 0, s->bloom_byte_size);
    memset(s->bloom_tmp, 0, s->bloom_byte_size);
    return true;
}

void surface_destroy(Surface *s) {
    if (!s) return;
    free(s->pixels);
    free(s->bloom_pixels);
    free(s->bloom_tmp);
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

static inline uint8_t sat_add_u8(uint8_t a, uint8_t b);

#define BLOOM_SHIFT 1
#define BLOOM_SCALE (1 << BLOOM_SHIFT)

static inline size_t bloom_index(const Surface *s, int x, int y) {
    return ((size_t)y * (size_t)s->bloom_width + (size_t)x) * 3u;
}

static inline void bloom_put(Surface *s, int bx, int by, Color c) {
    if (!s || !s->bloom_pixels || s_bloom_level <= 0)
        return;
    if (bx < 0 || by < 0 || bx >= s->bloom_width || by >= s->bloom_height)
        return;

    size_t i = bloom_index(s, bx, by);
    if (c.r > s->bloom_pixels[i + 0]) s->bloom_pixels[i + 0] = c.r;
    if (c.g > s->bloom_pixels[i + 1]) s->bloom_pixels[i + 1] = c.g;
    if (c.b > s->bloom_pixels[i + 2]) s->bloom_pixels[i + 2] = c.b;
    s->bloom_dirty = true;
}

static inline void bloom_put_scaled(Surface *s,
                                    int bx,
                                    int by,
                                    Color c,
                                    unsigned alpha_256) {
    if (alpha_256 == 0u)
        return;

    if (alpha_256 >= 255u) {
        bloom_put(s, bx, by, c);
        return;
    }

    Color scaled = rgb(
        (uint8_t)((unsigned)c.r * alpha_256 >> 8),
        (uint8_t)((unsigned)c.g * alpha_256 >> 8),
        (uint8_t)((unsigned)c.b * alpha_256 >> 8)
    );

    bloom_put(s, bx, by, scaled);
}

static void bloom_emit_line(Surface *s,
                            int x0, int y0,
                            int x1, int y1,
                            Color c) {
    if (!s || s_bloom_level <= 0)
        return;

    
    x0 >>= BLOOM_SHIFT;
    y0 >>= BLOOM_SHIFT;
    x1 >>= BLOOM_SHIFT;
    y1 >>= BLOOM_SHIFT;

    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        bloom_put(s, x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = err * 2;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void bloom_emit_rect(Surface *s,
                            int x, int y,
                            int w, int h,
                            Color c) {
    if (!s || w <= 0 || h <= 0 || s_bloom_level <= 0)
        return;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;
    if (x1 > s->width) x1 = s->width;
    if (y1 > s->height) y1 = s->height;
    if (x0 >= x1 || y0 >= y1)
        return;

    float rx0 = (float)x0 / (float)BLOOM_SCALE;
    float ry0 = (float)y0 / (float)BLOOM_SCALE;
    float rx1 = (float)x1 / (float)BLOOM_SCALE;
    float ry1 = (float)y1 / (float)BLOOM_SCALE;

    const float fringe = 1.35f;
    const float fringe2 = fringe * fringe;

    int bx0 = (int)floorf(rx0 - fringe);
    int by0 = (int)floorf(ry0 - fringe);
    int bx1 = (int)ceilf(rx1 + fringe);
    int by1 = (int)ceilf(ry1 + fringe);

    if (bx0 < 0) bx0 = 0;
    if (by0 < 0) by0 = 0;
    if (bx1 >= s->bloom_width) bx1 = s->bloom_width - 1;
    if (by1 >= s->bloom_height) by1 = s->bloom_height - 1;

    for (int by = by0; by <= by1; ++by) {
        float py = (float)by + 0.5f;
        float dy = 0.0f;
        if (py < ry0) dy = ry0 - py;
        else if (py > ry1) dy = py - ry1;

        for (int bx = bx0; bx <= bx1; ++bx) {
            float px = (float)bx + 0.5f;
            float dx = 0.0f;
            if (px < rx0) dx = rx0 - px;
            else if (px > rx1) dx = px - rx1;

            float dist2 = dx * dx + dy * dy;
            if (dist2 > fringe2)
                continue;

            unsigned alpha = 255u;
            if (dist2 > 0.0f) {
                float t = 1.0f - (dist2 / fringe2);
                if (t < 0.0f) t = 0.0f;
                alpha = (unsigned)(t * 255.0f);
            }
            bloom_put_scaled(s, bx, by, c, alpha);
        }
    }
}

static void bloom_emit_circle(Surface *s,
                              int cx, int cy,
                              int r,
                              Color c) {
    if (!s || r < 0 || s_bloom_level <= 0)
        return;

    int bcx = cx >> BLOOM_SHIFT;
    int bcy = cy >> BLOOM_SHIFT;
    int br = (r + BLOOM_SCALE - 1) >> BLOOM_SHIFT;
    if (br < 1) br = 1;
    int rr = br * br;

    for (int y = -br; y <= br; ++y) {
        int yy = y * y;
        for (int x = -br; x <= br; ++x) {
            if (x * x + yy <= rr)
                bloom_put(s, bcx + x, bcy + y, c);
        }
    }
}

static void bloom_emit_triangle(Surface *s,
                                int x0, int y0,
                                int x1, int y1,
                                int x2, int y2,
                                Color c) {
    if (!s || s_bloom_level <= 0)
        return;

    x0 >>= BLOOM_SHIFT; y0 >>= BLOOM_SHIFT;
    x1 >>= BLOOM_SHIFT; y1 >>= BLOOM_SHIFT;
    x2 >>= BLOOM_SHIFT; y2 >>= BLOOM_SHIFT;

    int min_x = x0, max_x = x0, min_y = y0, max_y = y0;
    if (x1 < min_x) min_x = x1;
    if (x2 < min_x) min_x = x2;
    if (x1 > max_x) max_x = x1;
    if (x2 > max_x) max_x = x2;
    if (y1 < min_y) min_y = y1;
    if (y2 < min_y) min_y = y2;
    if (y1 > max_y) max_y = y1;
    if (y2 > max_y) max_y = y2;

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= s->bloom_width) max_x = s->bloom_width - 1;
    if (max_y >= s->bloom_height) max_y = s->bloom_height - 1;

    int area =
        (x0 - x2) * (y1 - y2) -
        (y0 - y2) * (x1 - x2);

    if (area == 0) {
        bloom_emit_line(s,
                        x0 << BLOOM_SHIFT, y0 << BLOOM_SHIFT,
                        x1 << BLOOM_SHIFT, y1 << BLOOM_SHIFT, c);
        bloom_emit_line(s,
                        x1 << BLOOM_SHIFT, y1 << BLOOM_SHIFT,
                        x2 << BLOOM_SHIFT, y2 << BLOOM_SHIFT, c);
        bloom_emit_line(s,
                        x2 << BLOOM_SHIFT, y2 << BLOOM_SHIFT,
                        x0 << BLOOM_SHIFT, y0 << BLOOM_SHIFT, c);
        return;
    }

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            int e0 = (x - x0) * (y1 - y0) - (y - y0) * (x1 - x0);
            int e1 = (x - x1) * (y2 - y1) - (y - y1) * (x2 - x1);
            int e2 = (x - x2) * (y0 - y2) - (y - y2) * (x0 - x2);
            bool has_neg = (e0 < 0) || (e1 < 0) || (e2 < 0);
            bool has_pos = (e0 > 0) || (e1 > 0) || (e2 > 0);
            if (!(has_neg && has_pos))
                bloom_put(s, x, y, c);
        }
    }
}

static void bloom_emit_rotated_rect(Surface *s,
                                    float cx,
                                    float cy,
                                    float half_w,
                                    float half_h,
                                    float radians,
                                    Color c) {
    if (!s || half_w <= 0.0f || half_h <= 0.0f || s_bloom_level <= 0)
        return;

    
    float bcx = cx / (float)BLOOM_SCALE;
    float bcy = cy / (float)BLOOM_SCALE;
    float bhw = half_w / (float)BLOOM_SCALE;
    float bhh = half_h / (float)BLOOM_SCALE;
    if (bhw < 0.5f) bhw = 0.5f;
    if (bhh < 0.5f) bhh = 0.5f;

    float sn = sinf(radians);
    float cs = cosf(radians);

    
    const float fringe = 1.35f;
    const float fringe2 = fringe * fringe;

    int radius = (int)ceilf(sqrtf((bhw + fringe) * (bhw + fringe) +
                                  (bhh + fringe) * (bhh + fringe))) + 1;
    int min_x = (int)bcx - radius;
    int max_x = (int)bcx + radius;
    int min_y = (int)bcy - radius;
    int max_y = (int)bcy + radius;

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= s->bloom_width) max_x = s->bloom_width - 1;
    if (max_y >= s->bloom_height) max_y = s->bloom_height - 1;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float dx = ((float)x + 0.5f) - bcx;
            float dy = ((float)y + 0.5f) - bcy;
            float lx = dx * cs + dy * sn;
            float ly = -dx * sn + dy * cs;

            float ox = fabsf(lx) - bhw;
            float oy = fabsf(ly) - bhh;
            if (ox < 0.0f) ox = 0.0f;
            if (oy < 0.0f) oy = 0.0f;

            float dist2 = ox * ox + oy * oy;
            if (dist2 > fringe2)
                continue;

            unsigned alpha = 255u;
            if (dist2 > 0.0f) {
                float t = 1.0f - (dist2 / fringe2);
                if (t < 0.0f) t = 0.0f;
                alpha = (unsigned)(t * 255.0f);
            }
            bloom_put_scaled(s, x, y, c, alpha);
        }
    }
}

static void bloom_box_blur_horizontal(const uint8_t *src,
                                      uint8_t *dst,
                                      int w,
                                      int h,
                                      int radius) {
    for (int y = 0; y < h; ++y) {
        unsigned sum[3] = {0, 0, 0};
        int initial_end = radius < w - 1 ? radius : w - 1;
        for (int x = 0; x <= initial_end; ++x) {
            size_t i = ((size_t)y * (size_t)w + (size_t)x) * 3u;
            sum[0] += src[i + 0];
            sum[1] += src[i + 1];
            sum[2] += src[i + 2];
        }

        for (int x = 0; x < w; ++x) {
            int left = x - radius;
            int right = x + radius;
            int count =
                (right < w ? right : w - 1) -
                (left > 0 ? left : 0) + 1;
            size_t out = ((size_t)y * (size_t)w + (size_t)x) * 3u;
            dst[out + 0] = (uint8_t)(sum[0] / (unsigned)count);
            dst[out + 1] = (uint8_t)(sum[1] / (unsigned)count);
            dst[out + 2] = (uint8_t)(sum[2] / (unsigned)count);

            int remove_x = x - radius;
            if (remove_x >= 0) {
                size_t i = ((size_t)y * (size_t)w + (size_t)remove_x) * 3u;
                sum[0] -= src[i + 0];
                sum[1] -= src[i + 1];
                sum[2] -= src[i + 2];
            }
            int add_x = x + radius + 1;
            if (add_x < w) {
                size_t i = ((size_t)y * (size_t)w + (size_t)add_x) * 3u;
                sum[0] += src[i + 0];
                sum[1] += src[i + 1];
                sum[2] += src[i + 2];
            }
        }
    }
}

static void bloom_box_blur_vertical(const uint8_t *src,
                                    uint8_t *dst,
                                    int w,
                                    int h,
                                    int radius) {
    for (int x = 0; x < w; ++x) {
        unsigned sum[3] = {0, 0, 0};
        int initial_end = radius < h - 1 ? radius : h - 1;
        for (int y = 0; y <= initial_end; ++y) {
            size_t i = ((size_t)y * (size_t)w + (size_t)x) * 3u;
            sum[0] += src[i + 0];
            sum[1] += src[i + 1];
            sum[2] += src[i + 2];
        }

        for (int y = 0; y < h; ++y) {
            int top = y - radius;
            int bottom = y + radius;
            int count =
                (bottom < h ? bottom : h - 1) -
                (top > 0 ? top : 0) + 1;
            size_t out = ((size_t)y * (size_t)w + (size_t)x) * 3u;
            dst[out + 0] = (uint8_t)(sum[0] / (unsigned)count);
            dst[out + 1] = (uint8_t)(sum[1] / (unsigned)count);
            dst[out + 2] = (uint8_t)(sum[2] / (unsigned)count);

            int remove_y = y - radius;
            if (remove_y >= 0) {
                size_t i = ((size_t)remove_y * (size_t)w + (size_t)x) * 3u;
                sum[0] -= src[i + 0];
                sum[1] -= src[i + 1];
                sum[2] -= src[i + 2];
            }
            int add_y = y + radius + 1;
            if (add_y < h) {
                size_t i = ((size_t)add_y * (size_t)w + (size_t)x) * 3u;
                sum[0] += src[i + 0];
                sum[1] += src[i + 1];
                sum[2] += src[i + 2];
            }
        }
    }
}

void surface_apply_bloom(Surface *s) {
    if (!s || !s->pixels || !s->bloom_pixels || !s->bloom_tmp)
        return;

    if (s_bloom_level <= 0) {
        if (s->bloom_dirty)
            memset(s->bloom_pixels, 0, s->bloom_byte_size);
        s->bloom_dirty = false;
        return;
    }

    if (!s->bloom_dirty)
        return;

    






    static const int radius_for_level[5] = {0, 1, 2, 4, 6};
    int radius = radius_for_level[s_bloom_level];

    bloom_box_blur_horizontal(
        s->bloom_pixels, s->bloom_tmp,
        s->bloom_width, s->bloom_height, radius);
    bloom_box_blur_vertical(
        s->bloom_tmp, s->bloom_pixels,
        s->bloom_width, s->bloom_height, radius);

    static const unsigned strength_256[5] = {
        0u, 150u, 210u, 285u, 350u
    };
    unsigned strength = strength_256[s_bloom_level];

    for (int bx = 0; bx < s->bloom_width; ++bx) {
        int x = bx << 1;
        int bx1 = bx + 1 < s->bloom_width ? bx + 1 : bx;

        for (int by = 0; by < s->bloom_height; ++by) {
            int y = by << 1;
            int by1 = by + 1 < s->bloom_height ? by + 1 : by;

            size_t i00 = bloom_index(s, bx, by);
            size_t i10 = bloom_index(s, bx1, by);
            size_t i01 = bloom_index(s, bx, by1);
            size_t i11 = bloom_index(s, bx1, by1);

            unsigned r00 = s->bloom_pixels[i00 + 0];
            unsigned g00 = s->bloom_pixels[i00 + 1];
            unsigned b00 = s->bloom_pixels[i00 + 2];
            unsigned r10 = s->bloom_pixels[i10 + 0];
            unsigned g10 = s->bloom_pixels[i10 + 1];
            unsigned b10 = s->bloom_pixels[i10 + 2];
            unsigned r01 = s->bloom_pixels[i01 + 0];
            unsigned g01 = s->bloom_pixels[i01 + 1];
            unsigned b01 = s->bloom_pixels[i01 + 2];
            unsigned r11 = s->bloom_pixels[i11 + 0];
            unsigned g11 = s->bloom_pixels[i11 + 1];
            unsigned b11 = s->bloom_pixels[i11 + 2];

            if ((r00 | g00 | b00 | r10 | g10 | b10 |
                 r01 | g01 | b01 | r11 | g11 | b11) == 0)
                continue;

            unsigned rr[4] = {
                r00,
                (r00 + r10) >> 1,
                (r00 + r01) >> 1,
                (r00 + r10 + r01 + r11) >> 2
            };
            unsigned gg[4] = {
                g00,
                (g00 + g10) >> 1,
                (g00 + g01) >> 1,
                (g00 + g10 + g01 + g11) >> 2
            };
            unsigned bb[4] = {
                b00,
                (b00 + b10) >> 1,
                (b00 + b01) >> 1,
                (b00 + b10 + b01 + b11) >> 2
            };

            for (int py = 0; py < 2; ++py) {
                int sy = y + py;
                if (sy >= s->height) continue;

                for (int px = 0; px < 2; ++px) {
                    int sx = x + px;
                    if (sx >= s->width) continue;

                    int q = py * 2 + px;
                    unsigned ar = (rr[q] * strength) >> 8;
                    unsigned ag = (gg[q] * strength) >> 8;
                    unsigned ab = (bb[q] * strength) >> 8;

                    uint8_t add_r = (uint8_t)(ar > 255u ? 255u : ar);
                    uint8_t add_g = (uint8_t)(ag > 255u ? 255u : ag);
                    uint8_t add_b = (uint8_t)(ab > 255u ? 255u : ab);
                    if ((add_r | add_g | add_b) == 0) continue;

                    size_t pixel =
                        (size_t)sx * (size_t)s->fb_width +
                        (size_t)(s->fb_width - 1 - sy);
                    uint8_t *dst = s->pixels + pixel * 3u;

                    dst[0] = sat_add_u8(dst[0], add_b);
                    dst[1] = sat_add_u8(dst[1], add_g);
                    dst[2] = sat_add_u8(dst[2], add_r);
                }
            }
        }
    }

    
    memset(s->bloom_pixels, 0, s->bloom_byte_size);
    s->bloom_dirty = false;
}

static inline uint8_t sat_add_u8(uint8_t a, uint8_t b) {
    unsigned v = (unsigned)a + (unsigned)b;
    return (uint8_t)(v > 255u ? 255u : v);
}

void surface_clear(Surface *s, Color c) {
    if (!s || !s->pixels) return;

    

    if (s->bloom_dirty && s->bloom_pixels)
        memset(s->bloom_pixels, 0, s->bloom_byte_size);
    s->bloom_dirty = false;

    



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







#define GAMEPLAY_BLOOM_SOURCE_256 128u

static inline Color gameplay_bloom_color(Color c) {
    return rgb(
        (uint8_t)(((unsigned)c.r * GAMEPLAY_BLOOM_SOURCE_256) >> 8),
        (uint8_t)(((unsigned)c.g * GAMEPLAY_BLOOM_SOURCE_256) >> 8),
        (uint8_t)(((unsigned)c.b * GAMEPLAY_BLOOM_SOURCE_256) >> 8)
    );
}

void draw_bloom_circle(Surface *s, int cx, int cy, int r, Color core) {
    if (r < 1)
        r = 1;

    draw_circle_filled(s, cx, cy, r, core);
    bloom_emit_circle(s, cx, cy, r, gameplay_bloom_color(core));
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
    draw_triangle_filled(s, x0, y0, x1, y1, x2, y2, core);
    bloom_emit_triangle(s, x0, y0, x1, y1, x2, y2,
                        gameplay_bloom_color(core));
}


void draw_glow_square(Surface *s, int cx, int cy, int half, Color core) {
    if (half < 1)
        half = 1;

    draw_rect(s,
              cx - half,
              cy - half,
              half * 2 + 1,
              half * 2 + 1,
              core);

    bloom_emit_rect(s,
                    cx - half,
                    cy - half,
                    half * 2 + 1,
                    half * 2 + 1,
                    gameplay_bloom_color(core));
}

void draw_bloom_line(Surface *s, int x0, int y0, int x1, int y1, Color core) {
    draw_line(s, x0, y0, x1, y1, core);
    bloom_emit_line(s, x0, y0, x1, y1, gameplay_bloom_color(core));
}

void draw_bloom_rect(Surface *s, int cx, int cy, int half_w, int half_h, Color core) {
    if (half_w < 1) half_w = 1;
    if (half_h < 1) half_h = 1;

    int x = cx - half_w;
    int y = cy - half_h;
    int w = half_w * 2 + 1;
    int h = half_h * 2 + 1;

    draw_rect(s, x, y, w, h, core);
    bloom_emit_rect(s, x, y, w, h, gameplay_bloom_color(core));
}

void draw_bloom_rotated_square(Surface *s, float cx, float cy, float half,
                               float radians, Color core) {
    if (half < 1.0f)
        half = 1.0f;

    draw_rotated_square(s, cx, cy, half, radians, core);
    bloom_emit_rotated_rect(s, cx, cy, half, half, radians,
                            gameplay_bloom_color(core));
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
    draw_rotated_rect(s, cx, cy, half_w, half_h, radians, core);
    bloom_emit_rotated_rect(s, cx, cy, half_w, half_h, radians,
                            gameplay_bloom_color(core));
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
    if (half < 1.0f)
        half = 1.0f;

    draw_rotated_square(s, cx, cy, half, radians, c);
    bloom_emit_rotated_rect(s, cx, cy, half, half, radians,
                            gameplay_bloom_color(c));
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

void draw_bloom_text_center_animated(Surface *s,
                                     float center_x,
                                     float y,
                                     const char *text,
                                     int scale,
                                     float zoom,
                                     float radians,
                                     float swizzle,
                                     float twist,
                                     int bold_px,
                                     unsigned bloom_strength_256,
                                     Color c) {
    if (!s || !text || scale <= 0)
        return;

    if (zoom < 0.50f)
        zoom = 0.50f;
    if (zoom > 1.75f)
        zoom = 1.75f;
    if (bold_px < 0)
        bold_px = 0;
    if (bold_px > 3)
        bold_px = 3;
    if (bloom_strength_256 > 256u)
        bloom_strength_256 = 256u;

    const float base_w = (float)text_width(text, scale);
    const float base_h = (float)(7 * scale);
    const float start_x = center_x - base_w * 0.5f;
    const float pivot_y = y + base_h * 0.5f;
    const float half_base_w = base_w > 1.0f ? base_w * 0.5f : 1.0f;

    Color bloom_c = rgb(
        (uint8_t)(((unsigned)c.r * bloom_strength_256) >> 8),
        (uint8_t)(((unsigned)c.g * bloom_strength_256) >> 8),
        (uint8_t)(((unsigned)c.b * bloom_strength_256) >> 8)
    );

    int cursor = 0;
    for (const char *p = text; *p; ++p) {
        if (*p == ' ') {
            cursor += 6 * scale;
            continue;
        }

        uint8_t rows[7];
        glyph_for(*p, rows);

        for (int gy = 0; gy < 7; ++gy) {
            for (int gx = 0; gx < 5; ++gx) {
                if (!(rows[gy] & (1u << (4 - gx))))
                    continue;

                float px =
                    start_x +
                    (float)cursor +
                    (float)(gx * scale) +
                    (float)scale * 0.5f;

                float py =
                    y +
                    (float)(gy * scale) +
                    (float)scale * 0.5f;

                float lx = px - center_x;
                float ly = py - pivot_y;

                lx += ly * swizzle;

                float twist_pos = lx / half_base_w;
                if (twist_pos < -1.0f) twist_pos = -1.0f;
                if (twist_pos >  1.0f) twist_pos =  1.0f;

                lx *= zoom;
                ly *= zoom;

                float local_angle = radians + twist * twist_pos;
                float cs = cosf(local_angle);
                float sn = sinf(local_angle);
                
                ly += sinf(twist_pos * 1.57079633f) * twist * base_h * 0.32f;

                float tx = center_x + lx * cs - ly * sn;
                float ty = pivot_y + lx * sn + ly * cs;

                int cell = (int)lroundf((float)scale * zoom);
                if (cell < 1)
                    cell = 1;

                int w = cell + bold_px;
                int h = cell + bold_px;
                int x0 = (int)lroundf(tx - (float)w * 0.5f);
                int y0 = (int)lroundf(ty - (float)h * 0.5f);

                
                draw_rect(s, x0, y0, w, h, c);

                
                if ((bloom_c.r | bloom_c.g | bloom_c.b) != 0)
                    bloom_emit_rect(s, x0, y0, w, h, bloom_c);
            }
        }

        cursor += 6 * scale;
    }
}
