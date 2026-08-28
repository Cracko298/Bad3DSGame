#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stdint.h>
#include "physics.h"
#include "render.h"

#define MAX_BLOCKS 256
#define MAX_PARTICLES 192
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

    
    float half;
    float base_half;

    
    float angle;
    float angular_velocity;
    float anim_phase;

    
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

    
    uint32_t xp;
    uint32_t player_level;

    



    uint64_t cosmetic_owned;
    uint64_t cosmetic_owned2;

    uint32_t lifetime_destroyed;
    uint32_t best_speed;
    uint32_t best_combo;
    uint32_t missions_claimed;   
    uint32_t total_distance_traveled;

    
    uint8_t player_style;
    uint8_t rope_style;
    uint8_t pattern_style;       
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

    




    uint8_t settings_flags;

    




    uint8_t bloom_level_legacy;
    uint8_t bloom_enabled;    
    uint8_t bloom_radius;     
    uint8_t bloom_intensity;  
    uint8_t bloom_quads;      

    uint8_t force_lod;        
    uint8_t fps_limit;        
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

    



    float camera_x;
    float camera_y;

    



    float camera_zoom;

    float highest_y;
    Vec2 aim_world;
    bool aim_valid;

    



    SectionStamp sections[MAX_SECTION_STAMPS];
    int section_center_x;
    int section_center_y;

    
    int section_radius_x;
    int section_radius_y;
    bool section_cache_ready;

    
    uint32_t world_seed;

    
    float player_stretch;

    



    float run_distance;
    float run_path_distance;
    Vec2 run_start_pos;
    Vec2 last_distance_sample;

    
    bool starter_platform_active;

    Progress progress;

    int score;
    int combo;
    int max_combo;
    int destroyed;

    
    uint32_t run_xp_earned;
    uint32_t run_cash_earned;

    



    uint32_t normal_cash_kills;

    
    uint32_t run_end_xp_bonus;
    uint32_t run_end_cash_bonus;

    int run_levelups;
    uint32_t last_level_reward;
    float levelup_message_timer;

    float combo_timer;
    float spawn_timer;
    float difficulty_time;

    









    float bullet_timer;

    float screen_shake;
    float flash_timer;

    int title_index;
    float title_select_anim;
    int pause_index;
    int gameover_index;

    
    int settings_page;
    int settings_index;
    int settings_scroll;
    bool reset_save_confirm;
    int reset_save_choice;   

    
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
int game_target_fps(const Game *g);
bool game_bloom_enabled(const Game *g);
int game_bloom_radius(const Game *g);
int game_bloom_intensity(const Game *g);
int game_bloom_quads(const Game *g);
