#include "../xm/window.hpp"
#include <../xm/widgets.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

// ====================================================================
//  C SECTION — everything needed to apply GTK2/GTK3/GTK4 themes
// ====================================================================

extern "C" {

// escape shell-sensitive characters
static void escape_str(char* out, const char* in) {
    while (*in) {
        if (*in == '"' || *in == '\\')
            *out++ = '\\';
        *out++ = *in++;
    }
    *out = 0;
}

// Apply GTK themes for GTK2, GTK3, GTK4 (GNOME) without GTK libraries
int apply_gtk_theme(const char* theme_raw) {
    if (!theme_raw || !*theme_raw)
        return -1;

    char theme[256];
    escape_str(theme, theme_raw);

    const char* home = getenv("HOME");
    if (!home) return -2;

    // ------------------------------------------------ GTK2 (gtkrc-2.0)
    {
        std::string path = std::string(home) + "/.gtkrc-2.0";
        FILE* f = fopen(path.c_str(), "w");
        if (f) {
            fprintf(f, "gtk-theme-name=\"%s\"\n", theme);
            fclose(f);
        }
    }

    // ------------------------------------------------ GTK3 (settings.ini)
    {
        std::string dir  = std::string(home) + "/.config/gtk-3.0";
        std::string path = dir + "/settings.ini";
        std::filesystem::create_directories(dir);

        FILE* f = fopen(path.c_str(), "w");
        if (f) {
            fprintf(f,
                "[Settings]\n"
                "gtk-theme-name=%s\n",
                theme
            );
            fclose(f);
        }
    }

    // ------------------------------------------------ GTK4 / GNOME / Mutter
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "gsettings set org.gnome.desktop.interface gtk-theme \"%s\"",
            theme
        );
        system(cmd);
    }

    return 0;
}

} // extern "C"

// ====================================================================
//  C++17 SECTION — scans GTK themes for Xments GUI
// ====================================================================

static bool is_valid_gtk_theme(const std::filesystem::path& p) {
    if (!std::filesystem::is_directory(p)) return false;

    return std::filesystem::exists(p / "gtk-2.0") ||
           std::filesystem::exists(p / "gtk-3.0") ||
           std::filesystem::exists(p / "gtk-4.0");
}

static std::vector<std::string> list_gtk_themes() {
    std::vector<std::string> out;

    const char* home = std::getenv("HOME");
    std::filesystem::path usr_dir  = "/usr/share/themes";
    std::filesystem::path home_dir = home ? std::filesystem::path(home) / ".themes"
                                          : std::filesystem::path();

    auto scan = const std::filesystem::path& base {
        if (!std::filesystem::exists(base)) return;
        for (auto& e : std::filesystem::directory_iterator(base)) {
            if (is_valid_gtk_theme(e.path()))
                out.push_back(e.path().filename().string());
        }
    };

    scan(usr_dir);
    scan(home_dir);

    return out;
}

// ====================================================================
//                           XMENTS UI
// ====================================================================

int main() {
    xm::Window::Config cfg;
    cfg.title      = "Xments — GTK2/3/4 Theme Switcher";
    cfg.width      = 820;
    cfg.height     = 480;
    cfg.resizable  = true;
    cfg.target_fps = 60;

    xm::Window win(cfg);

    xm::Style style;

    bool running = true;
    std::string status = "Ready";

    // Load GTK theme list
    std::vector<std::string> gtk_themes = list_gtk_themes();
    if (gtk_themes.empty())
        gtk_themes.push_back("No GTK themes found");

    int gtk_theme_index = 0;

    // Xments accent preview
    xm::Color accent = style.accent;

    // MAIN LOOP
    win.run(xm::Renderer &r, xm::UI &ui, const xm::InputState &in {

        if (!running) return;

        ui.set_style(style);

        const int W  = r.fb().width();
        const int H  = r.fb().height();
        const int HH = style.header_bar_h;
        const int SH = style.status_bar_h;

        // ------------------------------------------------ Header Bar
        xm::HeaderBar hb(r, in, style);
        hb.begin(0, 0, W);
        hb.title("GTK Theme Switcher");
        if (hb.button("Reload")) {
            gtk_themes = list_gtk_themes();
            if (gtk_themes.empty())
                gtk_themes.push_back("No GTK themes found");
            gtk_theme_index = 0;
            status = "Reloaded GTK theme list";
        }
        if (hb.button("Quit")) running = false;
        hb.end();

        // ------------------------------------------------ Status Bar
        xm::StatusBar sb(r, in, style);
        sb.begin(0, H - SH, W);
        sb.color_dot(style.accent);
        sb.text(status);
        sb.text_right(std::to_string(W) + "×" + std::to_string(H));
        sb.end();

        // ------------------------------------------------ Left UI
        ui.set_cursor(20, HH + 20);

        ui.label("GTK Themes (GTK2 / GTK3 / GTK4):", style.accent);

        if (ui.dropdown(gtk_themes, gtk_theme_index, 260)) {
            status = "Selected: " + gtk_themes[gtk_theme_index];
        }

        ui.spacing(16);

        if (ui.button("Apply GTK Theme", 220)) {
            int ok = apply_gtk_theme(gtk_themes[gtk_theme_index].c_str());
            status = (ok == 0)
                ? "Applied GTK theme: " + gtk_themes[gtk_theme_index]
                : "Failed to apply GTK theme";
        }

        ui.spacing(20);
        ui.separator(260);

        ui.label("Xments Accent Color:");
        if (ui.color_picker(accent)) {
            style.accent = accent;
            status = "Updated Xments accent color";
        }

        // ------------------------------------------------ Preview Panel
        int bx = 340;
        int by = HH + 40;
        int bw = 440;
        int bh = 320;

        r.fill_rounded_rect({bx, by, bw, bh}, 12, style.surface);
        r.stroke_rect({bx, by, bw, bh}, style.border);

        r.draw_text(bx + 16, by + 30, "GTK Theme Preview", style.text);

        // Theme name
        r.draw_text(bx + 16, by + 70,
            "Selected GTK theme: " + gtk_themes[gtk_theme_index],
            style.text
        );

        // Accent color
        r.fill_rounded_rect({bx + 16, by + 110, 60, 60}, 6, style.accent);
        r.draw_text(bx + 90, by + 142, "Xments accent", style.text);

        // Surface/border
        r.fill_rounded_rect({bx + 16, by + 190, 60, 60}, 6, style.surface);
        r.stroke_rect({bx + 16, by + 190, 60, 60}, style.border);
        r.draw_text(bx + 90, by + 222, "surface / border", style.text_muted);

        // ------------------------------------------------ Context menu
        if (ui.begin_context_menu()) {
            if (ui.context_menu_item("Reload themes")) {
                gtk_themes = list_gtk_themes();
                if (gtk_themes.empty())
                    gtk_themes.push_back("No GTK themes found");
                gtk_theme_index = 0;
                status = "Theme list refreshed";
            }
            ui.end_context_menu();
        }

    }, style);

    return 0;
}
