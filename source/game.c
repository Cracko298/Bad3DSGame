#include "game.h"
#include "audio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Top LCD logical resolution: 400x240. Bottom LCD: 320x240. */
#define TOP_W 400.0f
#define TOP_H 240.0f
#define SECTION_W 400.0f
#define SECTION_H 240.0f
#define STREAM_BLOCK_START 3

/* Player motion tuning: both remain active while grappled. */
#define PLAYER_GRAVITY 132.0f
#define PLAYER_AIR_DRAG 0.115f

/*
   Dynamic speed zoom:
     1.0x = normal view
     4.0x = 1600x960 world units visible on the 400x240 top LCD
*/
#define CAMERA_ZOOM_MIN 1.0f
#define CAMERA_ZOOM_MAX 4.0f
#define CAMERA_ZOOM_STARTING 1.5f
#define CAMERA_ZOOM_SPEED_START 160.0f
#define CAMERA_ZOOM_SPEED_FULL 420.0f
#define CAMERA_ZOOM_OUT_RATE 3.84f
#define CAMERA_ZOOM_IN_RATE 2.2f

/*
   Software-render LOD thresholds. The expensive bloom/rotation path is
   reserved for close views, where the extra pixels are actually visible.
*/
#define RENDER_LOD_MEDIUM_ZOOM 1.60f
#define RENDER_LOD_FAR_ZOOM 2.35f

#define SETTING_SFX          (1u << 0)
#define SETTING_MUSIC        (1u << 1)
#define SETTING_BLOCK_ANIM   (1u << 2)
#define SETTING_LAVA_ANIM    (1u << 3)
#define SETTING_PARTICLES    (1u << 4)
#define SETTING_SCREENSHAKE  (1u << 5)
#define SETTING_STEREO_3D    (1u << 6)

#define SETTINGS_DEFAULT_FLAGS     (SETTING_SFX | SETTING_MUSIC | SETTING_BLOCK_ANIM |      SETTING_LAVA_ANIM | SETTING_PARTICLES | SETTING_SCREENSHAKE)

#define SETTINGS_VALID_MARKER 0xA7u
#define SETTINGS_BLOOM_DEFAULT 3u
#define SETTINGS_BLOOM_MAX 4u
#define SETTINGS_FORCE_LOD_AUTO 0u
#define SETTINGS_FORCE_LOD_MAX 3u
#define SETTINGS_ITEM_COUNT 9

/*
   Adaptive nine-shot spray timer.

   Internally the timer starts with 20 "seconds of work". Each real second
   removes more than one timer-second when the player has a stronger bullet
   level, combo, and horizontal velocity.

   Best possible play is capped at 5x timer speed, so the practical minimum
   interval is about 4 seconds rather than machine-gun fire.
*/
#define BULLET_TIMER_BASE_SECONDS 20.0f
#define BULLET_TIMER_MAX_RATE 5.0f
#define BULLET_SPEED_BONUS_START 80.0f
#define BULLET_SPEED_BONUS_FULL 420.0f
#define BULLET_FIRE_MIN_HORIZONTAL_SPEED 20.0f
#define BULLET_SPRAY_HALF_ANGLE_DEG 22.0f
#define BULLET_SPRAY_COUNT 9
#define LAVA_Y 218.0f

static const Color C_BG       = { 0, 0, 0 };
static const Color C_WHITE    = { 244, 244, 248 };
static const Color C_CYAN     = { 18, 220, 245 };
static const Color C_RED      = { 255, 42, 42 };
static const Color C_PURPLE   = { 228, 66, 236 };
static const Color C_GREEN    = { 55, 232, 88 };
static const Color C_YELLOW   = { 248, 235, 82 };
static const Color C_MONEY    = { 180, 188, 36 };
static const Color C_ORANGE   = { 255, 118, 18 };
static const Color C_DIM      = { 58, 63, 68 };

typedef struct {
    const char *name;
    const char *desc;
    int max_level;
    uint32_t base_cost;
    int cost_scale_percent;
} ShopDef;

static const ShopDef SHOP[SHOP_ITEM_COUNT] = {
    { "+HP",         "MORE HEARTS",          10,  1000, 145 },
    { "BETTER ROPE", "STRONGER MOMENTUM BOOST",   15,   850, 145 },
    { "+COMBO",      "HIGHER MAX COMBO",     10,  1250, 150 },
    { "BULLETS",     "AUTO SPRAY: LEVEL+SKILL+SPEED",   9, 1600, 150 },
    { "GREEN BOI",   "GREEN CUBES EXPLODE",   1,  7500, 100 },
    { "MONEY BOI",   "MONEY CUBES PAY MORE",  1, 10000, 100 },
    { "THICC",       "BIGGER AND BREAKS RED", 1, 15000, 100 },
    { "LASER ROPE",  "ROPE DESTROYS CUBES",   1, 22000, 100 }
};

typedef enum {
    COS_PLAYER_COLOR = 0,
    COS_ROPE_COLOR,
    COS_ROPE_ANIM,
    COS_SHAPE,
    COS_PLAYER_ANIM,
    COS_HAT,
    COS_BLOCK_THEME,
    COS_BACKGROUND,
    COS_LAVA_COLOR,
    COS_LAVA_ANIM,
    COS_TITLE_THEME,
    COS_UI_THEME
} CosmeticKind;

typedef struct {
    const char *name;
    const char *desc;
    const char *bonus;
    CosmeticKind kind;
    uint8_t style;
    uint32_t cost;
} CosmeticDef;

/* Cosmetic IDs 0..11 match the v8/v9 ownership layout. */
static const CosmeticDef COSMETICS[COSMETIC_COUNT] = {
    { "CYAN",      "CLASSIC CYAN PLAYER",      "",                         COS_PLAYER_COLOR, 0,     0 },
    { "PINK",      "HOT PINK PLAYER",          "",                         COS_PLAYER_COLOR, 1,  1400 },
    { "LIME",      "LIME PLAYER",              "",                         COS_PLAYER_COLOR, 2,  2100 },
    { "GOLD",      "GOLD PLAYER",              "",                         COS_PLAYER_COLOR, 3,  3600 },

    { "WHITE",     "CLASSIC WHITE ROPE",       "",                         COS_ROPE_COLOR,   0,     0 },
    { "CYAN",      "CYAN ROPE",                "",                         COS_ROPE_COLOR,   1,  1100 },
    { "PURPLE",    "PURPLE ROPE",              "",                         COS_ROPE_COLOR,   2,  1900 },
    { "GOLD",      "GOLD ROPE",                "",                         COS_ROPE_COLOR,   3,  3200 },

    { "SOLID",     "SOLID ROPE FX",            "",                         COS_ROPE_ANIM,    0,     0 },
    { "DASH",      "DASHED ROPE",              "",                         COS_ROPE_ANIM,    1,  2400 },
    { "DUAL",      "DUAL-COLOR ROPE",          "",                         COS_ROPE_ANIM,    2,  3900 },
    { "PULSE",     "PULSING NEON ROPE",        "",                         COS_ROPE_ANIM,    3,  5600 },

    { "ICE",       "PALE ICE PLAYER",           "",                         COS_PLAYER_COLOR, 4,  4800 },
    { "ORANGE",    "HOT ORANGE PLAYER",         "",                         COS_PLAYER_COLOR, 5,  6200 },

    { "RED",       "RED ROPE",                 "",                         COS_ROPE_COLOR,   4,  4400 },
    { "LIME",      "LIME ROPE",                "",                         COS_ROPE_COLOR,   5,  5200 },

    { "WAVE",      "ROPE WAVES AS YOU SWING",  "",                         COS_ROPE_ANIM,    4,  6800 },
    { "SPARK",     "ROPE SPARKLE SEGMENTS",    "",                         COS_ROPE_ANIM,    5,  8200 },

    { "CORE",      "CLASSIC GLIDE BODY",        "NO BONUS",                 COS_SHAPE,        0,     0 },
    { "DIAMOND",   "ANGLED SPEED BODY",         "+8% GRAPPLE LAUNCH",       COS_SHAPE,        1,  9000 },
    { "ORB",       "ROUND ORBIT BODY",           "+8% ROPE REACH",           COS_SHAPE,        2, 11000 },
    { "ARROW",     "AERODYNAMIC ARROW",          "-10% AIR DRAG",              COS_SHAPE,        3, 14000 },
    { "COMET",     "PREMIUM COMET CORE",         "+3% ROPE / -3% AIR DRAG",   COS_SHAPE,        4, 22000 },

    { "STEADY",    "CLASSIC PLAYER MOTION",      "",                         COS_PLAYER_ANIM,  0,     0 },
    { "BREATHE",   "SOFT SIZE PULSE",            "",                         COS_PLAYER_ANIM,  1,  3200 },
    { "TWIST",     "EXTRA MID-FLIGHT TWIST",     "",                         COS_PLAYER_ANIM,  2,  4600 },
    { "JELLY",     "BOUNCY SQUASH WOBBLE",       "",                         COS_PLAYER_ANIM,  3,  6100 },
    { "GHOST",     "FLICKERING GHOST MOTION",    "",                         COS_PLAYER_ANIM,  4,  7800 },

    { "NONE",      "NO HAT",                    "",                         COS_HAT,          0,     0 },
    { "TOPHAT",    "A VERY SERIOUS HAT",         "",                         COS_HAT,          1,  2500 },
    { "CROWN",     "TINY WRECKING CROWN",        "",                         COS_HAT,          2,  5200 },
    { "HALO",      "FLOATING HALO",              "",                         COS_HAT,          3,  7200 },
    { "ANTENNA",   "BENDY SPACE ANTENNA",        "",                         COS_HAT,          4,  8800 },

    { "CLASSIC",   "ORIGINAL BLOCK COLORS",      "",                         COS_BLOCK_THEME,  0,     0 },
    { "ICE",       "COOL BLUE BLOCK SET",        "",                         COS_BLOCK_THEME,  1,  4200 },
    { "SUNSET",    "WARM ORANGE/PINK SET",       "",                         COS_BLOCK_THEME,  2,  5400 },
    { "MONO",      "WHITE/GREY BLOCK SET",       "",                         COS_BLOCK_THEME,  3,  6600 },
    { "ARCADE",    "LOUD ARCADE BLOCK SET",      "",                         COS_BLOCK_THEME,  4,  8400 },

    { "VOID",      "CLASSIC BLACK SPACE",        "",                         COS_BACKGROUND,   0,     0 },
    { "MIDNIGHT",  "DEEP BLUE NIGHT",            "",                         COS_BACKGROUND,   1,  3500 },
    { "NEBULA",    "PURPLE NEBULA",              "",                         COS_BACKGROUND,   2,  5200 },
    { "MATRIX",    "DARK GREEN GRID",            "",                         COS_BACKGROUND,   3,  6400 },
    { "DUSK",      "WARM DUSK SKY",              "",                         COS_BACKGROUND,   4,  7800 },

    { "FIRE",      "CLASSIC ORANGE LAVA",        "",                         COS_LAVA_COLOR,   0,     0 },
    { "CYAN",      "CYAN PLASMA LAVA",           "",                         COS_LAVA_COLOR,   1,  4200 },
    { "PURPLE",    "PURPLE VOID LAVA",           "",                         COS_LAVA_COLOR,   2,  5200 },
    { "TOXIC",     "TOXIC GREEN LAVA",           "",                         COS_LAVA_COLOR,   3,  6200 },
    { "WHITEHOT",  "WHITE-HOT LAVA",             "",                         COS_LAVA_COLOR,   4,  7600 },
    { "PINK",      "PINK NEON LAVA",             "",                         COS_LAVA_COLOR,   5,  8800 },

    { "CALM",      "CLASSIC SMALL RIPPLE",       "",                         COS_LAVA_ANIM,    0,     0 },
    { "RIPPLE",    "LONG SOFT RIPPLE",           "",                         COS_LAVA_ANIM,    1,  2800 },
    { "WAVE",      "LARGE MOVING WAVE",          "",                         COS_LAVA_ANIM,    2,  4400 },
    { "SPARK",     "SPARKING SURFACE",           "",                         COS_LAVA_ANIM,    3,  6100 },
    { "CHAOS",     "FAST CHAOTIC SURFACE",       "",                         COS_LAVA_ANIM,    4,  7900 },

    /* Cosmetic IDs 54..63 retain their v2.11.1 meanings. */
    { "CAP",       "LOW PROFILE STREET CAP",      "",                         COS_HAT,          5,  3600 },
    { "WITCH",     "WITCH HAT WITH A CROOKED TIP","",                         COS_HAT,          6,  6200 },
    { "HORNS",     "TWIN NEON HORNS",             "",                         COS_HAT,          7,  7600 },
    { "CAT EARS",  "TINY TRIANGLE EARS",          "",                         COS_HAT,          8,  5400 },
    { "PARTY",     "PARTY CONE + POM",            "",                         COS_HAT,          9,  4300 },
    { "COWBOY",    "WIDE BRIM SPACE COWBOY",      "",                         COS_HAT,         10,  8900 },
    { "PROPELLER", "SPINNY PROPELLER BEANIE",     "",                         COS_HAT,         11, 10800 },
    { "UFO",       "A TINY SAUCER ORBITS YOU",    "",                         COS_HAT,         12, 14500 },

    { "CITY",      "NEON NIGHT SKYLINE",          "",                         COS_BACKGROUND,   5, 10500 },
    { "STORM",     "CLOUDS RAIN + LIGHTNING",     "",                         COS_BACKGROUND,   6, 12800 },

    /* IDs 64..90. */
    { "VIOLET",    "DEEP VIOLET PLAYER",          "",                         COS_PLAYER_COLOR, 6,  8200 },
    { "WHITEHOT",  "WHITE-HOT PLAYER CORE",       "+$1 NORMAL KILL",           COS_PLAYER_COLOR, 7, 16000 },

    { "ORANGE",    "ORANGE ROPE",                 "",                         COS_ROPE_COLOR,   6,  7200 },
    { "PINK",      "PINK ROPE",                   "",                         COS_ROPE_COLOR,   7,  8800 },

    { "ELECTRIC",  "HIGH VOLTAGE ROPE FX",        "12 SHOT SPRAY",            COS_ROPE_ANIM,    6, 18000 },
    { "RAINBOW",   "COLOR-CYCLING ROPE FX",       "LARGER BULLETS",           COS_ROPE_ANIM,    7, 22000 },

    { "STAR",      "FIVE-POINT SPEED STAR",       "+7% GRAPPLE LAUNCH",       COS_SHAPE,        5, 26000 },
    { "HEX",       "HEAVY HEX CORE",              "-7% AIR DRAG",             COS_SHAPE,        6, 30000 },

    { "SPIN",      "CONTINUOUS AIR SPIN",         "",                         COS_PLAYER_ANIM,  5,  9500 },
    { "GLITCH",    "DIGITAL JITTER TRAIL",        "",                         COS_PLAYER_ANIM,  6, 12000 },

    { "BEANIE",    "SOFT LITTLE BEANIE",          "",                         COS_HAT,         13,  5200 },
    { "HEADPHONES","OVERSIZED NEON HEADPHONES",   "+8% BULLET TIMER",         COS_HAT,         14, 16000 },

    { "NIGHT",     "DARK BLUE BLOCK PALETTE",     "",                         COS_BLOCK_THEME,  5, 10500 },
    { "CANDY",     "CANDY NEON BLOCK PALETTE",    "+$1 NORMAL KILL",           COS_BLOCK_THEME,  6, 18000 },

    { "AURORA",    "MOVING AURORA SKY",           "",                         COS_BACKGROUND,   7, 14000 },
    { "CLOUD9",    "BRIGHT SKY + CLOUD LAYERS",   "",                         COS_BACKGROUND,   8, 15000 },

    { "BLUEFIRE",  "DEEP BLUE LAVA",              "",                         COS_LAVA_COLOR,   6, 11000 },
    { "RAINBOW",   "COLOR-SHIFTING LAVA",         "+5% BLOCK XP",             COS_LAVA_COLOR,   7, 17000 },

    { "BUBBLES",   "LARGE RISING LAVA BUBBLES",   "",                         COS_LAVA_ANIM,    5,  9500 },
    { "GEYSER",    "PERIODIC MAGMA GEYSERS",      "",                         COS_LAVA_ANIM,    6, 13500 },

    { "CITY",      "TITLE: NIGHT CITY",           "",                         COS_TITLE_THEME,  0,     0 },
    { "STORM",     "TITLE: STORM FRONT",          "",                         COS_TITLE_THEME,  1,  6000 },
    { "GAMEPLAY",  "TITLE: FAKE GAMEPLAY SCENE",  "",                         COS_TITLE_THEME,  2,  9000 },
    { "LAVA",      "TITLE: MAGMA + BUBBLES",      "",                         COS_TITLE_THEME,  3, 11000 },
    { "VOID",      "TITLE: CLEAN STARFIELD",      "",                         COS_TITLE_THEME,  4,  4500 },
    { "MATRIX",    "TITLE: GREEN GRID",           "",                         COS_TITLE_THEME,  5,  7500 },
    { "ARCADE",    "TITLE: LOUD NEON ARCADE",     "",                         COS_TITLE_THEME,  6, 13000 },

    { "MINT",      "PASTEL MINT PLAYER",          "",                         COS_PLAYER_COLOR, 8, 11800 },
    { "CHOCO",     "CHOCOLATE ROPE",              "",                         COS_ROPE_COLOR,   8,  9800 },
    { "BANANA",    "FULL BANANA SHAPE",           "+6% CATCH WINDOW",        COS_SHAPE,        7, 20500 },
    { "MILK",      "MILK CARTON HAT",             "",                         COS_HAT,         15,  9400 },
    { "TVHEAD",    "TV HEAD LOOPING SCREEN",      "+4% COMBO TIME",          COS_SHAPE,        8, 24800 },
    { "CYCLE",     "COLOR CYCLE PLAYER MOTION",   "",                         COS_PLAYER_ANIM,  7, 13600 },
    { "BOUNCE",    "SPRINGY BOUNCE MOTION",       "",                         COS_PLAYER_ANIM,  8, 14800 },

    { "UI:CLASSIC","UI: CYAN PANELS",             "",                         COS_UI_THEME,     0,     0 },
    { "UI:OCEAN",  "UI: OCEAN BLUE PANELS",       "",                         COS_UI_THEME,     1,  5800 },
    { "UI:SUNSET", "UI: SUNSET ORANGE PANELS",    "",                         COS_UI_THEME,     2,  7600 },
    { "UI:VOID",   "UI: VOID PURPLE PANELS",      "",                         COS_UI_THEME,     3,  9800 }
};

#define SHOP_PAGE_COUNT 13

typedef struct {
    const char *name;
    const uint8_t *items;
    uint8_t count;
} CosmeticPage;

static const uint8_t PAGE_PLAYER_COLORS[] = { 0, 1, 2, 3, 12, 13, 64, 65, 91 };
static const uint8_t PAGE_ROPE_COLORS[]   = { 4, 5, 6, 7, 14, 15, 66, 67, 92 };
static const uint8_t PAGE_ROPE_ANIMS[]    = { 8, 9, 10, 11, 16, 17, 68, 69 };
static const uint8_t PAGE_SHAPES[]        = { 18, 19, 20, 21, 22, 70, 71, 93, 95 };
static const uint8_t PAGE_PLAYER_ANIMS[]  = { 23, 24, 25, 26, 27, 72, 73, 96, 97 };
static const uint8_t PAGE_HATS[]          = { 28, 29, 30, 31, 32, 54, 55, 56, 57, 58, 59, 60, 61, 74, 75, 94 };
static const uint8_t PAGE_BLOCKS[]        = { 33, 34, 35, 36, 37, 76, 77 };
static const uint8_t PAGE_BACKGROUNDS[]   = { 38, 39, 40, 41, 42, 62, 63, 78, 79 };
static const uint8_t PAGE_LAVA_COLORS[]   = { 43, 44, 45, 46, 47, 48, 80, 81 };
static const uint8_t PAGE_LAVA_ANIMS[]    = { 49, 50, 51, 52, 53, 82, 83 };
static const uint8_t PAGE_TITLE_THEMES[]   = { 84, 85, 86, 87, 88, 89, 90 };
static const uint8_t PAGE_UI_THEMES[]      = { 98, 99, 100, 101 };

static const CosmeticPage COSMETIC_PAGES[SHOP_PAGE_COUNT - 1] = {
    { "PLAYER COLORS", PAGE_PLAYER_COLORS, 9 },
    { "ROPE COLORS",   PAGE_ROPE_COLORS,   9 },
    { "ROPE FX",       PAGE_ROPE_ANIMS,    8 },
    { "SHAPES",        PAGE_SHAPES,        9 },
    { "PLAYER ANIMS",  PAGE_PLAYER_ANIMS,  9 },
    { "HATS",          PAGE_HATS,         16 },
    { "BLOCK COLORS",  PAGE_BLOCKS,        7 },
    { "BACKGROUNDS",   PAGE_BACKGROUNDS,   9 },
    { "LAVA COLORS",   PAGE_LAVA_COLORS,   8 },
    { "LAVA ANIMS",    PAGE_LAVA_ANIMS,    7 },
    { "TITLE THEMES",  PAGE_TITLE_THEMES,   7 },
    { "UI THEMES",    PAGE_UI_THEMES,      4 }
};

static const Color PLAYER_COLORS[9] = {
    { 18, 220, 245 },
    { 255, 72, 184 },
    { 80, 245, 82 },
    { 250, 211, 58 },
    { 170, 230, 255 },
    { 255, 126, 28 },
    { 154, 72, 245 },
    { 250, 250, 255 },
    { 132, 255, 212 }
};

static const Color ROPE_COLORS[9] = {
    { 244, 244, 248 },
    { 18, 220, 245 },
    { 228, 66, 236 },
    { 250, 211, 58 },
    { 255, 66, 66 },
    { 80, 245, 82 },
    { 255, 126, 28 },
    { 255, 72, 184 },
    { 138, 92, 52 }
};

typedef enum {
    GOAL_DISTANCE = 0,
    GOAL_BLOCKS,
    GOAL_LEVEL,
    GOAL_ROPE,
    GOAL_SPEED,
    GOAL_COMBO,
    GOAL_COSMETICS,
    GOAL_SCORE
} GoalKind;

typedef struct {
    const char *name;
    const char *desc;
    GoalKind kind;
    uint32_t target;
} AchievementDef;

#define ACHIEVEMENT_COUNT 10
#define MISSION_KIND_COUNT 6

static const AchievementDef ACHIEVEMENTS[ACHIEVEMENT_COUNT] = {
    { "FIRST FLIGHT",  "REACH 250 DIST",         GOAL_DISTANCE,   250 },
    { "SKYBOUND",      "REACH 1200 DIST",        GOAL_DISTANCE,  1200 },
    { "DEEP SPACE",    "REACH 3000 DIST",        GOAL_DISTANCE,  3000 },
    { "WRECKER",       "BREAK 50 BLOCKS",        GOAL_BLOCKS,      50 },
    { "DEMOLITION",    "BREAK 500 BLOCKS",       GOAL_BLOCKS,     500 },
    { "LEVEL TEN",     "REACH LEVEL 10",         GOAL_LEVEL,       10 },
    { "ROPE MASTER",   "ROPE UPGRADE 10",        GOAL_ROPE,        10 },
    { "TERMINAL SPEED","REACH SPEED 350",        GOAL_SPEED,      350 },
    { "COMBO KING",    "REACH COMBO X5",         GOAL_COMBO,        5 },
    { "COLLECTOR",     "OWN 15 COSMETICS",       GOAL_COSMETICS,   15 }
};

static const GoalKind MISSION_KIND_ROTATION[MISSION_KIND_COUNT] = {
    GOAL_BLOCKS,
    GOAL_DISTANCE,
    GOAL_SPEED,
    GOAL_SCORE,
    GOAL_COMBO,
    GOAL_LEVEL
};

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

    /* Orientation is unchanged below 7 world units/second. */
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

    /* Smoothstep maps speed to camera zoom. */
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
#define SAVE_VERSION        0x000Du
#define SAVE_VERSION_V12    0x000Cu
#define SAVE_VERSION_V11    0x000Bu
#define SAVE_VERSION_V10    0x000Au
#define SAVE_VERSION_V9     0x0009u
#define SAVE_VERSION_V8     0x0008u
#define SAVE_VERSION_V7     0x0007u
#define SAVE_VERSION_LEGACY 0x0006u


/* Forward declarations used by progression/save helpers. */
static void ensure_progress_defaults(Progress *p);
static void add_popup(Game *g, Vec2 pos, int value, bool money, Color c);
static void spawn_particles(Game *g, Vec2 pos, Color c, int count, float speed);

static uint32_t rol32(uint32_t v, unsigned n) {
    n &= 31u;
    return (v << n) | (v >> ((32u - n) & 31u));
}

static uint32_t ror32(uint32_t v, unsigned n) {
    n &= 31u;
    return (v >> n) | (v << ((32u - n) & 31u));
}

static uint64_t rol64(uint64_t v, unsigned n) {
    n &= 63u;
    return (v << n) | (v >> ((64u - n) & 63u));
}

static uint64_t ror64(uint64_t v, unsigned n) {
    n &= 63u;
    return (v >> n) | (v << ((64u - n) & 63u));
}

