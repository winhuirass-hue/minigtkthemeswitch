// xm/anim.hpp  —  Lightweight animation engine
// C++17. No deps. Color lives in core.hpp — not redefined here.
#pragma once
#include "core.hpp"
#include <cmath>
#include <unordered_map>
#include <algorithm>

namespace xm {

// ── Easing ───────────────────────────────────────────────────────────────────
namespace ease {
    inline float linear  (float t) { return t; }
    inline float in_quad (float t) { return t*t; }
    inline float out_quad(float t) { return t*(2.f-t); }
    inline float in_out  (float t) {
        return t < .5f ? 2.f*t*t : -1.f+(4.f-2.f*t)*t;
    }
    inline float out_expo(float t) {
        return t >= 1.f ? 1.f : 1.f - std::pow(2.f, -10.f*t);
    }
    inline float out_back(float t) {
        constexpr float c = 1.70158f, c1 = c+1.f;
        return 1.f + c1*(t-1.f)*(t-1.f)*(t-1.f) + c*(t-1.f)*(t-1.f);
    }
}

// ── Tween ─────────────────────────────────────────────────────────────────────
struct Tween {
    float from    = 0.f;
    float to      = 0.f;
    float dur     = 0.15f;
    float elapsed = 0.f;
    bool  active  = false;
    float (*ease_fn)(float) = ease::out_quad;

    void start(float target, float duration = 0.15f,
               float(*fn)(float) = ease::out_quad) {
        from    = current();
        to      = target;
        dur     = duration;
        elapsed = 0.f;
        active  = true;
        ease_fn = fn;
    }

    void snap(float v) { from=to=v; elapsed=dur; active=false; }

    bool tick(float dt) {
        if (!active) return false;
        elapsed += dt;
        if (elapsed >= dur) { elapsed=dur; active=false; }
        return active;
    }

    float current() const {
        if (!active || dur <= 0.f) return to;
        float t = std::min(elapsed/dur, 1.f);
        return from + (to-from)*ease_fn(t);
    }

    bool done() const { return !active; }
};

// ── AnimPool ──────────────────────────────────────────────────────────────────
class AnimPool {
public:
    Tween& get(uint32_t id, int ch=0) {
        return pool_[(uint64_t(id)<<8)|uint8_t(ch)];
    }

    void tick_all(float dt) {
        for (auto& [k,tw] : pool_) tw.tick(dt);
    }

    float hover(uint32_t id, bool hov, float dt,
                float di=0.08f, float dout=0.12f)
    {
        auto& tw = get(id,0);
        float tgt = hov ? 1.f : 0.f;
        if (tw.to != tgt) tw.start(tgt, hov?di:dout, ease::out_quad);
        tw.tick(dt);
        return tw.current();
    }

    float press(uint32_t id, bool act, float dt, float dur=0.06f) {
        auto& tw = get(id,1);
        float tgt = act ? 1.f : 0.f;
        if (tw.to != tgt) tw.start(tgt, dur, ease::in_out);
        tw.tick(dt);
        return tw.current();
    }

    float open(uint32_t id, bool is_open, float dt, float dur=0.14f) {
        auto& tw = get(id,2);
        float tgt = is_open ? 1.f : 0.f;
        if (tw.to != tgt)
            tw.start(tgt, dur, is_open ? ease::out_expo : ease::in_out);
        tw.tick(dt);
        return tw.current();
    }

    float select(uint32_t id, float tgt_y, float dt, float dur=0.18f) {
        auto& tw = get(id,3);
        if (tw.to != tgt_y) tw.start(tgt_y, dur, ease::out_back);
        tw.tick(dt);
        return tw.current();
    }

private:
    std::unordered_map<uint64_t, Tween> pool_;
};

// ── Color lerp ────────────────────────────────────────────────────────────────
inline Color lerp_color(Color a, Color b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return {
        uint8_t(a.r + (b.r-a.r)*t),
        uint8_t(a.g + (b.g-a.g)*t),
        uint8_t(a.b + (b.b-a.b)*t),
        uint8_t(a.a + (b.a-a.a)*t),
    };
}

} // namespace xm