#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stdint.h>
#include "physics.h"
#include "render.h"

#define MAX_BLOCKS 224
#define MAX_PARTICLES 144
#define MAX_BULLETS 32
#define MAX_POPUPS 16
#define MAX_STARS 28
#define SHOP_ITEM_COUNT 8
#define COSMETIC_COUNT 102
#define MISSION_SLOT_COUNT 5
#define MAX_SECTION_STAMPS 49

typedef enum {
    MODE_TITLE = 0,
    MODE_PLAYING,
    MODE_PAUSED,
    MODE_SHOP,
    MODE_SETTINGS,
    MODE_MISSIONS,
    MODE_ACHIEVEMENTS,
    MODE_GAMEOVER
} GameMode;

typedef enum {
    BLOCK_ANCHOR = 0,
    BLOCK_RED,
    BLOCK_PURPLE,
    BLOCK_GREEN,
    BLOCK_MONEY
} BlockType;

typedef enum {
    UPG_HP = 0,
    UPG_ROPE,
    UPG_COMBO,
    UPG_BULLETS,
    UPG_GREEN,
    UPG_MONEY,
    UPG_THICC,
    UPG_LASER
} UpgradeType;

typedef struct {
    bool active;
    BlockType type;
    RigidBody body;
    Vec2 home;

    /* Physics size stays fixed; visuals pulse independently. */
    float half;
    float base_half;

    /* Small visual breathing/rotation animation. */
    float angle;
    float angular_velocity;
    float anim_phase;

    /* Section ownership for off-screen chunk streaming. */
    int section_x;
    int section_y;
    bool streamed;

    int value;
} Block;

typedef struct {
    bool active;
    Vec2 pos;
    Vec2 vel;
    float life;
    Color color;
} Particle;

typedef struct {
    bool active;
    Vec2 pos;
    Vec2 vel;
    float life;
    Color color;
    int power;
} Bullet;

typedef struct {
    bool active;
    Vec2 pos;
    float life;
    int value;
    bool money;
    Color color;
} ScorePopup;

typedef struct {
    int x;
    int y;
    int brightness;
} Star;

typedef struct {
    bool active;
    bool latched;
    bool user_owned;
    int target_block;
    Vec2 direction;
    Vec2 hook_pos;
    float length;
    float desired_length;
} Rope;

typedef struct {
    uint32_t money;
    uint32_t high_score;
    uint32_t best_distance;

    /* Persistent skill progression. Level 1 is the default. */
    uint32_t xp;
    uint32_t player_level;

    /*
       Cosmetic IDs 0..63 use cosmetic_owned.
       Cosmetic IDs 64..127 use cosmetic_owned2.
    */
    uint64_t cosmetic_owned;
    uint64_t cosmetic_owned2;

    uint32_t lifetime_destroyed;
    uint32_t best_speed;
    uint32_t best_combo;
    uint32_t missions_claimed;   /* Repeatable missions completed. */
    uint32_t total_distance_traveled;

    /* Equipped cosmetic style IDs. */
    uint8_t player_style;
    uint8_t rope_style;
    uint8_t pattern_style;       /* Rope animation / FX. */
    uint8_t shape_style;
    uint8_t player_anim_style;
    uint8_t hat_style;
    uint8_t block_theme;
    uint8_t background_style;
    uint8_t lava_color_style;
    uint8_t lava_anim_style;
    uint8_t title_style;
    uint8_t reserved_style1;

    uint8_t levels[SHOP_ITEM_COUNT];

    /*
       Persistent presentation/audio settings.
       settings_reserved is a validity marker so "all toggles off" is still
       distinct from a pre-settings save.
    */
    uint8_t settings_flags;
    uint8_t bloom_level;      /* 0..4; default 3 */
    uint8_t force_lod;        /* 0=AUTO, 1=LOD0, 2=LOD1, 3=LOD2 */
    uint8_t settings_reserved;
} Progress;

typedef struct {
    bool valid;
    int x;
    int y;
} SectionStamp;