static uint8_t rol8(uint8_t v, unsigned n) {
    n &= 7u;
    return (uint8_t)((v << n) | (v >> ((8u - n) & 7u)));
}

static uint8_t ror8(uint8_t v, unsigned n) {
    n &= 7u;
    return (uint8_t)((v >> n) | (v << ((8u - n) & 7u)));
}

/* ASCII ROT13 is its own inverse. On arbitrary binary it only changes bytes
   that happen to fall in the A-Z/a-z ranges. */
static uint8_t rot13_byte(uint8_t b) {
    if (b >= (uint8_t)'A' && b <= (uint8_t)'Z')
        return (uint8_t)('A' + ((b - (uint8_t)'A' + 13u) % 26u));
    if (b >= (uint8_t)'a' && b <= (uint8_t)'z')
        return (uint8_t)('a' + ((b - (uint8_t)'a' + 13u) % 26u));
    return b;
}

static void put_u16le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 0);
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 0);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_u64le(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; ++i)
        p[i] = (uint8_t)(v >> (i * 8));
}

static uint16_t get_u16le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32le(const uint8_t *p) {
    return ((uint32_t)p[0] << 0) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t get_u64le(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= (uint64_t)p[i] << (i * 8);
    return v;
}

static uint64_t interleave_u32(uint32_t a, uint32_t b) {
    uint64_t v = 0;
    for (unsigned i = 0; i < 32; ++i) {
        v |= ((uint64_t)((a >> i) & 1u)) << (i * 2u);
        v |= ((uint64_t)((b >> i) & 1u)) << (i * 2u + 1u);
    }
    return v;
}

static uint32_t deinterleave_even(uint64_t v) {
    uint32_t out = 0;
    for (unsigned i = 0; i < 32; ++i)
        out |= (uint32_t)((v >> (i * 2u)) & 1u) << i;
    return out;
}

static uint32_t deinterleave_odd(uint64_t v) {
    uint32_t out = 0;
    for (unsigned i = 0; i < 32; ++i)
        out |= (uint32_t)((v >> (i * 2u + 1u)) & 1u) << i;
    return out;
}

static uint32_t pack_levels(const Progress *p) {
    uint32_t packed = 0;
    for (int i = 0; i < SHOP_ITEM_COUNT; ++i)
        packed |= ((uint32_t)p->levels[i] & 0xFu) << (i * 4);
    return packed;
}

static void unpack_levels(Progress *p, uint32_t packed) {
    for (int i = 0; i < SHOP_ITEM_COUNT; ++i) {
        uint8_t level = (uint8_t)((packed >> (i * 4)) & 0xFu);
        if (level > (uint8_t)SHOP[i].max_level)
            level = (uint8_t)SHOP[i].max_level;
        p->levels[i] = level;
    }
}

static uint32_t pack_settings(const Progress *p) {
    return ((uint32_t)p->settings_flags << 0) |
           ((uint32_t)p->bloom_level << 8) |
           ((uint32_t)p->force_lod << 16) |
           ((uint32_t)p->settings_reserved << 24);
}

static void unpack_settings(Progress *p, uint32_t packed) {
    p->settings_flags =
        (uint8_t)((packed >> 0) & 0xFFu);

    p->bloom_level =
        (uint8_t)((packed >> 8) & 0xFFu);

    p->force_lod =
        (uint8_t)((packed >> 16) & 0xFFu);

    p->settings_reserved =
        (uint8_t)((packed >> 24) & 0xFFu);
}


static uint32_t checksum_u32(uint32_t h, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        h ^= (uint8_t)(v >> (i * 8));
        h *= 16777619u;
    }
    return h;
}

static uint32_t progress_checksum_version(const Progress *p,
                                          uint32_t salt,
                                          uint16_t version) {
    uint32_t h = 2166136261u ^ salt ^ 0x91E10DA5u;

    h = checksum_u32(h, p->money);
    h = checksum_u32(h, p->high_score);

    if (version >= SAVE_VERSION_V7)
        h = checksum_u32(h, p->best_distance);

    if (version >= SAVE_VERSION_V8) {
        h = checksum_u32(h, p->xp);
        h = checksum_u32(h, p->player_level);

        /* v8/v9 ownership uses the low 32 bits. */
        h = checksum_u32(
            h,
            (uint32_t)(
                p->cosmetic_owned &
                0xFFFFFFFFull
            )
        );

        h = checksum_u32(h, p->lifetime_destroyed);

        uint32_t equipped =
            ((uint32_t)p->player_style << 0) |
            ((uint32_t)p->rope_style << 8) |
            ((uint32_t)p->pattern_style << 16);

        h = checksum_u32(h, equipped);
    }

    if (version >= SAVE_VERSION_V9)
        h = checksum_u32(h, pack_settings(p));

    if (version >= SAVE_VERSION_V10) {
        h = checksum_u32(
            h,
            (uint32_t)(
                p->cosmetic_owned >>
                32
            )
        );

        h = checksum_u32(h, p->best_speed);
        h = checksum_u32(h, p->best_combo);
        h = checksum_u32(h, p->missions_claimed);

        uint32_t equip2 =
            ((uint32_t)p->shape_style << 0) |
            ((uint32_t)p->player_anim_style << 8) |
            ((uint32_t)p->hat_style << 16) |
            ((uint32_t)p->block_theme << 24);

        uint32_t equip3 =
            ((uint32_t)p->background_style << 0) |
            ((uint32_t)p->lava_color_style << 8) |
            ((uint32_t)p->lava_anim_style << 16) |
            ((uint32_t)p->reserved_style1 << 24);

        h = checksum_u32(h, equip2);
        h = checksum_u32(h, equip3);
    }

    if (version >= SAVE_VERSION_V11) {
        h = checksum_u32(
            h,
            (uint32_t)(p->cosmetic_owned2 & 0xFFFFFFFFull)
        );

        h = checksum_u32(
            h,
            (uint32_t)(p->cosmetic_owned2 >> 32)
        );

        h = checksum_u32(
            h,
            (uint32_t)p->title_style
        );
    }

    if (version >= SAVE_VERSION_V12)
        h = checksum_u32(h, p->total_distance_traveled);

    for (int i = 0; i < SHOP_ITEM_COUNT; ++i) {
        h ^= p->levels[i];
        h *= 16777619u;
    }

    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

static uint32_t progress_checksum(const Progress *p, uint32_t salt) {
    return progress_checksum_version(p, salt, SAVE_VERSION);
}

static uint32_t save_stream_step(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void obfuscate_blob(uint8_t blob[SAVE_BYTES], unsigned bytes) {
    uint32_t stream = 0xC13FA9A9u;

    for (unsigned i = 0; i < bytes; ++i) {
        uint8_t key = (uint8_t)(save_stream_step(&stream) >> 24);
        uint8_t b = (uint8_t)(blob[i] ^ key ^ (uint8_t)(i * 37u + 0x5Bu));
        b = rol8(b, (i % 7u) + 1u);
        blob[i] = rot13_byte(b);
    }
}

static void deobfuscate_blob(uint8_t blob[SAVE_BYTES], unsigned bytes) {
    uint32_t stream = 0xC13FA9A9u;

    for (unsigned i = 0; i < bytes; ++i) {
        uint8_t key = (uint8_t)(save_stream_step(&stream) >> 24);
        uint8_t b = rot13_byte(blob[i]);
        b = ror8(b, (i % 7u) + 1u);
        blob[i] = (uint8_t)(b ^ key ^ (uint8_t)(i * 37u + 0x5Bu));
    }
}

static void encode_progress_blob(const Progress *p, uint8_t out[SAVE_BYTES]) {
    memset(out, 0, SAVE_BYTES);

    uint32_t owned_low =
        (uint32_t)(
            p->cosmetic_owned &
            0xFFFFFFFFull
        );

    uint32_t owned_high =
        (uint32_t)(
            p->cosmetic_owned >>
            32
        );

    uint32_t salt = (uint32_t)osGetTime();
    salt ^= p->money * 0x9E3779B9u;
    salt ^= rol32(p->high_score, 11);
    salt ^= rol32(p->best_distance, 19);
    salt ^= rol32(p->xp, 7);
    salt ^= rol32(p->player_level, 23);
    salt ^= owned_low * 0x85EBCA6Bu;
    salt ^= rol32(owned_high, 13);
    salt ^= rol32(p->missions_claimed, 5);
    salt ^= (uint32_t)(p->cosmetic_owned2 & 0xFFFFFFFFull) * 0xC2B2AE35u;
    salt ^= rol32((uint32_t)(p->cosmetic_owned2 >> 32), 9);
    salt ^= (uint32_t)p->title_style * 0x27D4EB2Fu;
    salt ^= rol32(p->total_distance_traveled, 3);
    salt ^= 0xA5D3C771u;

    if (salt == 0)
        salt = 0x6C8E9CF5u;

    uint64_t salt64 =
        ((uint64_t)salt << 32) |
        (uint64_t)(salt ^ 0x6D2B79F5u);

    uint64_t mixed =
        interleave_u32(
            p->high_score,
            p->money
        );

    mixed =
        rol64(
            mixed ^
            0xD6E8FEB86659FD93ULL ^
            salt64,
            13
        );

    uint32_t levels =
        rol32(
            pack_levels(p) ^
            salt ^
            0x51ED270Bu,
            13
        );

    uint32_t distance =
        rol32(
            p->best_distance ^
            salt ^
            0xC2B2AE35u,
            17
        );

    uint32_t equipped =
        ((uint32_t)p->player_style << 0) |
        ((uint32_t)p->rope_style << 8) |
        ((uint32_t)p->pattern_style << 16);

    uint32_t equip2 =
        ((uint32_t)p->shape_style << 0) |
        ((uint32_t)p->player_anim_style << 8) |
        ((uint32_t)p->hat_style << 16) |
        ((uint32_t)p->block_theme << 24);

    uint32_t equip3 =
        ((uint32_t)p->background_style << 0) |
        ((uint32_t)p->lava_color_style << 8) |
        ((uint32_t)p->lava_anim_style << 16);

    uint32_t xp_enc =
        rol32(
            p->xp ^
            salt ^
            0x27D4EB2Fu,
            9
        );

    uint32_t level_enc =
        rol32(
            p->player_level ^
            salt ^
            0x165667B1u,
            21
        );

    uint32_t owned_low_enc =
        rol32(
            owned_low ^
            salt ^
            0xD3A2646Cu,
            11
        );

    uint32_t equip_enc =
        rol32(
            equipped ^
            salt ^
            0x9E3779B9u,
            5
        );

    uint32_t blocks_enc =
        rol32(
            p->lifetime_destroyed ^
            salt ^
            0x7F4A7C15u,
            15
        );

    uint32_t settings_enc =
        rol32(
            pack_settings(p) ^
            salt ^
            0x4CF5AD43u,
            3
        );

    uint32_t owned_high_enc =
        rol32(
            owned_high ^
            salt ^
            0xB7E15163u,
            27
        );

    uint32_t speed_enc =
        rol32(
            p->best_speed ^
            salt ^
            0x1BF5A17Du,
            7
        );

    uint32_t combo_enc =
        rol32(
            p->best_combo ^
            salt ^
            0x94D049BBu,
            19
        );

    uint32_t missions_enc =
        rol32(
            p->missions_claimed ^
            salt ^
            0xDEADBEEFu,
            9
        );

    uint32_t equip2_enc =
        rol32(
            equip2 ^
            salt ^
            0x6A09E667u,
            11
        );

    uint32_t equip3_enc =
        rol32(
            equip3 ^
            salt ^
            0xBB67AE85u,
            17
        );

    uint32_t owned2_low =
        (uint32_t)(p->cosmetic_owned2 & 0xFFFFFFFFull);

    uint32_t owned2_high =
        (uint32_t)(p->cosmetic_owned2 >> 32);

    uint32_t owned2_low_enc =
        rol32(
            owned2_low ^
            salt ^
            0x3C6EF372u,
            13
        );

    uint32_t owned2_high_enc =
        rol32(
            owned2_high ^
            salt ^
            0xA54FF53Au,
            23
        );

    uint32_t title_enc =
        rol32(
            (uint32_t)p->title_style ^
            salt ^
            0x510E527Fu,
            3
        );

    uint32_t total_distance_enc =
        rol32(
            p->total_distance_traveled ^
            salt ^
            0x1F83D9ABu,
            29
        );

    uint32_t check =
        progress_checksum(
            p,
            salt
        );

    check =
        rol32(
            check ^
            salt ^
            0xB5297A4Du,
            7
        );

    put_u32le(out + 0, SAVE_MAGIC);
    put_u16le(out + 4, SAVE_VERSION);
    put_u16le(out + 6, (uint16_t)SAVE_BYTES);
    put_u32le(out + 8, salt);
    put_u64le(out + 12, mixed);
    put_u32le(out + 20, levels);
    put_u32le(out + 24, check);
    put_u32le(out + 28, distance);
    put_u32le(out + 32, xp_enc);
    put_u32le(out + 36, level_enc);
    put_u32le(out + 40, owned_low_enc);
    put_u32le(out + 44, equip_enc);
    put_u32le(out + 48, blocks_enc);
    put_u32le(out + 52, settings_enc);

    put_u32le(out + 56, owned_high_enc);
    put_u32le(out + 60, speed_enc);
    put_u32le(out + 64, combo_enc);
    put_u32le(out + 68, missions_enc);
    put_u32le(out + 72, equip2_enc);
    put_u32le(out + 76, equip3_enc);

    put_u32le(out + 80, owned2_low_enc);
    put_u32le(out + 84, owned2_high_enc);
    put_u32le(out + 88, title_enc);
    put_u32le(out + 92, total_distance_enc);

    uint32_t filler =
        salt ^
        0x243F6A88u;

    for (unsigned i = 96;
         i < SAVE_BYTES;
         ++i) {
        out[i] =
            (uint8_t)(
                save_stream_step(
                    &filler
                ) >>
                24
            );
    }

    obfuscate_blob(
        out,
        SAVE_BYTES
    );
}

static bool decode_progress_blob(Progress *p,
                                 const uint8_t input[SAVE_BYTES],
                                 unsigned file_bytes) {
    if (!p ||
        !input ||
        (file_bytes != SAVE_BYTES &&
         file_bytes != SAVE_LEGACY_BYTES)) {
        return false;
    }

    uint8_t blob[SAVE_BYTES];
    memset(blob, 0, sizeof(blob));
    memcpy(blob, input, file_bytes);

    deobfuscate_blob(
        blob,
        file_bytes
    );

    if (get_u32le(blob + 0) != SAVE_MAGIC)
        return false;

    uint16_t version =
        get_u16le(
            blob + 4
        );

    if (version != SAVE_VERSION &&
        version != SAVE_VERSION_V12 &&
        version != SAVE_VERSION_V11 &&
        version != SAVE_VERSION_V10 &&
        version != SAVE_VERSION_V9 &&
        version != SAVE_VERSION_V8 &&
        version != SAVE_VERSION_V7 &&
        version != SAVE_VERSION_LEGACY) {
        return false;
    }

    uint16_t stored_bytes =
        get_u16le(
            blob + 6
        );

    unsigned expected_bytes =
        version >= SAVE_VERSION_V10
        ? SAVE_BYTES
        : SAVE_LEGACY_BYTES;

    if (stored_bytes != expected_bytes ||
        file_bytes != expected_bytes) {
        return false;
    }

    uint32_t salt =
        get_u32le(
            blob + 8
        );

    uint64_t salt64 =
        ((uint64_t)salt << 32) |
        (uint64_t)(salt ^ 0x6D2B79F5u);

    uint64_t mixed =
        get_u64le(
            blob + 12
        );

    mixed =
        ror64(
            mixed,
            13
        ) ^
        0xD6E8FEB86659FD93ULL ^
        salt64;

    Progress decoded;
    memset(
        &decoded,
        0,
        sizeof(decoded)
    );

    decoded.high_score =
        deinterleave_even(
            mixed
        );

    decoded.money =
        deinterleave_odd(
            mixed
        );

    uint32_t levels =
        get_u32le(
            blob + 20
        );

    levels =
        ror32(
            levels,
            13
        ) ^
        salt ^
        0x51ED270Bu;

    unpack_levels(
        &decoded,
        levels
    );

    if (version >= SAVE_VERSION_V7) {
        uint32_t distance =
            get_u32le(
                blob + 28
            );

        decoded.best_distance =
            ror32(
                distance,
                17
            ) ^
            salt ^
            0xC2B2AE35u;
    }

    if (version >= SAVE_VERSION_V8) {
        decoded.xp =
            ror32(
                get_u32le(blob + 32),
                9
            ) ^
            salt ^
            0x27D4EB2Fu;

        decoded.player_level =
            ror32(
                get_u32le(blob + 36),
                21
            ) ^
            salt ^
            0x165667B1u;

        uint32_t owned_low =
            ror32(
                get_u32le(blob + 40),
                11
            ) ^
            salt ^
            0xD3A2646Cu;

        decoded.cosmetic_owned =
            (uint64_t)owned_low;

        uint32_t equipped =
            ror32(
                get_u32le(blob + 44),
                5
            ) ^
            salt ^
            0x9E3779B9u;

        decoded.player_style =
            (uint8_t)(
                (equipped >> 0) &
                0xFFu
            );

        decoded.rope_style =
            (uint8_t)(
                (equipped >> 8) &
                0xFFu
            );

        decoded.pattern_style =
            (uint8_t)(
                (equipped >> 16) &
                0xFFu
            );

        decoded.lifetime_destroyed =
            ror32(
                get_u32le(blob + 48),
                15
            ) ^
            salt ^
            0x7F4A7C15u;
    } else {
        decoded.player_level = 1;
    }

    if (version >= SAVE_VERSION_V9) {
        uint32_t settings =
            ror32(
                get_u32le(blob + 52),
                3
            ) ^
            salt ^
            0x4CF5AD43u;

        unpack_settings(
            &decoded,
            settings
        );
    } else {
        decoded.settings_flags =
            SETTINGS_DEFAULT_FLAGS;

        decoded.bloom_level =
            SETTINGS_BLOOM_DEFAULT;

        decoded.force_lod =
            SETTINGS_FORCE_LOD_AUTO;

        decoded.settings_reserved =
            SETTINGS_VALID_MARKER;
    }

    if (version >= SAVE_VERSION_V10) {
        uint32_t owned_high =
            ror32(
                get_u32le(blob + 56),
                27
            ) ^
            salt ^
            0xB7E15163u;

        decoded.cosmetic_owned |=
            (uint64_t)owned_high <<
            32;

        decoded.best_speed =
            ror32(
                get_u32le(blob + 60),
                7
            ) ^
            salt ^
            0x1BF5A17Du;

        decoded.best_combo =
            ror32(
                get_u32le(blob + 64),
                19
            ) ^
            salt ^
            0x94D049BBu;

        decoded.missions_claimed =
            ror32(
                get_u32le(blob + 68),
                9
            ) ^
            salt ^
            0xDEADBEEFu;

        uint32_t equip2 =
            ror32(
                get_u32le(blob + 72),
                11
            ) ^
            salt ^
            0x6A09E667u;

        uint32_t equip3 =
            ror32(
                get_u32le(blob + 76),
                17
            ) ^
            salt ^
            0xBB67AE85u;

        decoded.shape_style =
            (uint8_t)((equip2 >> 0) & 0xFFu);

        decoded.player_anim_style =
            (uint8_t)((equip2 >> 8) & 0xFFu);

        decoded.hat_style =
            (uint8_t)((equip2 >> 16) & 0xFFu);

        decoded.block_theme =
            (uint8_t)((equip2 >> 24) & 0xFFu);

        decoded.background_style =
            (uint8_t)((equip3 >> 0) & 0xFFu);

        decoded.lava_color_style =
            (uint8_t)((equip3 >> 8) & 0xFFu);

        decoded.lava_anim_style =
            (uint8_t)((equip3 >> 16) & 0xFFu);

        decoded.reserved_style1 =
            (uint8_t)((equip3 >> 24) & 0xFFu);
    } else {
        decoded.reserved_style1 = 0;
    }

    if (version >= SAVE_VERSION_V11) {
        uint32_t owned2_low =
            ror32(
                get_u32le(blob + 80),
                13
            ) ^
            salt ^
            0x3C6EF372u;

        uint32_t owned2_high =
            ror32(
                get_u32le(blob + 84),
                23
            ) ^
            salt ^
            0xA54FF53Au;

        decoded.cosmetic_owned2 =
            (uint64_t)owned2_low |
            ((uint64_t)owned2_high << 32);

        decoded.title_style =
            (uint8_t)(
                ror32(
                    get_u32le(blob + 88),
                    3
                ) ^
                salt ^
                0x510E527Fu
            );
    } else {
        decoded.cosmetic_owned2 = 0;
        decoded.title_style = 0;
    }

    if (version >= SAVE_VERSION_V12) {
        decoded.total_distance_traveled =
            ror32(
                get_u32le(blob + 92),
                29
            ) ^
            salt ^
            0x1F83D9ABu;
    } else {
        decoded.total_distance_traveled = 0;
    }

    uint32_t stored_check =
        get_u32le(
            blob + 24
        );

    stored_check =
        ror32(
            stored_check,
            7
        ) ^
        salt ^
        0xB5297A4Du;

    uint32_t expected =
        progress_checksum_version(
            &decoded,
            salt,
            version
        );

    if (stored_check != expected)
        return false;

    *p = decoded;
    return true;
}

static bool save_progress_native(const Progress *progress) {
    if (!progress) return false;

    uint8_t blob[SAVE_BYTES];
    encode_progress_blob(progress, blob);

    Handle file = 0;
    FS_Path archive_path = fsMakePath(PATH_EMPTY, "");
    FS_Path file_path = fsMakePath(PATH_ASCII, SAVE_FS_PATH);

    Result rc = FSUSER_OpenFileDirectly(
        &file,
        ARCHIVE_SDMC,
        archive_path,
        file_path,
        FS_OPEN_WRITE | FS_OPEN_CREATE,
        0
    );

    if (R_FAILED(rc))
        return false;

    rc = FSFILE_SetSize(file, SAVE_BYTES);
    if (R_FAILED(rc)) {
        FSFILE_Close(file);
        return false;
    }

    u32 written = 0;
    rc = FSFILE_Write(
        file,
        &written,
        0,
        blob,
        SAVE_BYTES,
        FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME
    );

    Result close_rc = FSFILE_Close(file);

    return R_SUCCEEDED(rc) &&
           R_SUCCEEDED(close_rc) &&
           written == SAVE_BYTES;
}

static bool load_progress_native(Progress *progress) {
    if (!progress)
        return false;

    memset(
        progress,
        0,
        sizeof(*progress)
    );

    Handle file = 0;

    FS_Path archive_path =
        fsMakePath(
            PATH_EMPTY,
            ""
        );

    FS_Path file_path =
        fsMakePath(
            PATH_ASCII,
            SAVE_FS_PATH
        );

    Result rc =
        FSUSER_OpenFileDirectly(
            &file,
            ARCHIVE_SDMC,
            archive_path,
            file_path,
            FS_OPEN_READ,
            0
        );

    if (R_FAILED(rc))
        return false;

    u64 file_size = 0;

    rc =
        FSFILE_GetSize(
            file,
            &file_size
        );

    if (R_FAILED(rc) ||
        (file_size != SAVE_BYTES &&
         file_size != SAVE_LEGACY_BYTES)) {
        FSFILE_Close(file);
        return false;
    }

    uint8_t blob[SAVE_BYTES];
    memset(blob, 0, sizeof(blob));

    u32 bytes_read = 0;

    rc =
        FSFILE_Read(
            file,
            &bytes_read,
            0,
            blob,
            (u32)file_size
        );

    Result close_rc =
        FSFILE_Close(
            file
        );

    if (R_FAILED(rc) ||
        R_FAILED(close_rc) ||
        bytes_read != (u32)file_size) {
        return false;
    }

    Progress decoded;
    memset(
        &decoded,
        0,
        sizeof(decoded)
    );

    if (!decode_progress_blob(
            &decoded,
            blob,
            (unsigned)file_size)) {
        return false;
    }

    *progress = decoded;
    return true;
}

static void save_progress(const Game *g) {
    if (!g) return;
    (void)save_progress_native(&g->progress);
}

static void load_progress(Game *g) {
    if (!g) return;

    memset(&g->progress, 0, sizeof(g->progress));
    (void)load_progress_native(&g->progress);
    ensure_progress_defaults(&g->progress);
}

static void apply_runtime_settings(Game *g) {
    if (!g)
        return;

    render_set_bloom_level(
        (int)g->progress.bloom_level
    );

    audio_set_sfx_enabled(
        setting_enabled(
            g,
            SETTING_SFX
        )
    );

    audio_set_music_enabled(
        setting_enabled(
            g,
            SETTING_MUSIC
        )
    );
}



static void ensure_progress_defaults(Progress *p) {
    if (!p)
        return;

    if (p->player_level == 0)
        p->player_level = 1;

    /*
       Free starter item in every cosmetic category.
       Existing v8/v9 ownership in bits 0..11 is preserved.
    */
    p->cosmetic_owned |=
        (1ull << 0)  |  /* cyan player */
        (1ull << 4)  |  /* white rope */
        (1ull << 8)  |  /* solid rope */
        (1ull << 18) |  /* core shape */
        (1ull << 23) |  /* steady player anim */
        (1ull << 28) |  /* no hat */
        (1ull << 33) |  /* classic blocks */
        (1ull << 38) |  /* void background */
        (1ull << 43) |  /* fire lava */
        (1ull << 49);   /* calm lava anim */

    p->cosmetic_owned2 |=
        (1ull << (84 - 64)) | /* CITY title theme */
        (1ull << (98 - 64));   /* CLASSIC UI theme */

    if (p->player_style > 8) p->player_style = 0;
    if (p->rope_style > 8) p->rope_style = 0;
    if (p->pattern_style > 7) p->pattern_style = 0;
    if (p->shape_style > 8) p->shape_style = 0;
    if (p->player_anim_style > 8) p->player_anim_style = 0;
    if (p->hat_style > 15) p->hat_style = 0;
    if (p->block_theme > 6) p->block_theme = 0;
    if (p->background_style > 8) p->background_style = 0;
    if (p->lava_color_style > 7) p->lava_color_style = 0;
    if (p->lava_anim_style > 6) p->lava_anim_style = 0;
    if (p->title_style > 6) p->title_style = 0;
    if (p->reserved_style1 > 3) p->reserved_style1 = 0;

    if (p->levels[UPG_BULLETS] > 9)
        p->levels[UPG_BULLETS] = 9;

    if (p->levels[UPG_ROPE] > 15)
        p->levels[UPG_ROPE] = 15;

    if (p->settings_reserved != SETTINGS_VALID_MARKER) {
        p->settings_flags =
            SETTINGS_DEFAULT_FLAGS;

        p->bloom_level =
            SETTINGS_BLOOM_DEFAULT;

        p->force_lod =
            SETTINGS_FORCE_LOD_AUTO;

        p->settings_reserved =
            SETTINGS_VALID_MARKER;
    }

    if (p->bloom_level > SETTINGS_BLOOM_MAX)
        p->bloom_level = SETTINGS_BLOOM_DEFAULT;

    if (p->force_lod > SETTINGS_FORCE_LOD_MAX)
        p->force_lod = SETTINGS_FORCE_LOD_AUTO;
}

static Color player_color(const Game *g) {
    int style =
        g
        ? (int)g->progress.player_style
        : 0;

    style =
        clampi(
            style,
            0,
            8
        );

    return PLAYER_COLORS[style];
}

static Color rope_color(const Game *g) {
    int style =
        g
        ? (int)g->progress.rope_style
        : 0;

    style =
        clampi(
            style,
            0,
            8
        );

    return ROPE_COLORS[style];
}

static uint32_t xp_required_for_level(uint32_t level) {
    if (level < 1) level = 1;

    uint64_t need =
        360ull +
        (uint64_t)level * 125ull +
        (uint64_t)level * (uint64_t)level * 8ull;

    if (need > 5000000ull) need = 5000000ull;
    return (uint32_t)need;
}

static uint32_t level_cash_base(uint32_t new_level) {
    uint64_t reward =
        700ull +
        (uint64_t)new_level * 300ull +
        (uint64_t)new_level * (uint64_t)new_level * 18ull;

    if (reward > 25000000ull) reward = 25000000ull;
    return (uint32_t)reward;
}

static uint32_t randomized_reward(uint32_t base, int low_percent, int high_percent) {
    if (high_percent <= low_percent) return base;

    int span = high_percent - low_percent + 1;
    int pct = low_percent + (rand() % span);
    uint64_t out = (uint64_t)base * (uint64_t)pct / 100ull;
    if (out > 0xFFFFFFFFull) out = 0xFFFFFFFFull;
    return (uint32_t)out;
}

static void add_cash(Game *g,
                     uint32_t amount,
                     Vec2 popup_pos,
                     bool popup,
                     bool play_reward_sound) {
    if (!g || amount == 0) return;

    uint64_t total = (uint64_t)g->progress.money + amount;
    g->progress.money = total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)total;

    uint64_t run_total = (uint64_t)g->run_cash_earned + amount;
    g->run_cash_earned =
        run_total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)run_total;

    /*
       Coin audio is reserved for occasional/not-constant rewards.

       Examples that DO chime:
         - player level-up reward
         - Money Boi block reward
         - future special/rare rewards when explicitly requested

       Routine income stays quiet:
         - distance ticks
         - ordinary block cash
    */
    if (play_reward_sound)
        audio_play_money_gain();

    if (popup) {
        add_popup(g, popup_pos,
                  (int)(amount > 0x7FFFFFFFu ? 0x7FFFFFFF : amount),
                  true, C_MONEY);
    }
}

