/**
 * @file    task_can_receive.c
 * @brief   Task for receiving CAN messages via TWAI and forwarding them to a queue.
 *
 * This file contains the implementation of a FreeRTOS task that handles
 * incoming CAN messages using the TWAI (Two-Wire Automotive Interface) driver.
 * The task blocks on TWAI receive alerts, waiting for new messages to arrive.
 * Once a message is received, it is forwarded to an appropriate FreeRTOS queue
 * for further processing by other tasks.
 * @version 1.0.0
 * @date    13.05.2025
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

/*******************************************************************************
 * Defines
 ******************************************************************************/
 
 /*******************************************************************************
 * User variables
 ******************************************************************************/
extern volatile bool can_alive_flag;

/* FreeRTOS objects */
extern QueueHandle_t xQueueCan;

/*******************************************************************************
 * Main function
 ******************************************************************************/
 
void task_can_receive(void *arg)
{
    twai_message_t rx_msg;
    uint32_t alerts;
    esp_err_t ret;

    while (1) {
        /* Wait for alerts to raise */
        ret = twai_read_alerts(&alerts, pdMS_TO_TICKS(1000));
        if (ret == ESP_OK) {
            /* Check if RX triggered CAN */
            if (alerts & TWAI_ALERT_RX_DATA) {
                if (twai_receive(&rx_msg, portMAX_DELAY) == ESP_OK) {
                    
                    /* Provera CAN ID */
                    if (rx_msg.identifier == CAN_REQUEST_VALUES_ID ||
                        rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID ||
                        rx_msg.identifier == CAN_REQUEST_VALUES_ID_2 ||
                        rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID_2) {

                        /* Update flag for watchdog task */
                        can_alive_flag = true;

                        /* Pošalji poruku u CAN red */
                        xQueueSend(xQueueCan, &rx_msg, 0);
                    }

                    // Ako želiš da loguješ ili obradiš ostale poruke, možeš ovde dodati else
                    // else { printf("Nepoznata CAN poruka: %x\n", rx_msg.identifier); }
                }
            }

            // TODO: Process other flags, consult with Slavoljub...
        }
    }
}