typedef struct {
    char name[20];
    char desc[40];
    uint8_t kind;
    uint32_t target;
    uint32_t cash_reward;
    uint32_t xp_reward;
} GeneratedMission;

typedef struct {
    GameMode mode;
    GameMode shop_return_mode;

    RigidBody player;
    float player_half;
    float player_angle;
    float player_spin;
    int health;
    int max_health;
    float invuln_timer;

    Block blocks[MAX_BLOCKS];
    Particle particles[MAX_PARTICLES];
    Bullet bullets[MAX_BULLETS];
    ScorePopup popups[MAX_POPUPS];
    Star stars[MAX_STARS];
    Rope rope;

    /*
       2D scrolling world. camera_x/camera_y are the world coordinates of
       the top-left of the top LCD.
    */
    float camera_x;
    float camera_y;

    /*
       1.0 = normal 400x240 world view.
       Higher values show a larger area of the world.
    */
    float camera_zoom;

    float highest_y;
    Vec2 aim_world;
    bool aim_valid;

    /*
       Sections are generated well outside the visible screen. The current
       visible area is never populated in the middle of a flight.
    */
    SectionStamp sections[MAX_SECTION_STAMPS];
    int section_center_x;
    int section_center_y;

    /* Section cache ranges from 3x3 at 1x zoom to 7x7 near 3x zoom. */
    int section_radius_x;
    int section_radius_y;
    bool section_cache_ready;

    /* Fresh procedural-world seed for each run. */
    uint32_t world_seed;

    /* Smoothed visual speed deformation for the cyan player. */
    float player_stretch;

    /*
       Current straight-line displacement from the run's starting point.
       Distance is not accumulated path length.
    */
    float run_distance;
    float run_path_distance;
    Vec2 run_start_pos;
    Vec2 last_distance_sample;

    /* The player begins standing on blocks[0]. It breaks after first hook. */
    bool starter_platform_active;

    Progress progress;

    int score;
    int combo;
    int max_combo;
    int destroyed;

    /* Per-run progression accounting. */
    uint32_t run_xp_earned;
    uint32_t run_cash_earned;

    /*
       Count of normal non-Money-Boi kills in this run.
       Every kill pays at least $1; every fifth gets a special bonus/chime.
    */
    uint32_t normal_cash_kills;

    /* One-time performance bonus awarded at death. */
    uint32_t run_end_xp_bonus;
    uint32_t run_end_cash_bonus;

    int run_levelups;
    uint32_t last_level_reward;
    float levelup_message_timer;

    float combo_timer;
    float spawn_timer;
    float difficulty_time;

    /*
       Adaptive nine-shot spray timer.

       The timer drains faster from a combination of:
         - BULLETS upgrade level,
         - active combo,
         - current horizontal speed.

       It is therefore a timer, but not a fixed "every N seconds" cooldown.
    */
    float bullet_timer;

    float screen_shake;
    float flash_timer;

    int title_index;
    float title_select_anim;
    int pause_index;
    int gameover_index;

    /* Two uncluttered settings pages, four rows each. */
    int settings_page;
    int settings_index;

    /* Shop page 0 is upgrades; later pages are cosmetic categories. */
    int shop_page;
    int shop_index;
    float shop_select_anim;
    int shop_touch_pending;
    float shop_message_timer;
    char shop_message[48];

    int missions_index;
    int achievements_index;
    GeneratedMission missions[MISSION_SLOT_COUNT];
    float mission_notice_timer;
    char mission_notice[64];
    int run_best_combo;

    unsigned frame_counter;
    bool request_exit;
} Game;

typedef struct {
    bool touch_down;
    bool touch_held;
    bool touch_up;
    int touch_x;
    int touch_y;
    int nav_x;
    int nav_y;
    u32 keys_down;
    u32 keys_held;
} GameInput;

void game_init(Game *g);
void game_shutdown(Game *g);
void game_update(Game *g, const GameInput *in, float dt);
void game_render_top(const Game *g, Surface *top);
void game_render_bottom(const Game *g, Surface *bottom, const GameInput *in);

bool game_wants_stereo_3d(const Game *g);
