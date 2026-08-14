#include "physics.h"

#include <math.h>

static float clampf_local(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

Vec2 vec2(float x, float y) { Vec2 v = {x, y}; return v; }
Vec2 vadd(Vec2 a, Vec2 b) { return vec2(a.x+b.x, a.y+b.y); }
Vec2 vsub(Vec2 a, Vec2 b) { return vec2(a.x-b.x, a.y-b.y); }
Vec2 vmul(Vec2 a, float s) { return vec2(a.x*s, a.y*s); }
float vdot(Vec2 a, Vec2 b) { return a.x*b.x + a.y*b.y; }
float vlen(Vec2 a) { return sqrtf(vdot(a,a)); }
Vec2 vnormalize(Vec2 a) { float l=vlen(a); return l<0.0001f?vec2(0,0):vmul(a,1.0f/l); }

void phys_integrate(RigidBody *b, Vec2 accel, float dt) {
    if (!b || b->inv_mass <= 0.0f) return;
    b->vel = vadd(b->vel, vmul(accel, dt));
    float d = clampf_local(1.0f - b->damping * dt, 0.0f, 1.0f);
    b->vel = vmul(b->vel, d);
    b->pos = vadd(b->pos, vmul(b->vel, dt));
}

void phys_confine_box(RigidBody *b, float half_w, float half_h,
                      float min_x, float min_y, float max_x, float max_y) {
    if (!b) return;
    if (b->pos.x-half_w < min_x) { b->pos.x=min_x+half_w; if (b->vel.x<0) b->vel.x=-b->vel.x*b->restitution; }
    else if (b->pos.x+half_w > max_x) { b->pos.x=max_x-half_w; if (b->vel.x>0) b->vel.x=-b->vel.x*b->restitution; }
    if (b->pos.y-half_h < min_y) { b->pos.y=min_y+half_h; if (b->vel.y<0) b->vel.y=-b->vel.y*b->restitution; }
    else if (b->pos.y+half_h > max_y) { b->pos.y=max_y-half_h; if (b->vel.y>0) b->vel.y=-b->vel.y*b->restitution; }
}

bool phys_aabb_overlap(Vec2 a, float ahw, float ahh, Vec2 b, float bhw, float bhh) {
    return fabsf(a.x-b.x) <= ahw+bhw && fabsf(a.y-b.y) <= ahh+bhh;
}

bool phys_resolve_aabb(RigidBody *a, float ahw, float ahh,
                       RigidBody *b, float bhw, float bhh) {
    if (!a || !b || !phys_aabb_overlap(a->pos,ahw,ahh,b->pos,bhw,bhh)) return false;
    float dx=b->pos.x-a->pos.x, px=(ahw+bhw)-fabsf(dx);
    float dy=b->pos.y-a->pos.y, py=(ahh+bhh)-fabsf(dy);
    float inv_sum=a->inv_mass+b->inv_mass;
    if (inv_sum <= 0.0f) return true;
    Vec2 n=vec2(0,0); float penetration;
    if (px<py) { n.x=dx<0?-1.0f:1.0f; penetration=px; }
    else { n.y=dy<0?-1.0f:1.0f; penetration=py; }
    Vec2 correction=vmul(n,penetration/inv_sum);
    a->pos=vsub(a->pos,vmul(correction,a->inv_mass));
    b->pos=vadd(b->pos,vmul(correction,b->inv_mass));
    Vec2 rv=vsub(b->vel,a->vel);
    float van=vdot(rv,n);
    if (van>0.0f) return true;
    float e=a->restitution<b->restitution?a->restitution:b->restitution;
    float j=-(1.0f+e)*van/inv_sum;
    Vec2 imp=vmul(n,j);
    a->vel=vsub(a->vel,vmul(imp,a->inv_mass));
    b->vel=vadd(b->vel,vmul(imp,b->inv_mass));
    return true;
}

void phys_rope_constraint(RigidBody *body, Vec2 anchor, float max_length,
                          float stiffness, float damping, float dt) {
    if (!body || body->inv_mass<=0.0f) return;
    Vec2 delta=vsub(body->pos,anchor);
    float dist=vlen(delta);
    if (dist<=max_length || dist<0.0001f) return;
    Vec2 n=vmul(delta,1.0f/dist);
    float stretch=dist-max_length;
    float radial=vdot(body->vel,n);
    float force=stiffness*stretch+damping*radial;
    if (force<0.0f) force=0.0f;
    body->vel=vadd(body->vel,vmul(n,-force*body->inv_mass*dt));

    float hard=max_length+10.0f;
    if (dist>hard) {
        body->pos=vadd(anchor,vmul(n,hard));
        float outward=vdot(body->vel,n);
        if (outward>0.0f) body->vel=vsub(body->vel,vmul(n,outward));
    }
}

bool segment_aabb(Vec2 p0, Vec2 p1, Vec2 center, float half, float *out_t) {
    Vec2 d=vsub(p1,p0);
    float tmin=0.0f, tmax=1.0f;
    float minx=center.x-half, maxx=center.x+half;
    float miny=center.y-half, maxy=center.y+half;

    if (fabsf(d.x)<0.00001f) {
        if (p0.x<minx || p0.x>maxx) return false;
    } else {
        float inv=1.0f/d.x;
        float t1=(minx-p0.x)*inv, t2=(maxx-p0.x)*inv;
        if (t1>t2) { float q=t1; t1=t2; t2=q; }
        if (t1>tmin) tmin=t1;
        if (t2<tmax) tmax=t2;
        if (tmin>tmax) return false;
    }

    if (fabsf(d.y)<0.00001f) {
        if (p0.y<miny || p0.y>maxy) return false;
    } else {
        float inv=1.0f/d.y;
        float t1=(miny-p0.y)*inv, t2=(maxy-p0.y)*inv;
        if (t1>t2) { float q=t1; t1=t2; t2=q; }
        if (t1>tmin) tmin=t1;
        if (t2<tmax) tmax=t2;
        if (tmin>tmax) return false;
    }

    if (out_t) *out_t=tmin;
    return true;
}
