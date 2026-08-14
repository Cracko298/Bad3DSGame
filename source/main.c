#include <3ds.h>
#include <stdlib.h>

#include "game.h"
#include "render.h"
#include "audio.h"

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

int main(void) {
    /* libctru BGR8 double buffering; CPU software surfaces are copied to framebuffers. */
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

    srand((unsigned)osGetTime());

    static Game game;
    game_init(&game);

    /* Audio initialization occurs after three visible frames. */
    bool audio_init_attempted = false;
    unsigned visible_frames = 0;

    NavRepeat nav = {0, 0, 0, 0};
    bool previous_touch = false;
    u64 previous_ms = osGetTime();

    while (aptMainLoop() && !game.request_exit) {
        u64 now_ms = osGetTime();
        float dt = (float)(now_ms - previous_ms) * 0.001f;
        previous_ms = now_ms;

        /* Frame delta is clamped to 1/60 after stalls or invalid timing. */
        if (dt <= 0.0f || dt > 0.050f) dt = 1.0f / 60.0f;
        hidScanInput();
        u32 down = hidKeysDown();
        u32 held = hidKeysHeld();

        circlePosition cp;
        hidCircleRead(&cp);

        int dx = axis_dir(cp.dx);
        /* Menu navigation uses -1 for up. */
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

        game_update(&game, &in, dt);

        /* Entering a new run restarts music; unpausing does not. */
        if (game.mode == MODE_PLAYING &&
            mode_before_update != MODE_PLAYING &&
            mode_before_update != MODE_PAUSED) {
            audio_restart_music();
        }

        /* Audio buffer maintenance runs once per frame. */
        audio_update();

        game_render_top(&game, &top);

        /* Gameplay bottom LCD redraws at 30 Hz except touch edges. */
        if (game.mode != MODE_PLAYING ||
            (game.frame_counter & 1u) == 0u ||
            in.touch_down || in.touch_up) {
            game_render_bottom(&game, &bottom, &in);
        }

        bool stereo_3d = game_wants_stereo_3d(&game);
        gfxSet3D(stereo_3d);

        if (stereo_3d) {
            float slider = osGet3DSliderState();
            if (slider < 0.0f)
                slider = 0.0f;
            if (slider > 1.0f)
                slider = 1.0f;

            int shift = (int)(slider * 6.0f + 0.5f);
            surface_present_shifted(&top, GFX_TOP, GFX_LEFT, -shift);
            surface_present_shifted(&top, GFX_TOP, GFX_RIGHT, shift);
        } else {
            surface_present(&top, GFX_TOP);
        }

        surface_present(&bottom, GFX_BOTTOM);

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();

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
    surface_destroy(&top);
    surface_destroy(&bottom);
    gfxExit();
    return 0;
}
