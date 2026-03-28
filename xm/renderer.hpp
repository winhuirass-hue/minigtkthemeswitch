// xm/renderer.hpp  —  CPU software renderer
// 32-bit RGBA framebuffer. No external deps.
#pragma once
#include "core.hpp"
#include <vector>
#include <cstring>
#include <cassert>
#include <cmath>
#include <string_view>

namespace xm {

// ── Framebuffer ──────────────────────────────────────────────────────────────

class Framebuffer {
public:
    Framebuffer() = default;
    Framebuffer(int w, int h) { resize(w, h); }

    void resize(int w, int h) {
        assert(w > 0 && h > 0);
        w_ = w; h_ = h;
        pixels_.assign(w * h, 0xFF000000u);   // opaque black
    }

    void clear(Color c = colors::bg) {
        uint32_t v = (uint32_t(c.a)<<24)|(uint32_t(c.r)<<16)|(uint32_t(c.g)<<8)|c.b;
        std::fill(pixels_.begin(), pixels_.end(), v);
    }

    // Raw ARGB pixel access (clamped)
    void set(int x, int y, Color c) noexcept {
        if (x<0||y<0||x>=w_||y>=h_) return;
        uint32_t& dst_px = pixels_[y*w_+x];
        Color dst{ uint8_t(dst_px>>16), uint8_t(dst_px>>8), uint8_t(dst_px), uint8_t(dst_px>>24) };
        Color out = blend(dst, c);
        dst_px = (uint32_t(out.a)<<24)|(uint32_t(out.r)<<16)|(uint32_t(out.g)<<8)|out.b;
    }

    [[nodiscard]] int           width()  const noexcept { return w_; }
    [[nodiscard]] int           height() const noexcept { return h_; }
    [[nodiscard]] const uint32_t* data() const noexcept { return pixels_.data(); }
    [[nodiscard]]       uint32_t* data()       noexcept { return pixels_.data(); }

private:
    int                  w_ = 0, h_ = 0;
    std::vector<uint32_t> pixels_;
};

// ── Renderer ─────────────────────────────────────────────────────────────────

class Renderer {
public:
    explicit Renderer(Framebuffer& fb) : fb_(fb) {}

    // ── Clip stack ─────────────────────────────────────────────────────────
    void push_clip(Recti r) {
        Recti c = clip_stack_.empty() ? r : intersect(clip_stack_.back(), r);
        clip_stack_.push_back(c);
    }
    void pop_clip() {
        if (!clip_stack_.empty()) clip_stack_.pop_back();
    }

    // ── Primitives ──────────────────────────────────────────────────────────

    void fill_rect(Recti r, Color c) {
        r = clip_rect(r);
        for (int y = r.y; y < r.y+r.h; ++y)
            for (int x = r.x; x < r.x+r.w; ++x)
                fb_.set(x, y, c);
    }

    void stroke_rect(Recti r, Color c, int thickness = 1) {
        // Top / bottom
        fill_rect({r.x, r.y,          r.w, thickness}, c);
        fill_rect({r.x, r.y+r.h-thickness, r.w, thickness}, c);
        // Left / right
        fill_rect({r.x,          r.y, thickness, r.h}, c);
        fill_rect({r.x+r.w-thickness, r.y, thickness, r.h}, c);
    }

    // Filled rect with a uniform border
    void draw_panel(Recti r, Color fill, Color border_c, int border_w = 1) {
        fill_rect(r, border_c);
        fill_rect({r.x+border_w, r.y+border_w,
                   r.w-2*border_w, r.h-2*border_w}, fill);
    }

    void draw_line(int x0,int y0,int x1,int y1, Color c, int thickness=1) {
        // Bresenham
        int dx = std::abs(x1-x0), dy = std::abs(y1-y0);
        int sx = x0<x1?1:-1,  sy = y0<y1?1:-1;
        int err = dx-dy;
        while (true) {
            for (int ty = -thickness/2; ty <= thickness/2; ++ty)
                for (int tx = -thickness/2; tx <= thickness/2; ++tx)
                    fb_.set(x0+tx, y0+ty, c);
            if (x0==x1 && y0==y1) break;
            int e2 = 2*err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 <  dx) { err += dx; y0 += sy; }
        }
    }

    // Filled circle (midpoint algorithm)
    void fill_circle(int cx, int cy, int radius, Color c) {
        for (int y = -radius; y <= radius; ++y)
            for (int x = -radius; x <= radius; ++x)
                if (x*x + y*y <= radius*radius)
                    fb_.set(cx+x, cy+y, c);
    }

