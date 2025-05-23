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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* Drivers header files */
#include "driver/twai.h"

/* Users header files */
#include "task_can_receive.h"
#include "hal/twai_types.h"
#include "main.h"
#include "esp_system.h"

/*******************************************************************************
 * Defines
 ******************************************************************************/
 
 /*******************************************************************************
 * User variables
 ******************************************************************************/

/* FreeRTOS objects */
extern QueueHandle_t queue_can;

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
                    /* Send data for further processing */
                    xQueueSend(queue_can, &rx_msg, 0);
                }
            }

            // TODO: Process other flags, consult with Slavoljub...
        }
    }
}