static void grant_xp(Game *g, uint32_t amount, Vec2 popup_pos) {
    if (!g || amount == 0) return;

    uint64_t run_total = (uint64_t)g->run_xp_earned + amount;
    g->run_xp_earned =
        run_total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)run_total;

    uint64_t xp_total = (uint64_t)g->progress.xp + amount;
    g->progress.xp =
        xp_total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)xp_total;

    while (g->progress.player_level < 100000u) {
        uint32_t need = xp_required_for_level(g->progress.player_level);
        if (g->progress.xp < need)
            break;

        g->progress.xp -= need;
        ++g->progress.player_level;
        ++g->run_levelups;

        /* Level-up cash scales by level and randomized payout variance. */
        uint32_t prize = randomized_reward(
            level_cash_base(g->progress.player_level),
            88, 116
        );

        g->last_level_reward = prize;
        g->levelup_message_timer = 2.1f;
        add_cash(g, prize, popup_pos, true, true);

        spawn_particles(g, popup_pos, C_YELLOW, 18, 115.0f);
        spawn_particles(g, popup_pos, C_CYAN, 10, 82.0f);
    }
}

static uint32_t block_xp_reward(BlockType type, int combo) {
    uint32_t xp = 14u + (uint32_t)(rand() % 15);
    if (type == BLOCK_RED) xp += 8u;
    if (type == BLOCK_GREEN) xp += 5u;
    if (combo > 1) xp += (uint32_t)(combo * 2);
    return xp;
}

static uint64_t cosmetic_bit(int item) {
    if (item < 0 ||
        item >= COSMETIC_COUNT) {
        return 0ull;
    }

    return
        1ull <<
        ((unsigned)item & 63u);
}

static bool cosmetic_owned(const Game *g, int item) {
    if (!g ||
        item < 0 ||
        item >= COSMETIC_COUNT) {
        return false;
    }

    uint64_t bit =
        cosmetic_bit(item);

    if (item < 64)
        return (g->progress.cosmetic_owned & bit) != 0;

    return
        (g->progress.cosmetic_owned2 & bit) != 0;
}

static void cosmetic_set_owned(Game *g, int item) {
    if (!g ||
        item < 0 ||
        item >= COSMETIC_COUNT) {
        return;
    }

    uint64_t bit =
        cosmetic_bit(item);

    if (item < 64)
        g->progress.cosmetic_owned |= bit;
    else
        g->progress.cosmetic_owned2 |= bit;
}

static bool cosmetic_equipped(const Game *g, int item) {
    if (!g ||
        item < 0 ||
        item >= COSMETIC_COUNT) {
        return false;
    }

    const CosmeticDef *d =
        &COSMETICS[item];

    switch (d->kind) {
        case COS_PLAYER_COLOR:
            return g->progress.player_style == d->style;

        case COS_ROPE_COLOR:
            return g->progress.rope_style == d->style;

        case COS_ROPE_ANIM:
            return g->progress.pattern_style == d->style;

        case COS_SHAPE:
            return g->progress.shape_style == d->style;

        case COS_PLAYER_ANIM:
            return g->progress.player_anim_style == d->style;

        case COS_HAT:
            return g->progress.hat_style == d->style;

        case COS_BLOCK_THEME:
            return g->progress.block_theme == d->style;

        case COS_BACKGROUND:
            return g->progress.background_style == d->style;

        case COS_LAVA_COLOR:
            return g->progress.lava_color_style == d->style;

        case COS_LAVA_ANIM:
            return g->progress.lava_anim_style == d->style;

        case COS_TITLE_THEME:
            return g->progress.title_style == d->style;

        case COS_UI_THEME:
            return g->progress.reserved_style1 == d->style;

        default:
            return false;
    }
}

static int cosmetic_owned_count(const Game *g) {
    if (!g)
        return 0;

    uint64_t masks[2] = {
        g->progress.cosmetic_owned,
        g->progress.cosmetic_owned2
    };

    int count = 0;

    for (int m = 0;
         m < 2;
         ++m) {
        uint64_t bits =
            masks[m];

        while (bits) {
            count +=
                (int)(bits & 1ull);
            bits >>= 1;
        }
    }

    return count;
}

static uint32_t goal_value(const Game *g, GoalKind kind) {
    if (!g)
        return 0;

    switch (kind) {
        case GOAL_DISTANCE:
            return g->progress.best_distance;

        case GOAL_BLOCKS:
            return g->progress.lifetime_destroyed;

        case GOAL_LEVEL:
            return g->progress.player_level;

        case GOAL_ROPE:
            return g->progress.levels[UPG_ROPE];

        case GOAL_SPEED:
            return g->progress.best_speed;

        case GOAL_COMBO:
            return g->progress.best_combo;

        case GOAL_COSMETICS:
            return (uint32_t)cosmetic_owned_count(g);

        case GOAL_SCORE:
            return g->progress.high_score;

        default:
            return 0;
    }
}

static bool achievement_unlocked(const Game *g, int index) {
    if (!g ||
        index < 0 ||
        index >= ACHIEVEMENT_COUNT) {
        return false;
    }

    const AchievementDef *a =
        &ACHIEVEMENTS[index];

    return
        goal_value(g, a->kind) >=
        a->target;
}

static const GeneratedMission *mission_slot_const(const Game *g,
                                                  int index) {
    if (!g ||
        index < 0 ||
        index >= MISSION_SLOT_COUNT) {
        return NULL;
    }

    return &g->missions[index];
}

static GeneratedMission *mission_slot(Game *g,
                                      int index) {
    if (!g ||
        index < 0 ||
        index >= MISSION_SLOT_COUNT) {
        return NULL;
    }

    return &g->missions[index];
}

static uint32_t mission_reward_cash(const Game *g,
                                    GoalKind kind,
                                    uint32_t tier) {
    (void)g;

    uint32_t base = 500u + tier * 140u;

    switch (kind) {
        case GOAL_DISTANCE: return base + 260u;
        case GOAL_SPEED:    return base + 220u;
        case GOAL_SCORE:    return base + 320u;
        case GOAL_COMBO:    return base + 280u;
        case GOAL_LEVEL:    return base + 420u;
        case GOAL_BLOCKS:
        default:            return base + 120u;
    }
}

static uint32_t mission_reward_xp(const Game *g,
                                  GoalKind kind,
                                  uint32_t tier) {
    (void)g;

    uint32_t base = 110u + tier * 32u;

    switch (kind) {
        case GOAL_DISTANCE: return base + 28u;
        case GOAL_SPEED:    return base + 34u;
        case GOAL_SCORE:    return base + 44u;
        case GOAL_COMBO:    return base + 38u;
        case GOAL_LEVEL:    return base + 56u;
        case GOAL_BLOCKS:
        default:            return base + 20u;
    }
}

static uint32_t mission_target_for_kind(const Game *g,
                                        GoalKind kind,
                                        uint32_t tier) {
    uint32_t current = goal_value(g, kind);

    switch (kind) {
        case GOAL_BLOCKS:
            return current + 18u + tier * 6u;

        case GOAL_DISTANCE:
            return current + 300u + tier * 130u;

        case GOAL_SPEED:
            return current + 28u + tier * 7u;

        case GOAL_SCORE:
            return current + 900u + tier * 240u;

        case GOAL_COMBO:
            return current + 1u + tier / 3u;

        case GOAL_LEVEL:
            if (current < 1u)
                current = 1u;
            return current + 1u + tier / 4u;

        default:
            return current + 1u;
    }
}

static void fill_generated_mission_text(GeneratedMission *m) {
    if (!m)
        return;

    GoalKind kind =
        (GoalKind)m->kind;

    switch (kind) {
        case GOAL_BLOCKS:
            snprintf(m->name, sizeof(m->name), "BLOCK BASH");
            snprintf(m->desc, sizeof(m->desc),
                     "BREAK %lu TOTAL BLOCKS",
                     (unsigned long)m->target);
            break;

        case GOAL_DISTANCE:
            snprintf(m->name, sizeof(m->name), "DISTANCE RUN");
            snprintf(m->desc, sizeof(m->desc),
                     "REACH %lu BEST DIST",
                     (unsigned long)m->target);
            break;

        case GOAL_SPEED:
            snprintf(m->name, sizeof(m->name), "FULL SEND");
            snprintf(m->desc, sizeof(m->desc),
                     "REACH SPEED %lu",
                     (unsigned long)m->target);
            break;

        case GOAL_SCORE:
            snprintf(m->name, sizeof(m->name), "STACK SCORE");
            snprintf(m->desc, sizeof(m->desc),
                     "REACH SCORE %lu",
                     (unsigned long)m->target);
            break;

        case GOAL_COMBO:
            snprintf(m->name, sizeof(m->name), "CHAIN BREAK");
            snprintf(m->desc, sizeof(m->desc),
                     "REACH COMBO X%lu",
                     (unsigned long)m->target);
            break;

        case GOAL_LEVEL:
            snprintf(m->name, sizeof(m->name), "LEVEL UP");
            snprintf(m->desc, sizeof(m->desc),
                     "REACH LEVEL %lu",
                     (unsigned long)m->target);
            break;

        default:
            snprintf(m->name, sizeof(m->name), "MISSION");
            snprintf(m->desc, sizeof(m->desc), "KEEP GOING");
            break;
    }
}

static void generate_mission_slot(Game *g,
                                  int index) {
    GeneratedMission *m =
        mission_slot(g, index);

    if (!m)
        return;

    uint32_t tier =
        g->progress.missions_claimed /
        (uint32_t)MISSION_SLOT_COUNT;

    uint32_t serial =
        g->progress.missions_claimed +
        (uint32_t)index;

    GoalKind kind =
        MISSION_KIND_ROTATION[
            (serial +
             (uint32_t)index * 2u) %
            MISSION_KIND_COUNT
        ];

    m->kind =
        (uint8_t)kind;

    m->target =
        mission_target_for_kind(
            g,
            kind,
            tier +
            (uint32_t)index
        );

    m->cash_reward =
        mission_reward_cash(
            g,
            kind,
            tier +
            (uint32_t)index
        );

    m->xp_reward =
        mission_reward_xp(
            g,
            kind,
            tier +
            (uint32_t)index
        );

    fill_generated_mission_text(m);
}

static void generate_all_missions(Game *g) {
    if (!g)
        return;

    for (int i = 0;
         i < MISSION_SLOT_COUNT;
         ++i) {
        generate_mission_slot(g, i);
    }
}

static bool mission_complete(const Game *g, int index) {
    const GeneratedMission *m =
        mission_slot_const(g, index);

    if (!m)
        return false;

    return goal_value(
               g,
               (GoalKind)m->kind
           ) >=
           m->target;
}

static void set_mission_notice(Game *g,
                               const char *text) {
    if (!g || !text)
        return;

    strncpy(
        g->mission_notice,
        text,
        sizeof(g->mission_notice) - 1
    );

    g->mission_notice[
        sizeof(g->mission_notice) - 1
    ] = '\0';

    g->mission_notice_timer = 2.0f;
}

static void complete_generated_mission(Game *g,
                                       int index) {
    GeneratedMission completed;
    GeneratedMission *m =
        mission_slot(g, index);

    if (!g || !m)
        return;

    completed = *m;

    add_cash(
        g,
        completed.cash_reward,
        g->player.pos,
        false,
        false
    );

    grant_xp(
        g,
        completed.xp_reward,
        g->player.pos
    );

    ++g->progress.missions_claimed;

    char notice[64];
    snprintf(
        notice,
        sizeof(notice),
        "%s +$%lu +%luXP",
        completed.name,
        (unsigned long)completed.cash_reward,
        (unsigned long)completed.xp_reward
    );
    set_mission_notice(g, notice);

    audio_play_purchase();
    generate_mission_slot(g, index);
    save_progress(g);
}

static void update_auto_missions(Game *g) {
    if (!g)
        return;

    for (int pass = 0;
         pass < 2;
         ++pass) {
        for (int i = 0;
             i < MISSION_SLOT_COUNT;
             ++i) {
            if (mission_complete(g, i))
                complete_generated_mission(g, i);
        }
    }
}

static float shape_rope_reach_mult(const Game *g) {
    if (!g)
        return 1.0f;

    switch (g->progress.shape_style) {
        case 2: return 1.08f; /* ORB */
        case 4: return 1.03f; /* COMET */
        default: return 1.0f;
    }
}

static float shape_launch_mult(const Game *g) {
    if (!g)
        return 1.0f;

    switch (g->progress.shape_style) {
        case 1: return 1.08f; /* DIAMOND */
        case 4: return 1.03f; /* COMET */
        case 5: return 1.07f; /* STAR */
        default: return 1.0f;
    }
}

static float shape_catch_mult(const Game *g) {
    if (!g)
        return 1.0f;

    switch (g->progress.shape_style) {
        case 4: return 1.03f; /* COMET */
        case 7: return 1.06f; /* BANANA */
        default: return 1.0f;
    }
}

static float shape_air_drag_mult(const Game *g) {
    if (!g)
        return 1.0f;

    switch (g->progress.shape_style) {
        case 3: return 0.90f; /* ARROW */
        case 4: return 0.97f; /* COMET */
        case 6: return 0.93f; /* HEX */
        default: return 1.0f;
    }
}

static float shape_combo_timer_mult(const Game *g) {
    if (!g)
        return 1.0f;

    switch (g->progress.shape_style) {
        case 8: return 1.04f; /* TVHEAD */
        default: return 1.0f;
    }
}

static uint32_t cosmetic_kill_cash_bonus(const Game *g) {
    if (!g)
        return 0;

    uint32_t bonus = 0;

    if (g->progress.player_style == 7)
        ++bonus; /* WHITEHOT */

    if (g->progress.block_theme == 6)
        ++bonus; /* CANDY */

    return bonus;
}

static float cosmetic_block_xp_mult(const Game *g) {
    if (!g)
        return 1.0f;

    return
        g->progress.lava_color_style == 7
        ? 1.05f
        : 1.0f;
}

static int cosmetic_bullet_count(const Game *g) {
    if (!g)
        return BULLET_SPRAY_COUNT;

    return
        g->progress.pattern_style == 6
        ? 12
        : BULLET_SPRAY_COUNT;
}

static float cosmetic_bullet_timer_mult(const Game *g) {
    if (!g)
        return 1.0f;

    return
        g->progress.hat_style == 14
        ? 1.08f
        : 1.0f;
}

static float cosmetic_bullet_size_mult(const Game *g) {
    if (!g)
        return 1.0f;

    return
        g->progress.pattern_style == 7
        ? 1.75f
        : 1.0f;
}

static float cosmetic_bullet_hit_bonus(const Game *g) {
    if (!g)
        return 0.0f;

    return
        g->progress.pattern_style == 7
        ? 2.5f
        : 0.0f;
}


static uint32_t shop_cost(const Game *g, int item) {
    int level = g->progress.levels[item];
    uint32_t cost = SHOP[item].base_cost;
    for (int i = 0; i < level; ++i) {
        uint64_t next = (uint64_t)cost * (uint64_t)SHOP[item].cost_scale_percent;
        cost = (uint32_t)(next / 100u);
        if (cost > 99999999u) return 99999999u;
    }
    return cost;
}

static float rope_max_length(const Game *g) {
    /* Rope reach scales with camera zoom and rope level. */
    float level_bonus =
        1.0f + 0.025f * (float)g->progress.levels[UPG_ROPE];

    return 500.0f * g->camera_zoom * level_bonus * shape_rope_reach_mult(g);
}

static float grapple_launch_speed(const Game *g) {
    /*
       Tangential velocity ADDED on latch. Existing velocity is preserved.
       Better Rope increases how much extra momentum a grab contributes.
    */
    return (62.0f + 8.0f * (float)g->progress.levels[UPG_ROPE]) * shape_launch_mult(g);
}

static float grapple_catch_step(const Game *g) {
    /*
       Small one-time inward catch on latch so the player visually "snaps"
       into the rope, but does not reel hard into the block.
    */
    return (2.75f + 0.35f * (float)g->progress.levels[UPG_ROPE]) * shape_catch_mult(g);
}

static float vcross(Vec2 a, Vec2 b) {
    return a.x * b.y - a.y * b.x;
}

static Vec2 tangent_from_dir(Vec2 dir, float sign) {
    return (sign >= 0.0f)
        ? vec2(-dir.y, dir.x)
        : vec2(dir.y, -dir.x);
}

static Color block_color(const Game *g, BlockType t) {
    uint8_t theme =
        g
        ? g->progress.block_theme
        : 0;

    switch (theme) {
        case 1: /* ICE */
            switch (t) {
                case BLOCK_RED:    return rgb(74, 150, 255);
                case BLOCK_PURPLE: return rgb(120, 210, 255);
                case BLOCK_GREEN:  return rgb(110, 255, 220);
                case BLOCK_MONEY:  return rgb(220, 245, 255);
                default:           return rgb(240, 250, 255);
            }

        case 2: /* SUNSET */
            switch (t) {
                case BLOCK_RED:    return rgb(255, 72, 62);
                case BLOCK_PURPLE: return rgb(255, 86, 175);
                case BLOCK_GREEN:  return rgb(255, 154, 58);
                case BLOCK_MONEY:  return rgb(255, 220, 82);
                default:           return rgb(255, 238, 205);
            }

        case 3: /* MONO */
            switch (t) {
                case BLOCK_RED:    return rgb(205, 205, 205);
                case BLOCK_PURPLE: return rgb(150, 150, 160);
                case BLOCK_GREEN:  return rgb(235, 235, 235);
                case BLOCK_MONEY:  return rgb(190, 190, 190);
                default:           return C_WHITE;
            }

        case 4: /* ARCADE */
            switch (t) {
                case BLOCK_RED:    return rgb(255, 45, 125);
                case BLOCK_PURPLE: return rgb(156, 68, 255);
                case BLOCK_GREEN:  return rgb(25, 255, 125);
                case BLOCK_MONEY:  return rgb(255, 246, 45);
                default:           return rgb(45, 235, 255);
            }

        case 5: /* NIGHT */
            switch (t) {
                case BLOCK_RED:    return rgb(42, 78, 160);
                case BLOCK_PURPLE: return rgb(78, 54, 170);
                case BLOCK_GREEN:  return rgb(32, 145, 142);
                case BLOCK_MONEY:  return rgb(175, 160, 72);
                default:           return rgb(90, 125, 185);
            }

        case 6: /* CANDY */
            switch (t) {
                case BLOCK_RED:    return rgb(255, 86, 138);
                case BLOCK_PURPLE: return rgb(202, 95, 255);
                case BLOCK_GREEN:  return rgb(88, 255, 176);
                case BLOCK_MONEY:  return rgb(255, 230, 100);
                default:           return rgb(94, 220, 255);
            }

        case 0:
        default:
            switch (t) {
                case BLOCK_RED: return C_RED;
                case BLOCK_PURPLE: return C_PURPLE;
                case BLOCK_GREEN: return C_GREEN;
                case BLOCK_MONEY: return C_MONEY;
                case BLOCK_ANCHOR:
                default: return C_WHITE;
            }
    }
}

static Color background_clear_color(const Game *g) {
    if (!g)
        return C_BG;

    switch (g->progress.background_style) {
        case 1: return rgb(2, 7, 20);   /* MIDNIGHT */
        case 2: return rgb(14, 3, 22);  /* NEBULA */
        case 3: return rgb(1, 13, 8);   /* MATRIX */
        case 4: return rgb(20, 7, 5);   /* DUSK */
        case 5: return rgb(1, 5, 16);   /* CITY */
        case 6: return rgb(5, 7, 12);   /* STORM */
        case 7: return rgb(2, 8, 24);   /* AURORA */
        case 8: return rgb(72, 142, 205); /* CLOUD9 */
        case 0:
        default: return C_BG;
    }
}

static int ui_style(const Game *g) {
    return clampi(g ? (int)g->progress.reserved_style1 : 0, 0, 3);
}

static Color ui_bg_color(const Game *g) {
    switch (ui_style(g)) {
        case 1: return rgb(4, 12, 20);
        case 2: return rgb(18, 10, 6);
        case 3: return rgb(11, 6, 18);
        default: return C_BG;
    }
}

static Color ui_panel_color(const Game *g) {
    switch (ui_style(g)) {
        case 1: return rgb(8, 24, 36);
        case 2: return rgb(34, 16, 10);
        case 3: return rgb(22, 10, 32);
        default: return rgb(7, 10, 14);
    }
}

static Color ui_panel_alt_color(const Game *g) {
    switch (ui_style(g)) {
        case 1: return rgb(11, 34, 50);
        case 2: return rgb(48, 22, 12);
        case 3: return rgb(30, 14, 42);
        default: return rgb(11, 15, 20);
    }
}

