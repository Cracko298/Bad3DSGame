static float clampf_local(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float wrap_angle_pi(float radians) {
    const float pi = 3.14159265358979323846f;
    const float tau = pi * 2.0f;

    while (radians > pi)
        radians -= tau;

    while (radians < -pi)
        radians += tau;

    return radians;
}

static void update_player_visual_angle(Game *g, float dt) {
    if (!g)
        return;

    float speed =
        vlen(
            g->player.vel
        );

    
    if (!isfinite(speed) ||
        speed < 7.0f) {
        return;
    }

    float target =
        atan2f(
            g->player.vel.y,
            g->player.vel.x
        );

    float delta =
        wrap_angle_pi(
            target -
            g->player_angle
        );

    float follow =
        clampf_local(
            10.0f * dt,
            0.0f,
            1.0f
        );

    g->player_angle =
        wrap_angle_pi(
            g->player_angle +
            delta * follow
        );
}

static float camera_zoom_for_speed(float speed) {
    speed *= 0.80f;

    float t =
        (speed - CAMERA_ZOOM_SPEED_START) /
        (CAMERA_ZOOM_SPEED_FULL - CAMERA_ZOOM_SPEED_START);

    t = clampf_local(t, 0.0f, 1.0f);

    
    t = t * t * (3.0f - 2.0f * t);

    return CAMERA_ZOOM_MIN +
           (CAMERA_ZOOM_MAX - CAMERA_ZOOM_MIN) * t;
}

static bool setting_enabled(const Game *g, uint8_t flag) {
    return g &&
           (g->progress.settings_flags & flag) != 0;
}

bool game_wants_stereo_3d(const Game *g) {
    return setting_enabled(g, SETTING_STEREO_3D);
}

int game_target_fps(const Game *g) {
    if (!g)
        return 60;

    switch (g->progress.fps_limit) {
        case SETTINGS_FPS_15:  return 15;
        case SETTINGS_FPS_30:  return 30;
        case SETTINGS_FPS_45:  return 45;
        case SETTINGS_FPS_OFF: return 0;
        case SETTINGS_FPS_60:
        default:
            return 60;
    }
}

bool game_bloom_enabled(const Game *g) {
    return g && g->progress.bloom_enabled != 0;
}

int game_bloom_radius(const Game *g) {
    return g ? (int)g->progress.bloom_radius
             : (int)SETTINGS_BLOOM_RADIUS_DEFAULT;
}

int game_bloom_intensity(const Game *g) {
    return g ? (int)g->progress.bloom_intensity
             : (int)SETTINGS_BLOOM_INTENSITY_DEFAULT;
}

int game_bloom_quads(const Game *g) {
    return g ? (int)g->progress.bloom_quads
             : (int)SETTINGS_BLOOM_QUADS_DEFAULT;
}

static void set_setting_flag(Game *g, uint8_t flag, bool enabled) {
    if (!g)
        return;

    if (enabled)
        g->progress.settings_flags |= flag;
    else
        g->progress.settings_flags &= (uint8_t)~flag;
}

static int automatic_lod_level(const Game *g) {
    if (!g)
        return 0;

    if (g->camera_zoom >= RENDER_LOD_FAR_ZOOM)
        return 2;

    if (g->camera_zoom >= RENDER_LOD_MEDIUM_ZOOM)
        return 1;

    return 0;
}

static int effective_lod_level(const Game *g) {
    if (!g)
        return 0;

    switch (g->progress.force_lod) {
        case 1: return 0;
        case 2: return 1;
        case 3: return 2;
        default:
            return automatic_lod_level(g);
    }
}

static float world_to_screen_x(const Game *g, float world_x) {
    return (world_x - g->camera_x) / g->camera_zoom;
}

static float world_to_screen_y(const Game *g, float world_y) {
    return (world_y - g->camera_y) / g->camera_zoom;
}

static float world_size_to_screen(const Game *g, float world_size) {
    return world_size / g->camera_zoom;
}


static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float frand01(void) {
    return (float)rand() / (float)RAND_MAX;
}

static float frand_range(float a, float b) {
    return a + (b - a) * frand01();
}

#define SAVE_FS_PATH "/Bad3DSGame.bin"

#define SAVE_BYTES        128u
#define SAVE_LEGACY_BYTES  64u
#define SAVE_MAGIC          0xB3D54A71u
#define SAVE_VERSION        0x000Eu
#define SAVE_VERSION_V13    0x000Du
#define SAVE_VERSION_V12    0x000Cu
#define SAVE_VERSION_V11    0x000Bu
#define SAVE_VERSION_V10    0x000Au
#define SAVE_VERSION_V9     0x0009u
#define SAVE_VERSION_V8     0x0008u
#define SAVE_VERSION_V7     0x0007u
#define SAVE_VERSION_LEGACY 0x0006u



static void ensure_progress_defaults(Progress *p);
static void add_popup(Game *g, Vec2 pos, int value, bool money, Color c);
static void spawn_particles(Game *g, Vec2 pos, Color c, int count, float speed);

