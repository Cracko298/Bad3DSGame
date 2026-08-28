#include "gpu_bloom.h"

#include <3ds.h>
#include <citro3d.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gpu_bloom_shbin.h"

#define SCENE_TEX_W 256
#define SCENE_TEX_H 512
#define BLOOM_TEX_W 256
#define BLOOM_TEX_H 128

#define TOP_RAW_W 240
#define TOP_RAW_H 400
#define BOT_RAW_W 240
#define BOT_RAW_H 320

#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | \
     GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define TEX_UPLOAD_RGB8_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | \
     GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct {
    bool initialized;

    DVLB_s *dvlb;
    shaderProgram_s program;
    int projection_loc;

    C3D_Mtx screen_top_proj;
    C3D_Mtx screen_bottom_proj;
    C3D_Mtx bloom_proj;

    C3D_RenderTarget *top_left;
    C3D_RenderTarget *top_right;
    C3D_RenderTarget *bottom;

    C3D_Tex top_scene;
    C3D_Tex bottom_scene;
    C3D_Tex top_mask;
    C3D_Tex bottom_mask;

    C3D_Tex blur_a;
    C3D_Tex blur_b;
    C3D_RenderTarget *blur_a_target;
    C3D_RenderTarget *blur_b_target;

    uint8_t *top_scene_stage;
    uint8_t *bottom_scene_stage;
    uint8_t *top_mask_stage;
    uint8_t *bottom_mask_stage;

    bool bloom_enabled;
    int bloom_radius;
    int bloom_intensity;
    int bloom_quad_passes;
} GpuBloom;

static GpuBloom g_gpu;

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void gpu_bloom_set_parameters(bool enabled,
                              int radius,
                              int intensity,
                              int quad_passes) {
    g_gpu.bloom_enabled = enabled;
    g_gpu.bloom_radius = clamp_int(radius, 1, 4);
    g_gpu.bloom_intensity = clamp_int(intensity, 1, 10);
    g_gpu.bloom_quad_passes = clamp_int(quad_passes, 1, 4);
}

static inline u32 gray_rgba(unsigned v) {
    if (v > 255u) v = 255u;
    return (u32)v |
           ((u32)v << 8) |
           ((u32)v << 16) |
           ((u32)v << 24);
}

static void reset_texenv_tail(void) {
    for (int i = 1; i < 6; ++i) {
        C3D_TexEnv *env = C3D_GetTexEnv(i);
        C3D_TexEnvInit(env);
    }
}

static void bind_replace(C3D_Tex *tex) {
    C3D_TexBind(0, tex);

    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    reset_texenv_tail();

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                   GPU_ONE, GPU_ZERO,
                   GPU_ONE, GPU_ZERO);
}

static void bind_weighted_add(C3D_Tex *tex, unsigned weight_256) {
    unsigned w = weight_256 > 255u ? 255u : weight_256;

    C3D_TexBind(0, tex);

    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_CONSTANT, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    C3D_TexEnvColor(env, gray_rgba(w));

    reset_texenv_tail();

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                   GPU_ONE, GPU_ONE,
                   GPU_ONE, GPU_ONE);
}

static void draw_quad(const C3D_Mtx *projection,
                      float x0, float y0,
                      float x1, float y1,
                      float u_tl, float v_tl,
                      float u_tr, float v_tr,
                      float u_bl, float v_bl,
                      float u_br, float v_br) {
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, g_gpu.projection_loc, projection);

    C3D_ImmDrawBegin(GPU_TRIANGLE_STRIP);

    C3D_ImmSendAttrib(x0, y0, 0.5f, 1.0f);
    C3D_ImmSendAttrib(u_tl, v_tl, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x1, y0, 0.5f, 1.0f);
    C3D_ImmSendAttrib(u_tr, v_tr, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x0, y1, 0.5f, 1.0f);
    C3D_ImmSendAttrib(u_bl, v_bl, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x1, y1, 0.5f, 1.0f);
    C3D_ImmSendAttrib(u_br, v_br, 0.0f, 0.0f);

    C3D_ImmDrawEnd();
}

