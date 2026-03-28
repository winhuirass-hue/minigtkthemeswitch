// demo/main.cpp  —  Xments full demo
// Build: g++ -std=c++17 main.cpp -lSDL2 -o demo
#include "../xm/window.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <filesystem>

int main() {
    xm::Window::Config cfg;
    cfg.title      = "Xments  —  animated UI";
    cfg.width      = 820;
    cfg.height     = 600;
    cfg.resizable  = true;
    cfg.target_fps = 60;

    xm::Window win(cfg);

    // Load external theme if present, otherwise use built-in light
    xm::Style style = std::filesystem::exists("test.xss")
                      ? xm::load_xss("test.xss")
                      : xm::Style{};

    std::string name;
    bool        check    = false;
    float       volume   = 0.4f;
    float       progress = 0.3f;
    int         list_sel = -1;
    int         drop_sel =  0;
    int         v_scroll =  0;
    int         h_scroll =  0;
    bool        running  =  true;   // ← actually used now
    std::string status_msg = "Ready";
    int         frame_count = 0;

    std::vector<std::string> list_items = {
        "renderer.hpp","widgets.hpp","input.hpp","window.hpp",
        "anim.hpp","core.hpp","draw.hpp","layout.hpp",
        "theme.hpp","fonts.hpp","events.hpp","platform.hpp"
    };
    std::vector<std::string> themes = {"Light","Dark","Catppuccin"};

    win.run([&](xm::Renderer& r, xm::UI& ui, const xm::InputState& in) {
        if (!running) return;   // graceful quit path
        ++frame_count;
        ui.set_style(style);

        const int W   = r.fb().width();
        const int H   = r.fb().height();
        const int MH  = style.menu_bar_h;
        const int SH  = style.status_bar_h;
        const int SBW = xm::ScrollBar::W;

        // ── Menu bar ──────────────────────────────────────────────────
        xm::MenuBar mb(r, in, style);
        mb.begin(0, 0, W);

        if (mb.menu("File")) {
            if (mb.item("New"))        status_msg="New file";
            if (mb.item("Open..."))    status_msg="Open...";
            mb.separator();
            if (mb.item("Save"))       status_msg="Saved";
            mb.separator();
            if (mb.item("Quit"))       running=false;
            mb.end_menu();
        }
        if (mb.menu("Edit")) {
            if (mb.item("Undo"))       status_msg="Undo";
            if (mb.item("Redo"))       status_msg="Redo";
            mb.separator();
            if (mb.item("Cut"))        status_msg="Cut";
            if (mb.item("Copy"))       status_msg="Copy";
            if (mb.item("Paste"))      status_msg="Paste";
            mb.end_menu();
        }
        if (mb.menu("View")) {
            if (mb.item("Dark mode"))  style=xm::dark_style();
            if (mb.item("Light mode")) style=xm::Style{};
            if (mb.item("Load theme")) {
                if (std::filesystem::exists("test.xss"))
                    style=xm::load_xss("test.xss");
            }
            mb.end_menu();
        }
        if (mb.menu("Help")) {
            if (mb.item("About"))      status_msg="Xments v0.2";
            mb.end_menu();
        }
        mb.end();

        // ── Status bar ────────────────────────────────────────────────
        xm::StatusBar sb(r, in, style);
        sb.begin(0, H-SH, W);
        sb.color_dot(xm::colors::teal);
        sb.text(status_msg);
        sb.divider();
        sb.progress(progress, 80);
        sb.divider();
        sb.text(std::to_string(int(volume*100))+"%");
        sb.text_right("frame "+std::to_string(frame_count));
        sb.end();

        // ── Scrollbars ────────────────────────────────────────────────
        const int content_h  = 720;
        const int viewport_h = H - MH - SH - SBW - 2;

        xm::ScrollBar vsb(r, in, style);
        int vd = vsb.vertical(W-SBW-1, MH+1, viewport_h, content_h, v_scroll);
        v_scroll = std::clamp(v_scroll+vd, 0, content_h-viewport_h);

        xm::ScrollBar hsb(r, in, style);
        int hd = hsb.horizontal(0, H-SH-SBW-1, W-SBW-1, 1000, h_scroll);
        h_scroll = std::clamp(h_scroll+hd, 0, 200);

        // ── Content viewport ──────────────────────────────────────────
        r.push_clip({0, MH, W-SBW-1, viewport_h});

        int ox = 8 - h_scroll;
        int oy = MH + 8 - v_scroll;

        // Left column
        ui.set_cursor(ox+10, oy+10);
        ui.label("Xments  —  animated widgets", style.accent);
        ui.separator(340);
        ui.label("Name:");
        ui.text_field(name, 300, "type here...");
        if (!name.empty()) ui.label("Hello, "+name+"!", xm::colors::teal);
        ui.spacing(4);
        if (ui.button("Click me", 110))       status_msg="Clicked!";
        ui.same_line();
        if (ui.button_outline("Reset", 100))  { name.clear(); progress=0.f; }
        ui.spacing(4);
        ui.checkbox("Enable feature", check);
        ui.spacing(4);
        ui.label("Volume:");
        ui.slider("vol", volume, 0.f, 1.f, 300);
        ui.spacing(4);
        ui.label("Progress:");
        ui.progress_bar(progress, 300, 14);
        if (ui.button("+ 10 %", 100))
            progress=std::min(1.f,progress+0.1f);
        ui.spacing(4);
        ui.label("Theme preset:");
        if (ui.dropdown(themes, drop_sel, 180)) {
            if (drop_sel==0) style=xm::Style{};
            else if (drop_sel==1) style=xm::dark_style();
            else if (drop_sel==2 && std::filesystem::exists("test.xss"))
                style=xm::load_xss("test.xss");
        }

        // Right column
        ui.set_cursor(ox+370, oy+50);
        ui.label("Source files:");
        if (ui.list_box(list_items, list_sel, 300, 200) >= 0)
            status_msg=list_items[list_sel];
        if (list_sel>=0)
            ui.label("Selected: "+list_items[list_sel], style.accent);

        // Animated sine wave
        {
            int bx=ox+370, by=oy+300, bw=300, bh=80;
            r.fill_rounded_rect({bx,by,bw,bh},6,style.surface);
            r.stroke_rect      ({bx,by,bw,bh},style.border);
            float t=float(in.frame_time);
            for (int x=2;x<bw-2;++x) {
                float v1=std::sin((float(x)/40.f)+t*2.f)*0.5f+0.5f;
                float v2=std::sin((float(x)/20.f)+t*3.f)*0.3f+0.3f;
                r.fb().set(bx+x, by+bh/2-int(v1*(bh/2-8)), style.accent);
                r.fb().set(bx+x, by+bh/2-int(v2*(bh/2-8)), xm::colors::teal);
            }
            r.draw_text(bx+8, by+bh-14, "live sine wave", style.text_muted);
        }

        r.pop_clip();

        // Context menu (right-click)
        if (ui.begin_context_menu()) {
            if (ui.context_menu_item("Copy"))   status_msg="Copy";
            if (ui.context_menu_item("Paste"))  status_msg="Paste";
            if (ui.context_menu_item("Delete")) status_msg="Delete";
            ui.end_context_menu();
        }

    }, style);
}