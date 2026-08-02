import sys
import re

with open("src/ui/settings_menu.cpp", "r") as f:
    content = f.read()

# We want to replace the static bools section and add the new getters, setters, and UI elements.

new_state = """static bool s_visible = false;
static bool s_pending_hide = false;
static bool s_show_airports = true;
static bool s_show_medium_airports = false;
static bool s_show_ground_aircraft = false;
static bool s_show_radar_sweep = true;
static bool s_auto_dimming = false;
static int s_max_altitude = 50000;
static int s_sweep_speed = 6000;
static lv_disp_draw_buf_t draw_buf;"""

content = re.sub(r'static bool s_visible = false;.*?static lv_disp_draw_buf_t draw_buf;', new_state, content, flags=re.DOTALL)

# Add event handlers
new_event_handlers = """
static void medium_airports_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    s_show_medium_airports = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void ground_aircraft_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    s_show_ground_aircraft = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void radar_sweep_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    s_show_radar_sweep = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void auto_dimming_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    s_auto_dimming = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void max_altitude_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    s_max_altitude = lv_slider_get_value(slider);
    lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
    if(label) lv_label_set_text_fmt(label, "%d ft", s_max_altitude);
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
    lv_obj_set_style_bg_color(row, get_lv_color(getCatppuccinFlavor(CATPPUCCIN_MOCHA).mantle), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 10, 0);

    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, get_lv_color(getCatppuccinFlavor(CATPPUCCIN_MOCHA).text), 0);

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
    lv_obj_set_style_bg_color(row, get_lv_color(getCatppuccinFlavor(CATPPUCCIN_MOCHA).mantle), 0);
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
    lv_obj_set_style_text_color(lbl, get_lv_color(getCatppuccinFlavor(CATPPUCCIN_MOCHA).text), 0);
    
    lv_obj_t * val_lbl = lv_label_create(header);
    lv_label_set_text_fmt(val_lbl, "%d %s", initial_val, unit);
    lv_obj_set_style_text_color(val_lbl, get_lv_color(getCatppuccinFlavor(CATPPUCCIN_MOCHA).blue), 0);

    lv_obj_t * slider = lv_slider_create(row);
    lv_slider_set_range(slider, min_val, max_val);
    lv_slider_set_value(slider, initial_val, LV_ANIM_OFF);
    lv_obj_set_width(slider, LV_PCT(100));
    lv_obj_add_event_cb(slider, event_cb, LV_EVENT_VALUE_CHANGED, val_lbl);
    
    return row;
}

void setup() {"""

content = content.replace("void setup() {", new_event_handlers)

# Replace the UI building section
new_ui = """
    // Settings List
    create_toggle_row(list, "Large Airports", s_show_airports, airports_switch_event_cb);
    create_toggle_row(list, "Medium Airports", s_show_medium_airports, medium_airports_switch_event_cb);
    create_toggle_row(list, "Ground Aircraft", s_show_ground_aircraft, ground_aircraft_switch_event_cb);
    create_toggle_row(list, "Radar Sweep", s_show_radar_sweep, radar_sweep_switch_event_cb);
    create_toggle_row(list, "Auto-Dim Night", s_auto_dimming, auto_dimming_switch_event_cb);
    
    create_slider_row(list, "Max Alt", 0, 50000, s_max_altitude, "ft", max_altitude_slider_event_cb);
    create_slider_row(list, "Sweep Speed", 1000, 15000, s_sweep_speed, "ms", sweep_speed_slider_event_cb);
"""

# Find the list setup and replace the Toggle Airports row
pattern = r'    // Toggle Airports row.*?lv_obj_add_event_cb\(sw_airports, airports_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL\);'
content = re.sub(pattern, new_ui, content, flags=re.DOTALL)

new_getters = """bool isMediumAirportsEnabled() { return s_show_medium_airports; }
bool isGroundAircraftEnabled() { return s_show_ground_aircraft; }
bool isRadarSweepEnabled() { return s_show_radar_sweep; }
bool isAutoDimmingEnabled() { return s_auto_dimming; }
int getMaxAltitudeFilter() { return s_max_altitude; }
int getSweepRotationSpeedMs() { return s_sweep_speed; }

} // namespace ui::settings"""

content = content.replace("} // namespace ui::settings", new_getters)

with open("src/ui/settings_menu.cpp", "w") as f:
    f.write(content)
