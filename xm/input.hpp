// xm/input.hpp  —  Input state snapshot (keyboard + mouse)
// Designed for immediate-mode polling (no callbacks needed in widget code).
#pragma once
#include "core.hpp"
#include <cstring>
#include <string>

namespace xm {

enum class Key : uint8_t {
    None=0,
    // Printable range mirrors ASCII 32–126 (cast char → Key)
    Escape=27, Enter=13, Backspace=8, Tab=9, Space=32,
    Left=128, Right, Up, Down,
    Delete, Home, End, PageUp, PageDown,
    F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,F11,F12,
    LeftShift,RightShift,LeftCtrl,RightCtrl,LeftAlt,RightAlt,
    COUNT
};

enum class MouseButton : uint8_t { Left=0, Right=1, Middle=2 };

struct InputState {
    // ── Mouse ────────────────────────────────────────────────────────────
    int  mouse_x = 0, mouse_y = 0;
    int  scroll_delta = 0;        // +1 up, -1 down
    bool mouse_down  [3] = {};    // currently held
    bool mouse_press [3] = {};    // went down this frame
    bool mouse_rel   [3] = {};    // went up   this frame

    // ── Keyboard ─────────────────────────────────────────────────────────
    bool key_down    [static_cast<int>(Key::COUNT)] = {};
    bool key_press   [static_cast<int>(Key::COUNT)] = {};
    bool key_rel     [static_cast<int>(Key::COUNT)] = {};
    std::string text_input;       // UTF-8 text entered this frame

    // ── Frame bookkeeping ─────────────────────────────────────────────────
    double frame_time  = 0.0;     // absolute time in seconds
    double delta_time  = 0.0;     // seconds since last frame

    // ── Convenient queries ────────────────────────────────────────────────
    [[nodiscard]] bool pressed (Key k) const { return key_press [int(k)]; }
    [[nodiscard]] bool released(Key k) const { return key_rel   [int(k)]; }
    [[nodiscard]] bool held    (Key k) const { return key_down  [int(k)]; }

    [[nodiscard]] bool clicked (MouseButton b=MouseButton::Left) const {
        return mouse_press[int(b)];
    }
    [[nodiscard]] bool mouse_in(Recti r) const {
        return contains(r, mouse_x, mouse_y);
    }
    [[nodiscard]] bool hovered (Recti r) const { return mouse_in(r); }
    [[nodiscard]] bool clicked_in(Recti r, MouseButton b=MouseButton::Left) const {
        return clicked(b) && mouse_in(r);
    }

    // Call at start of each frame to shift press/release state
    void begin_frame(double t) {
        frame_time  = t;
        delta_time  = t - last_time_;
        last_time_  = t;
        std::memset(mouse_press, 0, sizeof mouse_press);
        std::memset(mouse_rel,   0, sizeof mouse_rel);
        std::memset(key_press,   0, sizeof key_press);
        std::memset(key_rel,     0, sizeof key_rel);
        text_input.clear();
        scroll_delta = 0;
    }

    // Platform calls these to inject events
    void on_mouse_move  (int x, int y) { mouse_x=x; mouse_y=y; }
    void on_mouse_down  (MouseButton b) {
        int i=int(b);
        if (!mouse_down[i]) { mouse_press[i]=true; }
        mouse_down[i]=true;
    }
    void on_mouse_up    (MouseButton b) {
        int i=int(b);
        if (mouse_down[i]) { mouse_rel[i]=true; }
        mouse_down[i]=false;
    }
    void on_scroll      (int delta) { scroll_delta += delta; }
    void on_key_down    (Key k) {
        int i=int(k);
        if (!key_down[i]) key_press[i]=true;
        key_down[i]=true;
    }
    void on_key_up      (Key k) {
        int i=int(k);
        if (key_down[i]) key_rel[i]=true;
        key_down[i]=false;
    }
    void on_text_input  (std::string_view s) { text_input += s; }

private:
    double last_time_ = 0.0;
};

} // namespace xm
