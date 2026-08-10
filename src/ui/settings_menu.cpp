#include "ui/settings_menu.h"
#include <Arduino.h>
#include <lvgl.h>
#include "hardware/display.h"
#include "ui/radar_display.h"
#include "ui/radar_range.h"
#include "catppuccin.h"
#include "config.h"
#include <Preferences.h>
#include "services/adsb_client.h"
#include <WiFi.h>

extern LGFX tft;

namespace ui::settings {

static lv_color_t get_lv_color(uint32_t hex) {
    uint8_t r = (hex >> 16) & 0xFF;
    uint8_t g = (hex >> 8) & 0xFF;
    uint8_t b = hex & 0xFF;
    if (config::kDisplayRgbOrder) {
        return lv_color_make(b, g, r);
    }
    return lv_color_make(r, g, b);
}

static bool s_visible = false;
static bool s_pending_hide = false;
static bool s_show_airports = true;
static bool s_show_medium_airports = false;
static bool s_show_ground_aircraft = false;
static bool s_show_radar_sweep = true;
static bool s_auto_dimming = false;
static int s_max_altitude = 50000;
static int s_max_aircraft = 40;
static int s_sweep_speed = 6000;
static Preferences s_prefs;
static int s_theme_flavor = CATPPUCCIN_MOCHA;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_obj_t* settings_screen = nullptr;
static uint32_t last_tick_millis = 0;
static unsigned long s_show_ms = 0;

static lv_obj_t* wifi_info_dialog = nullptr;

static void close_wifi_info_cb(lv_event_t * e) {
    if (wifi_info_dialog != nullptr) {
        lv_obj_del(wifi_info_dialog);
        wifi_info_dialog = nullptr;
    }
}

static void wifi_icon_click_cb(lv_event_t * e) {
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP) return;
    if (wifi_info_dialog != nullptr) return;

    bool isLargeScreen = (lv_disp_get_hor_res(NULL) >= 480 || lv_disp_get_ver_res(NULL) >= 480);

    wifi_info_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(wifi_info_dialog, lv_pct(85), lv_pct(85));
    lv_obj_align(wifi_info_dialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(wifi_info_dialog, get_lv_color(getCatppuccinFlavor(s_theme_flavor).mantle), 0);
    lv_obj_set_style_border_color(wifi_info_dialog, get_lv_color(getCatppuccinFlavor(s_theme_flavor).overlay), 0);
    lv_obj_set_style_border_width(wifi_info_dialog, 2, 0);
    lv_obj_set_style_radius(wifi_info_dialog, 10, 0);
    lv_obj_clear_flag(wifi_info_dialog, LV_OBJ_FLAG_SCROLLABLE);

    // Title label
    lv_obj_t * lbl_title = lv_label_create(wifi_info_dialog);
    lv_label_set_text(lbl_title, "WiFi Info");
    lv_obj_set_style_text_color(lbl_title, get_lv_color(getCatppuccinFlavor(s_theme_flavor).text), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, isLargeScreen ? 10 : 5);

    // Status label
    lv_obj_t * lbl_status = lv_label_create(wifi_info_dialog);
    if (WiFi.getMode() == WIFI_AP) {
        lv_label_set_text(lbl_status, "AP Mode Active");
        lv_obj_set_style_text_color(lbl_status, get_lv_color(getCatppuccinFlavor(s_theme_flavor).mauve), 0);
    } else {
        lv_label_set_text(lbl_status, "Connected");
        lv_obj_set_style_text_color(lbl_status, get_lv_color(getCatppuccinFlavor(s_theme_flavor).green), 0);
    }
    lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, isLargeScreen ? 40 : 25);

