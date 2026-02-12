/**
 * @file    task_lvgl_ili9341.c
 * @brief
 *
 * @version 1.0.0
 * @date    30.05.2025
 * @author  LisumLab
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include <freertos/FreeRTOS.h>

/* Includes of FreeRTOS */
#include "esp_err.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>

/* Drivers header files */
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/timer.h"
#include "driver/twai.h"
#include "driver/uart.h"
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

/*******************************************************************************
 * Defines
 ******************************************************************************/

/*******************************************************************************
 * User variables
 ******************************************************************************/

/* User variables */
extern esp_err_t esp_err;

static bool led_on = false;
extern volatile bool can_alive_flag;

extern button_pressed_e button_flag;
static send_type_e last_flag_value = SEND_VOLTAGE;

static uint32_t last_encoder_activity_time = 0;
static uint32_t last_led_toggle_time = 0;
static const uint32_t encoder_timeout_ms = 2500;
static const uint32_t led_timeout_ms = 250;

static uint8_t led1_cnt = 0;
static uint8_t led2_cnt = 0;
static uint8_t led3_cnt = 0;

static char value_str[16];

/* LVGL variables */
extern lv_disp_t *global_disp; /* Global Current Active Display */
extern lv_disp_draw_buf_t
	disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
extern lv_disp_drv_t disp_drv; // contains callback functions
extern esp_lcd_panel_handle_t
	panel_handle; /* Display that indicate LVGL which one we use */
extern lv_disp_t *disp;
extern ui_objects_t objects;

/* FreeRTOS objects */
extern QueueHandle_t lvgl_voltage_queue;
extern QueueHandle_t lvgl_current_queue;
extern QueueHandle_t lvgl_update_queue;
extern QueueHandle_t lvgl_bolding_update;
extern QueueHandle_t lvgl_button_pressed;
extern QueueSetHandle_t xQueueSetLvgl;

extern SemaphoreHandle_t lvgl_mux;
extern SemaphoreHandle_t watchdog_semaphore;

/*******************************************************************************
 * Main function
 ******************************************************************************/

void task_lvgl_ili9341(void *arg) {
	QueueSetMemberHandle_t activated_queue;
	lvgl_bonded_data_t lvgl_received;
	uint32_t received_value;

	send_type_e voltage_current_flag = SEND_VOLTAGE;
	button_pressed_e button_pressed_flag = BUTTON_NOT_PRESSED;

	for (;;) {

		activated_queue = xQueueSelectFromSet(xQueueSetLvgl, portMAX_DELAY);

		if (!lvgl_lock(-1)) {
			continue;
		}

		if (activated_queue == watchdog_semaphore) {

			xSemaphoreTake(watchdog_semaphore, 0); // Clear the semaphore

			/* Handle CAN bus error indication */
			display_error(&led_on);
		}

		/* -------- Voltage -------- */
		if (activated_queue == lvgl_voltage_queue &&
			xQueueReceive(lvgl_voltage_queue, &received_value, 0)) {

			float voltage = ((float)received_value / 1000.0f) - 1.5f;
			change_voltage_value(voltage);
		}

		/* -------- Current limit -------- */
		else if (activated_queue == lvgl_current_queue &&
				 xQueueReceive(lvgl_current_queue, &received_value, 0)) {

			float cur_limit = received_value / 30.0f;
			change_current_limit_value(cur_limit);
		}

		/* -------- Main CAN update -------- */
		else if (activated_queue == lvgl_update_queue &&
				 xQueueReceive(lvgl_update_queue, &lvgl_received, 0)) {

			update_lvgl_display(&lvgl_received, button_pressed_flag,
								voltage_current_flag, &button_pressed_flag);
		}

		/* -------- Encoder selection -------- */
		else if (activated_queue == lvgl_bolding_update &&
				 xQueueReceive(lvgl_bolding_update, &voltage_current_flag, 0)) {

			update_voltage_current_labels(voltage_current_flag);
		}

		/* -------- Button -------- */
		else if (activated_queue == lvgl_button_pressed &&
				 xQueueReceive(lvgl_button_pressed, &button_pressed_flag, 0)) {

			update_voltage_current_change(button_pressed_flag,
										  voltage_current_flag);
		}

		/* -------- LVGL tick -------- */
		lv_timer_handler();

		lvgl_unlock();
	}
}