static void draw_normal_subtexture(const C3D_Mtx *projection,
                                   float x0, float y0,
                                   float x1, float y1,
                                   float valid_w,
                                   float valid_h,
                                   float tex_w,
                                   float tex_h,
                                   float du,
                                   float dv) {
    float u0 = 0.0f + du;
    float u1 = valid_w / tex_w + du;
    float v_top = 1.0f + dv;
    float v_bottom = 1.0f - valid_h / tex_h + dv;

    draw_quad(projection,
              x0, y0, x1, y1,
              u0, v_top,
              u1, v_top,
              u0, v_bottom,
              u1, v_bottom);
}

static void draw_scene_rotated(C3D_Tex *tex,
                               const C3D_Mtx *projection,
                               float logical_w,
                               float logical_h,
                               float raw_h,
                               float x_shift) {
    const float u_right = (float)TOP_RAW_W / (float)SCENE_TEX_W;
    const float v_top = 1.0f;
    const float v_bottom = 1.0f - raw_h / (float)SCENE_TEX_H;

    bind_replace(tex);

    




    draw_quad(projection,
              x_shift, 0.0f,
              logical_w + x_shift, logical_h,
              u_right, v_top,
              u_right, v_bottom,
              0.0f, v_top,
              0.0f, v_bottom);
}

static void stage_scene(const Surface *s,
                        uint8_t *stage,
                        int raw_h) {
    const size_t src_stride = (size_t)TOP_RAW_W * 3u;
    const size_t dst_stride = (size_t)SCENE_TEX_W * 3u;

    for (int row = 0; row < raw_h; ++row) {
        memcpy(stage + (size_t)row * dst_stride,
               s->pixels + (size_t)row * src_stride,
               src_stride);
    }

    GSPGPU_FlushDataCache(stage,
                          (size_t)SCENE_TEX_W *
                          (size_t)SCENE_TEX_H * 3u);
}

static void stage_mask(const Surface *s,
                       uint8_t *stage) {
    if (!s->bloom_pixels)
        return;

    const size_t dst_stride = (size_t)BLOOM_TEX_W * 3u;

    for (int y = 0; y < s->bloom_height; ++y) {
        const uint8_t *src =
            s->bloom_pixels +
            (size_t)y * (size_t)s->bloom_width * 3u;
        uint8_t *dst =
            stage + (size_t)y * dst_stride;

        
        for (int x = 0; x < s->bloom_width; ++x) {
            dst[x * 3 + 0] = src[x * 3 + 2];
            dst[x * 3 + 1] = src[x * 3 + 1];
            dst[x * 3 + 2] = src[x * 3 + 0];
        }
    }

    GSPGPU_FlushDataCache(stage,
                          (size_t)BLOOM_TEX_W *
                          (size_t)BLOOM_TEX_H * 3u);
}

static void upload_rgb8(uint8_t *stage,
                        int width,
                        int height,
                        C3D_Tex *tex) {
    C3D_SyncDisplayTransfer(
        (u32 *)stage,
        GX_BUFFER_DIM(width, height),
        (u32 *)tex->data,
        GX_BUFFER_DIM(width, height),
        TEX_UPLOAD_RGB8_FLAGS
    );
}

static void clear_surface_mask(Surface *s) {
    if (!s || !s->bloom_pixels)
        return;

    if (s->bloom_dirty)
        memset(s->bloom_pixels, 0, s->bloom_byte_size);
    s->bloom_dirty = false;
}