    // Info details
    lv_obj_t * lbl_info = lv_label_create(wifi_info_dialog);
    char infoBuf[256];
    if (WiFi.getMode() == WIFI_AP) {
        snprintf(infoBuf, sizeof(infoBuf), 
                 "SSID: %s\nIP: %s\nMAC: %s", 
                 WiFi.softAPSSID().c_str(), 
                 WiFi.softAPIP().toString().c_str(), 
                 WiFi.softAPmacAddress().c_str());
    } else {
        snprintf(infoBuf, sizeof(infoBuf), 
                 "SSID: %s\nIP: %s\nMAC: %s\nRSSI: %d dBm", 
                 WiFi.SSID().c_str(), 
                 WiFi.localIP().toString().c_str(), 
                 WiFi.macAddress().c_str(), 
                 WiFi.RSSI());
    }
    lv_label_set_text(lbl_info, infoBuf);
    lv_obj_set_style_text_align(lbl_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_info, get_lv_color(getCatppuccinFlavor(s_theme_flavor).text), 0);
    lv_obj_align(lbl_info, LV_ALIGN_CENTER, 0, isLargeScreen ? 10 : 15);

    // Close Button
    lv_obj_t * btn_close = lv_btn_create(wifi_info_dialog);
    lv_obj_set_size(btn_close, isLargeScreen ? 120 : 80, isLargeScreen ? 40 : 30);
    lv_obj_align(btn_close, LV_ALIGN_BOTTOM_MID, 0, isLargeScreen ? -15 : -5);
    lv_obj_set_style_bg_color(btn_close, get_lv_color(getCatppuccinFlavor(s_theme_flavor).overlay), 0);
    lv_obj_add_event_cb(btn_close, close_wifi_info_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "Close");
    lv_obj_set_style_text_color(lbl_close, get_lv_color(getCatppuccinFlavor(s_theme_flavor).crust), 0);
    lv_obj_align(lbl_close, LV_ALIGN_CENTER, 0, 0);
}

static void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp_drv);
}

static void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    if (!s_visible || (millis() - s_show_ms < 500)) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    uint16_t tx = 0, ty = 0;
    bool touched = tft.getTouch(&tx, &ty);
    if (touched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = tx;
        data->point.y = ty;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static void close_btn_event_handler(lv_event_t * e) {
    s_pending_hide = true;
}

static void airports_switch_event_cb(lv_event_t * e) {
    if (!s_visible) return;
    lv_obj_t * sw = lv_event_get_target(e);
    s_show_airports = lv_obj_has_state(sw, LV_STATE_CHECKED);
    s_prefs.putBool("airports", s_show_airports);
}


static void medium_airports_switch_event_cb(lv_event_t * e) {
    if (!s_visible) return;
    lv_obj_t * sw = lv_event_get_target(e);
    s_show_medium_airports = lv_obj_has_state(sw, LV_STATE_CHECKED);
    s_prefs.putBool("med_airports", s_show_medium_airports);
}

static void units_switch_event_cb(lv_event_t * e) {
    if (!s_visible) return;
    lv_obj_t * sw = lv_event_get_target(e);
    ui::radar::setUseMiles(!lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void ground_aircraft_switch_event_cb(lv_event_t * e) {
    if (!s_visible) return;
    lv_obj_t * sw = lv_event_get_target(e);
    s_show_ground_aircraft = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void radar_sweep_switch_event_cb(lv_event_t * e) {
    if (!s_visible) return;
    lv_obj_t * sw = lv_event_get_target(e);
    s_show_radar_sweep = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void auto_dimming_switch_event_cb(lv_event_t * e) {
    if (!s_visible) return;
    lv_obj_t * sw = lv_event_get_target(e);
    s_auto_dimming = lv_obj_has_state(sw, LV_STATE_CHECKED);
    s_prefs.putBool("auto_dim", s_auto_dimming);
}

static void max_altitude_slider_event_cb(lv_event_t * e) {
    if (!s_visible) return;
    lv_obj_t * slider = lv_event_get_target(e);
    s_max_altitude = lv_slider_get_value(slider);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
    if(label) lv_label_set_text_fmt(label, "%d ft", s_max_altitude);
}

static void max_aircraft_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    s_max_aircraft = lv_slider_get_value(slider);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
    if(label) lv_label_set_text_fmt(label, "%d", s_max_aircraft);
    s_prefs.putInt("max_ac", s_max_aircraft);
}

static void sweep_speed_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    s_sweep_speed = lv_slider_get_value(slider);
    lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
    if(label) lv_label_set_text_fmt(label, "%d ms", s_sweep_speed);
}

static lv_obj_t* create_toggle_row(lv_obj_t * parent, const char * text, bool initial_state, lv_event_cb_t event_cb) {
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row, get_lv_color(getCatppuccinFlavor(s_theme_flavor).mantle), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 10, 0);

    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, get_lv_color(getCatppuccinFlavor(s_theme_flavor).text), 0);

    lv_obj_t * sw = lv_switch_create(row);
    if (initial_state) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    return row;
}

static lv_obj_t* create_slider_row(lv_obj_t * parent, const char * text, int min_val, int max_val, int initial_val, const char* unit, lv_event_cb_t event_cb) {
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, LV_PCT(100), 70);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(row, get_lv_color(getCatppuccinFlavor(s_theme_flavor).mantle), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 10, 0);

    lv_obj_t * header = lv_obj_create(row);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl = lv_label_create(header);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, get_lv_color(getCatppuccinFlavor(s_theme_flavor).text), 0);
    
    lv_obj_t * val_lbl = lv_label_create(header);
    lv_label_set_text_fmt(val_lbl, "%d %s", initial_val, unit);
    lv_obj_set_style_text_color(val_lbl, get_lv_color(getCatppuccinFlavor(s_theme_flavor).blue), 0);

    lv_obj_t * slider = lv_slider_create(row);
    lv_slider_set_range(slider, min_val, max_val);
    lv_slider_set_value(slider, initial_val, LV_ANIM_OFF);
    lv_obj_set_width(slider, LV_PCT(100));
    lv_obj_add_event_cb(slider, event_cb, LV_EVENT_VALUE_CHANGED, val_lbl);
    
    return row;
}

