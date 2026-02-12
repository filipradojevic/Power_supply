/**
 * @file    lvgl_screens.c
 * @brief
 *
 * @version 1.0.0
 * @date    30.05.2025
 * @author  LisumLab
 */

#include <freertos/FreeRTOS.h>

/* Includes of FreeRTOS */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_err.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/* Drivers header files */
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/timer.h"
#include "driver/twai.h"
#include "hal/gpio_types.h"

/* Users header files */
#include "esp_err.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

/* LCD Display drivers */
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

/* LVGL Library */
#include "lvgl.h"

/* All tasks headers */
#include "lvgl_screens.h"
#include "main.h"
#include "task_can_receive.h"
#include "task_encoder.h"
#include "task_lvgl_ili9341.h"
#include "task_pwr_supply.h"

extern ui_objects_t objects;
extern lv_disp_t *global_disp;
extern lv_disp_t *disp;

void create_main_ui(lv_disp_t *disp);
extern bool example_lvgl_lock(int timeout_ms);
extern void example_lvgl_unlock(void);
void switch_to_main_ui(lv_timer_t *timer);
extern void voltage_update_cb(lv_timer_t *timer);

void switch_to_main_ui(lv_timer_t *timer) {
	LV_UNUSED(timer); // ako ne koristiš timer argument
	create_main_ui(global_disp);
}
void create_main_ui(lv_disp_t *disp) {
	/* Delete the current active disp */
	lv_obj_clean(lv_scr_act());

	/* Saving the pointer to global disp */
	global_disp = disp;

	// Postavi landscape orijentaciju
	lv_disp_set_rotation(disp, LV_DISP_ROT_90);
	lv_obj_t *scr = lv_disp_get_scr_act(disp);

	// Crna pozadina ekrana
	lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000),
							  LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

	// Glavni objekat
	lv_obj_t *obj = lv_obj_create(scr);
	objects.main = obj;
	lv_obj_set_pos(obj, 0, 0);
	lv_obj_set_size(obj, 320, 240);

	// Crna pozadina
	lv_obj_set_style_bg_color(obj, lv_color_hex(0x000000),
							  LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

	// Ukloni border/okvir
	lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	// Ukloni padding
	lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	// Ukloni outline
	lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	// Ukloni shadow/senku
	lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	{
		lv_obj_t *parent_obj = obj;
		// Voltage ARC
		{
			lv_obj_t *obj = lv_arc_create(parent_obj);
			objects.obj0 = obj;
			lv_obj_set_pos(obj, 12, 75);
			lv_obj_set_size(obj, 100, 95);
			lv_arc_set_range(obj, 408, 582);
			lv_arc_set_value(obj, 495);

			// Default (track)
			lv_obj_set_style_arc_width(obj, 10,
									   LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_arc_color(obj, lv_color_hex(0x2A2A2A),
									   LV_PART_MAIN | LV_STATE_DEFAULT);

			// Indicator
			lv_obj_set_style_arc_width(obj, 10,
									   LV_PART_INDICATOR | LV_STATE_DEFAULT);
			lv_obj_set_style_arc_color(obj, lv_color_hex(0x00E5FF),
									   LV_PART_INDICATOR | LV_STATE_DEFAULT);

			// Knob
			lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP,
									LV_PART_KNOB | LV_STATE_DEFAULT);

			// Current ARC
			{
				lv_obj_t *obj = lv_arc_create(parent_obj);
				objects.obj1 = obj;
				lv_obj_set_pos(obj, 135, 75);
				lv_obj_set_size(obj, 100, 95);
				lv_arc_set_range(obj, 0, 1500);
				lv_arc_set_value(obj, 20);

				// Default (track)
				lv_obj_set_style_arc_width(obj, 10,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_arc_color(obj, lv_color_hex(0x2A2A2A),
										   LV_PART_MAIN | LV_STATE_DEFAULT);

				// Indicator
				lv_obj_set_style_arc_width(
					obj, 10, LV_PART_INDICATOR | LV_STATE_DEFAULT);
				lv_obj_set_style_arc_color(obj, lv_color_hex(0x00E5FF),
										   LV_PART_INDICATOR |
											   LV_STATE_DEFAULT);

				// Knob
				lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP,
										LV_PART_KNOB | LV_STATE_DEFAULT);
			}
			{
				// Voltage_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.voltage_label = obj;
				lv_obj_set_pos(obj, 8, 170);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_18,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_pad_all(obj, 6,
										 LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "Voltage [V]");
			}
			{
				// Current_label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.voltage_label_1 = obj;
				lv_obj_set_pos(obj, 131, 170);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_18,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_pad_all(obj, 6,
										 LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "Current [A]");
			}
			{
				// Current_label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.label1 = obj;
				lv_obj_set_pos(obj, 250, 6);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_10,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_pad_all(obj, 6,
										 LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "Made by:");
			}
			{
				// Current_label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.label2 = obj;
				lv_obj_set_pos(obj, 250, 27);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_10,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_pad_all(obj, 6,
										 LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "LisumLab");
			}
			{
				// Curr_Limit_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.curr_limit_label = obj;
				lv_obj_set_pos(obj, 165, 205);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_16,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_pad_all(obj, 6,
										 LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "Curr Limit [A]");
			}
			{
				// Vol_Change
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.vol_change = obj;
				lv_obj_set_pos(obj, 29, 108);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_30,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xFFFFFF),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0.0");
			}

			{
				// Power_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.power1_label = obj;
				lv_obj_set_pos(obj, 5, 9);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "P1 [W]");
			}
			{
				// Power_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.power2_label = obj;
				lv_obj_set_pos(obj, 85, 9);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "P2 [W]");
			}
			{
				// Power_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.power3_label = obj;
				lv_obj_set_pos(obj, 165, 9);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "P3 [W]");
			}
			{
				// Power_value
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.power1_value = obj;
				lv_obj_set_pos(obj, 52, 9);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0");
			}
			{
				// Power_value
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.power2_value = obj;
				lv_obj_set_pos(obj, 135, 9);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0");
			}
			{
				// Power_value
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.power3_value = obj;
				lv_obj_set_pos(obj, 215, 9);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0");
			}
			{
				// Temp_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.temp1_label = obj;
				lv_obj_set_pos(obj, 6, 30);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "T1 [°C]");
			}
			{
				// Temp_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.temp2_label = obj;
				lv_obj_set_pos(obj, 86, 30);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "T2 [°C]");
			}
			{
				// Temp_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.temp3_label = obj;
				lv_obj_set_pos(obj, 166, 30);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "T3 [°C]");
			}
			{
				// Temp_value
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.temp1_value = obj;
				lv_obj_set_pos(obj, 53, 30);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0");
			}
			{
				// Temp_value
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.temp2_value = obj;
				lv_obj_set_pos(obj, 135, 30);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0");
			}
			{
				// Temp_value
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.temp3_value = obj;
				lv_obj_set_pos(obj, 215, 30);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_14,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0");
			}
			{
				// Curr1_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.curr1_label = obj;
				lv_obj_set_pos(obj, 240, 70);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_12,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "C1 [A]");
			}
			{
				// Curr2_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.curr2_label = obj;
				lv_obj_set_pos(obj, 240, 110);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_12,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "C2 [A]");
			}
			{
				// Curr3_Label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.curr3_label = obj;
				lv_obj_set_pos(obj, 240, 150);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_12,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "C3 [A]");
			}
			{
				// Curr1_value
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.curr1_value = obj;
				lv_obj_set_pos(obj, 280, 70);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_12,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0.0");
			}
			{
				// Curr2_value
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.curr2_value = obj;
				lv_obj_set_pos(obj, 280, 110);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_12,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0.0");
			}
			{
				// Curr3_value
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.curr3_value = obj;
				lv_obj_set_pos(obj, 280, 150);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_12,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0.0");
			}
			{
				// Curr_Limit_Change
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.curr_limit_change = obj;
				lv_obj_set_pos(obj, 285, 211);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_16,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0.0");
			}
			{
				// Curr Change
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.curr_change = obj;
				lv_obj_set_pos(obj, 159, 108);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_30,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_opa(obj, 255,
										  LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0.0");
			}
			{
				// Efficiency_label
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.curr_limit_label_1 = obj;
				lv_obj_set_pos(obj, 2, 209);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_16,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "Efficiency [%]");
			}
			{
				// LED1
				lv_obj_t *obj = lv_led_create(parent_obj);
				objects.led1 = obj;
				lv_obj_set_pos(obj, 308, 74);
				lv_obj_set_size(obj, 8, 8);
				lv_led_set_color(obj, lv_color_hex(0xff00ff00));
				lv_led_set_brightness(obj, 255);
			}
			{
				// LED2
				lv_obj_t *obj = lv_led_create(parent_obj);
				objects.led2 = obj;
				lv_obj_set_pos(obj, 308, 114);
				lv_obj_set_size(obj, 8, 8);
				lv_led_set_color(obj, lv_color_hex(0xff00ff00));
				lv_led_set_brightness(obj, 255);
			}
			{
				// LED3
				lv_obj_t *obj = lv_led_create(parent_obj);
				objects.led3 = obj;
				lv_obj_set_pos(obj, 308, 154);
				lv_obj_set_size(obj, 8, 8);
				lv_led_set_color(obj, lv_color_hex(0xff00ff00));
				lv_led_set_brightness(obj, 255);
			}
			{
				// Effieciency
				lv_obj_t *obj = lv_label_create(parent_obj);
				objects.effieciency = obj;
				lv_obj_set_pos(obj, 118, 211);
				lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
				lv_obj_set_style_text_font(obj, &lv_font_montserrat_16,
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff),
											LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_label_set_text(obj, "0.0");
			}
			{
				// Voltage_set_point_arc
				lv_obj_t *obj = lv_arc_create(parent_obj);
				objects.arc3 = obj;
				lv_obj_set_pos(obj, 5, 68);
				lv_obj_set_size(obj, 109, 130);
				lv_arc_set_range(obj, 408, 582);
				lv_arc_set_value(obj, 495);

				// Default
				lv_obj_set_style_arc_color(obj, lv_color_hex(0xFF616161),
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_arc_width(obj, 8,
										   LV_PART_MAIN | LV_STATE_DEFAULT);

				// Indicator
				lv_obj_set_style_arc_color(obj, lv_color_hex(0xEF0E0E),
										   LV_PART_INDICATOR |
											   LV_STATE_DEFAULT);
				lv_obj_set_style_arc_width(
					obj, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

				// Knob
				lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP,
										LV_PART_KNOB | LV_STATE_DEFAULT);
			}
			{
				// Current_set_point_arc
				lv_obj_t *obj = lv_arc_create(parent_obj);
				objects.arc4 = obj;
				lv_obj_set_pos(obj, 128, 68);
				lv_obj_set_size(obj, 109, 130);
				lv_arc_set_range(obj, 0, 500);
				lv_arc_set_value(obj, 20);

				// Default
				lv_obj_set_style_arc_color(obj, lv_color_hex(0xFF616161),
										   LV_PART_MAIN | LV_STATE_DEFAULT);
				lv_obj_set_style_arc_width(obj, 8,
										   LV_PART_MAIN | LV_STATE_DEFAULT);

				// Indicator
				lv_obj_set_style_arc_color(obj, lv_color_hex(0xEF0E0E),
										   LV_PART_INDICATOR |
											   LV_STATE_DEFAULT);
				lv_obj_set_style_arc_width(
					obj, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

				// Knob
				lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP,
										LV_PART_KNOB | LV_STATE_DEFAULT);
			}
		}
	}
}