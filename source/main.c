#include <3ds.h>
#include <stdlib.h>

#include "game.h"
#include "render.h"
#include "audio.h"
#include "gpu_bloom.h"

typedef struct {
    int x_dir;
    int y_dir;
    int x_hold;
    int y_hold;
} NavRepeat;

static int axis_dir(int value) {
    if (value > 55) return 1;
    if (value < -55) return -1;
    return 0;
}

static int repeat_axis(int dir, int *last_dir, int *hold_frames) {
    if (dir == 0) {
        *last_dir = 0;
        *hold_frames = 0;
        return 0;
    }

    if (dir != *last_dir) {
        *last_dir = dir;
        *hold_frames = 0;
        return dir;
    }

    ++(*hold_frames);
    if (*hold_frames >= 18 && ((*hold_frames - 18) % 7) == 0) return dir;
    return 0;
}


static void clear_input_edges(GameInput *in) {
    if (!in) return;
    in->touch_down = false;
    in->touch_up = false;
    in->nav_x = 0;
    in->nav_y = 0;
    in->keys_down = 0;
}

static void wait_for_frame_limit(int target_fps,
                                 unsigned *phase_45,
                                 u64 frame_start_ms) {
    unsigned target_ms = 0;

    switch (target_fps) {
        case 15:
            target_ms = 66;
            break;
        case 30:
            target_ms = 33;
            break;
        case 45:
            target_ms = ((*phase_45 % 3u) == 2u) ? 33u : 16u;
            ++(*phase_45);
            break;
        case 60:
            target_ms = 16;
            break;
        case 0:
        default:
            return;
    }

    while ((unsigned)(osGetTime() - frame_start_ms) < target_ms)
        gspWaitForVBlank();
}

int main(void) {
    
    gfxInitDefault();

    Surface top = {0};
    Surface bottom = {0};

    if (!surface_init(&top, 400, 240) ||
        !surface_init(&bottom, 320, 240)) {
        surface_destroy(&top);
        surface_destroy(&bottom);
        gfxExit();
        return 1;
    }

    bool gpu_backend = gpu_bloom_init();

    srand((unsigned)osGetTime());

    static Game game;
    game_init(&game);

    
    bool audio_init_attempted = false;
    unsigned visible_frames = 0;

    NavRepeat nav = {0, 0, 0, 0};
    bool previous_touch = false;
    u64 previous_ms = osGetTime();
    unsigned fps_45_phase = 0;

    while (aptMainLoop() && !game.request_exit) {
        u64 now_ms = osGetTime();
        float dt = (float)(now_ms - previous_ms) * 0.001f;
        previous_ms = now_ms;
        
        if (dt <= 0.0f || dt > 0.125f) dt = 1.0f / 60.0f;
        hidScanInput();
        u32 down = hidKeysDown();
        u32 held = hidKeysHeld();

        circlePosition cp;
        hidCircleRead(&cp);

        int dx = axis_dir(cp.dx);
        
        int dy = -axis_dir(cp.dy);

        int nav_x = repeat_axis(dx, &nav.x_dir, &nav.x_hold);
        int nav_y = repeat_axis(dy, &nav.y_dir, &nav.y_hold);

        if (down & KEY_DLEFT) nav_x = -1;
        if (down & KEY_DRIGHT) nav_x = 1;
        if (down & KEY_DUP) nav_y = -1;
        if (down & KEY_DDOWN) nav_y = 1;

        bool touching = (held & KEY_TOUCH) != 0;
        touchPosition tp = {160, 120};
        if (touching) hidTouchRead(&tp);

        GameInput in;
        in.touch_down = touching && !previous_touch;
        in.touch_held = touching;
        in.touch_up = !touching && previous_touch;
        in.touch_x = touching ? tp.px : 160;
        in.touch_y = touching ? tp.py : 120;
        in.nav_x = nav_x;
        in.nav_y = nav_y;
        in.keys_down = down;
        in.keys_held = held;

        GameMode mode_before_update = game.mode;

        float remaining_dt = dt;
        GameInput step_in = in;
        int sim_steps = 0;

        while (remaining_dt > 0.000001f && sim_steps < 8) {
            float step_dt = remaining_dt > (1.0f / 60.0f)
                ? (1.0f / 60.0f) : remaining_dt;

            game_update(&game, &step_in, step_dt);
            remaining_dt -= step_dt;
            ++sim_steps;
            clear_input_edges(&step_in);
        }

        
        if (game.mode == MODE_PLAYING &&
            mode_before_update != MODE_PLAYING &&
            mode_before_update != MODE_PAUSED) {
            audio_restart_music();
        }

        
        audio_update();

        game_render_top(&game, &top);

        
        bool bottom_changed =
            game.mode != MODE_PLAYING ||
            (game.frame_counter & 1u) == 0u ||
            in.touch_down || in.touch_up;

        if (bottom_changed)
            game_render_bottom(&game, &bottom, &in);

        bool stereo_3d = game_wants_stereo_3d(&game);
        gfxSet3D(stereo_3d);

        float slider = stereo_3d ? osGet3DSliderState() : 0.0f;
        if (slider < 0.0f) slider = 0.0f;
        if (slider > 1.0f) slider = 1.0f;
        int shift = (int)(slider * 6.0f + 0.5f);

        if (gpu_backend) {
            
            gpu_bloom_set_parameters(
                game_bloom_enabled(&game),
                game_bloom_radius(&game),
                game_bloom_intensity(&game),
                game_bloom_quads(&game)
            );

            gpu_bloom_present(
                &top,
                &bottom,
                bottom_changed,
                stereo_3d,
                shift
            );
        } else {
            
            surface_apply_bloom(&top);
            if (bottom_changed)
                surface_apply_bloom(&bottom);

            if (stereo_3d) {
                surface_present_shifted(&top, GFX_TOP, GFX_LEFT, -shift);
                surface_present_shifted(&top, GFX_TOP, GFX_RIGHT, shift);
            } else {
                surface_present(&top, GFX_TOP);
            }

            if (bottom_changed)
                surface_present(&bottom, GFX_BOTTOM);

            gfxFlushBuffers();
            gfxSwapBuffers();
        }

        wait_for_frame_limit(
            game_target_fps(&game),
            &fps_45_phase,
            now_ms
        );

        ++visible_frames;

        if (!audio_init_attempted &&
            visible_frames >= 3) {
            audio_init_attempted = true;
            audio_init();
        }

        previous_touch = touching;
    }

    audio_shutdown();
    game_shutdown(&game);
    if (gpu_backend)
        gpu_bloom_shutdown();
    surface_destroy(&top);
    surface_destroy(&bottom);
    gfxExit();
    return 0;
}
