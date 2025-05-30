/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// This demo UI is adapted from LVGL official example: https://docs.lvgl.io/master/widgets/extra/meter.html#simple-meter

/* Includes of FreeRTOS */
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include "esp_err.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* Drivers header files */
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "driver/timer.h"
#include "hal/gpio_types.h"

/* Users header files */
#include "esp_timer.h"
#include "esp_err.h"
#include "rom/ets_sys.h"

/* LCD Display drivers */
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"

/* LVGL Library */
#include "lvgl.h"

/* All tasks headers */
#include "main.h"
#include "task_encoder.h"
#include "task_lvgl_ili9341.h"
#include "task_can_receive.h"
#include "task_pwr_supply.h"

extern ui_objects_t objects;
extern lv_disp_t *global_disp;
extern lv_disp_t *disp;

void create_main_ui(lv_disp_t *disp);
void create_intro_ui(lv_disp_t *disp);
extern bool example_lvgl_lock(int timeout_ms);
extern void example_lvgl_unlock(void);
void switch_to_main_ui(lv_timer_t *timer);
extern void voltage_update_cb(lv_timer_t *timer);



void switch_to_main_ui(lv_timer_t *timer)
{
    LV_UNUSED(timer);  // ako ne koristiš timer argument
    create_main_ui(global_disp);
}

void create_intro_ui(lv_disp_t *disp)
{
	/* Saving the pointer to global disp */
    global_disp = disp;
    
	/* Rotate the screen */
    lv_disp_set_rotation(disp, LV_DISP_ROT_90);
    
    /* This function return pointer to active display */
    lv_obj_t *scr = lv_disp_get_scr_act(disp);

	/* This create object screen (active display) */
    lv_obj_t *obj = lv_obj_create(scr);
    objects.intro = obj;
    
    /* Initialize this size of active screen */
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);

    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_spinner_create(parent_obj, 1000, 60);
            lv_obj_set_pos(obj, 110, 40);
            lv_obj_set_size(obj, 80, 80);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj3 = obj;
            lv_obj_set_pos(obj, 74, 139);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Loading the system...");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj4 = obj;
            lv_obj_set_pos(obj, 74, 174);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Made by: BetaTehPro");
        }
    }
    /* Setting the timer callback to switch screen */
    lv_timer_create(switch_to_main_ui, 5000, NULL);
}

void create_main_ui(lv_disp_t *disp)
{
	/* Delete the current active disp */
	lv_obj_clean(lv_scr_act());
	
	/* Saving the pointer to global disp */
    global_disp = disp;
    
	// Postavi landscape orijentaciju
    lv_disp_set_rotation(disp, LV_DISP_ROT_90);
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
	lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_t *obj = lv_obj_create(scr);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj0 = obj;
            lv_obj_set_pos(obj, 0, 195);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
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
        	//Arc for voltage
        {
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.obj1 = obj;
            lv_obj_set_pos(obj, 128, 75);
            lv_obj_set_size(obj, 75, 75);
            lv_arc_set_range(obj, 41, 59);
            lv_arc_set_value(obj, 25);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xffff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_rounded(obj, true, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_MAIN | LV_STATE_CHECKED);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xff2196f3), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_INDICATOR | LV_STATE_CHECKED);
            lv_obj_set_style_arc_width(obj, 2121, LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xff000000), LV_PART_KNOB | LV_STATE_DEFAULT);
        }
        //Arc for curr
        {
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.obj2 = obj;
            lv_obj_set_pos(obj, 222, 75);
            lv_obj_set_size(obj, 75, 75);
            lv_arc_set_range(obj, 41, 59);
            lv_arc_set_value(obj, 25);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_rounded(obj, true, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xffff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xff2196f3), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
        {
            // Voltage_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.voltage_label = obj;
            lv_obj_set_pos(obj, 126, 151);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Voltage [V]");
        }
        {
            // Curr_Limit_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.curr_limit_label = obj;
            lv_obj_set_pos(obj, 174, 195);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Curr Limit [A]");
        }
        {
            // Vol_Change
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.vol_change = obj;
            lv_obj_set_pos(obj, 157, 105);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "47");
        }
        {
            // Power_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.power_label = obj;
            lv_obj_set_pos(obj, 1, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Power [W]");
        }
        {
            // Power_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.power_value = obj;
            lv_obj_set_pos(obj, 83, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "200");
        }
        {
            // Temp_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.temp_label = obj;
            lv_obj_set_pos(obj, 143, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Temperature [°C]");
        }
        {
            // Temp_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.temp_value = obj;
            lv_obj_set_pos(obj, 278, 10);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "20");
        }
        {
            // Curr_Limit_Change
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.curr_limit_change = obj;
            lv_obj_set_pos(obj, 278, 195);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "2");
        }
        {
            // Curr_Change
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.curr_change = obj;
            lv_obj_set_pos(obj, 251, 105);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "47");
        }
        {
            // Voltage_Label_1
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.voltage_label_1 = obj;
            lv_obj_set_pos(obj, 219, 151);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Current [A]");
        }
    }
}
