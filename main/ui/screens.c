#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;

void create_screen_main() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj0 = obj;
            lv_obj_set_pos(obj, 0, 224);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_CHECKED);
            lv_label_set_text(obj, "Made by: BetaTehPro");
        }
        {
            // Command_Roller
            lv_obj_t *obj = lv_roller_create(parent_obj);
            objects.command_roller = obj;
            lv_obj_set_pos(obj, 0, 58);
            lv_obj_set_size(obj, 114, 109);
            lv_roller_set_options(obj, "Voltage_Out\nCurrent_Limit", LV_ROLLER_MODE_NORMAL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.obj1 = obj;
            lv_obj_set_pos(obj, 128, 83);
            lv_obj_set_size(obj, 75, 75);
            lv_arc_set_range(obj, 41, 59);
            lv_arc_set_value(obj, 25);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xffa62424), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_rounded(obj, true, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_MAIN | LV_STATE_CHECKED);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xff2196f3), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_INDICATOR | LV_STATE_CHECKED);
            lv_obj_set_style_arc_width(obj, 2121, LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xffffffff), LV_PART_KNOB | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.obj2 = obj;
            lv_obj_set_pos(obj, 233, 83);
            lv_obj_set_size(obj, 75, 75);
            lv_arc_set_range(obj, 41, 59);
            lv_arc_set_value(obj, 25);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_rounded(obj, true, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xffa62424), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
        {
            // Voltage_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.voltage_label = obj;
            lv_obj_set_pos(obj, 126, 158);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "Voltage [V]");
        }
        {
            // Curr_Limit_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.curr_limit_label = obj;
            lv_obj_set_pos(obj, 187, 224);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "Curr Limit [A]");
        }
        {
            // Vol_Change
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.vol_change = obj;
            lv_obj_set_pos(obj, 150, 108);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "47");
        }
        {
            // Power_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.power_label = obj;
            lv_obj_set_pos(obj, 11, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "Power [W]");
        }
        {
            // Power_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.power_value = obj;
            lv_obj_set_pos(obj, 99, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "13131");
        }
        {
            // Temp_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.temp_label = obj;
            lv_obj_set_pos(obj, 159, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "Temperature [°C]");
        }
        {
            // Temp_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.temp_value = obj;
            lv_obj_set_pos(obj, 293, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "20");
        }
        {
            // Curr_Limit_Change
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.curr_limit_change = obj;
            lv_obj_set_pos(obj, 302, 224);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "2");
        }
        {
            // Curr_Change
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.curr_change = obj;
            lv_obj_set_pos(obj, 255, 108);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "47");
        }
        {
            // Voltage_Label_1
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.voltage_label_1 = obj;
            lv_obj_set_pos(obj, 229, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "Current [A]");
        }
    }
    
    tick_screen_main();
}

void tick_screen_main() {
}

void create_screen_intro() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.intro = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_spinner_create(parent_obj, 1000, 60);
            lv_obj_set_pos(obj, 120, 40);
            lv_obj_set_size(obj, 80, 80);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 33, 144);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Loading the system...");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 84, 184);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "Made by: BetaTehPro");
        }
    }
    
    tick_screen_intro();
}

void tick_screen_intro() {
}



typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
    tick_screen_intro,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_main();
    create_screen_intro();
}