void display_error(bool *led_on) {
	/* Set LED color to yellow */
	lv_led_set_color(objects.led1, lv_color_hex(0xFFFF00));
	lv_led_set_color(objects.led2, lv_color_hex(0xFFFF00));
	lv_led_set_color(objects.led3, lv_color_hex(0xFFFF00));

	/* Blink the LED by toggling brightness */
	if (*led_on) {
		lv_led_set_brightness(objects.led1, 255); /* Full brightness */
		lv_led_set_brightness(objects.led2, 255); /* Full brightness */
		lv_led_set_brightness(objects.led3, 255); /* Full brightness */
		// lv_label_set_text(objects.curr_limit_label_1, "CAN BUS ERROR");
		// lv_label_set_text(objects.effieciency, "");
	} else {
		lv_led_set_brightness(objects.led1, 50); /* Dimmed brightness */
		lv_led_set_brightness(objects.led2, 50); /* Dimmed brightness */
		lv_led_set_brightness(objects.led3, 50); /* Dimmed brightness */
		// lv_label_set_text(objects.curr_limit_label_1, "");
		// lv_label_set_text(objects.effieciency, "");
	}

	/* Toggle LED state */
	*led_on = !(*led_on);
}

void update_lvgl_display(const lvgl_bonded_data_t *data,
						 button_pressed_e button_flag,
						 send_type_e activity_encoder,
						 button_pressed_e *button_pressed_flag) {
	char buf[32];
	static bool led_state1 = false;
	static bool led_state2 = false;
	static bool led_state3 = false;

	// Static variables to track previous values
	static float prev_voltage = -1.0f;
	static float prev_limit = -1.0f;
	static float prev_current_total = -1.0f;

	// Separate tracking for each power supply
	static float prev_current1 = -1.0f;
	static float prev_current2 = -1.0f;
	static float prev_current3 = -1.0f;

	static int prev_temp1 = -999;
	static int prev_temp2 = -999;
	static int prev_temp3 = -999;

	static int prev_power1 = -999;
	static int prev_power2 = -999;
	static int prev_power3 = -999;

	static float prev_efficiency = -1.0f;

	/* =========================================================
	 * Voltage & Current limit (only if encoder not active)
	 * ========================================================= */
	if (*button_pressed_flag == BUTTON_NOT_PRESSED) {

		// Update voltage only if changed
		if (prev_voltage != data->display_1.voltage) {
			snprintf(buf, sizeof(buf), "%.1f", data->display_1.voltage);
			lv_label_set_text(objects.vol_change, buf);
			lv_arc_set_value(objects.obj0, (int)(data->display_1.voltage * 10));
			prev_voltage = data->display_1.voltage;
		}

		// Update current limit only if changed
		if (prev_limit != data->display_1.limit) {
			snprintf(buf, sizeof(buf), "%.1f", data->display_1.limit);
			lv_label_set_text(objects.curr_limit_change, buf);
			prev_limit = data->display_1.limit;
		}
	}

	/* =========================================================
	 * Total Current (sum of all three)
	 * ========================================================= */
	float current_total = data->display_1.current + data->display_2.current +
						  data->display_3.current;

	if (prev_current_total != current_total) {
		snprintf(buf, sizeof(buf), "%.1f", current_total);
		lv_label_set_text(objects.curr_change, buf);
		lv_arc_set_value(objects.obj1, (int)(current_total * 10));
		prev_current_total = current_total;
	}

	/* =========================================================
	 * Individual Current values
	 * ========================================================= */

	/* Curr1 value */
	if (prev_current1 != data->display_1.current) {
		snprintf(buf, sizeof(buf), "%.1f", data->display_1.current);
		lv_label_set_text(objects.curr1_value, buf);
		prev_current1 = data->display_1.current;
	}

	/* Curr2 value */
	if (prev_current2 != data->display_2.current) {
		snprintf(buf, sizeof(buf), "%.1f", data->display_2.current);
		lv_label_set_text(objects.curr2_value, buf);
		prev_current2 = data->display_2.current;
	}

	/* Curr3 value */
	if (prev_current3 != data->display_3.current) {
		snprintf(buf, sizeof(buf), "%.1f", data->display_3.current);
		lv_label_set_text(objects.curr3_value, buf);
		prev_current3 = data->display_3.current;
	}

	/* =========================================================
	 * Temperature values
	 * ========================================================= */

	/* Temp1 value */
	if (prev_temp1 != (int)data->display_1.temp) {
		snprintf(buf, sizeof(buf), "%d", (int)data->display_1.temp);
		lv_label_set_text(objects.temp1_value, buf);
		prev_temp1 = (int)data->display_1.temp;
	}

	/* Temp2 value */
	if (prev_temp2 != (int)data->display_2.temp) {
		snprintf(buf, sizeof(buf), "%d", (int)data->display_2.temp);
		lv_label_set_text(objects.temp2_value, buf);
		prev_temp2 = (int)data->display_2.temp;
	}

	/* Temp3 value */
	if (prev_temp3 != (int)data->display_3.temp) {
		snprintf(buf, sizeof(buf), "%d", (int)data->display_3.temp);
		lv_label_set_text(objects.temp3_value, buf);
		prev_temp3 = (int)data->display_3.temp;
	}

	/* =========================================================
	 * Power values
	 * ========================================================= */

	/* Power1 value */
	if (prev_power1 != (int)data->display_1.power) {
		snprintf(buf, sizeof(buf), "%d", (int)data->display_1.power);
		lv_label_set_text(objects.power1_value, buf);
		prev_power1 = (int)data->display_1.power;
	}

	/* Power2 value */
	if (prev_power2 != (int)data->display_2.power) {
		snprintf(buf, sizeof(buf), "%d", (int)data->display_2.power);
		lv_label_set_text(objects.power2_value, buf);
		prev_power2 = (int)data->display_2.power;
	}

	/* Power3 value */
	if (prev_power3 != (int)data->display_3.power) {
		snprintf(buf, sizeof(buf), "%d", (int)data->display_3.power);
		lv_label_set_text(objects.power3_value, buf);
		prev_power3 = (int)data->display_3.power;
	}

	/* =========================================================
	 * Efficiency
	 * ========================================================= */
	if (prev_efficiency != data->display_1.efficiency) {
		float efficiency =
			(data->display_1.efficiency + data->display_2.efficiency +
			 data->display_3.efficiency) /
			3.0f;
		snprintf(buf, sizeof(buf), "%.1f", efficiency);
		lv_label_set_text(objects.effieciency, buf);
		prev_efficiency = efficiency;
	}

	/* =========================================================
	 * Status LED (blink effect) &&  Encoder timeout handling
	 * ========================================================= */

	uint32_t now = lv_tick_get();

	if (can_alive_flag) {
		if ((now - last_led_toggle_time) > led_timeout_ms) {
			if (data->display_1.updated == 1) {
				lv_led_set_color(objects.led1, lv_color_hex(0xff00ff00));
				lv_led_set_brightness(objects.led1, led_state1 ? 255 : 0);

				led_state1 = !led_state1;

				led1_cnt = 0;
			} else {

				led1_cnt++;
				lv_led_set_color(objects.led1, lv_color_hex(0xff00ff00));
				lv_led_set_brightness(objects.led1, led_state1 ? 255 : 0);

				led_state1 = !led_state1;
				if (led1_cnt > 1) {
					lv_led_set_color(objects.led1, lv_color_hex(0xFFFF00));
					lv_led_set_brightness(objects.led1, led_state1 ? 255 : 0);

					// led_state1 = !led_state1;
					led1_cnt = 0;
				}
			}

			if (data->display_2.updated == 1) {
				lv_led_set_color(objects.led2, lv_color_hex(0xff00ff00));
				lv_led_set_brightness(objects.led2, led_state2 ? 255 : 0);

				led_state2 = !led_state2;

				led2_cnt = 0;
			} else {

				led2_cnt++;
				lv_led_set_color(objects.led2, lv_color_hex(0xff00ff00));
				lv_led_set_brightness(objects.led2, led_state2 ? 255 : 0);

				led_state2 = !led_state2;
				if (led2_cnt > 1) {
					lv_led_set_color(objects.led2, lv_color_hex(0xFFFF00));
					lv_led_set_brightness(objects.led2, led_state2 ? 255 : 0);

					// led_state2 = !led_state2;
					led2_cnt = 0;
				}
			}

			if (data->display_3.updated == 1) {

				lv_led_set_color(objects.led3, lv_color_hex(0xff00ff00));
				lv_led_set_brightness(objects.led3, led_state3 ? 255 : 0);

				led_state3 = !led_state3;

				led3_cnt = 0;
			} else {
				led3_cnt++;

				if (led3_cnt > 1) {
					lv_led_set_color(objects.led3, lv_color_hex(0xFFFF00));
					lv_led_set_brightness(objects.led3, led_state3 ? 255 : 0);

					led_state3 = !led_state3;
					led3_cnt = 0;
				} else {
					lv_led_set_color(objects.led3, lv_color_hex(0xff00ff00));
					lv_led_set_brightness(objects.led3, led_state3 ? 255 : 0);

					led_state3 = !led_state3;
				}
			}

			last_led_toggle_time = now;
		}
	}

	if (button_flag == BUTTON_NOT_PRESSED &&
		activity_encoder == last_flag_value) {

		if ((now - last_encoder_activity_time) > encoder_timeout_ms) {

			/* Deselect voltage label */
			lv_obj_set_style_text_color(objects.voltage_label,
										lv_color_hex(0xFFFFFF), LV_PART_MAIN);
			lv_obj_set_style_bg_opa(objects.voltage_label, LV_OPA_TRANSP,
									LV_PART_MAIN);
			lv_obj_set_style_pad_all(objects.voltage_label, 0, LV_PART_MAIN);

			/* Deselect current label */
			lv_obj_set_style_text_color(objects.voltage_label_1,
										lv_color_hex(0xFFFFFF), LV_PART_MAIN);
			lv_obj_set_style_bg_opa(objects.voltage_label_1, LV_OPA_TRANSP,
									LV_PART_MAIN);
			lv_obj_set_style_pad_all(objects.voltage_label_1, 0, LV_PART_MAIN);

			last_encoder_activity_time = now;
		}
	} else {
		last_encoder_activity_time = now;
		last_flag_value = activity_encoder;
	}
}

