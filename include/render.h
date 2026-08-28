#pragma once

#include <3ds.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t r, g, b;
} Color;

typedef struct {
    
    uint8_t *pixels;
    
    int width;
    int height;

    int fb_width;
    int fb_height;

    size_t byte_size;

    uint8_t *bloom_pixels;
    uint8_t *bloom_tmp;
    int bloom_width;
    int bloom_height;
    size_t bloom_byte_size;
    bool bloom_dirty;
} Surface;

static inline Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    Color c = { r, g, b };
    return c;
}

bool surface_init(Surface *s, int logical_width, int logical_height);
void surface_destroy(Surface *s);
void surface_present(const Surface *s, gfxScreen_t screen);
void surface_present_shifted(const Surface *s, gfxScreen_t screen, gfx3dSide_t side, int shift_x);
void surface_clear(Surface *s, Color c);

void surface_apply_bloom(Surface *s);
void draw_pixel(Surface *s, int x, int y, Color c);
void draw_rect(Surface *s, int x, int y, int w, int h, Color c);
void draw_rect_outline(Surface *s, int x, int y, int w, int h, Color c);
void draw_line(Surface *s, int x0, int y0, int x1, int y1, Color c);
void draw_circle_outline(Surface *s, int cx, int cy, int r, Color c);
void draw_circle_filled(Surface *s, int cx, int cy, int r, Color c);
void draw_bloom_circle(Surface *s, int cx, int cy, int r, Color core);
void draw_triangle_filled(Surface *s,
                          int x0, int y0,
                          int x1, int y1,
                          int x2, int y2,
                          Color c);
void draw_triangle_outline(Surface *s,
                           int x0, int y0,
                           int x1, int y1,
                           int x2, int y2,
                           Color c);
void draw_bloom_triangle(Surface *s,
                         int x0, int y0,
                         int x1, int y1,
                         int x2, int y2,
                         Color core);
void draw_glow_square(Surface *s, int cx, int cy, int half, Color core);


void draw_bloom_line(Surface *s, int x0, int y0, int x1, int y1, Color core);
void draw_bloom_rect(Surface *s, int cx, int cy, int half_w, int half_h, Color core);
void draw_bloom_rotated_square(Surface *s, float cx, float cy, float half,
                               float radians, Color core);

void draw_rotated_rect(Surface *s, float cx, float cy,
                       float half_w, float half_h,
                       float radians, Color c);
void draw_rotated_rect_outline(Surface *s, float cx, float cy,
                               float half_w, float half_h,
                               float radians, Color c);
void draw_bloom_rotated_rect(Surface *s, float cx, float cy,
                             float half_w, float half_h,
                             float radians, Color core);

void draw_rotated_square(Surface *s, float cx, float cy, float half, float radians, Color c);
void draw_glow_rotated_square(Surface *s, float cx, float cy, float half, float radians, Color c);
void draw_text(Surface *s, int x, int y, const char *text, int scale, Color c);
void draw_text_center(Surface *s, int center_x, int y, const char *text, int scale, Color c);


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
                                     Color c);

int text_width(const char *text, int scale);


void render_set_bloom_level(int level);
int render_get_bloom_level(void);