static Color ui_accent_color(const Game *g) {
    switch (ui_style(g)) {
        case 1: return rgb(90, 200, 255);
        case 2: return rgb(255, 170, 68);
        case 3: return rgb(190, 110, 255);
        default: return C_CYAN;
    }
}

static Color ui_good_color(const Game *g) {
    switch (ui_style(g)) {
        case 2: return rgb(255, 220, 110);
        default: return C_GREEN;
    }
}

static Color ui_dim_color(const Game *g) {
    switch (ui_style(g)) {
        case 1: return rgb(130, 170, 190);
        case 2: return rgb(185, 155, 130);
        case 3: return rgb(160, 135, 205);
        default: return C_DIM;
    }
}

static void draw_ui_frame(const Game *g,
                          Surface *s,
                          int x,
                          int y,
                          int w,
                          int h) {
    draw_rect(s, x, y, w, h, ui_panel_color(g));
    draw_rect_outline(s, x, y, w, h, ui_accent_color(g));
    draw_rect_outline(s, x + 3, y + 3, w - 6, h - 6, ui_panel_alt_color(g));
}

static Color background_star_color(const Game *g, int brightness) {
    int v = 15 + brightness * 7;

    if (!g)
        return rgb((uint8_t)(v + 20), (uint8_t)(v / 2), 3);

    switch (g->progress.background_style) {
        case 1:
            return rgb((uint8_t)(v / 2),
                       (uint8_t)(v + 24),
                       (uint8_t)(v + 46));

        case 2:
            return rgb((uint8_t)(v + 34),
                       (uint8_t)(v / 2),
                       (uint8_t)(v + 40));

        case 3:
            return rgb((uint8_t)(v / 3),
                       (uint8_t)(v + 40),
                       (uint8_t)(v / 2));

        case 4:
            return rgb((uint8_t)(v + 48),
                       (uint8_t)(v + 12),
                       (uint8_t)(v / 3));

        case 5:
            return rgb((uint8_t)(v / 2),
                       (uint8_t)(v + 28),
                       (uint8_t)(v + 58));

        case 6:
            return rgb((uint8_t)(v + 18),
                       (uint8_t)(v + 20),
                       (uint8_t)(v + 28));

        case 7:
            return rgb((uint8_t)(v / 2),
                       (uint8_t)(v + 48),
                       (uint8_t)(v + 30));

        case 8:
            return rgb(245, 250, 255);

        case 0:
        default:
            return rgb((uint8_t)(v + 20),
                       (uint8_t)(v / 2),
                       3);
    }
}

static Color lava_color(const Game *g) {
    if (!g)
        return C_ORANGE;

    switch (g->progress.lava_color_style) {
        case 1: return rgb(18, 210, 245);
        case 2: return rgb(190, 52, 235);
        case 3: return rgb(70, 235, 70);
        case 4: return rgb(245, 245, 250);
        case 5: return rgb(255, 65, 170);
        case 6: return rgb(35, 82, 255);
        case 7: {
            float p =
                g
                ? (float)g->frame_counter * 0.025f
                : 0.0f;

            int r =
                (int)(128.0f + 127.0f * sinf(p));

            int gg =
                (int)(128.0f + 127.0f * sinf(p + 2.094f));

            int b =
                (int)(128.0f + 127.0f * sinf(p + 4.188f));

            return rgb((uint8_t)r, (uint8_t)gg, (uint8_t)b);
        }
        case 0:
        default: return C_ORANGE;
    }
}

static Color lava_highlight_color(const Game *g) {
    Color c =
        lava_color(g);

    int r = c.r + 72;
    int gg = c.g + 72;
    int b = c.b + 72;

    if (r > 255) r = 255;
    if (gg > 255) gg = 255;
    if (b > 255) b = 255;

    return rgb(
        (uint8_t)r,
        (uint8_t)gg,
        (uint8_t)b
    );
}

static void rope_clear(Rope *r) {
    r->active = false;
    r->latched = false;
    r->user_owned = false;
    r->target_block = -1;
    r->direction = vec2(0.0f, -1.0f);
    r->hook_pos = vec2(0.0f, 0.0f);
    r->length = 0.0f;
    r->desired_length = 0.0f;
}

static void add_popup(Game *g, Vec2 pos, int value, bool money, Color c) {
    for (int i = 0; i < MAX_POPUPS; ++i) {
        if (!g->popups[i].active) {
            g->popups[i].active = true;
            g->popups[i].pos = pos;
            g->popups[i].life = 0.85f;
            g->popups[i].value = value;
            g->popups[i].money = money;
            g->popups[i].color = c;
            return;
        }
    }
}

static void spawn_particles(Game *g, Vec2 pos, Color c, int count, float speed) {
    if (!g ||
        !setting_enabled(g, SETTING_PARTICLES)) {
        return;
    }

    for (int n = 0; n < count; ++n) {
        int slot = -1;
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            if (!g->particles[i].active) { slot = i; break; }
        }
        if (slot < 0) return;

        float a = frand_range(0.0f, 6.2831853f);
        float s = frand_range(speed * 0.25f, speed);
        Particle *p = &g->particles[slot];
        p->active = true;
        p->pos = pos;
        p->vel = vec2(cosf(a) * s, sinf(a) * s);
        p->life = frand_range(0.22f, 0.72f);
        p->color = c;
    }
}

static void spawn_bullet(Game *g, Vec2 pos, Vec2 dir, Color c, float speed) {
    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (!g->bullets[i].active) {
            g->bullets[i].active = true;
            g->bullets[i].pos = pos;
            g->bullets[i].vel = vmul(vnormalize(dir), speed);
            g->bullets[i].life = 3.0f;
            g->bullets[i].color = c;
            g->bullets[i].power = 1;
            return;
        }
    }
}

static void spawn_green_burst(Game *g, Vec2 pos) {
    const int count = 10;
    for (int i = 0; i < count; ++i) {
        float a = ((float)i / (float)count) * 6.2831853f;
        spawn_bullet(g, pos, vec2(cosf(a), sinf(a)), C_GREEN, 150.0f);
    }
}

static void setup_anchor(Block *b, float x, float y, float half) {
    memset(b, 0, sizeof(*b));
    b->active = true;
    b->type = BLOCK_ANCHOR;
    b->half = half;
    b->body.pos = vec2(x, y);
    b->body.vel = vec2(0.0f, 0.0f);
    b->body.inv_mass = 0.0f;
    b->body.damping = 0.0f;
    b->body.restitution = 0.75f;
    b->home = b->body.pos;
    b->base_half = half;
    b->anim_phase = frand_range(0.0f, 6.2831853f);
    b->angle = sinf(b->anim_phase) * 0.175f;
    b->angular_velocity = frand_range(0.85f, 1.45f);
    b->section_x = 0;
    b->section_y = 0;
    b->streamed = false;
}

static void ensure_start_anchor(Game *g) {
    /*
       A real floating starting block. The player begins resting on top of it.
       It survives until the first successful grapple, then crumbles.
    */
    /* Starter platform center Y=138; bottom Y=152; lava begins at Y=218. */
    setup_anchor(&g->blocks[0], 200.0f, 138.0f, 14.0f);
    g->blocks[0].value = 0;
    g->starter_platform_active = true;
}

static void setup_initial_target(Block *b, BlockType type,
                                 float x, float y, float half, int value) {
    setup_anchor(b, x, y, half);
    b->type = type;
    b->value = value;
}

static int world_to_section(float v, float section_size) {
    return (int)floorf(v / section_size);
}

static bool section_stamp_exists(const Game *g, int sx, int sy) {
    for (int i = 0; i < MAX_SECTION_STAMPS; ++i) {
        if (g->sections[i].valid &&
            g->sections[i].x == sx &&
            g->sections[i].y == sy)
            return true;
    }
    return false;
}

static int find_free_section_stamp(const Game *g) {
    for (int i = 0; i < MAX_SECTION_STAMPS; ++i)
        if (!g->sections[i].valid) return i;
    return -1;
}

static int find_free_stream_block(const Game *g) {
    for (int i = STREAM_BLOCK_START; i < MAX_BLOCKS; ++i)
        if (!g->blocks[i].active) return i;
    return -1;
}

static void clear_streamed_section(Game *g, int sx, int sy) {
    for (int i = STREAM_BLOCK_START; i < MAX_BLOCKS; ++i) {
        Block *b = &g->blocks[i];
        if (!b->streamed || b->section_x != sx || b->section_y != sy)
            continue;

        if (g->rope.target_block == i)
            rope_clear(&g->rope);

        b->active = false;
        b->streamed = false;
    }
}

static uint32_t map_mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static uint32_t section_seed(const Game *g, int sx, int sy) {
    uint32_t x = (uint32_t)sx * 0x9E3779B9u;
    uint32_t y = (uint32_t)sy * 0x85EBCA6Bu;
    return map_mix32(g->world_seed ^ x ^ rol32(y, 13) ^ 0xA511E9B3u);
}

static uint32_t section_rng_next(uint32_t *state) {
    uint32_t x = *state ? *state : 0x6D2B79F5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float section_rand01(uint32_t *state) {
    return (float)(section_rng_next(state) & 0x00FFFFFFu) / 16777216.0f;
}

static float section_rand_range(uint32_t *state, float lo, float hi) {
    return lo + (hi - lo) * section_rand01(state);
}

static BlockType random_section_block_type(const Game *g,
                                           int ordinal,
                                           uint32_t *rng) {
    if (ordinal == 0) return BLOCK_ANCHOR;
    if (ordinal == 1) return BLOCK_PURPLE;

    float roll = section_rand01(rng);
    if (roll < 0.29f) return BLOCK_ANCHOR;
    if (roll < 0.58f) return BLOCK_PURPLE;
    if (roll < 0.74f) return BLOCK_RED;
    if (roll < 0.88f) return BLOCK_MONEY;
    if (g->progress.levels[UPG_GREEN] > 0) return BLOCK_GREEN;
    return BLOCK_PURPLE;
}

static float section_distance_from_start(int sx, int sy) {
    float x = (float)sx * SECTION_W;
    float y = (float)sy * SECTION_H;
    return sqrtf(x * x + y * y);
}

static int section_block_count(int sx, int sy) {
    /* Generated section density increases from 1 to 4 blocks with distance. */
    float d = section_distance_from_start(sx, sy);
    int count =
        d < 1200.0f ? 1 :
        d < 2800.0f ? 2 :
        d < 5200.0f ? 3 :
                      4;

    /* Higher sections intentionally thin out so the climb gets spookier. */
    float height = fmaxf(0.0f, -((float)sy * SECTION_H));
    if (height >= 900.0f)
        --count;
    if (height >= 2100.0f)
        --count;

    if (count < 1)
        count = 1;

    return count;
}

static float section_clump_amount(int sx, int sy) {
    /*
       Early/mid game has no intentional clumping.
       Far away, only some blocks are allowed to form loose pairs.
    */
    float d = section_distance_from_start(sx, sy);
    return clampf_local((d - 3600.0f) / 4200.0f, 0.0f, 0.55f);
}

static uint32_t block_anim_hash(const Game *g, int sx, int sy, int ordinal) {
    uint32_t x = (uint32_t)sx * 73856093u;
    uint32_t y = (uint32_t)sy * 19349663u;
    uint32_t o = (uint32_t)(ordinal + 1) * 83492791u;
    uint32_t h = x ^ y ^ o ^ g->world_seed ^ 0xA511E9B3u;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    return h;
}

static bool section_position_is_clear(const Game *g,
                                      int sx, int sy,
                                      Vec2 world_pos,
                                      float min_sep) {
    float min_sep2 = min_sep * min_sep;

    for (int i = STREAM_BLOCK_START; i < MAX_BLOCKS; ++i) {
        const Block *other = &g->blocks[i];

        if (!other->active || !other->streamed)
            continue;

        if (other->section_x != sx || other->section_y != sy)
            continue;

        Vec2 d = vsub(world_pos, other->body.pos);
        if (vdot(d, d) < min_sep2)
            return false;
    }

    return true;
}

static bool find_section_partner(const Game *g,
                                 int sx, int sy,
                                 Vec2 *out) {
    int matches[8];
    int count = 0;

    for (int i = STREAM_BLOCK_START;
         i < MAX_BLOCKS && count < (int)(sizeof(matches) / sizeof(matches[0]));
         ++i) {
        const Block *b = &g->blocks[i];

        if (!b->active || !b->streamed)
            continue;

        if (b->section_x == sx && b->section_y == sy)
            matches[count++] = i;
    }

    if (count <= 0)
        return false;

    int chosen = matches[rand() % count];
    *out = g->blocks[chosen].body.pos;
    return true;
}

static void setup_streamed_block(Game *g, int slot,
                                 int sx, int sy,
                                 int ordinal, int count,
                                 uint32_t *rng) {
    (void)count;

    Block *b = &g->blocks[slot];
    memset(b, 0, sizeof(*b));

    BlockType type = random_section_block_type(g, ordinal, rng);

    b->active = true;
    b->streamed = true;
    b->section_x = sx;
    b->section_y = sy;
    b->type = type;

    float half = 7.0f;
    int value = 80;

    switch (type) {
        case BLOCK_RED:
            half = section_rand_range(rng, 6.0f, 9.0f);
            value = 160;
            break;

        case BLOCK_PURPLE:
            half = section_rand_range(rng, 6.0f, 9.0f);
            value = 100;
            break;

        case BLOCK_GREEN:
            half = section_rand_range(rng, 6.0f, 8.0f);
            value = 175;
            break;

        case BLOCK_MONEY:
            half = 7.0f;
            value = 120;
            break;

        case BLOCK_ANCHOR:
        default:
            half = section_rand_range(rng, 6.0f, 9.0f);
            value = 80;
            break;
    }

    float d = section_distance_from_start(sx, sy);
    float clump = section_clump_amount(sx, sy);

    /*
       Wide spacing near spawn; progressively smaller minimum separation
       farther away as the map becomes denser.
    */
    float min_sep =
        d < 1200.0f ? 125.0f :
        d < 2800.0f ? 100.0f :
        d < 5200.0f ? 78.0f :
                      60.0f;

    float height = fmaxf(0.0f, -((float)sy * SECTION_H));
    min_sep += clampf_local(height / 260.0f, 0.0f, 36.0f);

    float base_x = (float)sx * SECTION_W;
    float base_y = (float)sy * SECTION_H;

    Vec2 chosen = vec2(base_x + SECTION_W * 0.5f,
                       base_y + SECTION_H * 0.5f);

    bool placed = false;

    for (int attempt = 0; attempt < 28; ++attempt) {
        float local_x;
        float local_y;

        /*
           There are NO lanes/bands anymore.
           Normal placement is uniform anywhere inside the section margins.
        */
        bool try_cluster =
            ordinal >= 1 &&
            clump > 0.05f &&
            section_rand01(rng) < clump * 0.55f;

        if (try_cluster) {
            Vec2 partner;

            if (find_section_partner(g, sx, sy, &partner)) {
                float a = section_rand_range(rng, 0.0f, 6.2831853f);
                float gap = section_rand_range(rng, 48.0f, 88.0f);

                local_x = (partner.x - base_x) + cosf(a) * gap;
                local_y = (partner.y - base_y) + sinf(a) * gap;
            } else {
                local_x = section_rand_range(rng, 32.0f, SECTION_W - 32.0f);
                local_y = section_rand_range(rng, 32.0f, SECTION_H - 32.0f);
            }
        } else {
            local_x = section_rand_range(rng, 32.0f, SECTION_W - 32.0f);
            local_y = section_rand_range(rng, 32.0f, SECTION_H - 32.0f);
        }

        local_x = clampf_local(local_x, 28.0f, SECTION_W - 28.0f);
        local_y = clampf_local(local_y, 28.0f, SECTION_H - 28.0f);

        Vec2 world_pos = vec2(base_x + local_x, base_y + local_y);

        if (section_position_is_clear(g, sx, sy, world_pos, min_sep)) {
            chosen = world_pos;
            placed = true;
            break;
        }
    }

    /*
       Dense far-away sections can occasionally fail the strict spacing test.
       Fall back to a fully random legal location instead of a fixed lane.
    */
    if (!placed) {
        chosen = vec2(
            base_x + section_rand_range(rng, 32.0f, SECTION_W - 32.0f),
            base_y + section_rand_range(rng, 32.0f, SECTION_H - 32.0f)
        );
    }

    b->half = half;
    b->base_half = half;
    b->value = value;
    b->body.pos = chosen;
    b->home = chosen;
    b->body.vel = vec2(0.0f, 0.0f);
    b->body.inv_mass = 0.0f;
    b->body.damping = 0.0f;
    b->body.restitution = 0.0f;

    uint32_t ah = block_anim_hash(g, sx, sy, ordinal);

    b->anim_phase =
        ((float)(ah % 6283u) / 1000.0f) +
        (float)ordinal * 0.41f;

    while (b->anim_phase >= 6.2831853f)
        b->anim_phase -= 6.2831853f;

    b->angular_velocity =
        0.85f + (float)((ah >> 8) % 61u) / 100.0f;

    b->angle = 0.0f;
}

static void generate_section(Game *g, int sx, int sy) {
    if (section_stamp_exists(g, sx, sy))
        return;

    int stamp = find_free_section_stamp(g);
    if (stamp < 0)
        return;

    g->sections[stamp].valid = true;
    g->sections[stamp].x = sx;
    g->sections[stamp].y = sy;

    /*
       Positive Y sections sit completely below the original lava line and can
       never be reached during a valid run. Stamp them, but don't waste blocks.
    */
    if ((float)sy * SECTION_H >= LAVA_Y)
        return;

    /* Section (0,0) contains the starter platform and two opening anchors. */
    if (sx == 0 && sy == 0)
        return;

    int count = section_block_count(sx, sy);
    uint32_t rng = section_seed(g, sx, sy);

    for (int n = 0; n < count; ++n) {
        int slot = find_free_stream_block(g);
        if (slot < 0)
            return;
        setup_streamed_block(g, slot, sx, sy, n, count, &rng);
    }
}

static bool section_needed(int sx, int sy,
                           int cx, int cy,
                           int radius_x, int radius_y) {
    return sx >= cx - radius_x &&
           sx <= cx + radius_x &&
           sy >= cy - radius_y &&
           sy <= cy + radius_y;
}

static void desired_section_radii(const Game *g,
                                  int *radius_x,
                                  int *radius_y) {
    float half_world_w = TOP_W * 0.5f * g->camera_zoom;
    float half_world_h = TOP_H * 0.5f * g->camera_zoom;

    int rx = (int)ceilf(half_world_w / SECTION_W);
    int ry = (int)ceilf(half_world_h / SECTION_H);

    /*
       At higher speed/zoom add one whole section of prefetch around the
       visible area. At normal zoom, a 3x3 cache is enough.
    */
    if (g->camera_zoom > 1.80f) {
        ++rx;
        ++ry;
    }

    *radius_x = clampi(rx, 1, 3);
    *radius_y = clampi(ry, 1, 3);
}

static void refresh_section_cache(Game *g, bool force) {
    float view_center_x =
        g->camera_x + TOP_W * 0.5f * g->camera_zoom;
    float view_center_y =
        g->camera_y + TOP_H * 0.5f * g->camera_zoom;

    int cx = world_to_section(view_center_x, SECTION_W);
    int cy = world_to_section(view_center_y, SECTION_H);

    int radius_x;
    int radius_y;
    desired_section_radii(g, &radius_x, &radius_y);

    if (!force &&
        g->section_cache_ready &&
        cx == g->section_center_x &&
        cy == g->section_center_y &&
        radius_x == g->section_radius_x &&
        radius_y == g->section_radius_y)
        return;

    g->section_center_x = cx;
    g->section_center_y = cy;
    g->section_radius_x = radius_x;
    g->section_radius_y = radius_y;
    g->section_cache_ready = true;

    /*
       Retire sections outside the zoom-dependent cache first.
       Generation still happens only when section/cache boundaries change,
       never continuously because the player happens to be moving.
    */
    for (int i = 0; i < MAX_SECTION_STAMPS; ++i) {
        SectionStamp *s = &g->sections[i];
        if (!s->valid) continue;

        if (!section_needed(s->x, s->y,
                            cx, cy,
                            radius_x, radius_y)) {
            clear_streamed_section(g, s->x, s->y);
            s->valid = false;
        }
    }

    for (int sy = cy - radius_y; sy <= cy + radius_y; ++sy) {
        for (int sx = cx - radius_x; sx <= cx + radius_x; ++sx) {
            if (!section_stamp_exists(g, sx, sy))
                generate_section(g, sx, sy);
        }
    }
}


static void reset_player_to_anchor(Game *g) {
    ensure_start_anchor(g);

    g->player_half = g->progress.levels[UPG_THICC] ? 10.0f : 6.0f;

    /* Rest exactly on top of the 28px-wide starter block. */
    g->player.pos = vec2(200.0f,
                         g->blocks[0].body.pos.y -
                         g->blocks[0].half -
                         g->player_half);

    g->player.vel = vec2(0.0f, 0.0f);
    g->player.inv_mass = 1.0f;
    g->player.damping = PLAYER_AIR_DRAG;
    g->player.restitution = 0.35f;
    g->player_angle = 0.0f;
    g->player_spin = 0.0f;
    g->player_stretch = 0.0f;

    rope_clear(&g->rope);
    g->aim_world = g->player.pos;
    g->aim_valid = false;
    g->last_distance_sample = g->player.pos;
}

static void start_run(Game *g) {
    memset(g->blocks, 0, sizeof(g->blocks));
    memset(g->particles, 0, sizeof(g->particles));
    memset(g->bullets, 0, sizeof(g->bullets));
    memset(g->popups, 0, sizeof(g->popups));

    g->score = 0;
    g->combo = 1;
    g->run_best_combo = 1;
    g->max_combo = 1 + (int)g->progress.levels[UPG_COMBO];
    g->destroyed = 0;
    g->combo_timer = 0.0f;
    g->spawn_timer = 0.35f;
    g->difficulty_time = 0.0f;
    g->bullet_timer = BULLET_TIMER_BASE_SECONDS;
    g->screen_shake = 0.0f;
    g->flash_timer = 0.0f;
    g->invuln_timer = 0.0f;

    g->max_health = 3 + (int)g->progress.levels[UPG_HP];
    g->health = g->max_health;
    g->player_half = g->progress.levels[UPG_THICC] ? 10.0f : 6.0f;

    g->camera_x = 0.0f;
    g->camera_y = 0.0f;
    g->camera_zoom = CAMERA_ZOOM_STARTING;
    g->highest_y = 118.0f;

    reset_player_to_anchor(g);

    /*
       Center the opening 1.5x view on the player so both random overhead
       grapple choices are easy to read.
    */
    g->camera_x =
        g->player.pos.x - TOP_W * 0.5f * g->camera_zoom;
    g->camera_y =
        g->player.pos.y - TOP_H * 0.5f * g->camera_zoom;

    /*
       Distance is measured from this exact starting point to the player's
       current/final position.
    */
    g->run_start_pos = g->player.pos;
    g->run_distance = 0.0f;
    g->run_path_distance = 0.0f;
    g->last_distance_sample = g->player.pos;

    g->run_xp_earned = 0;
    g->run_cash_earned = 0;
    g->normal_cash_kills = 0;
    g->run_end_xp_bonus = 0;
    g->run_end_cash_bonus = 0;
    g->run_levelups = 0;
    g->last_level_reward = 0;
    g->levelup_message_timer = 0.0f;

    /* Fresh random procedural map every round. */
    g->world_seed =
        map_mix32((uint32_t)osGetTime() ^
                  (uint32_t)rand() ^
                  (uint32_t)(g->frame_counter * 0x9E3779B9u));

    /* Opening anchors spawn above the player with one left and one right. */
    uint32_t opening_rng = map_mix32(g->world_seed ^ 0x51ED270Bu);

    float left_x =
        58.0f + section_rand01(&opening_rng) * 118.0f;
    float right_x =
        224.0f + section_rand01(&opening_rng) * 118.0f;

    float left_y =
        g->player.pos.y -
        (48.0f + section_rand01(&opening_rng) * 72.0f);

    float right_y =
        g->player.pos.y -
        (48.0f + section_rand01(&opening_rng) * 72.0f);

    left_y = clampf_local(left_y, -28.0f, 70.0f);
    right_y = clampf_local(right_y, -28.0f, 70.0f);

    BlockType first_type =
        section_rand01(&opening_rng) < 0.5f
        ? BLOCK_ANCHOR
        : BLOCK_PURPLE;

    BlockType second_type =
        first_type == BLOCK_ANCHOR
        ? BLOCK_PURPLE
        : BLOCK_ANCHOR;

    setup_initial_target(
        &g->blocks[1],
        first_type,
        left_x,
        left_y,
        section_rand_range(&opening_rng, 7.0f, 9.0f),
        first_type == BLOCK_PURPLE ? 100 : 80
    );

    setup_initial_target(
        &g->blocks[2],
        second_type,
        right_x,
        right_y,
        section_rand_range(&opening_rng, 7.0f, 9.0f),
        second_type == BLOCK_PURPLE ? 100 : 80
    );

    /*
       Opening animations are also derived from the random seed so even the
       initial pair does not repeat the exact same phase/speed.
    */
    g->blocks[0].anim_phase =
        section_rand_range(&opening_rng, 0.0f, 6.2831853f);
    g->blocks[1].anim_phase =
        section_rand_range(&opening_rng, 0.0f, 6.2831853f);
    g->blocks[2].anim_phase =
        section_rand_range(&opening_rng, 0.0f, 6.2831853f);

    g->blocks[0].angular_velocity =
        section_rand_range(&opening_rng, 0.85f, 1.45f);
    g->blocks[1].angular_velocity =
        section_rand_range(&opening_rng, 0.85f, 1.45f);
    g->blocks[2].angular_velocity =
        section_rand_range(&opening_rng, 0.85f, 1.45f);

    memset(g->sections, 0, sizeof(g->sections));
    g->section_radius_x = 0;
    g->section_radius_y = 0;
    g->section_cache_ready = false;

    /*
       Pre-generate the zoom-appropriate section cache before the first
       frame. The opening 1.5x view stays compact, then the cache expands only
       as speed zoom requires it.
    */
    refresh_section_cache(g, true);

    g->mode = MODE_PLAYING;
}

static uint32_t current_run_distance(const Game *g) {
    if (!g)
        return 0;

    Vec2 delta =
        vsub(
            g->player.pos,
            g->run_start_pos
        );

    float distance =
        vlen(
            delta
        );

    if (!isfinite(distance) ||
        distance <= 0.0f) {
        return 0;
    }

    if (distance >= 4294967040.0f)
        return 0xFFFFFFFFu;

    return (uint32_t)(distance + 0.5f);
}

/*
   Final performance reward.

   Distance no longer pays repeatedly during movement. At death, final
   start-to-death displacement, block kills, and score are folded into one
   XP/cash bonus. Cash is weighted higher than the corresponding XP.
*/
static uint32_t run_end_xp_base(const Game *g,
                                uint32_t distance) {
    if (!g)
        return 0;

    uint64_t score =
        g->score > 0
        ? (uint64_t)g->score
        : 0ull;

    uint64_t destroyed =
        g->destroyed > 0
        ? (uint64_t)g->destroyed
        : 0ull;

    uint64_t reward =
        (uint64_t)distance / 18ull +
        destroyed * 4ull +
        score / 120ull;

    if (reward > 2500000ull)
        reward = 2500000ull;

    return (uint32_t)reward;
}

static uint32_t run_end_cash_base(const Game *g,
                                  uint32_t distance) {
    if (!g)
        return 0;

    uint64_t score =
        g->score > 0
        ? (uint64_t)g->score
        : 0ull;

    uint64_t destroyed =
        g->destroyed > 0
        ? (uint64_t)g->destroyed
        : 0ull;

    uint64_t reward =
        (uint64_t)distance / 7ull +
        destroyed * 3ull +
        score / 80ull;

    if (reward > 5000000ull)
        reward = 5000000ull;

    return (uint32_t)reward;
}

static void grant_end_of_run_rewards(Game *g) {
    if (!g)
        return;

    uint32_t distance =
        current_run_distance(g);

    uint32_t xp_base =
        run_end_xp_base(
            g,
            distance
        );

    uint32_t cash_base =
        run_end_cash_base(
            g,
            distance
        );

    uint32_t xp_bonus =
        xp_base
        ? randomized_reward(
              xp_base,
              92,
              108
          )
        : 0u;

    uint32_t cash_bonus =
        cash_base
        ? randomized_reward(
              cash_base,
              90,
              115
          )
        : 0u;

    g->run_end_xp_bonus =
        xp_bonus;

    g->run_end_cash_bonus =
        cash_bonus;

    int levelups_before =
        g->run_levelups;

    if (xp_bonus) {
        grant_xp(
            g,
            xp_bonus,
            g->player.pos
        );
    }

    /*
       If the end XP already caused a level-up, its level-up payout already
       chimed. Otherwise the end cash bonus gets one reward chime.
    */
    bool levelup_chimed =
        g->run_levelups >
        levelups_before;

    if (cash_bonus) {
        add_cash(
            g,
            cash_bonus,
            g->player.pos,
            false,
            !levelup_chimed
        );
    }
}

static void commit_run_records(Game *g) {
    if (!g) return;

    if ((uint32_t)g->score > g->progress.high_score)
        g->progress.high_score = (uint32_t)g->score;

    uint32_t distance = current_run_distance(g);
    if (distance > g->progress.best_distance)
        g->progress.best_distance = distance;

    if ((uint32_t)g->run_best_combo >
        g->progress.best_combo) {
        g->progress.best_combo =
            (uint32_t)g->run_best_combo;
    }
}

static void spawn_player_destroy_burst(Game *g) {
    Color pc = player_color(g);
    Color rc = rope_color(g);

    spawn_particles(g, g->player.pos, pc, 30, 180.0f);
    spawn_particles(g, g->player.pos, rc, 14, 145.0f);
    spawn_particles(g, g->player.pos, C_WHITE, 10, 110.0f);

    g->screen_shake = fmaxf(g->screen_shake, 0.36f);
    g->flash_timer = fmaxf(g->flash_timer, 0.16f);
}

static void set_game_over(Game *g) {
    audio_play_sfx(AUDIO_SFX_PLAYER_DIED);
    spawn_player_destroy_burst(g);

    grant_end_of_run_rewards(g);

    if (g->run_path_distance > 0.0f) {
        uint64_t total =
            (uint64_t)g->progress.total_distance_traveled +
            (uint64_t)(g->run_path_distance + 0.5f);

        if (total > 0xFFFFFFFFull)
            total = 0xFFFFFFFFull;

        g->progress.total_distance_traveled = (uint32_t)total;
    }

    commit_run_records(g);
    save_progress(g);
    rope_clear(&g->rope);
    g->gameover_index = 0;
    g->mode = MODE_GAMEOVER;
}

static void damage_player(Game *g, Vec2 source, bool force_respawn) {
    if (g->invuln_timer > 0.0f || g->mode != MODE_PLAYING) return;

    --g->health;
    g->combo = 1;
    g->combo_timer = 0.0f;
    g->screen_shake = 0.22f;
    g->flash_timer = 0.09f;
    g->invuln_timer = 0.95f;
    spawn_particles(g, g->player.pos, player_color(g), 18, 140.0f);
    spawn_particles(g, g->player.pos, C_RED, 8, 105.0f);

    if (g->health <= 0) {
        g->health = 0;
        set_game_over(g);
        return;
    }

    audio_play_sfx(AUDIO_SFX_PLAYER_HIT);

    if (force_respawn) {
        reset_player_to_anchor(g);
        g->invuln_timer = 1.15f;
    } else {
        Vec2 away = vnormalize(vsub(g->player.pos, source));
        if (vlen(away) < 0.01f) away = vec2(0.0f, -1.0f);
        g->player.vel = vadd(g->player.vel, vmul(away, 120.0f));
        if (g->rope.target_block >= 0) rope_clear(&g->rope);
    }
}

/* Normal block cash = $1 base + height bonus. Money Boi uses its own payout. */
static uint32_t normal_kill_height_bonus(const Game *g) {
    if (!g)
        return 0;

    float height =
        g->run_start_pos.y -
        g->player.pos.y;

    if (!isfinite(height) ||
        height <= 0.0f) {
        return 0;
    }

    /*
       +$1 per 500 world pixels climbed, capped so very long runs cannot make
       one ordinary block dominate the economy.
    */
    uint32_t bonus =
        (uint32_t)(
            height /
            500.0f
        );

    if (bonus > 12u)
        bonus = 12u;

    return bonus;
}

static uint32_t normal_kill_base_cash(const Game *g) {
    return 1u +
        normal_kill_height_bonus(g);
}

static uint32_t fifth_kill_bonus(const Game *g) {
    if (!g)
        return 0;

    float speed =
        vlen(
            g->player.vel
        );

    if (!isfinite(speed) ||
        speed < 0.0f) {
        speed = 0.0f;
    }

    float height =
        g->run_start_pos.y -
        g->player.pos.y;

    if (!isfinite(height) ||
        height < 0.0f) {
        height = 0.0f;
    }

    /* Every fifth normal kill adds capped combo, speed, and height bonuses. */
    uint32_t combo_bonus =
        g->combo > 1
        ? (uint32_t)(g->combo - 1)
        : 0u;

    if (combo_bonus > 8u)
        combo_bonus = 8u;

    uint32_t speed_bonus =
        (uint32_t)(
            speed /
            120.0f
        );

    if (speed_bonus > 6u)
        speed_bonus = 6u;

    uint32_t height_bonus =
        (uint32_t)(
            height /
            350.0f
        );

    if (height_bonus > 12u)
        height_bonus = 12u;

    return 1u +
           combo_bonus +
           speed_bonus +
           height_bonus;
}

static void destroy_block(Game *g, int index, bool award) {
    if (index < 0 || index >= MAX_BLOCKS) return;
    Block *b = &g->blocks[index];
    if (!b->active) return;

    BlockType type = b->type;
    Vec2 pos = b->body.pos;
    Color c = block_color(g, type);
    int base = b->value > 0 ? b->value : 80;

    if (g->rope.target_block == index) rope_clear(&g->rope);
    b->active = false;

    /* destroy_block() plays the explosion SFX for every destruction source. */
    audio_play_sfx(AUDIO_SFX_EXPLOSION);

    spawn_particles(g, pos, c, type == BLOCK_RED ? 26 : 22, 155.0f);
    spawn_particles(g, pos, C_WHITE, 8, 105.0f);
    g->screen_shake = fmaxf(g->screen_shake, 0.13f);

    if (!award) return;

    if (g->combo_timer > 0.0f) {
        if (g->combo < g->max_combo) ++g->combo;
    } else {
        g->combo = 1;
    }
    g->combo_timer = 2.35f;

    if (g->combo > g->run_best_combo)
        g->run_best_combo = g->combo;

    int points = base * g->combo;
    g->score += points;
    ++g->destroyed;
    ++g->progress.lifetime_destroyed;
    add_popup(g, pos, points, false, C_YELLOW);

    /* Block destruction awards XP; cash rules are handled per block type. */
    uint32_t block_xp =
        block_xp_reward(
            type,
            g->combo
        );

    block_xp =
        (uint32_t)(
            (float)block_xp *
            cosmetic_block_xp_mult(g) +
            0.5f
        );

    if (type == BLOCK_MONEY) {
        uint32_t money_bonus =
            g->progress.levels[UPG_MONEY]
            ? randomized_reward(
                  260u,
                  80,
                  145
              )
            : randomized_reward(
                  120u,
                  75,
                  140
              );

        add_cash(
            g,
            money_bonus,
            vadd(
                pos,
                vec2(0.0f, 12.0f)
            ),
            true,
            true
        );
    } else {
        ++g->normal_cash_kills;

        /*
           Every normal kill pays at least $1 and is silent.

           Height can increase the base value of each kill.
        */
        uint32_t kill_cash =
            normal_kill_base_cash(g) +
            cosmetic_kill_cash_bonus(g);

        bool fifth =
            (g->normal_cash_kills %
             5u) == 0u;

        if (fifth) {
            /*
               Every fifth kill adds a skill reward based on combo, speed, and
               height. Only these milestone kills play Coin/Coin1.
            */
            kill_cash +=
                fifth_kill_bonus(g);
        }

        add_cash(
            g,
            kill_cash,
            vadd(
                pos,
                vec2(0.0f, 12.0f)
            ),
            true,
            fifth
        );
    }

    grant_xp(
        g,
        block_xp,
        vadd(
            pos,
            vec2(0.0f, -10.0f)
        )
    );

    if (type == BLOCK_GREEN) {
        spawn_green_burst(g, pos);
    }
}

static void update_particles(Game *g, float dt) {
    if (setting_enabled(g, SETTING_PARTICLES)) {
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            Particle *p = &g->particles[i];
            if (!p->active) continue;
            p->life -= dt;
            if (p->life <= 0.0f) { p->active = false; continue; }
            p->vel = vmul(p->vel, clampf_local(1.0f - 1.5f * dt, 0.0f, 1.0f));
            p->pos = vadd(p->pos, vmul(p->vel, dt));
        }
    }

    for (int i = 0; i < MAX_POPUPS; ++i) {
        ScorePopup *p = &g->popups[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.0f) { p->active = false; continue; }
        p->pos.y -= 22.0f * dt;
    }
}

