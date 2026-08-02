#include "ui/settings_menu.h"
#include <Arduino.h>
#include <lvgl.h>
#include "hardware/display.h"
#include "ui/radar_display.h"

extern LGFX tft;

namespace ui::settings {

static bool s_visible = false;
static bool s_pending_hide = false;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_obj_t* settings_screen = nullptr;
static uint32_t last_tick_millis = 0;

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
    if (!s_visible) {
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
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        s_pending_hide = true;
    }
}

void setup() {
    lv_init();
    
    // Allocate draw buffer (reduce from 40 lines to 10 lines to save heap for SSL)
    const size_t buf_size = tft.width() * 10;
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
    
    // Build the UI
    settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0x1e1e2e), 0); // Catppuccin Mocha Base
    
    lv_obj_t * title = lv_label_create(settings_screen);
    lv_label_set_text(title, "Radar Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(0xcdd6f4), 0); // Text
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    lv_obj_t * close_btn = lv_btn_create(settings_screen);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(close_btn, close_btn_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_t * close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Close");
    
    // Add a placeholder toggle
    lv_obj_t * sw = lv_switch_create(settings_screen);
    lv_obj_align(sw, LV_ALIGN_CENTER, 0, -20);
    lv_obj_t * sw_label = lv_label_create(settings_screen);
    lv_label_set_text(sw_label, "Toggle Airports");
    lv_obj_set_style_text_color(sw_label, lv_color_hex(0xa6adc8), 0); // Subtext0
    lv_obj_align_to(sw_label, sw, LV_ALIGN_OUT_TOP_MID, 0, -10);
    
    last_tick_millis = millis();
}

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
    lv_scr_load(settings_screen);
    lv_obj_invalidate(lv_scr_act()); // Force LVGL to redraw the entire screen
}

void hide() {
    s_visible = false;
    // We clear the screen completely so LovyanGFX can draw the radar over it again
    tft.fillScreen(tft.color565(30, 30, 46)); // Mocha base
    ui::radarDisplayRefreshAircraft(); // Force redraw of the bottom details panel
}

bool isVisible() {
    return s_visible;
}

} // namespace ui::settings
