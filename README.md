# Xments
 
Xments is cross-platform mini GUI toolkit with animations for minimall modern UI
Xments is a small, animated immediate-mode UI toolkit written in C++17.
It uses a software framebuffer renderer and an SDL2 window/event loop.

## Features

 Immediate-mode UI (`xm::UI`) with animated controls
 CPU software renderer (`xm::Renderer`   `xm::Framebuffer`)
 Theming support via simple `*.xss` key/value files
 Widgets:
  - Buttons, outline buttons, labels, separators
  - Checkbox, sliders, text fields, progress bars
  - List box, dropdown, context menu
  - Menu bar, status bar, scrollbars
  - Color picker   channel sliders
  - Framebuffer preview widget
  - CSD-style header bar

## Repository layout

 - `xm/core.hpp` – core math, color, and blend helpers
 - `xm/renderer.hpp` – framebuffer and 2D software renderer
 - `xm/input.hpp` – input snapshot model
 - `xm/anim.hpp` – easing/tween/animation pool
 - `xm/widgets.hpp` – style system   all widgets/UI API
 - `xm/window.hpp` – SDL2 window loop integration
 - `demo/main.cpp` – demo app showcasing widgets/features
 - `demo/themes/test.xss` – sample XSS theme
 
## Build
 
### Requirements
 
 C++17 compiler
 - CMake 3.16 
 - SDL2 development package (headers   CMake config)
 
 ### Configure and build
 
 ```bash
 cmake -S . -B build
 cmake --build build -j
 ```
 
 ## Run demo
 
 ```bash
 ./build/demo
 ```
 
 (Binary name/path may vary by generator/platform.)
 
 ## Theming with XSS
 
 Xments loads themes from plain text files with this format:
 
 ```txt
 key = value
 ```
 
 Example (`demo/themes/test.xss`):
 
 ```txt
 bg          = #1E1E2E
 surface     = #313244
 text        = #CDD6F4
 accent      = #89B4FA
 menu_bar_h  = 26
 status_bar_h= 22
 radius      = 6
 padding     = 8
 font_scale  = 1
 ```
 
 ### Supported XSS keys
 
 Color keys:
 
 - `bg`
 - `surface`
 - `surface_hov`
 - `surface_act`
 - `border`
 - `text`
 - `text_muted`
 - `accent`
 - `accent_hov`
 - `accent_act`
 - `accent_text`
 - `bar_bg`
 - `bar_border`
 
 Metric keys:
 
 - `menu_bar_h`
 - `status_bar_h`
 - `radius`
 - `padding`
 - `font_scale`
 
 Parser behavior:
 
 - ignores empty lines
 - ignores comment-only lines that start with `/*` or `*`
 - supports inline `//` trailing comments
 
 ## Minimal usage sketch
 
 ```cpp
 #include "xm/window.hpp"
 #include "xm/widgets.hpp"
 
 int main() {
     xm::Window win({"Xments", 900, 600, true});
     xm::Style theme = xm::load_xss("demo/themes/test.xss");
 
     win.run([&](xm::Renderer& r, xm::UI& ui, const xm::InputState&) {
         ui.set_style(theme);
         ui.set_cursor(16, 16);
         ui.label("Hello Xments");
     }, theme);
 }
 ```
 
 ## Notes
 
 - The project is intentionally lightweight and header-centric.
 - The software renderer is simple and designed for UI prototyping.