static void update_blocks(Game *g, float dt) {
    /*
       Block animation is purely visual.

       Do NOT run sinf()/cosf() for every cached block every frame.
       draw_world() derives the current phase only for blocks that survive
       screen culling.
    */
    (void)g;
    (void)dt;
}

static Vec2 touch_to_world(const Game *g, const GameInput *in) {
    /*
       A tap is sampled ONCE. Moving the finger after this does not retarget
       the rope and does not apply any additional force.
    */
    float screen_x = (float)in->touch_x * (400.0f / 320.0f);
    float screen_y = (float)in->touch_y;

    screen_x = clampf_local(screen_x, 0.0f, 399.0f);
    screen_y = clampf_local(screen_y, 0.0f, 239.0f);

    return vec2(
        g->camera_x + screen_x * g->camera_zoom,
        g->camera_y + screen_y * g->camera_zoom
    );
}

static int nearest_block_to_point(const Game *g, Vec2 world_point) {
    int best = -1;
    float pick_radius = 125.0f * g->camera_zoom;
    float best_d2 = pick_radius * pick_radius;
    float max_rope = rope_max_length(g);
    float max_rope2 = max_rope * max_rope;

    for (int i = 0; i < MAX_BLOCKS; ++i) {
        const Block *b = &g->blocks[i];
        if (!b->active) continue;
        if (i == 0 && g->starter_platform_active) continue;

        Vec2 d = vsub(b->body.pos, world_point);
        float d2 = vdot(d, d);
        if (d2 >= best_d2) continue;

        Vec2 from_player = vsub(b->body.pos, g->player.pos);
        if (vdot(from_player, from_player) > max_rope2) continue;

        best_d2 = d2;
        best = i;
    }

    return best;
}

static void begin_rope_shot(Game *g, const GameInput *in) {
    Rope *r = &g->rope;
    Vec2 cursor = touch_to_world(g, in);

    rope_clear(r);
    r->active = true;
    r->user_owned = true;
    r->hook_pos = cursor;

    g->aim_world = cursor;
    g->aim_valid = true;

    int hit = nearest_block_to_point(g, cursor);
    if (hit < 0) {
        r->direction = vnormalize(vsub(cursor, g->player.pos));
        return;
    }

    Block *target = &g->blocks[hit];
    r->latched = true;
    r->target_block = hit;
    r->hook_pos = target->body.pos;

    Vec2 delta = vsub(target->body.pos, g->player.pos);
    Vec2 dir = vnormalize(delta);
    r->direction = dir;

    /* Grapple latch adds tangential velocity based on current swing direction. */
    float spin = vcross(delta, g->player.vel);
    float sign = 0.0f;

    if (fabsf(spin) > 0.01f)
        sign = (spin > 0.0f) ? 1.0f : -1.0f;
    else if (fabsf(g->player.vel.x) > 0.5f)
        sign = (g->player.vel.x > 0.0f) ? 1.0f : -1.0f;
    else if (fabsf(g->player.vel.y) > 0.5f)
        sign = (g->player.vel.y > 0.0f) ? 1.0f : -1.0f;
    else
        sign = (dir.x >= 0.0f) ? 1.0f : -1.0f;

    Vec2 tangent = tangent_from_dir(dir, sign);

    /* Grapple launch adds tangential velocity without replacing momentum. */
    g->player.vel = vadd(
        g->player.vel,
        vmul(tangent, grapple_launch_speed(g))
    );

    /* Small one-time inward catch; no continuous reel. */
    g->player.pos = vadd(g->player.pos, vmul(dir, grapple_catch_step(g)));

    r->length = vlen(vsub(target->body.pos, g->player.pos));
    r->desired_length = r->length;

    audio_play_sfx(AUDIO_SFX_GRAPPLE);

    /*
       The floating start block has done its job once the first grapple lands.
       Let it visibly break beneath the player with no score reward.
    */
    if (g->starter_platform_active) {
        g->starter_platform_active = false;
        if (g->blocks[0].active)
            destroy_block(g, 0, false);
    }

    spawn_particles(g, target->body.pos, C_CYAN, 4, 34.0f);
}

static void apply_laser_rope(Game *g) {
    if (!g->progress.levels[UPG_LASER] || !g->rope.active) return;

    Vec2 end = g->rope.hook_pos;
    if (g->rope.latched && g->rope.target_block >= 0 &&
        g->rope.target_block < MAX_BLOCKS && g->blocks[g->rope.target_block].active)
        end = g->blocks[g->rope.target_block].body.pos;

    for (int i = 0; i < MAX_BLOCKS; ++i) {
        if (!g->blocks[i].active || i == g->rope.target_block) continue;
        if (segment_aabb(g->player.pos, end, g->blocks[i].body.pos,
                         g->blocks[i].half + 1.0f, NULL))
            destroy_block(g, i, true);
    }
}

static void update_rope(Game *g, const GameInput *in, float dt) {
    Rope *r = &g->rope;

    if (in->touch_up) {
        rope_clear(r);
        g->aim_valid = false;
        return;
    }

    /*
       Only the DOWN edge chooses the target.
       Dragging the finger while held still does NOT steer or retarget.
    */
    if (in->touch_down)
        begin_rope_shot(g, in);

    if (!r->active || !r->latched) return;

    if (r->target_block < 0 || r->target_block >= MAX_BLOCKS ||
        !g->blocks[r->target_block].active) {
        rope_clear(r);
        return;
    }

    Block *target = &g->blocks[r->target_block];
    r->hook_pos = target->body.pos;

    /* Latched ropes track one fixed anchor; held touch does not retarget. */
    (void)dt;
}


static void apply_grapple_anchor_constraint(Game *g) {
    Rope *r = &g->rope;

    if (!r->active || !r->latched ||
        r->target_block < 0 || r->target_block >= MAX_BLOCKS ||
        !g->blocks[r->target_block].active)
        return;

    Vec2 anchor = g->blocks[r->target_block].body.pos;
    r->hook_pos = anchor;

    float rope_len = r->desired_length;
    if (rope_len < 8.0f)
        rope_len = 8.0f;

    Vec2 delta = vsub(g->player.pos, anchor);
    float dist = vlen(delta);

    /* Zero-length anchor vectors are rebuilt from the last rope direction. */
    if (dist < 0.0001f) {
        Vec2 fallback = vmul(r->direction, -1.0f);
        if (vlen(fallback) < 0.0001f)
            fallback = vec2(0.0f, 1.0f);

        fallback = vnormalize(fallback);
        g->player.pos = vadd(anchor, vmul(fallback, rope_len));
        delta = vsub(g->player.pos, anchor);
        dist = vlen(delta);
    }

    Vec2 n = vmul(delta, 1.0f / dist);

    /* Rope projection preserves tangential velocity and removes radial velocity. */
    g->player.pos = vadd(anchor, vmul(n, rope_len));

    float radial = vdot(g->player.vel, n);
    g->player.vel = vsub(g->player.vel, vmul(n, radial));

}

static void confine_player(Game *g) {
    /*
       The world scrolls freely in X and Y. There are no side walls.
    */
    (void)g;
}

static void update_player_collisions(Game *g) {
    for (int i = 0; i < MAX_BLOCKS; ++i) {
        Block *b = &g->blocks[i];
        if (!b->active) continue;

        /*
           Before the first successful grapple, blocks[0] is a small floating
           platform. It behaves like a floor instead of breaking.
        */
        if (i == 0 && g->starter_platform_active) {
            float platform_top = b->body.pos.y - b->half;
            float px = fabsf(g->player.pos.x - b->body.pos.x);
            float player_bottom = g->player.pos.y + g->player_half;

            if (px <= b->half + g->player_half &&
                player_bottom >= platform_top &&
                player_bottom <= platform_top + 8.0f &&
                g->player.vel.y >= 0.0f) {
                g->player.pos.y = platform_top - g->player_half;
                g->player.vel.y = 0.0f;
                g->player.vel.x *= 0.985f;
            }
            continue;
        }

        if (!phys_aabb_overlap(g->player.pos, g->player_half, g->player_half,
                               b->body.pos, b->half, b->half))
            continue;

        bool hooked = (i == g->rope.target_block);

        /* Red block collision damages HP; grapple destruction remains valid. */
        if (b->type == BLOCK_RED &&
            !hooked &&
            !g->progress.levels[UPG_THICC]) {
            Vec2 p = b->body.pos;
            destroy_block(g, i, false);
            damage_player(g, p, false);
            continue;
        }

        destroy_block(g, i, true);
        g->player.vel.x *= 0.985f;
        g->player.vel.y *= 0.96f;
    }
}

static float bullet_level_factor(const Game *g) {
    /*
       BULLETS level contributes directly to reload performance.

         level 1 -> 1.000x
         level 5 -> 1.300x
         level 9 -> 1.600x
    */
    int level = clampi((int)g->progress.levels[UPG_BULLETS], 1, 9);
    return 1.0f + 0.075f * (float)(level - 1);
}

static float bullet_combo_factor(const Game *g) {
    /*
       Combo represents the skill component.

       Bullet reload benefit caps at combo 6 so a very large +COMBO upgrade
       cannot create runaway firing rates.

         combo 1 -> 1.00x
         combo 2 -> 1.12x
         combo 3 -> 1.24x
         ...
         combo 6+ -> 1.60x
    */
    int combo = clampi(g->combo, 1, 6);
    return 1.0f + 0.12f * (float)(combo - 1);
}

static float bullet_speed_factor(const Game *g) {
    /*
       Only HORIZONTAL velocity matters for the speed bonus because the burst
       itself is horizontally oriented.

       <= 80 vx  -> 1.00x
       >= 420 vx -> 2.00x
    */
    float horizontal_speed = fabsf(g->player.vel.x);

    float t =
        (horizontal_speed - BULLET_SPEED_BONUS_START) /
        (BULLET_SPEED_BONUS_FULL - BULLET_SPEED_BONUS_START);

    t = clampf_local(t, 0.0f, 1.0f);

    /* Smoothstep avoids a sudden timer-rate jump around the threshold. */
    t = t * t * (3.0f - 2.0f * t);

    return 1.0f + t;
}

static float bullet_timer_rate(const Game *g) {
    /*
       Final timer-speed formula:

         rate =
             level_factor
           * combo_factor
           * speed_factor

       capped at 5x.

       The effective interval if performance stayed constant would be:

         interval ~= 20 / rate seconds

       Examples:
         L1, combo1, low speed  -> ~20.0 s
         L5, combo3, ~250 vx   -> ~8-10 s
         L9, combo6, 420+ vx   -> ~4.0 s
    */
    float rate =
        bullet_level_factor(g) *
        bullet_combo_factor(g) *
        bullet_speed_factor(g) *
        cosmetic_bullet_timer_mult(g);

    return clampf_local(rate, 1.0f, BULLET_TIMER_MAX_RATE);
}


static void fire_horizontal_bullet_spray(Game *g) {
    float horizontal_speed = fabsf(g->player.vel.x);

    if (horizontal_speed < BULLET_FIRE_MIN_HORIZONTAL_SPEED)
        return;

    /*
       "Backwards" is supported naturally:

         player vx > 0 -> spray goes right
         player vx < 0 -> spray goes left

       Vertical velocity does NOT rotate the central firing direction.
       The burst is always a horizontal fan relative to current X movement.
    */
    float sign = g->player.vel.x >= 0.0f ? 1.0f : -1.0f;
    Vec2 forward = vec2(sign, 0.0f);

    float bullet_speed =
        fmaxf(280.0f, horizontal_speed + 150.0f);

    if (bullet_speed > 850.0f)
        bullet_speed = 850.0f;

    int spray_count =
        cosmetic_bullet_count(g);

    if (spray_count < 2)
        spray_count = 2;

    for (int i = 0; i < spray_count; ++i) {
        float t =
            (float)i /
            (float)(spray_count - 1);

        float degrees =
            -BULLET_SPRAY_HALF_ANGLE_DEG +
            (BULLET_SPRAY_HALF_ANGLE_DEG * 2.0f) * t;

        float radians =
            degrees * (3.14159265358979323846f / 180.0f);

        /*
           Mirroring the X component makes the exact same nine-angle fan work
           both forward/right and backward/left.
        */
        Vec2 dir = vec2(
            sign * cosf(radians),
            sinf(radians)
        );

        float vertical_offset =
            ((float)i -
             ((float)spray_count - 1.0f) * 0.5f) *
            0.75f;

        Vec2 pos = vadd(
            g->player.pos,
            vadd(
                vmul(forward, g->player_half + 5.0f),
                vec2(0.0f, vertical_offset)
            )
        );

        spawn_bullet(
            g,
            pos,
            dir,
            player_color(g),
            bullet_speed
        );
    }

    spawn_particles(
        g,
        vadd(g->player.pos, vmul(forward, g->player_half + 5.0f)),
        player_color(g),
        16,
        72.0f
    );
}