static void blur_pass_dense(C3D_RenderTarget *target,
                            C3D_Tex *source,
                            bool horizontal,
                            int radius) {
    if (radius < 1)
        radius = 1;
    if (radius > 4)
        radius = 4;

    C3D_RenderTargetClear(target, C3D_CLEAR_COLOR, 0x00000000, 0);
    C3D_FrameDrawOn(target);

    C3D_TexSetFilter(source, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(source, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    source->border = 0;

    













    const int taps = radius * 2 + 1;
    const unsigned base_weight = 255u / (unsigned)taps;
    unsigned remainder = 255u - base_weight * (unsigned)taps;

    for (int offset = -radius; offset <= radius; ++offset) {
        unsigned weight = base_weight;

        

        int distance = offset < 0 ? -offset : offset;
        if (remainder > 0u && distance == 0) {
            unsigned add = remainder;
            weight += add;
            remainder -= add;
        }

        float du = horizontal
            ? (float)offset / (float)BLOOM_TEX_W
            : 0.0f;
        float dv = horizontal
            ? 0.0f
            : (float)offset / (float)BLOOM_TEX_H;

        bind_weighted_add(source, weight);
        draw_normal_subtexture(&g_gpu.bloom_proj,
                               0.0f, 0.0f,
                               (float)BLOOM_TEX_W,
                               (float)BLOOM_TEX_H,
                               (float)BLOOM_TEX_W,
                               (float)BLOOM_TEX_H,
                               (float)BLOOM_TEX_W,
                               (float)BLOOM_TEX_H,
                               du,
                               dv);
    }
}

static C3D_Tex *build_blur(C3D_Tex *mask) {
    if (!g_gpu.bloom_enabled)
        return NULL;

    












    int radius = clamp_int(g_gpu.bloom_radius, 1, 4);
    int pairs = clamp_int(g_gpu.bloom_quad_passes, 1, 4);

    C3D_Tex *source = mask;

    for (int pass = 0; pass < pairs; ++pass) {
        blur_pass_dense(g_gpu.blur_a_target,
                        source,
                        true,
                        radius);

        blur_pass_dense(g_gpu.blur_b_target,
                        &g_gpu.blur_a,
                        false,
                        radius);

        source = &g_gpu.blur_b;
    }

    return &g_gpu.blur_b;
}

static void draw_final_bloom(C3D_Tex *blurred,
                             const C3D_Mtx *projection,
                             float logical_w,
                             float logical_h,
                             int mask_w,
                             int mask_h,
                             float x_shift) {
    if (!blurred || !g_gpu.bloom_enabled)
        return;

    




    int intensity = clamp_int(g_gpu.bloom_intensity, 1, 10);
    unsigned primary = (unsigned)intensity * 32u;
    if (primary > 255u) primary = 255u;

    unsigned extra = 0u;
    if (intensity == 9) extra = 64u;
    if (intensity >= 10) extra = 128u;

    C3D_TexSetFilter(blurred, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(blurred, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    blurred->border = 0;

    bind_weighted_add(blurred, primary);
    draw_normal_subtexture(projection,
                           x_shift, 0.0f,
                           logical_w + x_shift, logical_h,
                           (float)mask_w,
                           (float)mask_h,
                           (float)BLOOM_TEX_W,
                           (float)BLOOM_TEX_H,
                           0.0f,
                           0.0f);

    if (extra > 0u) {
        bind_weighted_add(blurred, extra);
        draw_normal_subtexture(projection,
                               x_shift, 0.0f,
                               logical_w + x_shift, logical_h,
                               (float)mask_w,
                               (float)mask_h,
                               (float)BLOOM_TEX_W,
                               (float)BLOOM_TEX_H,
                               0.0f,
                               0.0f);
    }
}

static bool alloc_stage(uint8_t **out,
                        size_t bytes) {
    *out = (uint8_t *)linearAlloc(bytes);
    if (!*out)
        return false;

    memset(*out, 0, bytes);
    return true;
}

static void free_resources(void) {
    if (g_gpu.blur_a_target) {
        C3D_RenderTargetDelete(g_gpu.blur_a_target);
        g_gpu.blur_a_target = NULL;
    }
    if (g_gpu.blur_b_target) {
        C3D_RenderTargetDelete(g_gpu.blur_b_target);
        g_gpu.blur_b_target = NULL;
    }

    if (g_gpu.top_left) {
        C3D_RenderTargetDelete(g_gpu.top_left);
        g_gpu.top_left = NULL;
    }
    if (g_gpu.top_right) {
        C3D_RenderTargetDelete(g_gpu.top_right);
        g_gpu.top_right = NULL;
    }
    if (g_gpu.bottom) {
        C3D_RenderTargetDelete(g_gpu.bottom);
        g_gpu.bottom = NULL;
    }

    if (g_gpu.top_scene.data) C3D_TexDelete(&g_gpu.top_scene);
    if (g_gpu.bottom_scene.data) C3D_TexDelete(&g_gpu.bottom_scene);
    if (g_gpu.top_mask.data) C3D_TexDelete(&g_gpu.top_mask);
    if (g_gpu.bottom_mask.data) C3D_TexDelete(&g_gpu.bottom_mask);
    if (g_gpu.blur_a.data) C3D_TexDelete(&g_gpu.blur_a);
    if (g_gpu.blur_b.data) C3D_TexDelete(&g_gpu.blur_b);

    memset(&g_gpu.top_scene, 0, sizeof(g_gpu.top_scene));
    memset(&g_gpu.bottom_scene, 0, sizeof(g_gpu.bottom_scene));
    memset(&g_gpu.top_mask, 0, sizeof(g_gpu.top_mask));
    memset(&g_gpu.bottom_mask, 0, sizeof(g_gpu.bottom_mask));
    memset(&g_gpu.blur_a, 0, sizeof(g_gpu.blur_a));
    memset(&g_gpu.blur_b, 0, sizeof(g_gpu.blur_b));

    if (g_gpu.top_scene_stage) linearFree(g_gpu.top_scene_stage);
    if (g_gpu.bottom_scene_stage) linearFree(g_gpu.bottom_scene_stage);
    if (g_gpu.top_mask_stage) linearFree(g_gpu.top_mask_stage);
    if (g_gpu.bottom_mask_stage) linearFree(g_gpu.bottom_mask_stage);

    g_gpu.top_scene_stage = NULL;
    g_gpu.bottom_scene_stage = NULL;
    g_gpu.top_mask_stage = NULL;
    g_gpu.bottom_mask_stage = NULL;

    if (g_gpu.dvlb) {
        shaderProgramFree(&g_gpu.program);
        DVLB_Free(g_gpu.dvlb);
        g_gpu.dvlb = NULL;
    }
}

bool gpu_bloom_init(void) {
    memset(&g_gpu, 0, sizeof(g_gpu));

    
    g_gpu.bloom_enabled = true;
    g_gpu.bloom_radius = 2;
    g_gpu.bloom_intensity = 7;
    g_gpu.bloom_quad_passes = 2;

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE))
        return false;

    bool ok = false;

    g_gpu.dvlb =
        DVLB_ParseFile((u32 *)gpu_bloom_shbin,
                       gpu_bloom_shbin_size);
    if (!g_gpu.dvlb)
        goto fail;

    shaderProgramInit(&g_gpu.program);
    shaderProgramSetVsh(&g_gpu.program,
                        &g_gpu.dvlb->DVLE[0]);
    C3D_BindProgram(&g_gpu.program);

    g_gpu.projection_loc =
        shaderInstanceGetUniformLocation(
            g_gpu.program.vertexShader,
            "projection"
        );

    C3D_AttrInfo *attr = C3D_GetAttrInfo();
    AttrInfo_Init(attr);
    AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 2);

    Mtx_OrthoTilt(&g_gpu.screen_top_proj,
                  0.0f, 400.0f,
                  240.0f, 0.0f,
                  0.0f, 1.0f,
                  true);

    Mtx_OrthoTilt(&g_gpu.screen_bottom_proj,
                  0.0f, 320.0f,
                  240.0f, 0.0f,
                  0.0f, 1.0f,
                  true);

    Mtx_Ortho(&g_gpu.bloom_proj,
              0.0f, (float)BLOOM_TEX_W,
              (float)BLOOM_TEX_H, 0.0f,
              0.0f, 1.0f,
              true);

    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);

    g_gpu.top_left =
        C3D_RenderTargetCreate(240, 400,
                               GPU_RB_RGBA8,
                               -1);
    g_gpu.top_right =
        C3D_RenderTargetCreate(240, 400,
                               GPU_RB_RGBA8,
                               -1);
    g_gpu.bottom =
        C3D_RenderTargetCreate(240, 320,
                               GPU_RB_RGBA8,
                               -1);

    if (!g_gpu.top_left ||
        !g_gpu.top_right ||
        !g_gpu.bottom) {
        goto fail;
    }

    C3D_RenderTargetSetOutput(g_gpu.top_left,
                              GFX_TOP,
                              GFX_LEFT,
                              DISPLAY_TRANSFER_FLAGS);
    C3D_RenderTargetSetOutput(g_gpu.top_right,
                              GFX_TOP,
                              GFX_RIGHT,
                              DISPLAY_TRANSFER_FLAGS);
    C3D_RenderTargetSetOutput(g_gpu.bottom,
                              GFX_BOTTOM,
                              GFX_LEFT,
                              DISPLAY_TRANSFER_FLAGS);

    if (!C3D_TexInit(&g_gpu.top_scene,
                     SCENE_TEX_W,
                     SCENE_TEX_H,
                     GPU_RGB8) ||
        !C3D_TexInit(&g_gpu.bottom_scene,
                     SCENE_TEX_W,
                     SCENE_TEX_H,
                     GPU_RGB8) ||
        !C3D_TexInit(&g_gpu.top_mask,
                     BLOOM_TEX_W,
                     BLOOM_TEX_H,
                     GPU_RGB8) ||
        !C3D_TexInit(&g_gpu.bottom_mask,
                     BLOOM_TEX_W,
                     BLOOM_TEX_H,
                     GPU_RGB8) ||
        !C3D_TexInitVRAM(&g_gpu.blur_a,
                         BLOOM_TEX_W,
                         BLOOM_TEX_H,
                         GPU_RGBA8) ||
        !C3D_TexInitVRAM(&g_gpu.blur_b,
                         BLOOM_TEX_W,
                         BLOOM_TEX_H,
                         GPU_RGBA8)) {
        goto fail;
    }

    C3D_TexSetFilter(&g_gpu.top_scene, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetFilter(&g_gpu.bottom_scene, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&g_gpu.top_scene, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    C3D_TexSetWrap(&g_gpu.bottom_scene, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    g_gpu.top_scene.border = 0;
    g_gpu.bottom_scene.border = 0;

    g_gpu.blur_a_target =
        C3D_RenderTargetCreateFromTex(&g_gpu.blur_a,
                                      GPU_TEXFACE_2D,
                                      0,
                                      -1);
    g_gpu.blur_b_target =
        C3D_RenderTargetCreateFromTex(&g_gpu.blur_b,
                                      GPU_TEXFACE_2D,
                                      0,
                                      -1);

    if (!g_gpu.blur_a_target || !g_gpu.blur_b_target)
        goto fail;

    const size_t scene_stage_bytes =
        (size_t)SCENE_TEX_W *
        (size_t)SCENE_TEX_H * 3u;
    const size_t mask_stage_bytes =
        (size_t)BLOOM_TEX_W *
        (size_t)BLOOM_TEX_H * 3u;

    if (!alloc_stage(&g_gpu.top_scene_stage, scene_stage_bytes) ||
        !alloc_stage(&g_gpu.bottom_scene_stage, scene_stage_bytes) ||
        !alloc_stage(&g_gpu.top_mask_stage, mask_stage_bytes) ||
        !alloc_stage(&g_gpu.bottom_mask_stage, mask_stage_bytes)) {
        goto fail;
    }

    g_gpu.initialized = true;
    ok = true;

fail:
    if (!ok) {
        free_resources();
        C3D_Fini();
        memset(&g_gpu, 0, sizeof(g_gpu));
    }

    return ok;
}

void gpu_bloom_shutdown(void) {
    if (!g_gpu.initialized)
        return;

    C3D_FrameSync();
    free_resources();
    C3D_Fini();
    memset(&g_gpu, 0, sizeof(g_gpu));
}

static C3D_Tex *prepare_bloom(Surface *surface,
                              C3D_Tex *mask_tex,
                              uint8_t *mask_stage) {
    if (!g_gpu.bloom_enabled ||
        !surface->bloom_dirty) {
        clear_surface_mask(surface);
        return NULL;
    }

    stage_mask(surface, mask_stage);
    upload_rgb8(mask_stage,
                BLOOM_TEX_W,
                BLOOM_TEX_H,
                mask_tex);

    clear_surface_mask(surface);

    return build_blur(mask_tex);
}

bool gpu_bloom_present(Surface *top,
                       Surface *bottom,
                       bool bottom_changed,
                       bool stereo,
                       int stereo_shift) {
    if (!g_gpu.initialized || !top || !bottom)
        return false;

    stage_scene(top,
                g_gpu.top_scene_stage,
                TOP_RAW_H);

    if (bottom_changed) {
        stage_scene(bottom,
                    g_gpu.bottom_scene_stage,
                    BOT_RAW_H);
    }

    if (!C3D_FrameBegin(0))
        return false;

    upload_rgb8(g_gpu.top_scene_stage,
                SCENE_TEX_W,
                SCENE_TEX_H,
                &g_gpu.top_scene);

    if (bottom_changed) {
        upload_rgb8(g_gpu.bottom_scene_stage,
                    SCENE_TEX_W,
                    SCENE_TEX_H,
                    &g_gpu.bottom_scene);
    }

    C3D_Tex *top_blur =
        prepare_bloom(top,
                      &g_gpu.top_mask,
                      g_gpu.top_mask_stage);

    C3D_RenderTargetClear(g_gpu.top_left,
                          C3D_CLEAR_COLOR,
                          0x00000000,
                          0);
    C3D_FrameDrawOn(g_gpu.top_left);

    draw_scene_rotated(&g_gpu.top_scene,
                       &g_gpu.screen_top_proj,
                       400.0f,
                       240.0f,
                       (float)TOP_RAW_H,
                       stereo ? (float)-stereo_shift : 0.0f);

    draw_final_bloom(top_blur,
                     &g_gpu.screen_top_proj,
                     400.0f,
                     240.0f,
                     top->bloom_width,
                     top->bloom_height,
                     stereo ? (float)-stereo_shift : 0.0f);

    if (stereo) {
        C3D_RenderTargetClear(g_gpu.top_right,
                              C3D_CLEAR_COLOR,
                              0x00000000,
                              0);
        C3D_FrameDrawOn(g_gpu.top_right);

        draw_scene_rotated(&g_gpu.top_scene,
                           &g_gpu.screen_top_proj,
                           400.0f,
                           240.0f,
                           (float)TOP_RAW_H,
                           (float)stereo_shift);

        draw_final_bloom(top_blur,
                         &g_gpu.screen_top_proj,
                         400.0f,
                         240.0f,
                         top->bloom_width,
                         top->bloom_height,
                         (float)stereo_shift);
    }

    if (bottom_changed) {
        C3D_Tex *bottom_blur =
            prepare_bloom(bottom,
                          &g_gpu.bottom_mask,
                          g_gpu.bottom_mask_stage);

        C3D_RenderTargetClear(g_gpu.bottom,
                              C3D_CLEAR_COLOR,
                              0x00000000,
                              0);
        C3D_FrameDrawOn(g_gpu.bottom);

        draw_scene_rotated(&g_gpu.bottom_scene,
                           &g_gpu.screen_bottom_proj,
                           320.0f,
                           240.0f,
                           (float)BOT_RAW_H,
                           0.0f);

        draw_final_bloom(bottom_blur,
                         &g_gpu.screen_bottom_proj,
                         320.0f,
                         240.0f,
                         bottom->bloom_width,
                         bottom->bloom_height,
                         0.0f);
    }

    C3D_FrameEnd(0);
    return true;
}
