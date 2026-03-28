// xm/window.hpp  —  SDL2 platform window
// Fixes: missing blit, wrong MouseButton casts, HiDPI, missing widgets include,
//        no frame cap, SDL_WINDOWEVENT missing break.
#pragma once
#include "widgets.hpp"   // pulls renderer + input + anim transitively
#include <functional>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <SDL2/SDL.h>

namespace xm {

class Window {
public:
    struct Config {
        const char* title      = "Xments";
        int         width      = 800;
        int         height     = 600;
        bool        resizable  = true;
        int         target_fps = 60;
    };

    explicit Window(const Config& cfg = {})
        : cfg_(cfg)
    {
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
            throw std::runtime_error(SDL_GetError());

        uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
        if (cfg.resizable) flags |= SDL_WINDOW_RESIZABLE;

        win_ = SDL_CreateWindow(cfg.title,
                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                cfg.width, cfg.height, flags);
        if (!win_) throw std::runtime_error(SDL_GetError());

        refresh_size();
        surf_ = SDL_GetWindowSurface(win_);
        fb_.resize(fb_w_, fb_h_);
    }

    ~Window() {
        if (win_) SDL_DestroyWindow(win_);
        SDL_Quit();
    }
    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    using FrameFn = std::function<void(Renderer&, UI&, const InputState&)>;

    void run(FrameFn frame, const Style& style = {}) {
        const int frame_ms = 1000 / std::max(1, cfg_.target_fps);
        bool running = true;

        while (running) {
            uint64_t t0 = SDL_GetTicks64();
            input_.begin_frame(double(t0) / 1000.0);

            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                switch (ev.type) {

                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_WINDOWEVENT:
                    if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                        refresh_size();
                        fb_.resize(fb_w_, fb_h_);
                        surf_ = SDL_GetWindowSurface(win_);
                    }
                    break;                              // ← was missing

                case SDL_MOUSEMOTION:
                    input_.on_mouse_move(
                        int(ev.motion.x * scale_x()),
                        int(ev.motion.y * scale_y()));
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    input_.on_mouse_down(sdl_btn(ev.button.button));
                    break;

                case SDL_MOUSEBUTTONUP:
                    input_.on_mouse_up(sdl_btn(ev.button.button));
                    break;

                case SDL_MOUSEWHEEL:
                    input_.on_scroll(ev.wheel.y);
                    break;

                case SDL_KEYDOWN:
                    input_.on_key_down(sdl_key(ev.key.keysym.sym));
                    break;

                case SDL_KEYUP:
                    input_.on_key_up(sdl_key(ev.key.keysym.sym));
                    break;

                case SDL_TEXTINPUT:
                    input_.on_text_input(ev.text.text);
                    break;
                }
            }
            if (!running) break;

            // ── Render ───────────────────────────────────────────────────
            fb_.clear(style.bg);
            {
                Renderer rend(fb_);
                UI       ui(rend, input_, style);
                frame(rend, ui, input_);
            }

            // ── Blit framebuffer → SDL surface ────────────────────────────
            // Try fast path first: formats match → one memcpy per row
            SDL_LockSurface(surf_);
            const uint32_t* src   = fb_.data();
            auto*           dst   = static_cast<uint8_t*>(surf_->pixels);
            const int       pitch = surf_->pitch;

            bool fast = (surf_->format->BytesPerPixel == 4 &&
                         surf_->format->Rmask == 0x00FF0000u &&
                         surf_->format->Gmask == 0x0000FF00u &&
                         surf_->format->Bmask == 0x000000FFu);

            for (int y = 0; y < fb_h_; ++y) {
                uint32_t* row = reinterpret_cast<uint32_t*>(dst + y*pitch);
                if (fast) {
                    std::memcpy(row, src + y*fb_w_, size_t(fb_w_)*4);
                } else {
                    for (int x = 0; x < fb_w_; ++x) {
                        uint32_t px = src[y*fb_w_+x];
                        row[x] = SDL_MapRGB(surf_->format,
                                            uint8_t(px>>16),
                                            uint8_t(px>> 8),
                                            uint8_t(px));
                    }
                }
            }
            SDL_UnlockSurface(surf_);
            SDL_UpdateWindowSurface(win_);

            // ── Frame cap ────────────────────────────────────────────────
            int elapsed = int(SDL_GetTicks64() - t0);
            if (elapsed < frame_ms) SDL_Delay(frame_ms - elapsed);
        }
    }

    [[nodiscard]] int fb_width()  const { return fb_w_; }
    [[nodiscard]] int fb_height() const { return fb_h_; }

private:
    Config        cfg_;
    SDL_Window*   win_  = nullptr;
    SDL_Surface*  surf_ = nullptr;
    Framebuffer   fb_;
    InputState    input_;
    int           fb_w_=0, fb_h_=0, log_w_=0, log_h_=0;

    void refresh_size() {
        SDL_GL_GetDrawableSize(win_, &fb_w_, &fb_h_);
        SDL_GetWindowSize     (win_, &log_w_, &log_h_);
        if (fb_w_ <= 0) fb_w_ = log_w_;
        if (fb_h_ <= 0) fb_h_ = log_h_;
    }
    [[nodiscard]] float scale_x() const {
        return log_w_>0 ? float(fb_w_)/float(log_w_) : 1.f;
    }
    [[nodiscard]] float scale_y() const {
        return log_h_>0 ? float(fb_h_)/float(log_h_) : 1.f;
    }

    // ── SDL → Xments enum conversions ────────────────────────────────────
    static MouseButton sdl_btn(uint8_t b) {
        switch (b) {
        case SDL_BUTTON_RIGHT:  return MouseButton::Right;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        default:                return MouseButton::Left;
        }
    }

    static Key sdl_key(SDL_Keycode k) {
        switch (k) {
        case SDLK_ESCAPE:    return Key::Escape;
        case SDLK_RETURN:    return Key::Enter;
        case SDLK_BACKSPACE: return Key::Backspace;
        case SDLK_TAB:       return Key::Tab;
        case SDLK_LEFT:      return Key::Left;
        case SDLK_RIGHT:     return Key::Right;
        case SDLK_UP:        return Key::Up;
        case SDLK_DOWN:      return Key::Down;
        case SDLK_DELETE:    return Key::Delete;
        case SDLK_HOME:      return Key::Home;
        case SDLK_END:       return Key::End;
        default:
            if (k>=32 && k<127) return Key(uint8_t(k));
            return Key::None;
        }
    }
};

} // namespace xm