static void update_auto_bullets(Game *g, float dt) {
    int level =
        clampi((int)g->progress.levels[UPG_BULLETS], 0, 9);

    if (level <= 0) {
        g->bullet_timer = BULLET_TIMER_BASE_SECONDS;
        return;
    }

    /*
       Good play continuously accelerates the timer. If speed/combo drops
       during the cooldown, it immediately slows back down.
    */
    float rate = bullet_timer_rate(g);
    g->bullet_timer -= dt * rate;

    if (g->bullet_timer > 0.0f)
        return;

    /* Completed bullet timers wait for nonzero horizontal velocity. */
    if (fabsf(g->player.vel.x) < BULLET_FIRE_MIN_HORIZONTAL_SPEED) {
        g->bullet_timer = 0.0f;
        return;
    }

    fire_horizontal_bullet_spray(g);
    g->bullet_timer = BULLET_TIMER_BASE_SECONDS;
}

static void update_bullets(Game *g, float dt) {
    for (int i = 0; i < MAX_BULLETS; ++i) {
        Bullet *b = &g->bullets[i];
        if (!b->active) continue;

        Vec2 old = b->pos;
        b->pos = vadd(b->pos, vmul(b->vel, dt));
        b->life -= dt;
        float screen_x = world_to_screen_x(g, b->pos.x);
        float screen_y = world_to_screen_y(g, b->pos.y);
        if (b->life <= 0.0f ||
            screen_x < -30.0f || screen_x > 430.0f ||
            screen_y < -30.0f || screen_y > 270.0f) {
            b->active = false;
            continue;
        }

        for (int j = 1; j < MAX_BLOCKS; ++j) {
            Block *target = &g->blocks[j];
            if (!target->active) continue;
            if (segment_aabb(
                    old,
                    b->pos,
                    target->body.pos,
                    target->half +
                        1.0f +
                        cosmetic_bullet_hit_bonus(g),
                    NULL)) {
                destroy_block(g, j, true);
                b->active = false;
                break;
            }
        }
    }
}

static void update_camera(Game *g, float dt) {
    /*
       Speed-controlled zoom-out.

       Zoom changes around the CURRENT center of the camera so the world
       doesn't appear to jump toward one corner when view scale changes.
    */
    float speed = vlen(g->player.vel);
    float target_zoom = camera_zoom_for_speed(speed);

    /* Starter-platform camera zoom floor: 1.5x. */
    if (g->starter_platform_active &&
        target_zoom < CAMERA_ZOOM_STARTING) {
        target_zoom = CAMERA_ZOOM_STARTING;
    }

    float old_zoom = g->camera_zoom;
    float old_center_x = g->camera_x + TOP_W * 0.5f * old_zoom;
    float old_center_y = g->camera_y + TOP_H * 0.5f * old_zoom;

    float zoom_rate =
        target_zoom > old_zoom
        ? CAMERA_ZOOM_OUT_RATE
        : CAMERA_ZOOM_IN_RATE;

    float zt = clampf_local(zoom_rate * dt, 0.0f, 1.0f);
    g->camera_zoom += (target_zoom - g->camera_zoom) * zt;
    g->camera_zoom =
        clampf_local(g->camera_zoom, CAMERA_ZOOM_MIN, CAMERA_ZOOM_MAX);

    /*
       Preserve camera center while changing zoom.
    */
    g->camera_x = old_center_x - TOP_W * 0.5f * g->camera_zoom;
    g->camera_y = old_center_y - TOP_H * 0.5f * g->camera_zoom;

    /* Camera dead zone is measured in screen pixels. */
    float screen_x = world_to_screen_x(g, g->player.pos.x);
    float screen_y = world_to_screen_y(g, g->player.pos.y);

    float desired_x = g->camera_x;
    float desired_y = g->camera_y;

    if (screen_x < 118.0f)
        desired_x = g->player.pos.x - 118.0f * g->camera_zoom;
    if (screen_x > 282.0f)
        desired_x = g->player.pos.x - 282.0f * g->camera_zoom;

    if (screen_y < 82.0f)
        desired_y = g->player.pos.y - 82.0f * g->camera_zoom;
    if (screen_y > 158.0f)
        desired_y = g->player.pos.y - 158.0f * g->camera_zoom;

    /*
       Keep the starting area's upper-left limit while at normal zoom.
       When zoomed out, allow enough negative camera Y to keep the same
       center rather than forcing the screen to lurch downward.
    */
    float max_camera_y =
        -TOP_H * 0.5f * (g->camera_zoom - CAMERA_ZOOM_MIN);
    if (desired_y > max_camera_y)
        desired_y = max_camera_y;

    float tx = clampf_local(8.5f * dt, 0.0f, 1.0f);
    float ty = clampf_local(6.5f * dt, 0.0f, 1.0f);

    g->camera_x += (desired_x - g->camera_x) * tx;
    g->camera_y += (desired_y - g->camera_y) * ty;

    if (g->player.pos.y < g->highest_y)
        g->highest_y = g->player.pos.y;
}

static void update_playing(Game *g, const GameInput *in, float dt) {
    if (in->keys_down & KEY_START) {
        g->pause_index = 0;
        g->mode = MODE_PAUSED;
        return;
    }

    g->difficulty_time += dt;
    if (g->invuln_timer > 0.0f) g->invuln_timer -= dt;

    if (g->combo_timer > 0.0f) {
        g->combo_timer -= dt / shape_combo_timer_mult(g);
        if (g->combo_timer <= 0.0f) {
            g->combo_timer = 0.0f;
            g->combo = 1;
        }
    }

    if (g->screen_shake > 0.0f) g->screen_shake -= dt;
    if (g->flash_timer > 0.0f) g->flash_timer -= dt;
    if (g->levelup_message_timer > 0.0f)
        g->levelup_message_timer -= dt;

    update_blocks(g, dt);

    /* Motion order: gravity/drag integration, then optional rope constraint. */
    update_rope(g, in, dt);

    bool grappled =
        g->rope.active &&
        g->rope.latched &&
        g->rope.target_block >= 0 &&
        g->rope.target_block < MAX_BLOCKS &&
        g->blocks[g->rope.target_block].active;

    /* Player velocity has no hard cap; damping is shape-dependent. */
    g->player.damping =
        PLAYER_AIR_DRAG *
        shape_air_drag_mult(g);

    phys_integrate(
        &g->player,
        vec2(0.0f, PLAYER_GRAVITY),
        dt
    );

    if (grappled)
        apply_grapple_anchor_constraint(g);

    confine_player(g);

    {
        Vec2 step_delta =
            vsub(
                g->player.pos,
                g->last_distance_sample
            );

        float step_distance =
            vlen(
                step_delta
            );

        if (isfinite(step_distance) &&
            step_distance >= 0.0f &&
            step_distance < 800.0f) {
            g->run_path_distance += step_distance;
        }

        g->last_distance_sample = g->player.pos;
    }

    /*
       Live distance is current straight-line displacement from run_start_pos.
       It pays nothing during the run; progression is settled once at death.
    */
    {
        Vec2 distance_delta =
            vsub(
                g->player.pos,
                g->run_start_pos
            );

        float displacement =
            vlen(
                distance_delta
            );

        if (isfinite(displacement) &&
            displacement >= 0.0f) {
            g->run_distance =
                displacement;
        }
    }

    update_camera(g, dt);

    /* Squash/stretch affects rendering only; collision remains square. */
    float player_speed = vlen(g->player.vel);

    if (isfinite(player_speed) &&
        player_speed > 0.0f) {
        uint32_t speed_record =
            (uint32_t)(
                player_speed +
                0.5f
            );

        if (speed_record >
            g->progress.best_speed) {
            g->progress.best_speed =
                speed_record;
        }
    }

    float stretch_target = clampf_local((player_speed - 35.0f) / 285.0f,
                                        0.0f, 1.0f);
    float stretch_lerp = clampf_local(10.0f * dt, 0.0f, 1.0f);
    g->player_stretch += (stretch_target - g->player_stretch) * stretch_lerp;

    update_player_visual_angle(
        g,
        dt
    );

    apply_laser_rope(g);
    update_player_collisions(g);
    if (g->mode != MODE_PLAYING) return;

    /*
       One run = one fall chance. Falling into the lava ends the round
       immediately regardless of remaining hearts. Hearts are exclusively for
       dangerous-block damage.
    */
    if (g->player.pos.y + g->player_half >= LAVA_Y) {
        spawn_particles(g, g->player.pos, lava_color(g), 14, 125.0f);
        set_game_over(g);
        return;
    }

    update_auto_bullets(g, dt);
    update_bullets(g, dt);

    /*
       Section streaming is tied to camera section boundaries, not a frame
       timer or player speed. Newly-created sections are several screens away.
    */
    refresh_section_cache(g, false);
}

static void shop_message(Game *g, const char *msg) {
    strncpy(g->shop_message, msg, sizeof(g->shop_message) - 1);
    g->shop_message[sizeof(g->shop_message) - 1] = '\0';
    g->shop_message_timer = 1.4f;
}

static void buy_selected_upgrade(Game *g) {
    int i = g->shop_index;
    int level = g->progress.levels[i];
    if (level >= SHOP[i].max_level) {
        shop_message(g, "SOLD OUT");
        return;
    }

    uint32_t cost = shop_cost(g, i);
    if (g->progress.money < cost) {
        shop_message(g, "NOT ENOUGH MONEY");
        return;
    }

    g->progress.money -= cost;
    ++g->progress.levels[i];

    audio_play_purchase();

    save_progress(g);
    shop_message(g, "PURCHASED");
}

static void equip_cosmetic(Game *g, int item) {
    if (!g ||
        item < 0 ||
        item >= COSMETIC_COUNT) {
        return;
    }

    const CosmeticDef *d =
        &COSMETICS[item];

    switch (d->kind) {
        case COS_PLAYER_COLOR:
            g->progress.player_style =
                d->style;
            break;

        case COS_ROPE_COLOR:
            g->progress.rope_style =
                d->style;
            break;

        case COS_ROPE_ANIM:
            g->progress.pattern_style =
                d->style;
            break;

        case COS_SHAPE:
            g->progress.shape_style =
                d->style;
            break;

        case COS_PLAYER_ANIM:
            g->progress.player_anim_style =
                d->style;
            break;

        case COS_HAT:
            g->progress.hat_style =
                d->style;
            break;

        case COS_BLOCK_THEME:
            g->progress.block_theme =
                d->style;
            break;

        case COS_BACKGROUND:
            g->progress.background_style =
                d->style;
            break;

        case COS_LAVA_COLOR:
            g->progress.lava_color_style =
                d->style;
            break;

        case COS_LAVA_ANIM:
            g->progress.lava_anim_style =
                d->style;
            break;

        case COS_TITLE_THEME:
            g->progress.title_style =
                d->style;
            break;

        case COS_UI_THEME:
            g->progress.reserved_style1 =
                d->style;
            break;

        default:
            break;
    }
}

static int shop_page_item_count(const Game *g) {
    if (!g)
        return 0;

    if (g->shop_page == 0)
        return SHOP_ITEM_COUNT;

    int page =
        g->shop_page - 1;

    if (page < 0 ||
        page >= SHOP_PAGE_COUNT - 1) {
        return 0;
    }

    return
        COSMETIC_PAGES[page].count;
}

static int current_cosmetic_item(const Game *g) {
    if (!g ||
        g->shop_page <= 0 ||
        g->shop_page >= SHOP_PAGE_COUNT) {
        return -1;
    }

    const CosmeticPage *page =
        &COSMETIC_PAGES[
            g->shop_page - 1
        ];

    if (g->shop_index < 0 ||
        g->shop_index >= page->count) {
        return -1;
    }

    return
        page->items[
            g->shop_index
        ];
}

static void activate_selected_cosmetic(Game *g) {
    int item =
        current_cosmetic_item(g);

    if (item < 0 ||
        item >= COSMETIC_COUNT) {
        return;
    }

    const CosmeticDef *d =
        &COSMETICS[item];

    if (!cosmetic_owned(g, item)) {
        if (g->progress.money <
            d->cost) {
            shop_message(
                g,
                "NOT ENOUGH MONEY"
            );
            return;
        }

        g->progress.money -=
            d->cost;

        cosmetic_set_owned(
            g,
            item
        );

        equip_cosmetic(
            g,
            item
        );

        audio_play_purchase();
        save_progress(g);

        shop_message(
            g,
            "PURCHASED + EQUIPPED"
        );
        return;
    }

    equip_cosmetic(
        g,
        item
    );

    audio_play_purchase();
    save_progress(g);

    shop_message(
        g,
        "EQUIPPED"
    );
}


static void update_title(Game *g, const GameInput *in, float dt) {
    g->title_select_anim +=
        dt * 7.5f;

    if (g->title_select_anim > 1.0f)
        g->title_select_anim = 1.0f;
    if (in->nav_y != 0) {
        g->title_index +=
            in->nav_y;

        if (g->title_index < 0)
            g->title_index = 5;

        if (g->title_index > 5)
            g->title_index = 0;

        g->title_select_anim = 0.0f;
        audio_play_menu_move();
    }

    if (!(in->keys_down & KEY_A))
        return;

    switch (g->title_index) {
        case 0:
            start_run(g);
            break;

        case 1:
            g->shop_return_mode =
                MODE_TITLE;

            g->shop_page = 0;
            g->shop_index = 0;
            g->shop_select_anim = 0.0f;
            g->shop_touch_pending = -1;
            g->mode = MODE_SHOP;
            break;

        case 2:
            g->missions_index = 0;
            g->mode = MODE_MISSIONS;
            break;

        case 3:
            g->achievements_index = 0;
            g->mode = MODE_ACHIEVEMENTS;
            break;

        case 4:
            g->settings_page = 0;
            g->settings_index = 0;
            g->mode = MODE_SETTINGS;
            break;

        case 5:
        default:
            g->request_exit = true;
            break;
    }
}

static void settings_save_and_feedback(Game *g) {
    if (!g)
        return;

    apply_runtime_settings(g);
    save_progress(g);
    audio_play_purchase();
}

static void settings_set_flag(Game *g,
                              uint8_t flag,
                              bool enabled) {
    if (!g)
        return;

    bool was_on =
        setting_enabled(
            g,
            flag
        );

    if (was_on == enabled)
        return;

    /* SFX OFF confirms before muting; SFX ON confirms after enabling. */
    if (flag == SETTING_SFX &&
        was_on &&
        !enabled) {
        audio_play_purchase();
    }

    set_setting_flag(
        g,
        flag,
        enabled
    );

    apply_runtime_settings(g);
    save_progress(g);

    if (!(flag == SETTING_SFX &&
          was_on &&
          !enabled)) {
        audio_play_purchase();
    }
}

static void settings_change_flag(Game *g,
                                 uint8_t flag,
                                 int horizontal,
                                 bool activate) {
    bool current =
        setting_enabled(
            g,
            flag
        );

    bool desired =
        current;

    if (activate)
        desired = !current;
    else if (horizontal < 0)
        desired = false;
    else if (horizontal > 0)
        desired = true;

    settings_set_flag(
        g,
        flag,
        desired
    );
}

static int settings_items_on_page(int page) {
    return page == 0 ? 5 : 4;
}

static void settings_switch_page(Game *g, int direction) {
    if (!g ||
        direction == 0) {
        return;
    }

    g->settings_page +=
        direction;

    if (g->settings_page < 0)
        g->settings_page = 1;

    if (g->settings_page > 1)
        g->settings_page = 0;

    if (g->settings_index < 0)
        g->settings_index = 0;

    int max_index =
        settings_items_on_page(g->settings_page) - 1;

    if (g->settings_index > max_index)
        g->settings_index = max_index;

    audio_play_menu_move();
}

static void update_settings(Game *g, const GameInput *in) {
    if (!g || !in)
        return;

    if (in->keys_down & KEY_L)
        settings_switch_page(g, -1);

    if (in->keys_down & KEY_R)
        settings_switch_page(g, 1);

    if (in->nav_y != 0) {
        int count =
            settings_items_on_page(g->settings_page);

        g->settings_index +=
            in->nav_y;

        if (g->settings_index < 0)
            g->settings_index = count - 1;

        if (g->settings_index >= count)
            g->settings_index = 0;

        audio_play_menu_move();
    }

    if ((in->keys_down & KEY_B) ||
        (in->keys_down & KEY_START)) {
        g->mode = MODE_TITLE;
        return;
    }

    int horizontal =
        in->nav_x;

    bool activate =
        (in->keys_down &
         KEY_A) != 0;

    if (horizontal == 0 &&
        !activate) {
        return;
    }

    if (g->settings_page == 0) {
        switch (g->settings_index) {
            case 0:
                settings_change_flag(
                    g,
                    SETTING_SFX,
                    horizontal,
                    activate
                );
                break;

            case 1:
                settings_change_flag(
                    g,
                    SETTING_MUSIC,
                    horizontal,
                    activate
                );
                break;

            case 2: {
                int dir =
                    horizontal != 0
                    ? horizontal
                    : 1;

                int level =
                    (int)g->progress.bloom_level +
                    dir;

                level =
                    clampi(
                        level,
                        0,
                        (int)SETTINGS_BLOOM_MAX
                    );

                if (level !=
                    (int)g->progress.bloom_level) {
                    g->progress.bloom_level =
                        (uint8_t)level;

                    settings_save_and_feedback(g);
                }
                break;
            }

            case 3: {
                int dir =
                    horizontal != 0
                    ? horizontal
                    : 1;

                int lod =
                    (int)g->progress.force_lod +
                    dir;

                if (lod < 0)
                    lod =
                        SETTINGS_FORCE_LOD_MAX;

                if (lod >
                    (int)SETTINGS_FORCE_LOD_MAX) {
                    lod = 0;
                }

                if (lod !=
                    (int)g->progress.force_lod) {
                    g->progress.force_lod =
                        (uint8_t)lod;

                    settings_save_and_feedback(g);
                }
                break;
            }

            case 4:
                settings_change_flag(
                    g,
                    SETTING_STEREO_3D,
                    horizontal,
                    activate
                );
                break;

            default:
                break;
        }
    } else {
        switch (g->settings_index) {
            case 0:
                settings_change_flag(
                    g,
                    SETTING_BLOCK_ANIM,
                    horizontal,
                    activate
                );
                break;

            case 1:
                settings_change_flag(
                    g,
                    SETTING_LAVA_ANIM,
                    horizontal,
                    activate
                );
                break;

            case 2: {
                bool current =
                    setting_enabled(
                        g,
                        SETTING_PARTICLES
                    );

                bool enable =
                    current;

                if (activate)
                    enable = !current;
                else if (horizontal < 0)
                    enable = false;
                else if (horizontal > 0)
                    enable = true;

                if (enable != current) {
                    settings_set_flag(
                        g,
                        SETTING_PARTICLES,
                        enable
                    );

                    if (!enable) {
                        memset(
                            g->particles,
                            0,
                            sizeof(g->particles)
                        );
                    }
                }
                break;
            }

            case 3:
                settings_change_flag(
                    g,
                    SETTING_SCREENSHAKE,
                    horizontal,
                    activate
                );
                break;

            default:
                break;
        }
    }
}

static void update_pause(Game *g, const GameInput *in) {
    if ((in->keys_down & KEY_START) ||
        (in->keys_down & KEY_B)) {
        g->mode = MODE_PLAYING;
        return;
    }

    if (in->nav_y != 0) {
        g->pause_index +=
            in->nav_y;

        if (g->pause_index < 0)
            g->pause_index = 2;

        if (g->pause_index > 2)
            g->pause_index = 0;

        audio_play_menu_move();
    }

    if (in->keys_down & KEY_A) {
        if (g->pause_index == 0) {
            g->mode = MODE_PLAYING;
        } else if (g->pause_index == 1) {
            commit_run_records(g);
            save_progress(g);
            start_run(g);
        } else {
            commit_run_records(g);
            save_progress(g);
            g->mode = MODE_TITLE;
        }
    }
}

static void update_gameover(Game *g, const GameInput *in) {
    if (in->nav_y != 0) {
        g->gameover_index +=
            in->nav_y;

        if (g->gameover_index < 0)
            g->gameover_index = 2;

        if (g->gameover_index > 2)
            g->gameover_index = 0;

        audio_play_menu_move();
    }

    if (in->keys_down & KEY_A) {
        if (g->gameover_index == 0) {
            start_run(g);
        } else if (g->gameover_index == 1) {
            g->shop_return_mode =
                MODE_GAMEOVER;

            g->shop_page = 0;
            g->shop_index = 0;
            g->shop_select_anim = 0.0f;
            g->shop_touch_pending = -1;
            g->mode = MODE_SHOP;
        } else {
            g->mode = MODE_TITLE;
        }
    }

    if (in->keys_down & KEY_B)
        g->mode = MODE_TITLE;
}

static void update_missions(Game *g, const GameInput *in) {
    if (!g || !in)
        return;

    if (in->nav_y != 0) {
        g->missions_index +=
            in->nav_y;

        if (g->missions_index < 0)
            g->missions_index =
                MISSION_SLOT_COUNT - 1;

        if (g->missions_index >= MISSION_SLOT_COUNT)
            g->missions_index = 0;

        audio_play_menu_move();
    }

    if ((in->keys_down & KEY_B) ||
        (in->keys_down & KEY_START)) {
        g->mode = MODE_TITLE;
    }
}

static void update_achievements(Game *g, const GameInput *in) {
    if (!g || !in)
        return;

    if (in->nav_y != 0) {
        g->achievements_index +=
            in->nav_y;

        if (g->achievements_index < 0)
            g->achievements_index =
                ACHIEVEMENT_COUNT - 1;

        if (g->achievements_index >=
            ACHIEVEMENT_COUNT) {
            g->achievements_index = 0;
        }

        audio_play_menu_move();
    }

    if ((in->keys_down & KEY_B) ||
        (in->keys_down & KEY_START) ||
        (in->keys_down & KEY_A)) {
        g->mode = MODE_TITLE;
    }
}

static void shop_change_page(Game *g, int direction) {
    if (!g ||
        direction == 0) {
        return;
    }

    g->shop_page +=
        direction;

    if (g->shop_page < 0)
        g->shop_page =
            SHOP_PAGE_COUNT - 1;

    if (g->shop_page >=
        SHOP_PAGE_COUNT) {
        g->shop_page = 0;
    }

    g->shop_index = 0;
    g->shop_select_anim = 0.0f;

    audio_play_menu_move();
}

static void update_shop(Game *g, const GameInput *in, float dt) {
    if (!g || !in)
        return;

    if (g->shop_message_timer > 0.0f)
        g->shop_message_timer -= dt;

    g->shop_select_anim +=
        dt * 5.5f;

    if (g->shop_select_anim > 1.0f)
        g->shop_select_anim = 1.0f;

    /* L/R changes shop category pages. */
    if (in->keys_down & KEY_L) {
        shop_change_page(g, -1);
    } else if (in->keys_down & KEY_R) {
        shop_change_page(g, 1);
    } else {
        int count =
            shop_page_item_count(g);

        if (count <= 0)
            count = 1;

        int columns =
            g->shop_page == 0
            ? 2
            : 3;

        bool moved = false;

        if (in->nav_x != 0) {
            int col =
                g->shop_index %
                columns;

            int row =
                g->shop_index /
                columns;

            int new_col =
                col +
                in->nav_x;

            if (new_col < 0) {
                shop_change_page(g, -1);
            } else if (new_col >= columns) {
                shop_change_page(g, 1);
            } else {
                int next =
                    row * columns +
                    new_col;

                if (next >= 0 &&
                    next < count) {
                    g->shop_index =
                        next;

                    moved = true;
                }
            }
        }

        if (in->nav_y != 0) {
            int next =
                g->shop_index +
                in->nav_y *
                columns;

            if (next >= 0 &&
                next < count) {
                g->shop_index =
                    next;

                moved = true;
            }
        }

        if (moved) {
            g->shop_select_anim = 0.0f;
            audio_play_menu_move();
        }
    }

    if (in->keys_down & KEY_A) {
        if (g->shop_page == 0)
            buy_selected_upgrade(g);
        else
            activate_selected_cosmetic(g);
    }

    if ((in->keys_down & KEY_B) ||
        (in->keys_down & KEY_START)) {
        g->mode =
            g->shop_return_mode;
    }
}

void game_init(Game *g) {
    memset(g, 0, sizeof(*g));
    load_progress(g);
    apply_runtime_settings(g);

    for (int i = 0; i < MAX_STARS; ++i) {
        g->stars[i].x = rand() % 400;
        g->stars[i].y = rand() % 218;
        g->stars[i].brightness = 1 + rand() % 3;
    }

    g->mode = MODE_TITLE;
    g->shop_return_mode = MODE_TITLE;
    g->title_index = 0;
    g->title_select_anim = 1.0f;

    /* Menu camera zoom initializes to 1.0x. */
    g->camera_zoom = CAMERA_ZOOM_MIN;
    g->camera_x = 0.0f;
    g->camera_y = 0.0f;

    g->settings_page = 0;
    g->settings_index = 0;

    g->shop_page = 0;
    g->shop_index = 0;
    g->shop_select_anim = 1.0f;
    g->shop_touch_pending = -1;

    g->missions_index = 0;
    g->achievements_index = 0;
    generate_all_missions(g);
    g->mission_notice_timer = 0.0f;
    g->mission_notice[0] = '\0';
    g->run_best_combo = 1;

    rope_clear(&g->rope);
}

void game_shutdown(Game *g) {
    if (!g) return;
    commit_run_records(g);
    save_progress(g);
}

