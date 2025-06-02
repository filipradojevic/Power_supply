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

/* Includes of FreeRTOS */
#include <stdio.h>
#include <stdbool.h>
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

/*******************************************************************************
 * Defines
 ******************************************************************************/

/*******************************************************************************
 * User variables
 ******************************************************************************/

/* User variables */
extern esp_err_t esp_err;

/* LVGL variables */
extern lv_disp_t *global_disp; /* Global Current Active Display */
extern lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
extern lv_disp_drv_t disp_drv;      // contains callback functions
extern esp_lcd_panel_handle_t panel_handle; /* Display that indicate LVGL which one we use */
extern lv_disp_t *disp;
extern ui_objects_t objects;

/* FreeRTOS objects */
extern QueueHandle_t lvgl_voltage_queue;
extern QueueHandle_t lvgl_current_queue;
extern QueueHandle_t lvgl_update_queue;
extern QueueSetHandle_t xQueueSetLvgl;

extern SemaphoreHandle_t lvgl_mux;

/*******************************************************************************
 * Main function
 ******************************************************************************/

void task_lvgl_ili9341(void *arg)
{
    QueueSetMemberHandle_t activated_queue = NULL;
    lvgl_data_t lvgl_received;
    uint32_t received_value;
    char value_str[16];
    bool flag = 1;
    static TickType_t last_lvgl_tick = 0;


    while (1) {
        activated_queue = xQueueSelectFromSet(xQueueSetLvgl, portMAX_DELAY);

        if (activated_queue == lvgl_voltage_queue) {
            
            if (xQueueReceive(lvgl_voltage_queue, &received_value, 0) == pdTRUE) {

			    float voltage = ((float)received_value / 1000.0f) - 1.5f;
			
			    if (lvgl_lock(-1)) {
			        snprintf(value_str, sizeof(value_str), "%.1f", voltage);  // npr. "48.2 V"
			        
			        lv_label_set_text(objects.vol_change, value_str);
			        
			        // Za arc - konverzija float -> int unutar opsega 410 - 590
					int arc_value = (int)(voltage * 10.0f);  // 465
        			
        			lv_arc_set_value(objects.obj0, arc_value);
        			
        			lv_timer_handler();
			        
			        lvgl_unlock();
			    }
			}

        } else if (activated_queue == lvgl_current_queue) {
            
            if (xQueueReceive(lvgl_current_queue, &received_value, 0) == pdTRUE) {
    
			    float current = (float)received_value / 100.0f;
			
			    if (lvgl_lock(-1)) {
			        snprintf(value_str, sizeof(value_str), "%.1f", current);
			        
			        lv_label_set_text(objects.curr_limit_change, value_str);
			        
			        lv_timer_handler();
			        lvgl_unlock();
			    }
			}
        } else if (activated_queue == lvgl_update_queue) {
			  if (xQueueReceive(lvgl_update_queue, &lvgl_received, 0) == pdTRUE) {
			  	update_lvgl_display(&lvgl_received, &flag);
    		}
		} 
    }
}

void update_lvgl_display(const lvgl_data_t *data, bool *flag) {
    char value_str[32];
    static bool led_state = false;  // Pamti trenutno stanje LED

    if (lvgl_lock(-1)) {
        if (*flag == 1) {
            // voltage sa jednom decimalom
            snprintf(value_str, sizeof(value_str), "%.1f", data->voltage);
            lv_label_set_text(objects.vol_change, value_str);
            lv_arc_set_value(objects.obj0, (int)(data->voltage * 10));
            *flag = 0;
        }

        // current
        snprintf(value_str, sizeof(value_str), "%.1f", data->current);
        lv_label_set_text(objects.curr_change, value_str);
        lv_arc_set_value(objects.obj1, (int)(data->current * 10));

        // limit
        snprintf(value_str, sizeof(value_str), "%.1f", data->limit);
        lv_label_set_text(objects.curr_limit_change, value_str);

        // temperature
        snprintf(value_str, sizeof(value_str), "%d", (int)data->temp);
        lv_label_set_text(objects.temp_value, value_str);
        lv_slider_set_value(objects.slider_temp, (int)(data->temp), LV_ANIM_OFF);

        // power
        snprintf(value_str, sizeof(value_str), "%d", (int)data->power);
        lv_label_set_text(objects.power_value, value_str);
        lv_slider_set_value(objects.slider_power, (int)(data->power), LV_ANIM_OFF);

        // efficiency
        snprintf(value_str, sizeof(value_str), "%.1f", data->efficiency);
        lv_label_set_text(objects.effieciency, value_str);

        // --- LED TOGGLE ---
        if (led_state) {
            lv_led_set_brightness(objects.led, 180); // upaljena
        } else {
            lv_led_set_brightness(objects.led, 0);   // ugašena
        }
        led_state = !led_state;  // obrni stanje

        lv_timer_handler();
        lvgl_unlock();
    }
}

bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
void lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);

        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);

        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);

        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);

        break;
    }
}



void increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

bool lvgl_lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}
