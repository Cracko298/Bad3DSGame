#define TOP_W 400.0f
#define TOP_H 240.0f
#define SECTION_W 400.0f
#define SECTION_H 240.0f
#define STREAM_BLOCK_START 3


#define PLAYER_GRAVITY 132.0f
#define PLAYER_AIR_DRAG 0.115f






#define CAMERA_ZOOM_MIN 1.0f
#define CAMERA_ZOOM_MAX 4.0f
#define CAMERA_ZOOM_STARTING 1.5f
#define CAMERA_ZOOM_SPEED_START 160.0f
#define CAMERA_ZOOM_SPEED_FULL 420.0f
#define CAMERA_ZOOM_OUT_RATE 3.84f
#define CAMERA_ZOOM_IN_RATE 2.2f





#define RENDER_LOD_MEDIUM_ZOOM 1.60f
#define RENDER_LOD_FAR_ZOOM 2.35f

#define SETTING_SFX          (1u << 0)
#define SETTING_MUSIC        (1u << 1)
#define SETTING_BLOCK_ANIM   (1u << 2)
#define SETTING_LAVA_ANIM    (1u << 3)
#define SETTING_PARTICLES    (1u << 4)
#define SETTING_SCREENSHAKE  (1u << 5)
#define SETTING_STEREO_3D    (1u << 6)
#define SETTING_BLOCK_OUTLINES (1u << 7)

#define SETTINGS_DEFAULT_FLAGS     (SETTING_SFX | SETTING_MUSIC | SETTING_BLOCK_ANIM |      SETTING_LAVA_ANIM | SETTING_PARTICLES | SETTING_SCREENSHAKE)

#define SETTINGS_VALID_MARKER 0xA7u


#define SETTINGS_BLOOM_ENABLED_DEFAULT   1u
#define SETTINGS_BLOOM_RADIUS_DEFAULT    2u
#define SETTINGS_BLOOM_RADIUS_MIN        1u
#define SETTINGS_BLOOM_RADIUS_MAX        4u
#define SETTINGS_BLOOM_INTENSITY_DEFAULT 7u
#define SETTINGS_BLOOM_INTENSITY_MIN     1u
#define SETTINGS_BLOOM_INTENSITY_MAX     10u
#define SETTINGS_BLOOM_QUADS_DEFAULT     2u
#define SETTINGS_BLOOM_QUADS_MIN         1u
#define SETTINGS_BLOOM_QUADS_MAX         4u

#define SETTINGS_FORCE_LOD_AUTO 0u
#define SETTINGS_FORCE_LOD_MAX 3u


#define SETTINGS_FPS_60      0u
#define SETTINGS_FPS_15      1u
#define SETTINGS_FPS_30      2u
#define SETTINGS_FPS_45      3u
#define SETTINGS_FPS_OFF     4u
#define SETTINGS_FPS_DEFAULT SETTINGS_FPS_60
#define SETTINGS_FPS_MAX     SETTINGS_FPS_OFF

#define SETTINGS_ITEM_COUNT 15











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

