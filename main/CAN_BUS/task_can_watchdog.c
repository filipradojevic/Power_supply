/**
 * @file    task_can_watchdog.c
 * @brief
 * @version 1.0.0
 * @date    13.05.2025
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
volatile bool can_alive_flag = false;

/* FreeRTOS objects */
extern SemaphoreHandle_t watchdog_semaphore;

/*******************************************************************************
 * Main function
 ******************************************************************************/

void task_can_watchdog(void *arg) {

	const TickType_t xDelay = pdMS_TO_TICKS(250);

	while (1) {
		vTaskDelay(xDelay);

		if (!can_alive_flag) {
			xSemaphoreGive(watchdog_semaphore);
		}

		/* Reset the flag */
		can_alive_flag = false;
	}
}