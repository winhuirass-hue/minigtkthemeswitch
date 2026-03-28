// xm/core.hpp  —  Xments core primitives
// C++17. No dependencies.
#pragma once
#include <cstdint>
#include <algorithm>
#include <string_view>

namespace xm {

// ── Geometry ────────────────────────────────────────────────────────────────

struct Vec2  { float x = 0, y = 0; };
struct Recti { int x = 0, y = 0, w = 0, h = 0; };
struct Rectf { float x = 0, y = 0, w = 0, h = 0; };

[[nodiscard]] inline bool contains(Recti r, int px, int py) noexcept {
    return px >= r.x && px < r.x + r.w &&
           py >= r.y && py < r.y + r.h;
}
[[nodiscard]] inline Recti intersect(Recti a, Recti b) noexcept {
    int x1 = std::max(a.x, b.x),  y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x+a.w, b.x+b.w), y2 = std::min(a.y+a.h, b.y+b.h);
    if (x2 <= x1 || y2 <= y1) return {};
    return {x1, y1, x2-x1, y2-y1};
}

// ── Color ────────────────────────────────────────────────────────────────────

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    [[nodiscard]] static constexpr Color rgb (uint8_t r,uint8_t g,uint8_t b)           noexcept { return {r,g,b,255}; }
    [[nodiscard]] static constexpr Color rgba(uint8_t r,uint8_t g,uint8_t b,uint8_t a) noexcept { return {r,g,b,a};   }
    [[nodiscard]] static constexpr Color hex (uint32_t v) noexcept {
        return { uint8_t(v>>16), uint8_t(v>>8), uint8_t(v), 255 };
    }
    [[nodiscard]] constexpr uint32_t pack_rgba() const noexcept {
        return (uint32_t(r)<<24)|(uint32_t(g)<<16)|(uint32_t(b)<<8)|a;
    }
    [[nodiscard]] constexpr uint32_t pack_argb() const noexcept {
        return (uint32_t(a)<<24)|(uint32_t(r)<<16)|(uint32_t(g)<<8)|b;
    }
};

// Built-in palette  (simple, readable names)
namespace colors {
    inline constexpr Color white        = Color::hex(0xFFFFFF);
    inline constexpr Color black        = Color::hex(0x000000);
    inline constexpr Color transparent  = {0,0,0,0};
    inline constexpr Color red          = Color::hex(0xE24B4A);
    inline constexpr Color green        = Color::hex(0x639922);
    inline constexpr Color blue         = Color::hex(0x378ADD);
    inline constexpr Color amber        = Color::hex(0xEF9F27);
    inline constexpr Color teal         = Color::hex(0x1D9E75);
    inline constexpr Color gray         = Color::hex(0x888780);
    inline constexpr Color light_gray   = Color::hex(0xD3D1C7);
    inline constexpr Color dark_gray    = Color::hex(0x444441);
    // UI semantic
    inline constexpr Color bg           = Color::hex(0xF5F5F0);
    inline constexpr Color bg_dark      = Color::hex(0x1E1E1C);
    inline constexpr Color surface      = Color::hex(0xFFFFFF);
    inline constexpr Color surface_dark = Color::hex(0x2C2C2A);
    inline constexpr Color text         = Color::hex(0x1A1A18);
    inline constexpr Color text_dark    = Color::hex(0xE8E6DC);
    inline constexpr Color border       = Color::hex(0xC8C6BC);
    inline constexpr Color accent       = Color::hex(0x534AB7);   // purple
}

// ── Alpha blend (Porter-Duff src-over) ─────────────────────────────────────

[[nodiscard]] inline Color blend(Color dst, Color src) noexcept {
    if (src.a == 255) return src;
    if (src.a == 0)   return dst;
    const uint32_t a = src.a, ia = 255 - a;
    return {
        uint8_t((src.r * a + dst.r * ia) / 255),
        uint8_t((src.g * a + dst.g * ia) / 255),
        uint8_t((src.b * a + dst.b * ia) / 255),
        uint8_t(src.a + (dst.a * ia) / 255)
    };
}

} // namespace xm
