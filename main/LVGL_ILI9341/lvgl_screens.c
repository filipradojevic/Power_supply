/**
 * @file    lvgl_screens.c
 * @brief   
 * 
 * @version 1.0.0
 * @date    30.05.2025
 * @author  LisumLab
 */


/* Includes of FreeRTOS */
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
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
#include "lvgl_screens.h"

extern ui_objects_t objects;
extern lv_disp_t *global_disp;
extern lv_disp_t *disp;

void create_main_ui(lv_disp_t *disp);
extern bool example_lvgl_lock(int timeout_ms);
extern void example_lvgl_unlock(void);
void switch_to_main_ui(lv_timer_t *timer);
extern void voltage_update_cb(lv_timer_t *timer);


void switch_to_main_ui(lv_timer_t *timer)
{
    LV_UNUSED(timer);  // ako ne koristiš timer argument
    create_main_ui(global_disp);
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
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.obj0 = obj;
            lv_obj_set_pos(obj, 29, 68);
            lv_obj_set_size(obj, 103, 99);
            lv_arc_set_range(obj, 408, 582);
            lv_arc_set_value(obj, 495);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xFFBDBDBD), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_MAIN | LV_STATE_CHECKED);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xFFFF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_INDICATOR | LV_STATE_CHECKED);
            
			// Potpuno sakrij knob
			lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_DEFAULT);
        {
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.obj1 = obj;
            lv_obj_set_pos(obj, 178, 68);
            lv_obj_set_size(obj, 103, 99);
            lv_arc_set_range(obj, 0, 500);
            lv_arc_set_value(obj, 20);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xFFBDBDBD), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 6, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(0xFFFF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            
            
			// Potpuno sakrij knob
			lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_DEFAULT);
        }
        {
            // Voltage_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.voltage_label = obj;
            lv_obj_set_pos(obj, 28, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    		lv_obj_set_style_pad_all(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);   
            lv_label_set_text(obj, "Voltage [V]");
        }
        {
            // Curr_Limit_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.curr_limit_label = obj;
            lv_obj_set_pos(obj, 136, 189);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
		    lv_obj_set_style_pad_all(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Current Limit [A]");
        }
        {
            // Vol_Change
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.vol_change = obj;
            lv_obj_set_pos(obj, 47, 102);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "49.5");
        }
        
        {
            // Power_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.power_label = obj;
            lv_obj_set_pos(obj, 0, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Power [W]");
        }
        {
            // Power_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.power_value = obj;
            lv_obj_set_pos(obj, 81, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "200");
        }
        {
            // Temp_Label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.temp_label = obj;
            lv_obj_set_pos(obj, 134, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Temperature [°C]");
        }
        {
            // Temp_value
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.temp_value = obj;
            lv_obj_set_pos(obj, 264, 9);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "20");
        }
        {
            // Curr_Limit_Change
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.curr_limit_change = obj;
            lv_obj_set_pos(obj, 269, 195);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "2");
        }
        {
            // Curr_Change
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.curr_change = obj;
            lv_obj_set_pos(obj, 210, 102);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "1.6");
        }
        {
            // Voltage_Label_1
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.voltage_label_1 = obj;
            lv_obj_set_pos(obj, 180, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Current [A]");
        }
        {
            // Curr_Limit_Label_1
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.curr_limit_label_1 = obj;
            lv_obj_set_pos(obj, -2, 195);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Efficiency [%]");
        }
        {
            // Slider_power
            lv_obj_t *obj = lv_slider_create(parent_obj);
            objects.slider_power = obj;
            lv_obj_set_pos(obj, -1, 34);
            lv_obj_set_size(obj, 108, 1);
            lv_slider_set_range(obj, 0, 2900);
            lv_slider_set_value(obj, 200, LV_ANIM_OFF);
        }
        {
            // Slider_temp
            lv_obj_t *obj = lv_slider_create(parent_obj);
            objects.slider_temp = obj;
            lv_obj_set_pos(obj, 134, 33);
            lv_obj_set_size(obj, 139, 1);
            lv_slider_set_range(obj, 0, 100);
            lv_slider_set_value(obj, 60, LV_ANIM_OFF);
        }
        {
            // LED
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.led = obj;
            lv_obj_set_pos(obj, 294, 200);
            lv_obj_set_size(obj, 4, 4);
            lv_led_set_color(obj, lv_color_hex(0xff00ff26));
            lv_led_set_brightness(obj, 180);
        }
        {
            // Effieciency
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.effieciency = obj;
            lv_obj_set_pos(obj, 98, 195);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "96");
        }
        {
		    lv_obj_t *obj = lv_arc_create(parent_obj);
		    objects.arc3 = obj;
		    lv_obj_set_pos(obj, 22, 61);
		    lv_obj_set_size(obj, 113, 125);
		    lv_arc_set_range(obj, 408, 582);
		    lv_arc_set_value(obj, 495);
		
		    // Diskretna glavna linija
			lv_obj_set_style_arc_color(obj, lv_color_hex(0xFF616161), LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_arc_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
			
			// Indikator
			lv_obj_set_style_arc_color(obj, lv_color_hex(0xaa00aa), LV_PART_INDICATOR | LV_STATE_DEFAULT);
			lv_obj_set_style_arc_width(obj, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);
			
			// Potpuno sakrij knob
			lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_DEFAULT);
		}
		{
		    lv_obj_t *obj = lv_arc_create(parent_obj);
			objects.arc4 = obj;
			lv_obj_set_pos(obj, 171, 61);
			lv_obj_set_size(obj, 113, 125);
			lv_arc_set_range(obj, 0, 500);
			lv_arc_set_value(obj, 20);
			
			// Diskretna glavna linija
			lv_obj_set_style_arc_color(obj, lv_color_hex(0xFF616161), LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_arc_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
			
			// Indikator
			lv_obj_set_style_arc_color(obj, lv_color_hex(0xaa00aa), LV_PART_INDICATOR | LV_STATE_DEFAULT);
			lv_obj_set_style_arc_width(obj, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);
			
			lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_DEFAULT);
		}
    }
}
}