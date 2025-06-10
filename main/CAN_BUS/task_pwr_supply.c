/**
 * @file    task_pwr_supply.c
 * @brief   Task for sending CAN requests and parsing responses from a power supply.
 *
 * This file contains the implementation of a FreeRTOS task responsible for 
 * communicating with a power supply unit over the CAN bus. The task sends 
 * a request message via CAN and then waits for a response on a FreeRTOS queue.
 * 
 * Once a response is received, the task parses the incoming CAN message 
 * and extracts the relevant data for further processing or monitoring.
 *
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

/* User variables */
volatile system_stats_t g_stats_1;
volatile system_stats_t g_stats_2;
esp_err_t esp_err_pwr;

/* FreeRTOS objects */
extern QueueHandle_t lvgl_voltage_queue;
extern QueueHandle_t lvgl_current_queue;
extern QueueHandle_t lvgl_update_queue;
extern QueueHandle_t xQueueCan;

lvgl_data_t display_1 = {0};
lvgl_data_t display_2 = {0};

lvgl_bonded_data_t screen = {0};
/*******************************************************************************
 * Main function
 ******************************************************************************/
 
void task_pwr_supply(void *arg)
{
    twai_message_t rx_msg;
    TickType_t timeout = pdMS_TO_TICKS(1000);
    twai_message_t request_msg;
	TickType_t gui_update_period = pdMS_TO_TICKS(300);
	TickType_t last_update = xTaskGetTickCount();
    can_init_msg(&request_msg, CAN_REQUEST_PARAMETERS_ID, 0, 0, CAN_REQUEST_FLAG);

    for (;;) {
		
		/* Send request message  */
		//vTaskDelay(pdMS_TO_TICKS(1.5));
        
        twai_transmit(&request_msg, pdMS_TO_TICKS(1000));

	        if (xQueueReceive(xQueueCan, &rx_msg, timeout) == pdPASS) {
	    
                if (rx_msg.identifier == CAN_REQUEST_VALUES_ID || rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID) {
	
					/* PARSE THE DATA */
	                parse_statistics(rx_msg.data, &g_stats_1);
	                
	                if (rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID &&  rx_msg.data[1] == CAN_ANSWER_PARAMETERS_UNKNOWN_ID) {
						
						    display_1.voltage    = g_stats_1.output_voltage;
						    display_1.current    = g_stats_1.output_current_1;
						    display_1.limit      = g_stats_1.output_current_max;
						    display_1.temp       = g_stats_1.output_temp;
						    display_1.power      = g_stats_1.output_voltage * g_stats_1.output_current_1;
						    display_1.efficiency = g_stats_1.efficiency;
                            display_1.display_id = CAN_GADGET_1;
                            display_1.updated    = 1;
						
	                }
	            } else if(rx_msg.identifier == CAN_REQUEST_VALUES_ID_2 || rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID_2) {
	
					/* PARSE THE DATA */
	                parse_statistics(rx_msg.data, &g_stats_2);
	                
	                if (rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID_2 &&  rx_msg.data[1] == CAN_ANSWER_PARAMETERS_UNKNOWN_ID) {
						    
						    display_2.voltage    = g_stats_2.output_voltage;
						    display_2.current    = g_stats_2.output_current_1;
						    display_2.limit      = g_stats_2.output_current_max;
						    display_2.temp       = g_stats_2.output_temp;
						    display_2.power      = g_stats_2.output_voltage * g_stats_2.output_current_1;
						    display_2.efficiency = g_stats_2.efficiency;
                            display_2.display_id = CAN_GADGET_2;
                            display_2.updated    = 1;
						
	                }
	            }
	            
	            TickType_t now = xTaskGetTickCount();
		        
		        if ((display_1.updated == 1 && display_2.updated == 1) || (now - last_update) > gui_update_period) {
		
		            screen.display_1 = display_1;
		            screen.display_2 = display_2;
		            xQueueSend(lvgl_update_queue, &screen, 0);
		
		            display_1.updated = 0;
				    display_2.updated = 0;
		            last_update = now;
		        }
	            
            }
    } 
}


/* ============================ User Functions ============================= */

void can_init_msg(twai_message_t *msg, uint32_t id, uint32_t command, uint32_t value, uint8_t flag)
{
    msg->identifier = id;
    msg->extd = 1;
    msg->rtr = 0;
    msg->data_length_code = 8;

    if (flag == CAN_COMMAND_FLAG) {
        msg->data[0] = (command >> 24) & 0xFF;
        msg->data[1] = (command >> 16) & 0xFF;
        msg->data[2] = (command >> 8) & 0xFF;
        msg->data[3] = command & 0xFF;
        msg->data[4] = (value >> 24) & 0xFF;
        msg->data[5] = (value >> 16) & 0xFF;
        msg->data[6] = (value >> 8) & 0xFF;
        msg->data[7] = value & 0xFF;
    } else {
        memset(msg->data, 0, 8);
    }
}

void parse_statistics(uint8_t *data, volatile system_stats_t *g_stats)
{
    uint8_t command = data[1];
    uint32_t value = ((uint32_t)data[4] << 24) |
                     ((uint32_t)data[5] << 16) |
                     ((uint32_t)data[6] << 8)  |
                     ((uint32_t)data[7]);


    switch (command) {
        case CAN_ANSWER_PARAMETERS_INPUT_POWER_ID:
        	g_stats->input_power        = value / 1024.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_INPUT_FREQ_ID:
        	g_stats->input_freq         = value / 1024.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_INPUT_CURRENT_ID:
        	g_stats->input_current      = value / 1024.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_OUTPUT_POWER_ID:
        	g_stats->output_power       = value / 1024.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_EFFICIENCY_ID:
        	g_stats->efficiency	       = (value/ 1024.0f) * 100.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_OUTPUT_VOLTAGE_ID:
        	g_stats->output_voltage     = value / 1024.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_OUTPUT_CURR_MAX_ID:
        	g_stats->output_current_max = value / 30.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_INPUT_VOLTAGE_ID:
        	g_stats->input_voltage      = value / 1024.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_OUTPUT_TEMP_ID:
        	g_stats->output_temp        = value / 1024.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_OUTPUT_CURR_1_ID:
        	g_stats->output_current_1   = value / 1024.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_OUTPUT_CURR_2_ID:
        	g_stats->output_current_2   = value / 1024.0f;
        	break;

        case CAN_ANSWER_PARAMETERS_UNKNOWN_ID:
        	g_stats->unknown = value;
        	break;
        	
        default:
        /* Unknown command - optionally log or handle error */
         	break;
    }
}
