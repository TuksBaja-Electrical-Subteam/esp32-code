#ifdef ESP_PLATFORM
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

static lv_obj_t *arc;
static lv_obj_t *speed_label;
static lv_obj_t *rpm_label;

void ui_init(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x111111), LV_PART_MAIN);

    arc = lv_arc_create(lv_scr_act());
    lv_obj_set_size(arc, 220, 220);

    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 10);
    lv_arc_set_rotation(arc, 0);
    lv_arc_set_value(arc, 0);
    lv_arc_set_range(arc, 0, 80);

    speed_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(speed_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(speed_label, "0");
    lv_obj_align(speed_label, LV_ALIGN_TOP_MID, 0, 90);

    rpm_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(rpm_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(rpm_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_label_set_text(rpm_label, "0 RPM");
    lv_obj_align(rpm_label, LV_ALIGN_TOP_MID, 0, 240);
}

void ui_update(float speed_kmh, float rpm)
{
    lv_arc_set_value(arc, (int)speed_kmh);

    if (speed_kmh < 40.0f)
    {
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x1D9E75), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    else if (speed_kmh < 65.0f)
    {
        lv_obj_set_style_arc_color(arc, lv_color_hex(0xEF9F27), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    else
    {
        lv_obj_set_style_arc_color(arc, lv_color_hex(0xE24B4A), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }

    lv_label_set_text_fmt(speed_label, "%.0f", speed_kmh);
    lv_label_set_text_fmt(rpm_label, "%.0f RPM", rpm);
}