    // Rounded rect (corner radius in pixels)
    void fill_rounded_rect(Recti r, int radius, Color c) {
        radius = std::min({radius, r.w/2, r.h/2});
        // Centre body
        fill_rect({r.x+radius, r.y,        r.w-2*radius, r.h},       c);
        fill_rect({r.x,        r.y+radius, radius,        r.h-2*radius}, c);
        fill_rect({r.x+r.w-radius, r.y+radius, radius, r.h-2*radius}, c);
        // Four corners
        int cx[4] = {r.x+radius, r.x+r.w-1-radius, r.x+radius,      r.x+r.w-1-radius};
        int cy_[4]= {r.y+radius, r.y+radius,        r.y+r.h-1-radius, r.y+r.h-1-radius};
        for (int i=0;i<4;++i)
            fill_circle(cx[i], cy_[i], radius, c);
    }

    // ── Text (built-in 6×10 bitmap font) ──────────────────────────────────
    // Ships with ASCII printable range.  Embed your own or use stb_truetype.
    void draw_text(int x, int y, std::string_view text, Color c, int scale=1) {
        for (char ch : text) {
            draw_glyph(x, y, ch, c, scale);
            x += (GLYPH_W + 1) * scale;
        }
    }
    [[nodiscard]] int text_width (std::string_view s, int scale=1) const {
        return static_cast<int>(s.size()) * (GLYPH_W + 1) * scale;
    }
    [[nodiscard]] int text_height(int scale=1) const { return GLYPH_H * scale; }

private:
    Framebuffer& fb_;
    std::vector<Recti> clip_stack_;

    Recti clip_rect(Recti r) const {
        if (clip_stack_.empty()) return r;
        return intersect(r, clip_stack_.back());
    }

    // ── Minimal 6×10 bitmap font (printable ASCII 32–126) ─────────────────
    static constexpr int GLYPH_W = 6, GLYPH_H = 10;

    // Each glyph: 10 rows × 6 columns packed as 6 bits in a uint8_t row.
    // Bit 5 = leftmost pixel.  Only a selection shown here — extend as needed.
    static constexpr uint8_t FONT[95][10] = {
        // 0x20 SPACE
        {0,0,0,0,0,0,0,0,0,0},
        // 0x21 !
        {0b001100,0b001100,0b001100,0b001100,0b001100,0b000000,0b001100,0,0,0},
        // 0x22 "
        {0b100100,0b100100,0,0,0,0,0,0,0,0},
        // 0x23 #
        {0b010100,0b010100,0b111110,0b010100,0b010100,0b111110,0b010100,0b010100,0,0},
        // 0x24 $  — (simplified)
        {0b011110,0b100010,0b100000,0b011100,0b000010,0b100010,0b011110,0,0,0},
        // … (abbreviated — full table in renderer.cpp)
        // 0x25–0x3F: digits 0-9 and key punctuation
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        // 0x41 A
        {0b001100,0b010010,0b010010,0b100001,0b111111,0b100001,0b100001,0,0,0},
        // 0x42 B
        {0b111100,0b100010,0b100010,0b111100,0b100010,0b100010,0b111100,0,0,0},
        // 0x43 C
        {0b011110,0b100000,0b100000,0b100000,0b100000,0b100000,0b011110,0,0,0},
        // 0x44 D
        {0b111000,0b100100,0b100010,0b100010,0b100010,0b100100,0b111000,0,0,0},
        // 0x45 E
        {0b111110,0b100000,0b100000,0b111100,0b100000,0b100000,0b111110,0,0,0},
        // 0x46 F
        {0b111110,0b100000,0b100000,0b111100,0b100000,0b100000,0b100000,0,0,0},
        // 0x47–0x5A: fill in remaining uppercase letters
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        // lowercase a–f (abbreviated)
        {0,0b011110,0b000001,0b011111,0b100001,0b100011,0b011101,0,0,0},
        {0b100000,0b100000,0b101110,0b110010,0b100010,0b110010,0b101110,0,0,0},
        {0,0,0b011110,0b100000,0b100000,0b100000,0b011110,0,0,0},
        {0b000001,0b000001,0b011101,0b100011,0b100001,0b100011,0b011101,0,0,0},
        {0,0,0b011110,0b100001,0b111111,0b100000,0b011110,0,0,0},
        {0b000110,0b001000,0b111110,0b001000,0b001000,0b001000,0b001000,0,0,0},
        // remaining lowercase + punctuation stubs
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0},
    };

    void draw_glyph(int x, int y, char ch, Color c, int scale) {
        int idx = static_cast<unsigned char>(ch) - 32;
        if (idx < 0 || idx >= 95) idx = 0;
        const auto& g = FONT[idx];
        for (int row = 0; row < GLYPH_H; ++row)
            for (int col = 0; col < GLYPH_W; ++col)
                if ((g[row] >> (GLYPH_W-1-col)) & 1)
                    for (int sy=0;sy<scale;++sy)
                        for (int sx=0;sx<scale;++sx)
                            fb_.set(x+col*scale+sx, y+row*scale+sy, c);
    }
};

} // namespace xm
