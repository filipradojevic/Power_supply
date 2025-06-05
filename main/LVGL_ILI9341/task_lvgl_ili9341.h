/**
 * @file    task_lvgl_ili9341.h
 * @brief   
 * @version 1.0.0
 * @date    30.05.2025
 * @author  LisumLab
 */


#ifndef MAIN_TASK_LVGL_PORT_H_
#define MAIN_TASK_LVGL_PORT_H_


/******************************************************************************* 
 * Includes 
 ******************************************************************************/
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
#include "main.h"

/* Users header files */
#include "esp_timer.h"
#include "esp_err.h"
#include "rom/ets_sys.h"
#include "task_encoder.h"

/* LCD Display drivers */
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"

/* LVGL Library */
#include "lvgl.h"
#include "lvgl_screens.h"
#include "task_pwr_supply.h"

/******************************************************************************* 
 * Defines 
 ******************************************************************************/

/* Using SPI2 in the example */
#define LCD_HOST  SPI2_HOST

/* LCD PINS */
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_SCLK           19
#define EXAMPLE_PIN_NUM_MOSI           12
#define EXAMPLE_PIN_NUM_MISO           13
#define EXAMPLE_PIN_NUM_LCD_DC         21
#define EXAMPLE_PIN_NUM_LCD_RST        18
#define EXAMPLE_PIN_NUM_LCD_CS         14
#define EXAMPLE_PIN_NUM_BK_LIGHT       5
#define EXAMPLE_PIN_NUM_TOUCH_CS       15

// The pixel number in horizontal and vertical
#define EXAMPLE_LCD_H_RES              240
#define EXAMPLE_LCD_V_RES              320

// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2


/* Range for ARC's*/
#define LVGL_ARC_VOLTAGE_MIN 410
#define LVGL_ARC_VOLTAGE_MAX 590
#define LVGL_ARC_CURRENT_MIN 0
#define LVGL_ARC_CURRENT_MAX 600

/* Range for SLIDER's*/
#define LVGL_SLIDER_POWER_MIN 0
#define LVGL_SLIDER_POWER_MAX 3540
#define LVGL_SLIDER_TEMP_MIN 0
#define LVGL_SLIDER_TEMP_MAX 100


/******************************************************************************* 
 * Structures
 ******************************************************************************/
typedef enum {
    VOLTAGE_UPDATE_ONCE = 0,
    VOLTAGE_UPDATE_IGNORE
} voltage_update_e;

typedef enum {
    CURRENT_UPDATE_ONCE = 0,
    CURRENT_UPDATE_IGNORE
} current_update_e;

/******************************************************************************* 
 * Prototypes
 ******************************************************************************/
bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);

void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);

void lvgl_port_update_callback(lv_disp_drv_t *drv);

void increase_lvgl_tick(void *arg);

bool lvgl_lock(int timeout_ms);

void lvgl_unlock(void);

void update_lvgl_display(const lvgl_data_t *data, button_pressed_e button_flag, 
													send_type_e activity_encoder, button_pressed_e *button_pressed_flag);

void set_arc_value_and_color(lv_obj_t *arc, float value, int min, int max);

void update_voltage_current_labels(send_type_e flag);

void update_voltage_current_change(button_pressed_e flag_change, send_type_e flag_label);

void display_error(bool *led_on);

lv_color_t get_scaled_color(int value, int min, int max);

void change_voltage_value(float voltage);

void change_current_limit_value(float cur_limit);

#endif /* MAIN_TASK_LVGL_PORT_H_ */