struct TimezonePreset {
    const char* label;
    const char* value;
};
static const TimezonePreset tz_presets[] = {
    {"UTC", "UTC0"},
    {"London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"CET", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"EET", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"US East", "EST5EDT,M3.2.0,M11.1.0"},
    {"US Central", "CST6CDT,M3.2.0,M11.1.0"},
    {"US Mount.", "MST7MDT,M3.2.0,M11.1.0"},
    {"US Pacific", "PST8PDT,M3.2.0,M11.1.0"},
    {"US Alaska", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"US Hawaii", "HST10"},
    {"AU East", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"AU Central", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
    {"AU West", "AWST-8"}
};
static const int num_tz_presets = sizeof(tz_presets)/sizeof(tz_presets[0]);
static int s_tz_idx = 0;
static lv_obj_t * tz_val_label = nullptr;
static bool s_tz_changed = false;

static void tz_btn_event_cb(lv_event_t * e) {
    intptr_t dir = (intptr_t)lv_event_get_user_data(e);
    s_tz_idx += dir;
    if (s_tz_idx < 0) s_tz_idx = num_tz_presets - 1;
    if (s_tz_idx >= num_tz_presets) s_tz_idx = 0;
    
    s_tz_changed = true;
    lv_label_set_text(tz_val_label, tz_presets[s_tz_idx].label);
}

static lv_obj_t * range_val_label = nullptr;

static void update_range_label() {
    char range_label[12];
    ui::radar::formatCurrentRing3Label(range_label, sizeof(range_label));
    lv_label_set_text(range_val_label, range_label);
}

static void range_btn_event_cb(lv_event_t * e) {
    intptr_t dir = (intptr_t)lv_event_get_user_data(e);
    int new_idx = (int)ui::radar::rangeIndex() + dir;
    if (new_idx < 0) {
        new_idx = ui::radar::kRangePresetCount - 1;
    } else if (new_idx >= ui::radar::kRangePresetCount) {
        new_idx = 0;
    }
    ui::radar::setRangeIndex(new_idx);
    update_range_label();
}

static lv_obj_t* create_range_row(lv_obj_t * parent) {
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, LV_PCT(100), 75);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row, get_lv_color(getCatppuccinFlavor(s_theme_flavor).mantle), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 10, 0);

    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, "Radar Radius");
    lv_obj_set_style_text_color(lbl, get_lv_color(getCatppuccinFlavor(s_theme_flavor).text), 0);
    lv_obj_set_width(lbl, LV_PCT(100));

    lv_obj_t * controls = lv_obj_create(row);
    lv_obj_set_size(controls, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(controls, 0, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_minus = lv_btn_create(controls);
    lv_obj_set_size(btn_minus, 35, 30);
    lv_obj_add_event_cb(btn_minus, range_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);
    lv_obj_set_style_bg_color(btn_minus, get_lv_color(getCatppuccinFlavor(s_theme_flavor).overlay), 0);
    lv_obj_t * lbl_minus = lv_label_create(btn_minus);
    lv_label_set_text(lbl_minus, "<");
    lv_obj_center(lbl_minus);

    range_val_label = lv_label_create(controls);
    update_range_label();
    lv_obj_set_style_text_color(range_val_label, get_lv_color(getCatppuccinFlavor(s_theme_flavor).blue), 0);
    lv_obj_set_flex_grow(range_val_label, 1);
    lv_obj_set_style_text_align(range_val_label, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t * btn_plus = lv_btn_create(controls);
    lv_obj_set_size(btn_plus, 35, 30);
    lv_obj_add_event_cb(btn_plus, range_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);
    lv_obj_set_style_bg_color(btn_plus, get_lv_color(getCatppuccinFlavor(s_theme_flavor).overlay), 0);
    lv_obj_t * lbl_plus = lv_label_create(btn_plus);
    lv_label_set_text(lbl_plus, ">");
    lv_obj_center(lbl_plus);

    return row;
}


static const char* theme_names[] = {"Mocha", "Macchiato", "Frappe", "Latte"};
static lv_obj_t * theme_val_label = nullptr;

static void theme_btn_event_cb(lv_event_t * e) {
    intptr_t dir = (intptr_t)lv_event_get_user_data(e);
    s_theme_flavor += dir;
    if (s_theme_flavor < 1) s_theme_flavor = 4;
    if (s_theme_flavor > 4) s_theme_flavor = 1;
    
    lv_label_set_text(theme_val_label, theme_names[s_theme_flavor - 1]);
    s_prefs.putInt("theme", s_theme_flavor);
    ui::updateThemeColors();
    
    // Changing the theme immediately requires refreshing all colors in the settings menu,
    // which is complex. For now, it will apply when re-opening settings.
}

static lv_obj_t* create_theme_row(lv_obj_t * parent) {
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, LV_PCT(100), 75);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row, get_lv_color(getCatppuccinFlavor(s_theme_flavor).mantle), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 10, 0);

    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, "Theme Flavor");
    lv_obj_set_style_text_color(lbl, get_lv_color(getCatppuccinFlavor(s_theme_flavor).text), 0);
    lv_obj_set_width(lbl, LV_PCT(100));

    lv_obj_t * controls = lv_obj_create(row);
    lv_obj_set_size(controls, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(controls, 0, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_minus = lv_btn_create(controls);
    lv_obj_set_size(btn_minus, 35, 30);
    lv_obj_add_event_cb(btn_minus, theme_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);
    lv_obj_set_style_bg_color(btn_minus, get_lv_color(getCatppuccinFlavor(s_theme_flavor).overlay), 0);
    lv_obj_t * lbl_minus = lv_label_create(btn_minus);
    lv_label_set_text(lbl_minus, "<");
    lv_obj_center(lbl_minus);

    theme_val_label = lv_label_create(controls);
    lv_label_set_text(theme_val_label, theme_names[s_theme_flavor - 1]);
    lv_obj_set_style_text_color(theme_val_label, get_lv_color(getCatppuccinFlavor(s_theme_flavor).blue), 0);
    lv_obj_set_flex_grow(theme_val_label, 1);
    lv_obj_set_style_text_align(theme_val_label, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t * btn_plus = lv_btn_create(controls);
    lv_obj_set_size(btn_plus, 35, 30);
    lv_obj_add_event_cb(btn_plus, theme_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);
    lv_obj_set_style_bg_color(btn_plus, get_lv_color(getCatppuccinFlavor(s_theme_flavor).overlay), 0);
    lv_obj_t * lbl_plus = lv_label_create(btn_plus);
    lv_label_set_text(lbl_plus, ">");
    lv_obj_center(lbl_plus);

    return row;
}

static lv_obj_t* create_timezone_row(lv_obj_t * parent) {
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, LV_PCT(100), 75);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row, get_lv_color(getCatppuccinFlavor(s_theme_flavor).mantle), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 10, 0);

    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, "Timezone");
    lv_obj_set_style_text_color(lbl, get_lv_color(getCatppuccinFlavor(s_theme_flavor).text), 0);
    lv_obj_set_width(lbl, LV_PCT(100));

    lv_obj_t * controls = lv_obj_create(row);
    lv_obj_set_size(controls, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(controls, 0, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_minus = lv_btn_create(controls);
    lv_obj_set_size(btn_minus, 35, 30);
    lv_obj_add_event_cb(btn_minus, tz_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);
    lv_obj_set_style_bg_color(btn_minus, get_lv_color(getCatppuccinFlavor(s_theme_flavor).overlay), 0);
    lv_obj_t * lbl_minus = lv_label_create(btn_minus);
    lv_label_set_text(lbl_minus, "<");
    lv_obj_center(lbl_minus);

    tz_val_label = lv_label_create(controls);
    lv_label_set_text(tz_val_label, tz_presets[s_tz_idx].label);
    lv_obj_set_style_text_color(tz_val_label, get_lv_color(getCatppuccinFlavor(s_theme_flavor).blue), 0);
    lv_obj_set_flex_grow(tz_val_label, 1);
    lv_obj_set_style_text_align(tz_val_label, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t * btn_plus = lv_btn_create(controls);
    lv_obj_set_size(btn_plus, 35, 30);
    lv_obj_add_event_cb(btn_plus, tz_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);
    lv_obj_set_style_bg_color(btn_plus, get_lv_color(getCatppuccinFlavor(s_theme_flavor).overlay), 0);
    lv_obj_t * lbl_plus = lv_label_create(btn_plus);
    lv_label_set_text(lbl_plus, ">");
    lv_obj_center(lbl_plus);

    return row;
}

static void build_ui() {
    settings_screen = lv_obj_create(NULL);
    if (!settings_screen) return;
    
    lv_obj_set_style_bg_color(settings_screen, get_lv_color(getCatppuccinFlavor(s_theme_flavor).base), 0);
    
    lv_obj_t * title = lv_label_create(settings_screen);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, get_lv_color(getCatppuccinFlavor(s_theme_flavor).text), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    
    lv_obj_t * wifi_label = lv_label_create(settings_screen);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    
    lv_color_t wifi_color;
    if (WiFi.status() == WL_CONNECTED) {
        wifi_color = get_lv_color(getCatppuccinFlavor(s_theme_flavor).green);
    } else if (WiFi.getMode() == WIFI_AP) {
        wifi_color = get_lv_color(getCatppuccinFlavor(s_theme_flavor).mauve);
    } else {
        wifi_color = get_lv_color(getCatppuccinFlavor(s_theme_flavor).red);
    }
    lv_obj_set_style_text_color(wifi_label, wifi_color, 0);
    lv_obj_align(wifi_label, LV_ALIGN_TOP_RIGHT, -10, 15);
    lv_obj_add_flag(wifi_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(wifi_label, 15);
    lv_obj_add_event_cb(wifi_label, wifi_icon_click_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * list = lv_obj_create(settings_screen);
    lv_obj_set_size(list, LV_PCT(95), LV_PCT(70));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(list, get_lv_color(getCatppuccinFlavor(s_theme_flavor).base), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 5, 0);
    lv_obj_set_style_pad_row(list, 10, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    

    // Settings List
    create_toggle_row(list, "Large Airports", s_show_airports, airports_switch_event_cb);
    create_toggle_row(list, "Medium Airports", s_show_medium_airports, medium_airports_switch_event_cb);
    create_toggle_row(list, "Metric Units", !ui::radar::useMiles(), units_switch_event_cb);
    create_toggle_row(list, "Ground Aircraft", s_show_ground_aircraft, ground_aircraft_switch_event_cb);
    create_toggle_row(list, "Radar Sweep", s_show_radar_sweep, radar_sweep_switch_event_cb);
    create_toggle_row(list, "Auto-Dim Night", s_auto_dimming, auto_dimming_switch_event_cb);
    
    create_slider_row(list, "Max Alt", 0, 50000, s_max_altitude, "ft", max_altitude_slider_event_cb);
    create_slider_row(list, "Max Planes", 1, services::adsb::kMaxAircraft, s_max_aircraft, "", max_aircraft_slider_event_cb);
    create_slider_row(list, "Sweep Speed", 1000, 15000, s_sweep_speed, "ms", sweep_speed_slider_event_cb);
    create_timezone_row(list);
    create_range_row(list);
    create_theme_row(list);
    
    // Close button
    lv_obj_t * close_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(close_btn, 100, 30);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(close_btn, close_btn_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(close_btn, get_lv_color(getCatppuccinFlavor(s_theme_flavor).blue), 0);
    
    lv_obj_t * close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Close");
    lv_obj_set_style_text_color(close_label, get_lv_color(getCatppuccinFlavor(s_theme_flavor).crust), 0);
    lv_obj_center(close_label);
}

void setup() {
    s_prefs.begin("settings", false);
    s_theme_flavor = s_prefs.getInt("theme", CATPPUCCIN_MOCHA);
    if (s_theme_flavor < 1 || s_theme_flavor > 4) {
        s_theme_flavor = CATPPUCCIN_MOCHA;
    }
    s_max_aircraft = s_prefs.getInt("max_ac", services::adsb::kMaxAircraft);
    if (s_max_aircraft < 1) s_max_aircraft = 1;
    if (s_max_aircraft > services::adsb::kMaxAircraft) s_max_aircraft = services::adsb::kMaxAircraft;

    lv_init();
    
    // Allocate draw buffer (reduce to 5 lines to save heap for SSL)
    const size_t buf_size = tft.width() * 5;
    buf1 = (lv_color_t*)malloc(buf_size * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, buf_size);

    // Initialize the display
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = tft.width();
    disp_drv.ver_res = tft.height();
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Initialize the (dummy) input device driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    last_tick_millis = millis();
}

static lv_obj_t* s_default_screen = nullptr;

void loop() {
    if (s_visible) {
        lv_timer_handler();
        
        if (s_pending_hide) {
            s_pending_hide = false;
            hide();
        }
    }
}

void show() {
    s_visible = true;
    if (s_default_screen == nullptr) {
        s_default_screen = lv_scr_act();
    }
    build_ui();
    if (!settings_screen) {
        // OOM: cannot open settings screen right now
        return;
    }
    lv_scr_load(settings_screen);
    s_visible = true;
    s_pending_hide = false;
    s_show_ms = millis();
}

void hide() {
    s_visible = false;
    // Switch back to the default empty screen BEFORE deleting the settings screen
    if (s_default_screen) {
        lv_scr_load(s_default_screen);
    }
    lv_obj_del(settings_screen);
    settings_screen = nullptr;
    
    // We clear the screen completely so LovyanGFX can draw the radar over it again
    const CatppuccinColors& flavor = getCatppuccinFlavor(s_theme_flavor);
    uint32_t bg_color = flavor.base;
    tft.fillScreen(tft.color565((bg_color >> 16) & 0xFF, (bg_color >> 8) & 0xFF, bg_color & 0xFF));
    ui::radarDisplayRefreshAircraft(); // Force redraw of the bottom details panel
}

bool isVisible() { return s_visible; }
bool isAirportsEnabled() { return s_show_airports; }
bool isMediumAirportsEnabled() { return s_show_medium_airports; }
bool isGroundAircraftEnabled() { return s_show_ground_aircraft; }
bool isRadarSweepEnabled() { return s_show_radar_sweep; }
bool isAutoDimmingEnabled() { return s_auto_dimming; }
int getMaxAltitudeFilter() { return s_max_altitude; }
int getSweepRotationSpeedMs() { return s_sweep_speed; }
int getMaxAircraftLimit() { return s_max_aircraft; }

bool isTimezoneChanged() { return s_tz_changed; }
void clearTimezoneChanged() { s_tz_changed = false; }
const char* getTimezoneStr() { return tz_presets[s_tz_idx].value; }

int getThemeFlavor() { return s_theme_flavor; }
void setThemeFlavor(int flavor) {
    if (flavor >= 1 && flavor <= 4) {
        if (s_theme_flavor != flavor) {
            s_theme_flavor = flavor;
            s_prefs.putInt("theme", s_theme_flavor);
            ui::updateThemeColors();
        }
    }
}

} // namespace ui::settings
