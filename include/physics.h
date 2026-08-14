#pragma once

#include <stdbool.h>

typedef struct {
    float x;
    float y;
} Vec2;

typedef struct {
    Vec2 pos;
    Vec2 vel;
    float inv_mass;
    float damping;
    float restitution;
} RigidBody;

Vec2 vec2(float x, float y);
Vec2 vadd(Vec2 a, Vec2 b);
Vec2 vsub(Vec2 a, Vec2 b);
Vec2 vmul(Vec2 a, float s);
float vdot(Vec2 a, Vec2 b);
float vlen(Vec2 a);
Vec2 vnormalize(Vec2 a);

void phys_integrate(RigidBody *b, Vec2 accel, float dt);
void phys_confine_box(RigidBody *b, float half_w, float half_h,
                      float min_x, float min_y, float max_x, float max_y);
bool phys_aabb_overlap(Vec2 a, float ahw, float ahh,
                       Vec2 b, float bhw, float bhh);
bool phys_resolve_aabb(RigidBody *a, float ahw, float ahh,
                       RigidBody *b, float bhw, float bhh);
void phys_rope_constraint(RigidBody *body, Vec2 anchor,
                          float max_length, float stiffness,
                          float damping, float dt);
bool segment_aabb(Vec2 p0, Vec2 p1, Vec2 center, float half, float *out_t);
