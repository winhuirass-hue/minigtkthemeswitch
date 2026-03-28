// xm/widgets.hpp  —  Animated immediate-mode widgets
// MenuBar · ScrollBar · StatusBar · load_xss theme loader
// C++17. Deps: renderer.hpp, input.hpp, anim.hpp
#pragma once
#include "renderer.hpp"
#include "input.hpp"
#include "anim.hpp"
#include <string_view>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>

// ── UTF-8 backspace ───────────────────────────────────────────────────────────
namespace xm::utf8 {
    inline void backspace(std::string& s) {
        if (s.empty()) return;
        int i = int(s.size())-1;
        while (i>0 && (uint8_t(s[i])>>6)==0b10) --i;
        s.erase(i);
    }
}

namespace xm {

// ─────────────────────────────────────────────────────────────────────────────
// Style
// ─────────────────────────────────────────────────────────────────────────────
struct Style {
    Color bg          = colors::bg;
    Color surface     = colors::surface;
    Color surface_hov = Color::hex(0xECEAE0);
    Color surface_act = Color::hex(0xDDDBD0);
    Color border      = colors::border;
    Color text        = colors::text;
    Color text_muted  = Color::hex(0x888780);
    Color accent      = colors::accent;
    Color accent_hov  = Color::hex(0x3C3489);
    Color accent_act  = Color::hex(0x2A2466);
    Color accent_text = colors::white;
    Color bar_bg      = Color::hex(0xEAE8DE);
    Color bar_border  = Color::hex(0xC8C6BC);
    int   menu_bar_h   = 26;
    int   status_bar_h = 22;
    int   radius       = 5;
    int   padding      = 8;
    int   font_scale   = 1;
};

inline Style dark_style() {
    Style s;
    s.bg          = colors::bg_dark;
    s.surface     = colors::surface_dark;
    s.surface_hov = Color::hex(0x3A3A38);
    s.surface_act = Color::hex(0x4A4A48);
    s.border      = Color::hex(0x55534F);
    s.text        = colors::text_dark;
    s.text_muted  = Color::hex(0x888780);
    s.accent      = Color::hex(0x7F77DD);
    s.accent_hov  = Color::hex(0xAFA9EC);
    s.accent_act  = Color::hex(0x6B64C6);
    s.accent_text = colors::black;
    s.bar_bg      = Color::hex(0x252523);
    s.bar_border  = Color::hex(0x3A3A38);
    return s;
}

// ── XSS theme file loader ─────────────────────────────────────────────────────
// Format:  key = #RRGGBB   (inline # comments after value are stripped)
//          key = integer
// Lines starting with # or / are skipped entirely.
inline Style load_xss(const std::string& path) {
    Style s;
    std::ifstream file(path);
    if (!file.is_open()) return s;

    auto strip_comment = [](std::string& val) {
        // Drop everything from first unquoted '#' onward
        auto pos = val.find('#', 1);   // skip leading '#' of hex color
        if (pos != std::string::npos) val.erase(pos);
        // Trim trailing whitespace
        while (!val.empty() && std::isspace(uint8_t(val.back()))) val.pop_back();
    };

    auto parse_color = [](const std::string& hex) -> Color {
        if (hex.size() < 2 || hex[0] != '#') return colors::transparent;
        uint32_t v = 0;
        try { v = std::stoul(hex.substr(1), nullptr, 16); }
        catch (...) { return colors::transparent; }
        return Color::rgba(uint8_t(v>>16), uint8_t(v>>8), uint8_t(v), 255);
    };

    std::string line;
    while (std::getline(file, line)) {
        // Skip blank lines and full-line comments
        if (line.empty() || line[0]=='#' || line[0]=='/') continue;

        std::istringstream iss(line);
        std::string key, eq, val;
        if (!(iss >> key >> eq >> val) || eq != "=") continue;

        strip_comment(val);
        if (val.empty()) continue;

        bool is_hex = (val[0]=='#');

        if      (key=="bg")          s.bg          = parse_color(val);
        else if (key=="surface")     s.surface     = parse_color(val);
        else if (key=="surface_hov") s.surface_hov = parse_color(val);
        else if (key=="surface_act") s.surface_act = parse_color(val);
        else if (key=="border")      s.border      = parse_color(val);
        else if (key=="text")        s.text        = parse_color(val);
        else if (key=="text_muted")  s.text_muted  = parse_color(val);
        else if (key=="accent")      s.accent      = parse_color(val);
        else if (key=="accent_hov")  s.accent_hov  = parse_color(val);
        else if (key=="accent_act")  s.accent_act  = parse_color(val);
        else if (key=="accent_text") s.accent_text = parse_color(val);
        else if (key=="bar_bg")      s.bar_bg      = parse_color(val);
        else if (key=="bar_border")  s.bar_border  = parse_color(val);
        else if (!is_hex) {
            try {
                int n = std::stoi(val);
                if      (key=="radius")       s.radius       = n;
                else if (key=="padding")      s.padding      = n;
                else if (key=="font_scale")   s.font_scale   = n;
                else if (key=="menu_bar_h")   s.menu_bar_h   = n;
                else if (key=="status_bar_h") s.status_bar_h = n;
            } catch (...) {}
        }
    }
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Stable ID  (FNV-1a)
// ─────────────────────────────────────────────────────────────────────────────
using UIID = uint32_t;
inline UIID make_id(std::string_view sv) {
    uint32_t h = 2166136261u;
    for (char c : sv) h = (h ^ uint8_t(c)) * 16777619u;
    return h;
}
inline UIID make_id(const void* p) {
    return uint32_t(reinterpret_cast<uintptr_t>(p) * 2654435761u);
}

// ─────────────────────────────────────────────────────────────────────────────
// UI — main immediate-mode context
// ─────────────────────────────────────────────────────────────────────────────
class UI {
public:
    UI(Renderer& r, const InputState& in, const Style& s = {})
        : r_(r), in_(in), s_(s) {}

    // No Window constructor here — UI is not Window
    void set_style(const Style& s)    { s_ = s; }
    [[nodiscard]] const Style& style() const { return s_; }
    [[nodiscard]] float dt() const { return float(in_.delta_time); }

    // ── Layout ───────────────────────────────────────────────────────────
    void set_cursor(int x, int y) { cx_=x; cy_=y; }
    void spacing   (int px=8)     { cy_+=px; }
    void same_line (int gap=8)    { cx_+=last_w_+gap; cy_-=last_h_; }
    [[nodiscard]] int cursor_x() const { return cx_; }
    [[nodiscard]] int cursor_y() const { return cy_; }

    // ── Label ────────────────────────────────────────────────────────────
    void label(std::string_view text, Color c=colors::transparent) {
        if (c.a==0) c=s_.text;
        r_.draw_text(cx_, cy_, text, c, s_.font_scale);
        last_w_=r_.text_width(text, s_.font_scale);
        last_h_=r_.text_height(s_.font_scale);
        cy_+=last_h_+s_.padding;
    }

    // ── Separator ────────────────────────────────────────────────────────
    void separator(int w=400) {
        int mid=cy_+r_.text_height(s_.font_scale)/2;
        r_.draw_line(cx_, mid, cx_+w, mid, s_.border, 1);
        cy_+=r_.text_height(s_.font_scale)+s_.padding;
        last_w_=w; last_h_=r_.text_height(s_.font_scale);
    }

    // ── Button ───────────────────────────────────────────────────────────
    bool button(std::string_view text, int w=0) {
        int tw=r_.text_width(text,s_.font_scale);
        int th=r_.text_height(s_.font_scale);
        if (w==0) w=tw+s_.padding*4;
        int h=th+s_.padding*2;
        Recti rect{cx_,cy_,w,h};
        UIID id=make_id(text);

        bool hov=in_.hovered(rect);
        if (in_.mouse_press[0]&&hov) active_id_=id;
        bool pressed=(!in_.mouse_down[0]&&active_id_==id&&hov);
        if (!in_.mouse_down[0]&&active_id_==id) active_id_=0;
        bool act=(active_id_==id);

        float hv=anim_.hover(id,hov,dt());
        float pv=anim_.press(id,act,dt());

        Color bg=lerp_color(lerp_color(s_.accent,s_.accent_hov,hv),s_.accent_act,pv);
        int inset=int(pv*1.5f);
        Recti dr{rect.x+inset,rect.y+inset,rect.w-inset*2,rect.h-inset*2};

        r_.fill_rounded_rect(dr,s_.radius,bg);
        r_.draw_text(dr.x+(dr.w-tw)/2,dr.y+(dr.h-th)/2,text,s_.accent_text,s_.font_scale);

        last_w_=w; last_h_=h; cy_+=h+s_.padding;
        return pressed;
    }

    // ── Outline button ───────────────────────────────────────────────────
    bool button_outline(std::string_view text, int w=0) {
        int tw=r_.text_width(text,s_.font_scale);
        int th=r_.text_height(s_.font_scale);
        if (w==0) w=tw+s_.padding*4;
        int h=th+s_.padding*2;
        Recti rect{cx_,cy_,w,h};
        UIID id=make_id(text)^0xDEAD;

        bool hov    =in_.hovered(rect);
        bool pressed=in_.clicked_in(rect);
        float hv    =anim_.hover(id,hov,dt());

        r_.fill_rounded_rect(rect,s_.radius,lerp_color(s_.surface,s_.surface_hov,hv));
        r_.stroke_rect(rect,lerp_color(s_.border,s_.accent,hv*0.5f));
        r_.draw_text(cx_+(w-tw)/2,cy_+(h-th)/2,text,s_.text,s_.font_scale);

        last_w_=w; last_h_=h; cy_+=h+s_.padding;
        return pressed;
    }

    // ── Checkbox ─────────────────────────────────────────────────────────
    bool checkbox(std::string_view text, bool& value) {
        int th=r_.text_height(s_.font_scale);
        int box=th+4;
        Recti rbox{cx_,cy_,box,box};
        Recti full{cx_,cy_,box+s_.padding+r_.text_width(text,s_.font_scale),box};
        UIID id=make_id(text)^0xCB;

        bool clk=in_.clicked_in(full);
        if (clk) value=!value;
        bool hov=in_.hovered(full);

        float fv=anim_.hover(id,value,dt(),0.12f,0.10f);
        float hv=anim_.press(id,hov&&!value,dt(),0.08f);

        Color bg=lerp_color(lerp_color(s_.surface,s_.surface_hov,hv),s_.accent,fv);
        r_.draw_panel(rbox,bg,s_.border);

        if (fv>0.01f) {
            Color ck={s_.accent_text.r,s_.accent_text.g,s_.accent_text.b,uint8_t(fv*255)};
            r_.draw_line(rbox.x+2,    rbox.y+box/2, rbox.x+box/2,  rbox.y+box-3, ck, 2);
            r_.draw_line(rbox.x+box/2,rbox.y+box-3, rbox.x+box-2,  rbox.y+2,     ck, 2);
        }
        r_.draw_text(cx_+box+s_.padding,cy_+(box-th)/2,text,s_.text,s_.font_scale);

        last_w_=full.w; last_h_=box; cy_+=box+s_.padding;
        return clk;
    }

    // ── Slider ───────────────────────────────────────────────────────────
    bool slider(std::string_view id_str, float& value,
                float lo=0.f, float hi=1.f, int w=200)
    {
        int th=r_.text_height(s_.font_scale);
        int h=6, ky=cy_+th/2-h/2;
        Recti track{cx_,ky,w,h};
        Recti hit  {cx_,cy_-4,w,th+8};
        UIID id=make_id(id_str);

        bool hov =in_.hovered(hit);
        bool drag=hov&&in_.mouse_down[0];
        if (drag||in_.clicked_in(hit)) {
            float t=std::clamp(float(in_.mouse_x-cx_)/float(w),0.f,1.f);
            value=lo+t*(hi-lo);
        }
        float hv=anim_.hover(id,hov,dt());

        float t  =(hi>lo)?std::clamp((value-lo)/(hi-lo),0.f,1.f):0.f;
        int   knob=cx_+int(t*w);
        int   krad=int(5.f+hv*2.f);

        r_.fill_rounded_rect(track,3,s_.border);
        r_.fill_rounded_rect({track.x,track.y,int(t*w),h},3,
                              lerp_color(s_.accent,s_.accent_hov,hv));
        r_.fill_circle(knob,ky+h/2,krad+1,lerp_color(s_.accent,s_.accent_hov,hv));
        r_.fill_circle(knob,ky+h/2,krad-2,s_.accent_text);

        last_w_=w; last_h_=th; cy_+=th+s_.padding+4;
        return drag;
    }

    // ── Text field ───────────────────────────────────────────────────────
    bool text_field(std::string& buf, int w=200, std::string_view placeholder={}) {
        int th=r_.text_height(s_.font_scale);
        int h=th+s_.padding*2;
        Recti rect{cx_,cy_,w,h};
        UIID  id=make_id(&buf);
        bool  focused=(focused_id_==id);

        if (in_.clicked_in(rect))    { focused_id_=id; focused=true;  }
        else if (in_.mouse_press[0]) { focused_id_=0;  focused=false; }

        if (focused) {
            buf+=in_.text_input;
            if (in_.pressed(Key::Backspace)) utf8::backspace(buf);
        }

        float fv=anim_.hover(id,focused,dt(),0.10f,0.15f);
        r_.draw_panel(rect,
                      lerp_color(s_.surface,s_.surface_hov,fv*0.5f),
                      lerp_color(s_.border,s_.accent,fv),
                      focused?2:1);

        bool empty=buf.empty();
        r_.draw_text(cx_+s_.padding,cy_+(h-th)/2,
                     empty?placeholder:std::string_view(buf),
                     empty?s_.text_muted:s_.text, s_.font_scale);

        if (focused && int(in_.frame_time*2)%2==0) {
            int cx=cx_+s_.padding+r_.text_width(buf,s_.font_scale);
            r_.draw_line(cx,cy_+s_.padding,cx,cy_+h-s_.padding,s_.text,1);
        }
        last_w_=w; last_h_=h; cy_+=h+s_.padding;
        return focused;
    }

    // ── Progress bar ─────────────────────────────────────────────────────
    void progress_bar(float value, int w=200, int h=14) {
        value=std::clamp(value,0.f,1.f);
        int th=r_.text_height(s_.font_scale);
        r_.fill_rounded_rect({cx_,cy_,w,h},3,s_.surface);
        r_.stroke_rect      ({cx_,cy_,w,h},s_.border,1);
        if (value>0.f)
            r_.fill_rounded_rect({cx_,cy_,int(w*value),h},3,s_.accent);
        std::string pct=std::to_string(int(value*100))+"%";
        int tw=r_.text_width(pct,s_.font_scale);
        r_.draw_text(cx_+(w-tw)/2,cy_+(h-th)/2,pct,s_.text,s_.font_scale);
        last_w_=w; last_h_=h; cy_+=h+s_.padding;
    }

    // ── Image box ────────────────────────────────────────────────────────
    void image_box(const uint32_t* pixels, int img_w, int img_h, int w=0, int h=0) {
        if (!pixels) return;
        if (w==0) w=img_w; if (h==0) h=img_h;
        for (int y=0;y<h&&y<img_h;++y) {
            int sy=(h==img_h)?y:y*img_h/h;
            for (int x=0;x<w&&x<img_w;++x) {
                int sx=(w==img_w)?x:x*img_w/w;
                uint32_t px=pixels[sy*img_w+sx];
                r_.fb().set(cx_+x,cy_+y,
                    {uint8_t(px>>16),uint8_t(px>>8),uint8_t(px),uint8_t(px>>24)});
            }
        }
        r_.stroke_rect({cx_,cy_,w,h},s_.border);
        last_w_=w; last_h_=h; cy_+=h+s_.padding;
    }

    // ── List box ─────────────────────────────────────────────────────────
    int list_box(const std::vector<std::string>& items,
                 int& selected, int w=200, int visible_h=120)
    {
        int th     =r_.text_height(s_.font_scale);
        int item_h =th+s_.padding*2;
        int total_h=int(items.size())*item_h;
        int clicked=-1;
        UIID pool_id=make_id(&items);

        Recti box{cx_,cy_,w,visible_h};
        r_.draw_panel(box,s_.surface,s_.border);

        if (in_.hovered(box)) list_scroll_-=in_.scroll_delta*item_h;
        list_scroll_=std::clamp(list_scroll_,0,std::max(0,total_h-visible_h));

        if (selected>=0) {
            float tgt_y=float(selected*item_h-list_scroll_+box.y+1);
            float sel_y=anim_.select(pool_id,tgt_y,dt());
            Recti sel_r{box.x+1,int(sel_y),w-2,item_h};
            sel_r=intersect(sel_r,{box.x+1,box.y+1,w-2,visible_h-2});
            if (sel_r.h>0) r_.fill_rounded_rect(sel_r,3,s_.accent);
        }

        r_.push_clip({box.x+1,box.y+1,box.w-2,box.h-2});
        for (int i=0;i<int(items.size());++i) {
            int iy=cy_+i*item_h-list_scroll_;
            if (iy+item_h<box.y||iy>box.y+box.h) continue;
            Recti row{cx_+1,iy,w-2,item_h};
            bool hov=in_.hovered(row)&&in_.hovered(box);
            bool sel=(i==selected);
            UIID rid=pool_id^uint32_t(i*31337);
            float hv=anim_.hover(rid,hov&&!sel,dt());
            if (!sel&&hv>0.01f)
                r_.fill_rect(row,{s_.surface_hov.r,s_.surface_hov.g,
                                  s_.surface_hov.b,uint8_t(hv*200)});
            r_.draw_text(cx_+1+s_.padding,iy+(item_h-th)/2,
                         items[i],sel?s_.accent_text:s_.text,s_.font_scale);
            if (hov&&in_.mouse_press[0]) { selected=i; clicked=i; }
        }
        r_.pop_clip();

        if (total_h>visible_h) {
            int sb_w=5, sb_x=cx_+w-sb_w-1;
            float frac=float(visible_h)/float(total_h);
            int sh=std::max(14,int(visible_h*frac));
            int sy=box.y+1+int((float(list_scroll_)/float(total_h))*visible_h);
            r_.fill_rect        ({sb_x,box.y+1,sb_w,visible_h-2},s_.surface_act);
            r_.fill_rounded_rect({sb_x,sy,sb_w,sh},2,s_.border);
        }
        last_w_=w; last_h_=visible_h; cy_+=visible_h+s_.padding;
        return clicked;
    }

    void reset_list_scroll() { list_scroll_=0; }

    // ── Dropdown ─────────────────────────────────────────────────────────
    bool dropdown(const std::vector<std::string>& items, int& selected, int w=200) {
        if (items.empty()) return false;
        int th=r_.text_height(s_.font_scale), h=th+s_.padding*2;
        Recti head{cx_,cy_,w,h};
        UIID  id=make_id(&selected)^0xDD;
        bool  open   =(dropdown_id_==id);
        bool  hov    =in_.hovered(head);
        bool  changed=false;

        float hv=anim_.hover(id,hov,dt());
        float ov=anim_.open (id,open,dt(),0.14f);

        r_.draw_panel(head,
                      open?s_.surface_act:lerp_color(s_.surface,s_.surface_hov,hv),
                      open?s_.accent:lerp_color(s_.border,s_.accent,hv*0.4f));

        const std::string& cur=(selected>=0&&selected<int(items.size()))
                                ?items[selected]:"---";
        r_.draw_text(cx_+s_.padding,cy_+(h-th)/2,cur,s_.text,s_.font_scale);

        int ax=cx_+w-s_.padding-6, ay=cy_+h/2-2;
        int flip=open?1:0;
        r_.draw_line(ax,  ay+flip,   ax+6,ay+flip,   s_.text_muted,1);
        r_.draw_line(ax+1,ay+2-flip*2,ax+5,ay+2-flip*2,s_.text_muted,1);
        r_.draw_line(ax+2,ay+4-flip*4,ax+4,ay+4-flip*4,s_.text_muted,1);

        if (in_.clicked_in(head)) dropdown_id_=open?0:id;

        if (ov>0.01f) {
            int item_h =h;
            int full_ph=int(items.size())*item_h;
            int clip_h =int(ov*full_ph);
            int py=cy_+h;
            Recti panel{cx_,py,w,clip_h};
            r_.push_clip(panel);
            r_.fill_rect(panel,s_.surface);
            r_.stroke_rect({cx_,py,w,full_ph},s_.border);
            for (int i=0;i<int(items.size());++i) {
                Recti row{cx_,py+i*item_h,w,item_h};
                UIID rid=id^uint32_t(i*7919);
                bool rhov=in_.hovered(row)&&open;
                bool rsel=(i==selected);
                float rhv=anim_.hover(rid,rhov&&!rsel,dt());
                r_.fill_rect(row,rsel?s_.accent:lerp_color(s_.surface,s_.surface_hov,rhv));
                r_.draw_text(cx_+s_.padding,py+i*item_h+(item_h-th)/2,
                             items[i],rsel?s_.accent_text:s_.text,s_.font_scale);
                if (rhov&&in_.mouse_press[0]) { selected=i; dropdown_id_=0; changed=true; }
            }
            r_.pop_clip();
            if (in_.mouse_press[0]&&!in_.hovered(panel)&&!in_.hovered(head))
                dropdown_id_=0;
        }
        last_w_=w; last_h_=h; cy_+=h+s_.padding;
        return changed;
    }

    // ── Context menu ─────────────────────────────────────────────────────
    bool begin_context_menu() {
        if (in_.mouse_press[1]) {
            menu_open_=true; menu_x_=in_.mouse_x;
            menu_y_=in_.mouse_y; menu_off_=0;
        }
        return menu_open_;
    }

    bool context_menu_item(std::string_view text) {
        if (!menu_open_) return false;
        int tw=r_.text_width(text,s_.font_scale), th=r_.text_height(s_.font_scale);
        int mw=tw+s_.padding*4, mh=th+s_.padding*2;
        UIID cid=make_id("ctx")^0xDEAD;
        float ov=anim_.open(cid,menu_open_,dt(),0.12f);

        Recti row{menu_x_,menu_y_+menu_off_,mw,mh};
        int vis_h=int(ov*float(mh));
        r_.push_clip({row.x,row.y,row.w,vis_h});

        bool hov=in_.hovered(row)&&menu_open_;
        bool clk=in_.clicked_in(row);
        float hv=anim_.hover(make_id(text)^0xCCCC,hov,dt());
        r_.fill_rounded_rect(row,s_.radius,lerp_color(s_.surface,s_.surface_hov,hv));
        r_.stroke_rect(row,s_.border);
        r_.draw_text(menu_x_+s_.padding,menu_y_+menu_off_+(mh-th)/2,
                     text,s_.text,s_.font_scale);
        r_.pop_clip();
        menu_off_+=mh;

        if (clk)                        { menu_open_=false; return true; }
        if (in_.mouse_press[0]&&!hov)   menu_open_=false;
        return false;
    }
    void end_context_menu() { menu_open_=false; menu_off_=0; }

    AnimPool& anims() { return anim_; }

private:
    Renderer&         r_;
    const InputState& in_;
    Style             s_;
    AnimPool          anim_;

    int  cx_=0,cy_=0,last_w_=0,last_h_=0;
    UIID active_id_=0,focused_id_=0,dropdown_id_=0;
    int  list_scroll_=0;
    bool menu_open_=false;
    int  menu_x_=0,menu_y_=0,menu_off_=0;
};

// ─────────────────────────────────────────────────────────────────────────────
// MenuBar
// ─────────────────────────────────────────────────────────────────────────────
class MenuBar {
public:
    MenuBar(Renderer& r, const InputState& in, const Style& s)
        : r_(r),in_(in),s_(s) {}

    void begin(int x, int y, int w) {
        x_=x; y_=y; w_=w;
        r_.fill_rect({x,y,w,s_.menu_bar_h},s_.bar_bg);
        r_.draw_line(x,y+s_.menu_bar_h-1,x+w,y+s_.menu_bar_h-1,s_.bar_border,1);
        cur_x_=x+8;
        open_id_=pending_open_;
    }

    bool menu(std::string_view label) {
        int tw=r_.text_width(label,s_.font_scale);
        int bw=tw+20, bh=s_.menu_bar_h;
        Recti rc{cur_x_,y_,bw,bh};
        UIID id=make_id(label);

        bool hov=in_.hovered(rc);
        bool is_open=(open_id_==id);
        float hv=anim_.hover(id,hov||is_open,float(in_.delta_time));
        Color bg=is_open?s_.accent:lerp_color(s_.bar_bg,s_.surface_hov,hv);

        r_.fill_rect(rc,bg);
        r_.draw_text(cur_x_+10,y_+(bh-r_.text_height(s_.font_scale))/2,
                     label,is_open?s_.accent_text:s_.text,s_.font_scale);

        if (in_.clicked_in(rc)) pending_open_=(is_open?0:id);
        if (in_.mouse_press[0]&&!hov&&!in_.hovered(dropdown_rect_))
            pending_open_=0;

        cur_item_x_=cur_x_;
        cur_item_w_=std::max(bw,160);
        cur_x_+=bw+2;
        item_y_=y_+bh;

        if (is_open) {
            dropdown_rect_={cur_item_x_,item_y_,cur_item_w_,4};
            r_.fill_rect  (dropdown_rect_,s_.surface);
            r_.stroke_rect(dropdown_rect_,s_.bar_border);
            item_offset_=0;
        }
        return is_open;
    }

    bool item(std::string_view label) {
        int th=r_.text_height(s_.font_scale);
        int ih=th+s_.padding*2;
        Recti rc{cur_item_x_+1,item_y_+item_offset_,cur_item_w_-2,ih};
        UIID id=make_id(label)^0xABCD;

        bool hov=in_.hovered(rc);
        bool clk=in_.clicked_in(rc);
        float hv=anim_.hover(id,hov,float(in_.delta_time));

        Color bg=lerp_color(s_.surface,s_.surface_hov,hv);
        r_.fill_rect(rc,bg);
        r_.draw_text(cur_item_x_+1+s_.padding,item_y_+item_offset_+(ih-th)/2,
                     label,lerp_color(s_.text,s_.accent_text,hv*0.5f),s_.font_scale);

        item_offset_+=ih;
        dropdown_rect_.h=item_offset_+4;
        r_.stroke_rect(dropdown_rect_,s_.bar_border);

        if (clk) pending_open_=0;
        return clk;
    }

    void separator() {
        int sy=item_y_+item_offset_+4;
        r_.draw_line(cur_item_x_+4,sy,cur_item_x_+cur_item_w_-4,sy,s_.bar_border,1);
        item_offset_+=10;
    }

    void end_menu() {}
    void end()      {}
    [[nodiscard]] int height() const { return s_.menu_bar_h; }

private:
    Renderer&         r_;
    const InputState& in_;
    const Style&      s_;
    AnimPool          anim_;

    int  x_=0,y_=0,w_=0,cur_x_=0;
    UIID open_id_=0,pending_open_=0;
    int  cur_item_x_=0,cur_item_w_=160;
    int  item_y_=0,item_offset_=0;
    Recti dropdown_rect_{};
};

// ─────────────────────────────────────────────────────────────────────────────
// ScrollBar
// ─────────────────────────────────────────────────────────────────────────────
class ScrollBar {
public:
    ScrollBar(Renderer& r, const InputState& in, const Style& s)
        : r_(r),in_(in),s_(s) {}

    static constexpr int W=10;

    int vertical(int x,int y,int viewport_h,int content_h,int scroll_pos) {
        if (content_h<=viewport_h) return 0;
        Recti track{x,y,W,viewport_h};
        UIID id=make_id(&scroll_pos)^0x5CB;

        float ratio  =float(viewport_h)/float(content_h);
        int   thumb_h=std::max(20,int(viewport_h*ratio));
        float sf     =float(scroll_pos)/float(content_h-viewport_h);
        int   thumb_y=y+int(sf*float(viewport_h-thumb_h));
        Recti thumb  {x+1,thumb_y,W-2,thumb_h};

        bool hov=in_.hovered(track);
        float hv=anim_.hover(id,hov,float(in_.delta_time));

        r_.fill_rect        (track,lerp_color(s_.surface,s_.surface_act,hv*0.6f));
        r_.fill_rounded_rect(thumb,3,lerp_color(s_.border,s_.accent,hv));

        int delta=0;
        if (in_.mouse_press[0]&&in_.hovered(thumb)) drag_id_=id;
        if (drag_id_==id) {
            if (in_.mouse_down[0]) {
                float dy=float(in_.mouse_y-last_my_);
                delta=int(dy*float(content_h)/float(viewport_h));
            } else { drag_id_=0; }
        }
        last_my_=in_.mouse_y;
        if (hov) delta-=in_.scroll_delta*20;
        return delta;
    }

    int horizontal(int x,int y,int viewport_w,int content_w,int scroll_pos) {
        if (content_w<=viewport_w) return 0;
        Recti track{x,y,viewport_w,W};
        UIID id=make_id(&scroll_pos)^0x6CA;

        float ratio  =float(viewport_w)/float(content_w);
        int   thumb_w=std::max(20,int(viewport_w*ratio));
        float sf     =float(scroll_pos)/float(content_w-viewport_w);
        int   thumb_x=x+int(sf*float(viewport_w-thumb_w));
        Recti thumb  {thumb_x,y+1,thumb_w,W-2};

        bool hov=in_.hovered(track);
        float hv=anim_.hover(id,hov,float(in_.delta_time));

        r_.fill_rect        (track,lerp_color(s_.surface,s_.surface_act,hv*0.6f));
        r_.fill_rounded_rect(thumb,3,lerp_color(s_.border,s_.accent,hv));

        int delta=0;
        if (in_.mouse_press[0]&&in_.hovered(thumb)) drag_id_=id;
        if (drag_id_==id) {
            if (in_.mouse_down[0]) {
                float dx=float(in_.mouse_x-last_mx_);
                delta=int(dx*float(content_w)/float(viewport_w));
            } else { drag_id_=0; }
        }
        last_mx_=in_.mouse_x;
        return delta;
    }

private:
    Renderer&         r_;
    const InputState& in_;
    const Style&      s_;
    AnimPool          anim_;
    UIID drag_id_=0;
    int  last_my_=0,last_mx_=0;
};

// ─────────────────────────────────────────────────────────────────────────────
// StatusBar
// ─────────────────────────────────────────────────────────────────────────────
class StatusBar {
public:
    StatusBar(Renderer& r, const InputState& in, const Style& s)
        : r_(r),in_(in),s_(s) {}

    void begin(int x,int y,int w) {
        x_=x; y_=y; w_=w;
        r_.fill_rect({x,y,w,s_.status_bar_h},s_.bar_bg);
        r_.draw_line(x,y,x+w,y,s_.bar_border,1);
        left_x_=x+8; right_x_=x+w-8;
    }

    void text(std::string_view t, Color c=colors::transparent) {
        if (c.a==0) c=s_.text_muted;
        int th=r_.text_height(s_.font_scale);
        int tw=r_.text_width(t,s_.font_scale);
        r_.draw_text(left_x_,y_+(s_.status_bar_h-th)/2,t,c,s_.font_scale);
        left_x_+=tw+16;
    }

    void text_right(std::string_view t, Color c=colors::transparent) {
        if (c.a==0) c=s_.text_muted;
        int th=r_.text_height(s_.font_scale);
        int tw=r_.text_width(t,s_.font_scale);
        right_x_-=tw;
        r_.draw_text(right_x_,y_+(s_.status_bar_h-th)/2,t,c,s_.font_scale);
        right_x_-=12;
    }

    void progress(float value,int w=60) {
        value=std::clamp(value,0.f,1.f);
        int h=4, py=y_+(s_.status_bar_h-h)/2;
        r_.fill_rect        ({left_x_,py,w,h},s_.surface_act);
        r_.fill_rounded_rect({left_x_,py,int(w*value),h},2,s_.accent);
        left_x_+=w+10;
    }

    void color_dot(Color c,int radius=3) {
        r_.fill_circle(left_x_+radius,y_+s_.status_bar_h/2,radius,c);
        left_x_+=radius*2+8;
    }

    void divider() {
        r_.draw_line(left_x_,y_+4,left_x_,y_+s_.status_bar_h-4,s_.bar_border,1);
        left_x_+=12;
    }

    void end() {}
    [[nodiscard]] int height() const { return s_.status_bar_h; }

private:
    Renderer&         r_;
    const InputState& in_;
    const Style&      s_;
    int x_=0,y_=0,w_=0,left_x_=0,right_x_=0;
};

} // namespace xm