void game_update(Game *g, const GameInput *in, float dt) {
    ++g->frame_counter;
    update_particles(g, dt);

    if (g->mission_notice_timer > 0.0f) {
        g->mission_notice_timer -= dt;
        if (g->mission_notice_timer < 0.0f)
            g->mission_notice_timer = 0.0f;
    }

    switch (g->mode) {
        case MODE_TITLE: update_title(g, in, dt); break;
        case MODE_PLAYING: update_playing(g, in, dt); break;
        case MODE_PAUSED: update_pause(g, in); break;
        case MODE_SHOP: update_shop(g, in, dt); break;
        case MODE_SETTINGS: update_settings(g, in); break;
        case MODE_MISSIONS: update_missions(g, in); break;
        case MODE_ACHIEVEMENTS: update_achievements(g, in); break;
        case MODE_GAMEOVER: update_gameover(g, in); break;
        default: g->mode = MODE_TITLE; break;
    }

    update_auto_missions(g);
}

static void draw_heart(Surface *s, int x, int y, Color c) {
    draw_rect(s, x, y + 2, 3, 4, c);
    draw_rect(s, x + 5, y + 2, 3, 4, c);
    draw_rect(s, x + 1, y, 6, 7, c);
    draw_rect(s, x + 2, y + 6, 4, 3, c);
}

static void draw_hearts(const Game *g, Surface *s) {
    int shown = g->max_health;
    if (shown > 12) shown = 12;
    for (int i = 0; i < shown; ++i) {
        Color c = i < g->health ? C_RED : rgb(58, 16, 18);
        draw_heart(s, 14 + i * 13, 14, c);
    }
}

static void draw_filled_circle_cheap(Surface *s,
                                     int cx,
                                     int cy,
                                     int radius,
                                     Color c) {
    if (!s ||
        radius <= 0) {
        return;
    }

    int rr =
        radius *
        radius;

    for (int y = -radius;
         y <= radius;
         ++y) {
        int inside =
            rr -
            y * y;

        if (inside < 0)
            continue;

        int half =
            (int)sqrtf(
                (float)inside
            );

        draw_rect(
            s,
            cx - half,
            cy + y,
            half * 2 + 1,
            1,
            c
        );
    }
}

static void draw_cloud_cluster(Surface *s,
                               int cx,
                               int cy,
                               int scale,
                               Color c) {
    if (!s)
        return;

    if (scale < 1)
        scale = 1;

    draw_filled_circle_cheap(s, cx - 10 * scale, cy + 2 * scale, 6 * scale, c);
    draw_filled_circle_cheap(s, cx, cy - 2 * scale, 8 * scale, c);
    draw_filled_circle_cheap(s, cx + 11 * scale, cy + 2 * scale, 6 * scale, c);

    draw_rect(
        s,
        cx - 15 * scale,
        cy + 1 * scale,
        31 * scale,
        7 * scale,
        c
    );
}

static void draw_city_skyline_layer(const Game *g,
                                    Surface *s,
                                    int lod,
                                    bool title_scene) {
    if (!g || !s)
        return;

    int scroll =
        title_scene
        ? 0
        : ((int)(
              -g->camera_x *
              0.035f /
              g->camera_zoom
          )) % 52;

    if (scroll < 0)
        scroll += 52;

    Color building = rgb(4, 9, 22);
    Color edge = rgb(9, 28, 48);
    Color window_a = rgb(28, 150, 210);
    Color window_b = rgb(235, 188, 52);

    int base_y =
        title_scene
        ? 236
        : 218;

    for (int i = -1;
         i < 9;
         ++i) {
        int x =
            i * 52 +
            scroll;

        int seed =
            i + 17;

        int w =
            30 +
            ((seed * 13) & 15);

        int h =
            46 +
            ((seed * 29) & 63);

        /* Center skyline height is reduced behind menu text. */
        if (title_scene &&
            x > 100 &&
            x < 270) {
            h =
                20 +
                ((seed * 7) & 15);
        }

        int y =
            base_y -
            h;

        draw_rect(s, x, y, w, h, building);
        draw_rect_outline(s, x, y, w, h, edge);

        if (lod < 2) {
            int step_x =
                lod == 0
                ? 9
                : 13;

            int step_y =
                lod == 0
                ? 11
                : 16;

            for (int wy = y + 8;
                 wy < base_y - 5;
                 wy += step_y) {
                for (int wx = x + 6;
                     wx < x + w - 5;
                     wx += step_x) {
                    unsigned hash =
                        (unsigned)(
                            wx * 17 +
                            wy * 31 +
                            i * 13
                        );

                    if ((hash & 3u) == 0u)
                        continue;

                    draw_rect(
                        s,
                        wx,
                        wy,
                        2,
                        3,
                        (hash & 4u)
                        ? window_a
                        : window_b
                    );
                }
            }
        }

        if (((unsigned)seed & 1u) == 0u) {
            int ax =
                x +
                w / 2;

            draw_line(s, ax, y, ax, y - 9, edge);
            draw_pixel(s, ax, y - 10, C_CYAN);
        }
    }

    /*
       Landmark tower: oversized antenna and alternating neon windows.
    */
    int tower_x =
        title_scene
        ? 330
        : 334 + scroll / 3;

    int tower_base =
        base_y;

    draw_rect(
        s,
        tower_x,
        tower_base - 128,
        28,
        128,
        rgb(3, 8, 20)
    );

    draw_rect_outline(
        s,
        tower_x,
        tower_base - 128,
        28,
        128,
        rgb(16, 54, 78)
    );

    draw_line(
        s,
        tower_x + 14,
        tower_base - 128,
        tower_x + 14,
        tower_base - 158,
        ui_accent_color(g)
    );

    if (lod < 2) {
        for (int y = tower_base - 114;
             y < tower_base - 8;
             y += 13) {
            draw_rect(
                s,
                tower_x + 7,
                y,
                4,
                4,
                C_YELLOW
            );

            draw_rect(
                s,
                tower_x + 17,
                y + 4,
                4,
                4,
                C_CYAN
            );
        }
    }
}

static bool storm_flash_active(const Game *g) {
    if (!g)
        return false;

    unsigned phase =
        g->frame_counter %
        420u;

    return
        phase == 31u ||
        phase == 32u ||
        phase == 38u ||
        phase == 39u;
}

static void draw_storm_layer(const Game *g,
                             Surface *s,
                             int lod,
                             bool title_scene) {
    if (!g || !s)
        return;

    float drift =
        title_scene
        ? (float)(g->frame_counter % 800u) * 0.05f
        : -g->camera_x * 0.018f +
          (float)(g->frame_counter % 1200u) * 0.03f;

    Color cloud = rgb(18, 23, 33);
    Color cloud_hi = rgb(27, 34, 46);

    for (int i = -1;
         i < 6;
         ++i) {
        int x =
            (int)(
                (float)(i * 92) +
                drift
            ) % 520;

        if (x < -80)
            x += 520;

        int y =
            7 +
            ((i * 19) & 15);

        draw_cloud_cluster(
            s,
            x,
            y,
            1,
            (i & 1)
            ? cloud
            : cloud_hi
        );
    }

    if (lod < 2) {
        int rain_step =
            lod == 0
            ? 17
            : 29;

        int rain_phase =
            (int)(
                g->frame_counter *
                3u
            ) %
            rain_step;

        for (int x = 5;
             x < 400;
             x += rain_step) {
            int y =
                42 +
                ((x * 11 +
                  rain_phase * 7) %
                 156);

            draw_line(
                s,
                x,
                y,
                x - 2,
                y + 7,
                rgb(20, 58, 82)
            );
        }
    }

    if (storm_flash_active(g)) {
        draw_line(
            s,
            0,
            1,
            399,
            1,
            rgb(130, 160, 185)
        );

        int bx =
            title_scene
            ? 306
            : 290;

        int by = 33;

        draw_line(s, bx, by, bx - 11, by + 22, C_WHITE);
        draw_line(s, bx - 11, by + 22, bx - 4, by + 22, C_WHITE);
        draw_line(s, bx - 4, by + 22, bx - 17, by + 47, C_WHITE);
        draw_line(s, bx - 17, by + 47, bx - 10, by + 45, C_CYAN);
    }
}

static void draw_title_decals(const Game *g,
                              Surface *s) {
    if (!g || !s)
        return;

    int lod =
        effective_lod_level(g);

    /* Title stars render before the skyline. */
    int star_stride =
        lod >= 2
        ? 4
        : (lod >= 1 ? 3 : 2);

    for (int i = 0;
         i < MAX_STARS;
         i += star_stride) {
        int sx =
            g->stars[i].x;

        int sy =
            g->stars[i].y %
            190;

        draw_pixel(
            s,
            sx,
            sy,
            (g->stars[i].brightness & 1)
            ? rgb(80, 128, 180)
            : rgb(190, 190, 150)
        );
    }

    /*
       Title has its own neon-city identity regardless of the equipped world
       theme. Everything remains dim enough to sit behind menu text.
    */
    draw_city_skyline_layer(
        g,
        s,
        lod,
        true
    );

    Color cloud =
        rgb(19, 25, 34);

    draw_cloud_cluster(s, 28, 9, 1, cloud);
    draw_cloud_cluster(s, 92, 4, 1, rgb(14, 20, 29));
    draw_cloud_cluster(s, 314, 5, 1, rgb(14, 20, 29));
    draw_cloud_cluster(s, 378, 11, 1, cloud);

    if (storm_flash_active(g)) {
        draw_line(s, 319, 27, 309, 43, C_WHITE);
        draw_line(s, 309, 43, 316, 43, C_WHITE);
        draw_line(s, 316, 43, 305, 60, C_CYAN);
    }

    /* Small neon slashes/brackets and a dangling grapple-hook decal. */
    draw_bloom_line(s, 76, 30, 101, 30, C_CYAN);
    draw_bloom_line(s, 299, 30, 324, 30, C_CYAN);

    draw_line(s, 68, 21, 75, 28, C_YELLOW);
    draw_line(s, 68, 31, 75, 24, C_YELLOW);
    draw_line(s, 325, 24, 332, 31, C_YELLOW);
    draw_line(s, 325, 28, 332, 21, C_YELLOW);

    draw_line(s, 42, 54, 42, 68, C_DIM);
    draw_circle_outline(s, 42, 72, 4, C_CYAN);
    draw_line(s, 42, 76, 47, 80, C_CYAN);
}

static void draw_aurora_layer(const Game *g,
                              Surface *s,
                              int lod) {
    int step = lod >= 2 ? 24 : 12;

    for (int band = 0; band < 3; ++band) {
        Color c =
            band == 0 ? rgb(35, 220, 160) :
            band == 1 ? rgb(45, 145, 255) :
                        rgb(180, 70, 235);

        int prev_x = 0;
        int prev_y = 28 + band * 17;

        for (int x = step; x <= 400; x += step) {
            float phase =
                (float)x * 0.025f +
                (float)g->frame_counter *
                (0.012f + (float)band * 0.004f);

            int y =
                28 +
                band * 17 +
                (int)lroundf(
                    sinf(phase) *
                    (8.0f + (float)band * 2.0f)
                );

            if (lod < 2)
                draw_bloom_line(s, prev_x, prev_y, x, y, c);
            else
                draw_line(s, prev_x, prev_y, x, y, c);

            prev_x = x;
            prev_y = y;
        }
    }
}

static void draw_cloud9_layer(const Game *g,
                              Surface *s,
                              int lod) {
    float drift =
        (float)(g->frame_counter % 1800u) *
        0.025f -
        g->camera_x *
        0.018f;

    float y_scroll =
        -g->camera_y * 0.020f;

    int count = lod >= 2 ? 4 : 7;

    for (int i = 0; i < count; ++i) {
        int x =
            (int)((float)(i * 78) + drift) %
            500;

        if (x < -60)
            x += 500;

        int y =
            18 +
            (i % 3) * 40 +
            (int)lroundf(y_scroll * (0.55f + 0.15f * (float)(i % 3)));

        Color c =
            (i & 1)
            ? rgb(225, 238, 248)
            : rgb(245, 250, 255);

        draw_cloud_cluster(s, x, y, 1, c);
    }

    if (lod < 2) {
        int prev_y =
            181 + (int)lroundf(y_scroll);
        for (int x = 8; x <= 400; x += 8) {
            float phase =
                (float)x * 0.035f +
                (float)g->frame_counter * 0.030f;
            int y =
                181 +
                (int)lroundf(y_scroll) +
                (int)lroundf(sinf(phase) * 2.0f);
            draw_bloom_line(
                s,
                x - 8,
                prev_y,
                x,
                y,
                rgb(180, 220, 255)
            );
            prev_y = y;
        }
    }
}