void change_voltage_value(float voltage) {

	snprintf(value_str, sizeof(value_str), "%.1f", voltage); // npr. "48.2 V"

	lv_label_set_text(objects.vol_change, value_str);

	int16_t arc_val =
		(int16_t)(voltage * 10.0f); // Sačuvaj tačnost pre kastovanja

	lv_arc_set_value(objects.arc3, arc_val);
}

void change_current_limit_value(float cur_limit) {

	snprintf(value_str, sizeof(value_str), "%.1f", cur_limit);

	lv_label_set_text(objects.curr_limit_change, value_str);

	int16_t arc_val =
		(int16_t)(cur_limit * 10.0f); // Sačuvaj tačnost pre kastovanja

	lv_arc_set_value(objects.arc4, arc_val);
}

lv_color_t get_scaled_color(int value, int min, int max) {
	if (value < min)
		value = min;
	if (value > max)
		value = max;

	// Normalizacija u opsegu 0.0 do 1.0
	float ratio = (float)(value - min) / (float)(max - min);

	// Linearna interpolacija između zelene i crvene
	uint8_t red = (uint8_t)(ratio * 255);
	uint8_t green = (uint8_t)((1.0f - ratio) * 255);
	uint8_t blue = 0;

	return lv_color_make(red, green, blue);
}
void update_voltage_current_change(button_pressed_e flag_change,
								   send_type_e flag_label) {
	if (flag_change == BUTTON_PRESSED) {
		if (flag_label == SEND_VOLTAGE) {

			/* Selected voltage - bright green */
			lv_obj_set_style_text_color(objects.vol_change,
										lv_color_hex(0x00FF00),
										LV_PART_MAIN | LV_STATE_DEFAULT);

			/* Selected label - bright green text with darker green background
			 */
			lv_obj_set_style_text_color(objects.voltage_label,
										lv_color_hex(0x00FF00),
										LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_bg_color(
				objects.voltage_label,
				lv_color_hex(0x1A5C1A), // Tamno zelena pozadina
				LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_bg_opa(objects.voltage_label, 255, // Puna opacity
									LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_pad_all(objects.voltage_label, 4, // Manji padding
									 LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_border_color(
				objects.voltage_label,
				lv_color_hex(0x00FF00), // Zeleni border
				LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_border_width(objects.voltage_label, 2,
										  LV_PART_MAIN | LV_STATE_DEFAULT);

			/* Deselect current - svetlija siva */
			lv_obj_set_style_text_color(objects.curr_limit_change,
										lv_color_hex(0xB0B0B0), // Svetlija siva
										LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_text_color(objects.curr_limit_label,
										lv_color_hex(0xB0B0B0), // Svetlija siva
										LV_PART_MAIN | LV_STATE_DEFAULT);

		} else {

			/* Selected current - bright cyan/blue */
			lv_obj_set_style_text_color(
				objects.curr_limit_change,
				lv_color_hex(0x00FFFF), // Cyan umesto zelene
				LV_PART_MAIN | LV_STATE_DEFAULT);

			/* Selected label - cyan text with dark blue background */
			lv_obj_set_style_text_color(objects.curr_limit_label,
										lv_color_hex(0x00FFFF),
										LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_bg_color(
				objects.curr_limit_label,
				lv_color_hex(0x1A4D5C), // Tamno plava pozadina
				LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_bg_opa(objects.curr_limit_label, 255,
									LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_pad_all(objects.curr_limit_label,
									 4, // Manji padding
									 LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_border_color(objects.curr_limit_label,
										  lv_color_hex(0x00FFFF), // Cyan border
										  LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_border_width(objects.curr_limit_label, 2,
										  LV_PART_MAIN | LV_STATE_DEFAULT);

			/* Deselect voltage - svetlija siva */
			lv_obj_set_style_text_color(objects.vol_change,
										lv_color_hex(0xB0B0B0), // Svetlija siva
										LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_text_color(objects.voltage_label,
										lv_color_hex(0xB0B0B0), // Svetlija siva
										LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_bg_opa(objects.voltage_label,
									0, // Isključi pozadinu
									LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_border_width(objects.voltage_label,
										  0, // Isključi border
										  LV_PART_MAIN | LV_STATE_DEFAULT);
		}
	} else {

		/* Deselected - normal white without background */
		lv_obj_set_style_text_color(objects.vol_change, lv_color_hex(0xFFFFFF),
									LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_color(objects.voltage_label,
									lv_color_hex(0xFFFFFF),
									LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_opa(objects.voltage_label, 0,
								LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_border_width(objects.voltage_label, 0,
									  LV_PART_MAIN | LV_STATE_DEFAULT);

		lv_obj_set_style_text_color(objects.curr_limit_change,
									lv_color_hex(0xFFFFFF),
									LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_color(objects.curr_limit_label,
									lv_color_hex(0xFFFFFF),
									LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_opa(objects.curr_limit_label, 0,
								LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_border_width(objects.curr_limit_label, 0,
									  LV_PART_MAIN | LV_STATE_DEFAULT);
	}
}

void update_voltage_current_labels(send_type_e flag) {
	if (flag == SEND_VOLTAGE) {
		/* Voltage label: selected gray */
		lv_obj_set_style_text_color(objects.voltage_label,
									lv_color_hex(0xFFFFFFFF),
									LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_color(objects.voltage_label, lv_color_hex(0x444444),
								  LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_opa(objects.voltage_label, LV_OPA_COVER,
								LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_pad_all(objects.voltage_label, 6,
								 LV_PART_MAIN | LV_STATE_DEFAULT);

		lv_obj_set_style_text_color(objects.voltage_label_1,
									lv_color_hex(0xFFFFFFFF),
									LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_color(objects.voltage_label_1,
								  lv_color_hex(0x000000),
								  LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_opa(objects.voltage_label_1, LV_OPA_TRANSP,
								LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_pad_all(objects.voltage_label_1, 0,
								 LV_PART_MAIN | LV_STATE_DEFAULT);
	} else if (flag == SEND_CURRENT_LIMIT) {
		/* Voltage label: deselected gray */
		lv_obj_set_style_text_color(objects.voltage_label,
									lv_color_hex(0xFFFFFF),
									LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_color(objects.voltage_label, lv_color_hex(0x000000),
								  LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_opa(objects.voltage_label, LV_OPA_TRANSP,
								LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_pad_all(objects.voltage_label, 0,
								 LV_PART_MAIN | LV_STATE_DEFAULT);

		/* Current limit label: selected gray */
		lv_obj_set_style_text_color(objects.voltage_label_1,
									lv_color_hex(0xFFFFFFFF),
									LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_color(objects.voltage_label_1,
								  lv_color_hex(0x444444),
								  LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_opa(objects.voltage_label_1, LV_OPA_COVER,
								LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_pad_all(objects.voltage_label_1, 6,
								 LV_PART_MAIN | LV_STATE_DEFAULT);
	}
}

// void set_arc_value_and_color(lv_obj_t *arc, float value, int min, int max) {
// 	int16_t scaled_value = (int16_t)(value * 10.0f);
// 	lv_arc_set_value(arc, scaled_value);

// 	/* Limits */
// 	if (scaled_value < min)
// 		scaled_value = min;
// 	if (scaled_value > max)
// 		scaled_value = max;

// 	/* Calculate factor */
// 	float ratio = (float)(scaled_value - min) / (float)(max - min);

// 	/* Green -> Yellow -> Red */
// 	uint8_t r, g;
// 	if (ratio < 0.5f) {
// 		r = (uint8_t)(ratio * 2 * 255);
// 		g = 255;
// 	} else {
// 		r = 255;
// 		g = (uint8_t)((1.0f - (ratio - 0.5f) * 2) * 255);
// 	}
// 	lv_color_t arc_color = lv_color_make(r, g, 0);

// 	/* Set color of arc */
// 	lv_obj_set_style_arc_color(arc, arc_color,
// 							   LV_PART_INDICATOR | LV_STATE_DEFAULT);

// 	/* Set color of knob */
// 	lv_obj_set_style_arc_color(arc, lv_color_white(),
// 							   LV_PART_KNOB | LV_STATE_DEFAULT);
// 	lv_obj_set_style_arc_width(arc, 3, LV_PART_KNOB | LV_STATE_DEFAULT);
// }

bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
							 esp_lcd_panel_io_event_data_t *edata,
							 void *user_ctx) {
	lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;

	lv_disp_flush_ready(disp_driver);

	return false;
}

void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
				   lv_color_t *color_map) {
	esp_lcd_panel_handle_t panel_handle =
		(esp_lcd_panel_handle_t)drv->user_data;

	int offsetx1 = area->x1;
	int offsetx2 = area->x2;
	int offsety1 = area->y1;
	int offsety2 = area->y2;

	/* copy a buffer's content to a specific area of the display */
	esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1,
							  offsety2 + 1, color_map);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver
 * parameters are updated. */
void lvgl_port_update_callback(lv_disp_drv_t *drv) {
	esp_lcd_panel_handle_t panel_handle =
		(esp_lcd_panel_handle_t)drv->user_data;

	switch (drv->rotated) {
	case LV_DISP_ROT_NONE:
		/* Rotate LCD display */
		esp_lcd_panel_swap_xy(panel_handle, false);
		esp_lcd_panel_mirror(panel_handle, true, false);

		break;
	case LV_DISP_ROT_90:
		/* Rotate LCD display */
		esp_lcd_panel_swap_xy(panel_handle, true);
		esp_lcd_panel_mirror(panel_handle, true, true);

		break;
	case LV_DISP_ROT_180:
		/* Rotate LCD display */
		esp_lcd_panel_swap_xy(panel_handle, false);
		esp_lcd_panel_mirror(panel_handle, false, true);

		break;
	case LV_DISP_ROT_270:
		/* Rotate LCD display */
		esp_lcd_panel_swap_xy(panel_handle, true);
		esp_lcd_panel_mirror(panel_handle, false, false);

		break;
	default:
		break;
	}
}

void increase_lvgl_tick(void *arg) {
	/* Tell LVGL how many milliseconds has elapsed */
	lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

bool lvgl_lock(int timeout_ms) {
	/* Convert timeout in milliseconds to FreeRTOS ticks */
	/* If `timeout_ms` is set to -1, the program will block until the condition
	 * is met */

	const TickType_t timeout_ticks =
		(timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

	return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_unlock(void) { xSemaphoreGiveRecursive(lvgl_mux); }
