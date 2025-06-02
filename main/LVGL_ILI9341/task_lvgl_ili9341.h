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


/******************************************************************************* 
 * Structures
 ******************************************************************************/



/******************************************************************************* 
 * Prototypes
 ******************************************************************************/
bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
void lvgl_port_update_callback(lv_disp_drv_t *drv);
void increase_lvgl_tick(void *arg);
bool lvgl_lock(int timeout_ms);
void lvgl_unlock(void);
void update_lvgl_display(const lvgl_data_t *data, bool *flag);



#endif /* MAIN_TASK_LVGL_PORT_H_ */