static void draw_background(const Game *g, Surface *s) {
    surface_clear(
        s,
        background_clear_color(g)
    );

    float px_scroll =
        g->camera_x *
        0.13f /
        g->camera_zoom;

    float py_scroll =
        g->camera_y *
        0.18f /
        g->camera_zoom;

    int lod =
        effective_lod_level(g);

    int star_stride =
        lod >= 2 ? 3 :
        lod >= 1 ? 2 : 1;

    if (g->progress.background_style != 8) {
    for (int i = 0;
         i < MAX_STARS;
         i += star_stride) {

        float xf =
            (float)g->stars[i].x -
            px_scroll;

        float yf =
            (float)g->stars[i].y -
            py_scroll;

        while (xf < 0.0f) xf += 400.0f;
        while (xf >= 400.0f) xf -= 400.0f;
        while (yf < 0.0f) yf += 218.0f;
        while (yf >= 218.0f) yf -= 218.0f;

        draw_pixel(
            s,
            (int)xf,
            (int)yf,
            background_star_color(
                g,
                g->stars[i].brightness
            )
        );
    }
    }

    if (g->progress.background_style == 5) {
        draw_city_skyline_layer(g, s, lod, false);
        return;
    }

    if (g->progress.background_style == 6) {
        draw_storm_layer(g, s, lod, false);
        return;
    }

    if (g->progress.background_style == 7) {
        draw_aurora_layer(g, s, lod);
        return;
    }

    if (g->progress.background_style == 8) {
        draw_cloud9_layer(g, s, lod);
        return;
    }

    if (lod >= 2)
        return;

    int scroll_x =
        ((int)(
            -g->camera_x *
            0.06f /
            g->camera_zoom
        )) %
        64;

    int scroll_y =
        ((int)(
            -g->camera_y *
            0.06f /
            g->camera_zoom
        )) %
        48;

    if (scroll_x < 0) scroll_x += 64;
    if (scroll_y < 0) scroll_y += 48;

    Color grid =
        rgb(6, 8, 10);

    switch (g->progress.background_style) {
        case 1:
            grid = rgb(5, 12, 28);
            break;

        case 2:
            grid = rgb(23, 7, 31);
            break;

        case 3:
            grid = rgb(4, 28, 14);
            break;

        case 4:
            grid = rgb(34, 12, 8);
            break;

        case 5:
            grid = rgb(7, 24, 45);
            break;

        case 6:
            grid = rgb(17, 24, 32);
            break;

        default:
            break;
    }

    int column_step =
        lod >= 1
        ? 96
        : 64;

    if (g->progress.background_style == 3) {
        /*
           MATRIX theme: a true grid rather than the classic tall columns.
        */
        for (int x = -64 + scroll_x;
             x < 432;
             x += column_step) {
            draw_line(
                s,
                x,
                0,
                x,
                239,
                grid
            );
        }

        for (int y = -48 + scroll_y;
             y < 260;
             y += 48) {
            draw_line(
                s,
                0,
                y,
                399,
                y,
                grid
            );
        }
    } else {
        for (int x = -40 + scroll_x;
             x < 420;
             x += column_step) {
            draw_rect_outline(
                s,
                x,
                -48 + scroll_y,
                34,
                288,
                grid
            );
        }
    }
}

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
        case 1: { /* TOPHAT */
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

        case 2: { /* CROWN */
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

        case 3: /* HALO */
            draw_circle_outline(
                top,
                (int)(hx + up_x * 4.0f),
                (int)(hy + up_y * 4.0f),
                6,
                C_YELLOW
            );
            break;

        case 4: { /* ANTENNA */
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

        case 5: { /* CAP */
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

        case 6: { /* WITCH */
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

        case 7: { /* HORNS */
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

        case 8: { /* CAT EARS */
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

        case 9: { /* PARTY */
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

        case 10: { /* COWBOY */
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

        case 11: { /* PROPELLER */
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

        case 12: { /* UFO */
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

        case 13: { /* BEANIE */
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

        case 14: { /* HEADPHONES */
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

        case 15: { /* MILK */
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
        case 1: { /* DIAMOND */
            float hw =
                half_w *
                0.90f;

            float hh =
                half_h *
                1.10f;

            if (lod < 2) {
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

        case 2: { /* ORB */
            int radius =
                (int)fmaxf(
                    2.0f,
                    (half_w +
                     half_h) *
                    0.52f
                );

            if (lod < 2)
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

        case 3: { /* ARROW */
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

            if (lod < 2)
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

        case 4: { /* COMET */
            if (lod < 2) {
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

        case 5: { /* STAR */
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

                if (lod < 2)
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

        case 6: { /* HEX */
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

                if (lod < 2)
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


        case 8: { /* TVHEAD */
            Color shell = rgb(42, 46, 58);
            Color bezel = rgb(190, 210, 230);
            float body_w = half_w * 1.18f;
            float body_h = half_h * 1.10f;

            if (lod < 2) {
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
                if (lod < 2)
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

        case 7: { /* BANANA */
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
                if (lod < 2)
                    draw_bloom_circle(top, (int)px, (int)py,
                                      (int)fmaxf(2.0f, radius * 0.22f), peel);
                else
                    draw_circle_filled(top, (int)px, (int)py,
                                       (int)fmaxf(2.0f, radius * 0.22f), peel);
                if (have_prev) {
                    if (lod < 2)
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
            if (lod < 2) {
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
                    case 1: /* RIPPLE */
                        wobble =
                            (int)lroundf(
                                sinf(phase * 0.65f) *
                                2.0f
                            );
                        break;

                    case 2: /* WAVE */
                        wobble =
                            (int)lroundf(
                                sinf(phase * 0.42f) *
                                4.0f
                            );
                        break;

                    case 3: /* SPARK */
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

                    case 4: /* CHAOS */
                        wobble =
                            (int)lroundf(
                                sinf(phase * 1.31f) * 3.0f +
                                sinf(phase * 0.47f + 1.2f) * 2.0f
                            );
                        break;

                    case 5: /* BUBBLES */
                        wobble =
                            (int)lroundf(
                                sinf(phase * 0.74f) *
                                2.0f
                            );
                        break;

                    case 6: /* GEYSER */
                        wobble =
                            (int)lroundf(
                                sinf(phase * 0.90f) *
                                2.5f
                            );
                        break;

                    case 0:
                    default: /* CALM */
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
                    continue; /* DASH */
                }

                Color sc =
                    rope_core;

                if (pattern == 2 &&
                    (s & 1)) {
                    sc =
                        player_core; /* DUAL */
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

                if (lod < 1)
                    draw_bloom_line(
                        top,
                        sx0,
                        sy0,
                        sx1,
                        sy1,
                        sc
                    );
                else
                    draw_line(
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

    /*
       Show the top-screen point represented by the current bottom-screen
       touch. The actual grapple target is the closest block to this cursor.
    */
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
            /*
               Only visible blocks pay the trig cost.
            */
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
            /* Full close-up quality. */
            draw_bloom_rotated_square(top,
                                      sx + (float)ox,
                                      sy + (float)oy,
                                      visual_half,
                                      angle,
                                      c);
        } else if (lod < 2) {
            /*
               Medium LOD: one rotated filled square plus one cheap
               axis-aligned glow. Avoid the multi-outline bloom stack.
            */
            draw_glow_square(top,
                             (int)sx + ox,
                             (int)sy + oy,
                             (int)fmaxf(1.0f, visual_half),
                             c);
        } else {
            /*
               Far LOD: blocks are only a few pixels on screen, so a simple
               filled square + one outline is visually sufficient.
            */
            int half = (int)fmaxf(1.0f, visual_half);
            draw_rect(top,
                      (int)sx - half + ox,
                      (int)sy - half + oy,
                      half * 2 + 1,
                      half * 2 + 1,
                      c);

            draw_rect_outline(top,
                              (int)sx - half - 1 + ox,
                              (int)sy - half - 1 + oy,
                              half * 2 + 3,
                              half * 2 + 3,
                              c);
        }

        /*
           Tiny symbols cost proportionally more than the blocks themselves.
           Only draw them while close enough to read.
        */
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

        draw_rect(top,
                  (int)sx - bullet_size / 2 + ox,
                  (int)sy - bullet_size / 2 + oy,
                  bullet_size, bullet_size, b->color);
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

            int base_size = p->life > 0.4f ? 2 : 1;
            int size = clampi((int)lroundf((float)base_size / g->camera_zoom), 1, 2);
            draw_rect(top,
                      (int)sx + ox,
                      (int)sy + oy,
                      size, size, p->color);
        }
    }

    bool blink_off =
        g->invuln_timer > 0.0f &&
        ((g->frame_counter / 4u) & 1u);

    /* GHOST animation includes periodic visibility flicker. */
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
            case 1: { /* BREATHE */
                float pulse =
                    1.0f +
                    sinf(anim_phase) *
                    0.08f;

                half_w_world *= pulse;
                half_h_world *= pulse;
                break;
            }

            case 2: /* TWIST */
                render_angle +=
                    sinf(
                        anim_phase *
                        1.35f
                    ) *
                    0.18f;
                break;

            case 3: { /* JELLY */
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

            case 5: /* SPIN */
                render_angle +=
                    (float)g->frame_counter *
                    0.075f;
                break;

            case 6: { /* GLITCH */
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

            case 7: /* CYCLE */
                render_angle +=
                    sinf(anim_phase * 1.15f) *
                    0.12f;
                break;

            case 8: { /* BOUNCE */
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

            if (lod < 1)
                draw_bloom_line(
                    top,
                    x1, y1,
                    x0, y0,
                    player_color(g)
                );
            else
                draw_line(
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
        case 1: /* STORM */
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

        case 2: /* GAMEPLAY */
            draw_title_fake_gameplay(
                g,
                top
            );
            break;

        case 3: { /* LAVA */
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

        case 4: /* VOID */
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

        case 5: { /* MATRIX */
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

        case 6: { /* ARCADE */
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

    int spread =
        18 +
        (int)lroundf(
            ease *
            18.0f
        );

    int label_half =
        (int)strlen(label) * 8 +
        10 +
        (int)lroundf(ease * 2.0f);

    draw_rect_outline(
        s,
        200 - label_half,
        yy - 6,
        label_half * 2,
        22,
        C_CYAN
    );

    draw_rect_outline(
        s,
        200 - label_half - 3,
        yy - 9,
        label_half * 2 + 6,
        28,
        rgb(20, 80, 100)
    );

    draw_bloom_line(
        s,
        200 - spread - 58,
        yy + 9,
        200 - label_half - 5,
        yy + 9,
        C_CYAN
    );

    draw_bloom_line(
        s,
        200 + label_half + 5,
        yy + 9,
        200 + spread + 58,
        yy + 9,
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

    draw_text_center(
        top,
        200,
        12,
        "BAD GAME",
        3,
        ui_accent_color(g)
    );

    draw_text_center(
        top,
        200,
        42,
        "3DS EDITION",
        1,
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

static void draw_settings_row(Surface *top,
                              int y,
                              const char *label,
                              const char *value,
                              bool selected) {
    Color label_color =
        selected
        ? C_CYAN
        : C_WHITE;

    Color value_color =
        selected
        ? C_YELLOW
        : C_DIM;

    if (selected)
        draw_text(
            top,
            52,
            y,
            ">",
            1,
            C_CYAN
        );

    draw_text(
        top,
        72,
        y,
        label,
        1,
        label_color
    );

    if (value) {
        draw_text(
            top,
            280,
            y,
            value,
            1,
            value_color
        );
    }
}

static void draw_settings(const Game *g, Surface *top) {
    draw_background(g, top);

    draw_rect(
        top,
        34,
        12,
        332,
        216,
        rgb(7, 9, 11)
    );

    draw_rect_outline(
        top,
        34,
        12,
        332,
        216,
        C_DIM
    );

    draw_text_center(
        top,
        200,
        20,
        "SETTINGS",
        3,
        C_WHITE
    );

    draw_text_center(
        top,
        200,
        52,
        g->settings_page == 0
        ? "AUDIO + DISPLAY"
        : "EFFECTS",
        1,
        C_CYAN
    );

    char page[24];
    snprintf(
        page,
        sizeof(page),
        "L  %d/2  R",
        g->settings_page + 1
    );

    draw_text_center(
        top,
        200,
        212,
        page,
        1,
        C_DIM
    );

    if (g->settings_page == 0) {
        draw_settings_row(
            top,
            72,
            "SFX",
            setting_enabled(g, SETTING_SFX)
            ? "ON"
            : "OFF",
            g->settings_index == 0
        );

        draw_settings_row(
            top,
            98,
            "MUSIC",
            setting_enabled(g, SETTING_MUSIC)
            ? "ON"
            : "OFF",
            g->settings_index == 1
        );

        draw_settings_row(
            top,
            124,
            "BLOOM",
            NULL,
            g->settings_index == 2
        );

        for (int i = 0;
             i <= (int)SETTINGS_BLOOM_MAX;
             ++i) {
            int x =
                267 +
                i * 17;

            Color c =
                i <=
                    (int)g->progress.bloom_level
                ? C_YELLOW
                : rgb(42, 46, 50);

            draw_rect_outline(
                top,
                x,
                125,
                12,
                8,
                C_DIM
            );

            if (i <=
                (int)g->progress.bloom_level) {
                draw_rect(
                    top,
                    x + 2,
                    127,
                    8,
                    4,
                    c
                );
            }
        }

        const char *lod =
            "AUTO";

        if (g->progress.force_lod == 1)
            lod = "0";
        else if (g->progress.force_lod == 2)
            lod = "1";
        else if (g->progress.force_lod == 3)
            lod = "2";

        draw_settings_row(
            top,
            150,
            "FORCE LOD",
            lod,
            g->settings_index == 3
        );

        draw_settings_row(
            top,
            176,
            "STEREO 3D",
            setting_enabled(g, SETTING_STEREO_3D)
            ? "ON"
            : "OFF",
            g->settings_index == 4
        );

        char dist_buf[32];
        snprintf(dist_buf,
                 sizeof(dist_buf),
                 "%lu",
                 (unsigned long)g->progress.total_distance_traveled);
        draw_text(top, 72, 202, "TOTAL DIST", 1, C_DIM);
        draw_text(top, 270, 202, dist_buf, 1, C_YELLOW);
    } else {
        draw_settings_row(
            top,
            82,
            "BLOCK ANIM",
            setting_enabled(g, SETTING_BLOCK_ANIM)
            ? "ON"
            : "OFF",
            g->settings_index == 0
        );

        draw_settings_row(
            top,
            116,
            "LAVA ANIM",
            setting_enabled(g, SETTING_LAVA_ANIM)
            ? "ON"
            : "OFF",
            g->settings_index == 1
        );

        draw_settings_row(
            top,
            150,
            "PARTICLES",
            setting_enabled(g, SETTING_PARTICLES)
            ? "ON"
            : "OFF",
            g->settings_index == 2
        );

        draw_settings_row(
            top,
            184,
            "SCREEN SHAKE",
            setting_enabled(g, SETTING_SCREENSHAKE)
            ? "ON"
            : "OFF",
            g->settings_index == 3
        );
    }
}

static void draw_progress_bar(Surface *s,
                              int x,
                              int y,
                              int w,
                              uint32_t value,
                              uint32_t target,
                              Color c) {
    draw_rect_outline(
        s,
        x,
        y,
        w,
        8,
        C_DIM
    );

    float pct =
        target > 0
        ? (float)value /
          (float)target
        : 1.0f;

    pct =
        clampf_local(
            pct,
            0.0f,
            1.0f
        );

    int fill =
        (int)(
            (float)(w - 4) *
            pct
        );

    if (fill > 0) {
        draw_rect(
            s,
            x + 2,
            y + 2,
            fill,
            4,
            c
        );
    }
}

static void draw_missions(const Game *g, Surface *top) {
    draw_background(g, top);

    draw_rect(
        top,
        24,
        10,
        352,
        220,
        rgb(7, 9, 11)
    );

    draw_rect_outline(
        top,
        24,
        10,
        352,
        220,
        C_DIM
    );

    draw_text_center(
        top,
        200,
        18,
        "AUTO MISSIONS",
        3,
        C_WHITE
    );

    char completed[40];
    snprintf(
        completed,
        sizeof(completed),
        "COMPLETED %lu",
        (unsigned long)g->progress.missions_claimed
    );
    draw_text_center(top, 200, 40, completed, 1, C_YELLOW);

    int first =
        g->missions_index - 2;

    if (first < 0)
        first = 0;

    if (first >
        MISSION_SLOT_COUNT - 5) {
        first =
            MISSION_SLOT_COUNT - 5;
    }

    if (first < 0)
        first = 0;

    for (int slot = 0;
         slot < 5;
         ++slot) {
        int i =
            first +
            slot;

        if (i >= MISSION_SLOT_COUNT)
            break;

        const GeneratedMission *m =
            mission_slot_const(g, i);

        if (!m)
            continue;

        bool selected =
            i ==
            g->missions_index;

        int y =
            62 +
            slot * 31;

        if (selected) {
            draw_rect_outline(
                top,
                42,
                y - 5,
                316,
                27,
                C_CYAN
            );
        }

        draw_text(
            top,
            54,
            y,
            m->name,
            1,
            C_WHITE
        );

        char progress[32];

        snprintf(
            progress,
            sizeof(progress),
            "%lu/%lu",
            (unsigned long)goal_value(g, (GoalKind)m->kind),
            (unsigned long)m->target
        );

        draw_text(
            top,
            277,
            y,
            progress,
            1,
            C_WHITE
        );

        draw_progress_bar(
            top,
            54,
            y + 13,
            200,
            goal_value(g, (GoalKind)m->kind),
            m->target,
            C_CYAN
        );
    }
}

static void draw_achievements(const Game *g, Surface *top) {
    draw_background(g, top);

    draw_rect(
        top,
        24,
        10,
        352,
        220,
        rgb(7, 9, 11)
    );

    draw_rect_outline(
        top,
        24,
        10,
        352,
        220,
        C_DIM
    );

    int unlocked = 0;

    for (int i = 0;
         i < ACHIEVEMENT_COUNT;
         ++i) {
        if (achievement_unlocked(g, i))
            ++unlocked;
    }

    char heading[48];

    snprintf(
        heading,
        sizeof(heading),
        "ACHIEVEMENTS %d/%d",
        unlocked,
        ACHIEVEMENT_COUNT
    );

    draw_text_center(
        top,
        200,
        20,
        heading,
        2,
        C_WHITE
    );

    int first =
        g->achievements_index - 2;

    if (first < 0)
        first = 0;

    if (first >
        ACHIEVEMENT_COUNT - 5) {
        first =
            ACHIEVEMENT_COUNT - 5;
    }

    if (first < 0)
        first = 0;

    for (int slot = 0;
         slot < 5;
         ++slot) {
        int i =
            first +
            slot;

        if (i >= ACHIEVEMENT_COUNT)
            break;

        const AchievementDef *a =
            &ACHIEVEMENTS[i];

        bool selected =
            i ==
            g->achievements_index;

        bool unlocked_now =
            achievement_unlocked(
                g,
                i
            );

        int y =
            62 +
            slot * 31;

        if (selected) {
            draw_rect_outline(
                top,
                42,
                y - 5,
                316,
                27,
                C_CYAN
            );
        }

        Color state =
            unlocked_now
            ? C_GREEN
            : C_DIM;

        draw_text(
            top,
            54,
            y,
            a->name,
            1,
            state
        );

        draw_text(
            top,
            292,
            y,
            unlocked_now
            ? "DONE"
            : "LOCKED",
            1,
            state
        );

        draw_progress_bar(
            top,
            54,
            y + 13,
            200,
            goal_value(g, a->kind),
            a->target,
            state
        );
    }
}

static void draw_pause(const Game *g, Surface *top) {
    draw_rect(top, 88, 61, 224, 128, rgb(7, 9, 11));
    draw_rect_outline(top, 88, 61, 224, 128, C_DIM);
    draw_text_center(top, 200, 73, "PAUSED", 3, C_WHITE);
    draw_menu_choice(top, 105, "RESUME", g->pause_index == 0);
    draw_menu_choice(top, 133, "RESTART", g->pause_index == 1);
    draw_menu_choice(top, 161, "TITLE", g->pause_index == 2);
}

static void draw_gameover(const Game *g, Surface *top) {
    /* Game-over panel bounds: x=48..351, y=10..233. */
    draw_rect(top, 48, 10, 304, 224, rgb(7, 9, 11));
    draw_rect_outline(top, 48, 10, 304, 224, C_DIM);

    draw_text_center(top, 200, 20, "GAME OVER", 3, C_RED);

    char buf[64];

    snprintf(buf, sizeof(buf), "SCORE %d", g->score);
    draw_text_center(top, 200, 54, buf, 1, C_YELLOW);

    snprintf(buf, sizeof(buf), "DIST %lu   BLOCKS %d",
             (unsigned long)current_run_distance(g),
             g->destroyed);
    draw_text_center(top, 200, 66, buf, 1, C_WHITE);

    snprintf(buf, sizeof(buf), "+XP %lu   +$%lu",
             (unsigned long)g->run_xp_earned,
             (unsigned long)g->run_cash_earned);
    draw_text_center(top, 200, 78, buf, 1, C_GREEN);

    snprintf(buf, sizeof(buf), "RUN BONUS +%luXP +$%lu",
             (unsigned long)g->run_end_xp_bonus,
             (unsigned long)g->run_end_cash_bonus);
    draw_text_center(top, 200, 90, buf, 1, C_YELLOW);

    snprintf(buf, sizeof(buf), "LEVEL %lu",
             (unsigned long)g->progress.player_level);
    draw_text_center(top, 200, 105, buf, 2, C_CYAN);

    uint32_t need =
        xp_required_for_level(
            g->progress.player_level
        );

    float pct =
        clampf_local(
            (float)g->progress.xp /
            (float)need,
            0.0f,
            1.0f
        );

    draw_rect_outline(top, 120, 128, 160, 10, C_DIM);
    draw_rect(top, 122, 130, (int)(156.0f * pct), 6, C_GREEN);

    snprintf(buf, sizeof(buf), "%lu / %lu XP",
             (unsigned long)g->progress.xp,
             (unsigned long)need);
    draw_text_center(top, 200, 142, buf, 1, C_DIM);

    if (g->run_levelups > 0) {
        snprintf(buf, sizeof(buf), "LEVEL UPS +%d", g->run_levelups);
        draw_text_center(top, 200, 153, buf, 1, C_YELLOW);
    }

    draw_menu_choice(top, 171, "RETRY", g->gameover_index == 0);
    draw_menu_choice(top, 193, "SHOP", g->gameover_index == 1);
    draw_menu_choice(top, 215, "TITLE", g->gameover_index == 2);
}

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

static void draw_mission_notice_overlay(const Game *g, Surface *top) {
    if (!g || !top ||
        g->mission_notice_timer <= 0.0f ||
        g->mission_notice[0] == '\0') {
        return;
    }

    int w = 252;
    int x = (400 - w) / 2;
    int y = 196;

    draw_rect(top, x, y, w, 20, rgb(8, 10, 14));
    draw_rect_outline(top, x, y, w, 20, C_YELLOW);
    draw_text_center(top, 200, y + 6, g->mission_notice, 1, C_YELLOW);
}

void game_render_top(const Game *g, Surface *top) {
    if (g->mode == MODE_TITLE) {
        draw_title(g, top);
        return;
    }

    if (g->mode == MODE_SHOP) {
        draw_shop(g, top);
        return;
    }

    if (g->mode == MODE_SETTINGS) {
        draw_settings(g, top);
        return;
    }

    if (g->mode == MODE_MISSIONS) {
        draw_missions(g, top);
        return;
    }

    if (g->mode == MODE_ACHIEVEMENTS) {
        draw_achievements(g, top);
        return;
    }

    draw_world(g, top);
    draw_mission_notice_overlay(g, top);

    if (g->mode == MODE_PAUSED)
        draw_pause(g, top);
    else if (g->mode == MODE_GAMEOVER)
        draw_gameover(g, top);
}

static void draw_bottom_menu_help(const Game *g, Surface *bottom, const char *title, const char *line) {
    surface_clear(bottom, ui_bg_color(g));
    draw_ui_frame(g, bottom, 16, 18, 288, 196);
    draw_text_center(bottom, 160, 44, title, 3, C_WHITE);
    draw_text_center(bottom, 160, 102, "DPAD OR CIRCLE PAD", 1, ui_accent_color(g));
    draw_text_center(bottom, 160, 118, "MOVE SELECTION", 1, C_WHITE);
    draw_text_center(bottom, 160, 146, "A SELECT", 1, C_YELLOW);
    draw_text_center(bottom, 160, 161, "B BACK", 1, ui_dim_color(g));
    if (line) draw_text_center(bottom, 160, 196, line, 1, ui_dim_color(g));
}

static void draw_bottom_grapple(const Game *g, Surface *bottom, const GameInput *in) {
    (void)in;
    surface_clear(bottom, C_BG);

    const int cx = 160;
    const int cy = 120;

    draw_circle_outline(bottom, cx, cy, 34, rgb(42, 47, 52));
    draw_circle_outline(bottom, cx, cy, 72, rgb(22, 26, 30));
    draw_circle_outline(bottom, cx, cy, 108, rgb(12, 15, 18));
    draw_line(bottom, cx - 9, cy, cx + 9, cy, C_DIM);
    draw_line(bottom, cx, cy - 9, cx, cy + 9, C_DIM);
    draw_rect_outline(bottom, cx - 4, cy - 4, 9, 9, C_CYAN);

    if (g->aim_valid) {
        float top_sx = world_to_screen_x(g, g->aim_world.x);
        float top_sy = world_to_screen_y(g, g->aim_world.y);

        int bx = (int)(top_sx * (320.0f / 400.0f));
        int by = (int)top_sy;

        bx = clampi(bx, 0, 319);
        by = clampi(by, 0, 239);

        draw_line(bottom, cx, cy, bx, by,
                  g->progress.levels[UPG_LASER] ? C_GREEN : C_CYAN);
        draw_glow_square(bottom, bx, by, 3, C_CYAN);
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "ROPE %d/15",
             (int)g->progress.levels[UPG_ROPE]);
    draw_text(bottom, 8, 8, buf, 1, C_WHITE);

    if (g->rope.latched) {
        draw_text_center(bottom, 160, 42, "HOOKED", 2, C_GREEN);
        draw_text_center(bottom, 160, 202, "ANCHOR + GRAVITY + MOMENTUM", 1, C_WHITE);
        draw_text_center(bottom, 160, 216, "HOLD TO STAY ATTACHED", 1, C_DIM);
    } else {
        draw_text_center(bottom, 160, 42, "GRAPPLE", 2, C_CYAN);
        draw_text_center(bottom, 160, 202, "TAP NEAR A TOP SCREEN BLOCK", 1, C_WHITE);
        draw_text_center(bottom, 160, 216, "DRAGGING DOES NOT STEER", 1, C_DIM);
    }
}

static void draw_bottom_title(const Game *g, Surface *bottom) {
    surface_clear(
        bottom,
        ui_bg_color(g)
    );

    draw_ui_frame(g, bottom, 12, 10, 296, 220);

    draw_text_center(
        bottom,
        160,
        24,
        "BAD GAME 3DS",
        2,
        C_WHITE
    );

    char buf[64];

    snprintf(
        buf,
        sizeof(buf),
        "LEVEL %lu",
        (unsigned long)g->progress.player_level
    );

    draw_text(
        bottom,
        32,
        67,
        buf,
        1,
        C_CYAN
    );

    snprintf(
        buf,
        sizeof(buf),
        "$%lu",
        (unsigned long)g->progress.money
    );

    draw_text(
        bottom,
        210,
        67,
        buf,
        1,
        C_YELLOW
    );

    snprintf(
        buf,
        sizeof(buf),
        "BEST DIST %lu",
        (unsigned long)g->progress.best_distance
    );

    draw_text(
        bottom,
        32,
        91,
        buf,
        1,
        C_WHITE
    );

    snprintf(
        buf,
        sizeof(buf),
        "HIGH SCORE %lu",
        (unsigned long)g->progress.high_score
    );

    draw_text(
        bottom,
        32,
        113,
        buf,
        1,
        C_WHITE
    );

    int unlocked = 0;

    for (int i = 0;
         i < ACHIEVEMENT_COUNT;
         ++i) {
        if (achievement_unlocked(g, i))
            ++unlocked;
    }

    int ready = MISSION_SLOT_COUNT;

    snprintf(
        buf,
        sizeof(buf),
        "ACHIEVEMENTS %d/%d",
        unlocked,
        ACHIEVEMENT_COUNT
    );

    draw_text(
        bottom,
        32,
        143,
        buf,
        1,
        C_GREEN
    );

    snprintf(
        buf,
        sizeof(buf),
        "MISSIONS READY %d",
        ready
    );

    draw_text(
        bottom,
        32,
        165,
        buf,
        1,
        ready > 0
        ? C_YELLOW
        : C_DIM
    );

    draw_text_center(
        bottom,
        160,
        211,
        "UP/DOWN MOVE   A SELECT",
        1,
        C_DIM
    );
}

static void draw_bottom_shop(const Game *g, Surface *bottom) {
    surface_clear(
        bottom,
        ui_bg_color(g)
    );

    draw_ui_frame(g, bottom, 12, 10, 296, 220);

    const char *page_name =
        g->shop_page == 0
        ? "UPGRADES"
        : COSMETIC_PAGES[
              g->shop_page - 1
          ].name;

    draw_text_center(
        bottom,
        160,
        12,
        page_name,
        2,
        C_WHITE
    );

    if (g->shop_page == 0) {
        int item =
            clampi(
                g->shop_index,
                0,
                SHOP_ITEM_COUNT - 1
            );

        draw_upgrade_icon(
            bottom,
            item,
            160,
            72,
            g->progress.levels[item] > 0
        );

        draw_text_center(
            bottom,
            160,
            105,
            SHOP[item].name,
            2,
            C_CYAN
        );

        draw_text_center(
            bottom,
            160,
            132,
            SHOP[item].desc,
            1,
            C_WHITE
        );

        char state[64];

        int level =
            g->progress.levels[item];

        int max =
            SHOP[item].max_level;

        if (level >= max) {
            snprintf(
                state,
                sizeof(state),
                "LEVEL %d/%d   MAXED",
                level,
                max
            );
        } else {
            snprintf(
                state,
                sizeof(state),
                "LEVEL %d/%d   NEXT $%lu",
                level,
                max,
                (unsigned long)shop_cost(g, item)
            );
        }

        draw_text_center(
            bottom,
            160,
            157,
            state,
            1,
            level >= max
            ? C_GREEN
            : C_YELLOW
        );
    } else {
        int item =
            current_cosmetic_item(g);

        if (item >= 0) {
            const CosmeticDef *d =
                &COSMETICS[item];

            float ease =
                shop_selection_ease(g);

            float scale =
                1.65f +
                0.18f * ease;

            float twist =
                (1.0f - ease) *
                0.38f +
                sinf(
                    (float)g->frame_counter *
                    0.055f
                ) *
                0.035f;

            draw_cosmetic_preview_item(
                g,
                bottom,
                item,
                160,
                80,
                scale,
                twist
            );

            draw_text_center(
                bottom,
                160,
                116,
                d->name,
                2,
                cosmetic_equipped(g, item)
                ? C_GREEN
                : C_CYAN
            );

            draw_text_center(
                bottom,
                160,
                144,
                d->desc,
                1,
                C_WHITE
            );

            if (d->bonus &&
                d->bonus[0] != '\0') {
                draw_text_center(
                    bottom,
                    160,
                    160,
                    d->bonus,
                    1,
                    C_GREEN
                );
            }

            char state[48];

            if (cosmetic_equipped(g, item)) {
                strcpy(
                    state,
                    "EQUIPPED"
                );
            } else if (cosmetic_owned(g, item)) {
                strcpy(
                    state,
                    "OWNED - A TO EQUIP"
                );
            } else {
                snprintf(
                    state,
                    sizeof(state),
                    "$%lu - A TO BUY",
                    (unsigned long)d->cost
                );
            }

            draw_text_center(
                bottom,
                160,
                181,
                state,
                1,
                cosmetic_equipped(g, item)
                ? C_GREEN
                : C_YELLOW
            );
        }
    }

    draw_text_center(
        bottom,
        160,
        212,
        "D-PAD SELECT   L/R PAGE",
        1,
        C_DIM
    );

    draw_text_center(
        bottom,
        160,
        226,
        "A BUY/EQUIP   B BACK",
        1,
        C_DIM
    );
}

static void draw_bottom_missions(const Game *g, Surface *bottom) {
    surface_clear(
        bottom,
        ui_bg_color(g)
    );

    draw_ui_frame(g, bottom, 12, 10, 296, 220);

    int i =
        clampi(
            g->missions_index,
            0,
            MISSION_SLOT_COUNT - 1
        );

    const GeneratedMission *m =
        mission_slot_const(g, i);

    if (!m)
        return;

    draw_text_center(
        bottom,
        160,
        22,
        "MISSION GOAL",
        2,
        C_WHITE
    );

    draw_text_center(
        bottom,
        160,
        63,
        m->name,
        2,
        C_CYAN
    );

    draw_text_center(
        bottom,
        160,
        96,
        m->desc,
        1,
        C_WHITE
    );

    char progress[48];

    snprintf(
        progress,
        sizeof(progress),
        "%lu / %lu",
        (unsigned long)goal_value(g, (GoalKind)m->kind),
        (unsigned long)m->target
    );

    draw_text_center(
        bottom,
        160,
        119,
        progress,
        1,
        C_YELLOW
    );

    draw_progress_bar(
        bottom,
        45,
        137,
        230,
        goal_value(g, (GoalKind)m->kind),
        m->target,
        C_GREEN
    );

    char reward[64];

    snprintf(
        reward,
        sizeof(reward),
        "AUTO REWARD $%lu + %lu XP",
        (unsigned long)m->cash_reward,
        (unsigned long)m->xp_reward
    );

    draw_text_center(
        bottom,
        160,
        160,
        reward,
        1,
        C_GREEN
    );

    draw_text_center(
        bottom,
        160,
        188,
        "COMPLETES + REFRESHES",
        2,
        C_YELLOW
    );

    char total[48];
    snprintf(
        total,
        sizeof(total),
        "TOTAL COMPLETED %lu",
        (unsigned long)g->progress.missions_claimed
    );
    draw_text_center(
        bottom,
        160,
        206,
        total,
        1,
        C_WHITE
    );

    draw_text_center(
        bottom,
        160,
        224,
        "UP/DOWN GOAL   AUTO   B BACK",
        1,
        C_DIM
    );
}

static void draw_bottom_achievements(const Game *g, Surface *bottom) {
    surface_clear(
        bottom,
        ui_bg_color(g)
    );

    draw_ui_frame(g, bottom, 12, 10, 296, 220);

    int i =
        clampi(
            g->achievements_index,
            0,
            ACHIEVEMENT_COUNT - 1
        );

    const AchievementDef *a =
        &ACHIEVEMENTS[i];

    bool unlocked =
        achievement_unlocked(
            g,
            i
        );

    draw_text_center(
        bottom,
        160,
        25,
        unlocked
        ? "ACHIEVEMENT UNLOCKED"
        : "ACHIEVEMENT",
        2,
        unlocked
        ? C_GREEN
        : C_WHITE
    );

    draw_text_center(
        bottom,
        160,
        72,
        a->name,
        2,
        unlocked
        ? C_GREEN
        : C_CYAN
    );

    draw_text_center(
        bottom,
        160,
        106,
        a->desc,
        1,
        C_WHITE
    );

    char progress[48];

    snprintf(
        progress,
        sizeof(progress),
        "%lu / %lu",
        (unsigned long)goal_value(g, a->kind),
        (unsigned long)a->target
    );

    draw_text_center(
        bottom,
        160,
        132,
        progress,
        1,
        unlocked
        ? C_GREEN
        : C_YELLOW
    );

    draw_progress_bar(
        bottom,
        45,
        151,
        230,
        goal_value(g, a->kind),
        a->target,
        unlocked
        ? C_GREEN
        : C_CYAN
    );

    draw_text_center(
        bottom,
        160,
        190,
        unlocked
        ? "COMPLETE"
        : "KEEP WRECKING",
        2,
        unlocked
        ? C_GREEN
        : C_DIM
    );

    draw_text_center(
        bottom,
        160,
        224,
        "UP/DOWN VIEW   A/B BACK",
        1,
        C_DIM
    );
}

void game_render_bottom(const Game *g,
                        Surface *bottom,
                        const GameInput *in) {
    switch (g->mode) {
        case MODE_PLAYING:
            draw_bottom_grapple(
                g,
                bottom,
                in
            );
            break;

        case MODE_TITLE:
            draw_bottom_title(
                g,
                bottom
            );
            break;

        case MODE_SHOP:
            draw_bottom_shop(
                g,
                bottom
            );
            break;

        case MODE_SETTINGS:
            draw_bottom_menu_help(
                g,
                bottom,
                g->settings_page == 0
                ? "AUDIO + DISPLAY"
                : "EFFECTS",
                "L/R PAGE  LEFT/RIGHT CHANGE"
            );
            break;

        case MODE_MISSIONS:
            draw_bottom_missions(
                g,
                bottom
            );
            break;

        case MODE_ACHIEVEMENTS:
            draw_bottom_achievements(
                g,
                bottom
            );
            break;

        case MODE_PAUSED:
            draw_bottom_menu_help(
                g,
                bottom,
                "PAUSED",
                "START OR B RESUMES"
            );
            break;

        case MODE_GAMEOVER:
            draw_bottom_menu_help(
                g,
                bottom,
                "GAME OVER",
                "SPEND MONEY IN SHOP"
            );
            break;

        default:
            surface_clear(
                bottom,
                C_BG
            );
            break;
    }
}
