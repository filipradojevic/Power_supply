/**
 * @file    task_encoder.c
 * @brief   Task to handle rotary encoder input signaled via ISR semaphore.
 *          Performs debouncing, updates encoder position, calculates voltage,
 *          and sends updated values over the CAN interface.
 * 
 * This module receives a semaphore given by the encoder ISR,
 * applies debounce logic in the task context,
 * accumulates encoder steps to update voltage,
 * and transmits the new voltage value via CAN bus.
 * 
 * @version 1.0.0
 * @date    20.05.2025
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
static send_type_e current_send_flag = SEND_VOLTAGE;
volatile int encoder_voltage = 0;
volatile int encoder_current = 0;
uint32_t curr_voltage_value = CAN_DEFAULT_VOLTAGE_VALUE;
uint32_t curr_current_value = CAN_DEFAULT_CURRENT_LIMIT;

static int64_t last_step_time_encoder_pulse_us = 0;
static int64_t last_step_time_encoder_switch_us = 0;
static const int64_t STEP_DEBOUNCE_INTERVAL_US = 17500; // 50ms

/* FreeRTOS objects */
extern QueueHandle_t lvgl_voltage_queue;
extern QueueHandle_t lvgl_current_queue;
extern QueueHandle_t lvgl_bolding_update;
extern SemaphoreHandle_t encoder_semaphore;
extern SemaphoreHandle_t switch_semaphore;
extern SemaphoreHandle_t command_semaphore;
extern QueueSetHandle_t xQueueSetEncoder;

extern ui_objects_t objects;

/*******************************************************************************
 * Main function
 ******************************************************************************/

void task_encoder(void *arg) {
    uint16_t i;
    uint8_t currentButtonState = 1;
    QueueSetMemberHandle_t activated_handle = NULL;

    while (1) {
        activated_handle = xQueueSelectFromSet(xQueueSetEncoder, portMAX_DELAY);

        /* Send command triggered by encoder rotation */
        if (activated_handle == encoder_semaphore) {
            xSemaphoreTake(encoder_semaphore, 0);

            /* Software debounce */
            for (i = 0; i < 1000; i++);

            int a = gpio_get_level(ENCODER_CLK_PIN);
            int b = gpio_get_level(ENCODER_DT_PIN);

            int step = 0;

            /* Check if still active */
            if (a == 1) {
                step = (b != a) ? +1 : -1;
            }

            if (step != 0) {
				
				/* This checks if the pulse of encoder is triggered too fast bcs lvgl is not thread safe
               It should not overload the LVGL update of the screen                        */
				int64_t now_pulse = esp_timer_get_time();
			    
			    if ((now_pulse - last_step_time_encoder_pulse_us) < STEP_DEBOUNCE_INTERVAL_US) {
			        continue;
			    }
			    last_step_time_encoder_pulse_us = now_pulse;
			    
                /* Check the type of command */
                if (current_send_flag == SEND_VOLTAGE) {
					encoder_voltage += step;
                    uint32_t new_voltage_value = pack_current_voltage();

					twai_send_voltage(new_voltage_value, CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE);
					
					xQueueSend(lvgl_voltage_queue, &new_voltage_value, 0);
                } else {
					encoder_current += step;
                    uint32_t new_current_limit = pack_current_limit();
                    
                    twai_send_current(new_current_limit, CAN_SETTING_VALUES_ON_LINE_CURRENT_LIMIT);
                    
                    xQueueSend(lvgl_current_queue, &new_current_limit, 0);
                }
            }
        
        /* Changes the flag for command by state of a switch */
        }else if (activated_handle == switch_semaphore) {
            xSemaphoreTake(switch_semaphore, 0);
            
            /* This checks if the button of encoder is triggered too fast bcs lvgl is not thread safe
               It should not overload the LVGL update of the screen                        */
            int64_t now_switch = esp_timer_get_time();
			    
			    if ((now_switch - last_step_time_encoder_switch_us) < STEP_DEBOUNCE_INTERVAL_US) {
			        continue;
			    }
			    
		    last_step_time_encoder_switch_us = now_switch;
			
			/* Software debounce */
            for (i = 0; i < 500; i++);

            /* Read button state again to verify rising edge of button signal */
            currentButtonState = gpio_get_level(ENCODER_SW_PIN);

            if (currentButtonState == 0) {
                current_send_flag = (current_send_flag == SEND_VOLTAGE) ? SEND_CURRENT_LIMIT : SEND_VOLTAGE;
            }
            
            xQueueSend(lvgl_bolding_update, &current_send_flag, 0);
            
        /* Triggered by Timer interrupt: send command to avoid reset of a device */
        }else if(activated_handle == command_semaphore){
            xSemaphoreTake(command_semaphore, 0);

            uint32_t new_voltage_value = pack_current_voltage();

            twai_send_voltage(new_voltage_value,CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE);

        }else {
            Error_Handler();
        }
    }
}

/* Sending command for a voltage ON/OFF Line */
void twai_send_voltage(uint32_t new_voltage_value, uint32_t command){
	twai_message_t msg;
    
    can_init_msg(&msg,
                 CAN_SETTING_VALUES_ID,
                 command,
                 new_voltage_value,
                 CAN_COMMAND_FLAG);

    twai_transmit(&msg, pdMS_TO_TICKS(100));
}

/* Sending command for a Current LIMIT ON/OFF Line */
void twai_send_current(uint32_t new_current_limit, uint32_t command){
	twai_message_t msg;

    can_init_msg(&msg,
                    CAN_SETTING_VALUES_ID,
                    command,
                    new_current_limit,
                    CAN_COMMAND_FLAG);

    twai_transmit(&msg, pdMS_TO_TICKS(100));
}

/* Packing structure for voltage */
uint32_t pack_current_limit(){
    /* Formula for new current limit to be send */
	int32_t new_current_limit = (int32_t)CAN_DEFAULT_CURRENT_LIMIT + encoder_current * (int32_t)CURRENT_STEP_HEX;

    /* Check limits */
    if (new_current_limit < (int32_t)MIN_CURRENT_LIMIT_VALUE)
        new_current_limit = (int32_t)MIN_CURRENT_LIMIT_VALUE;
		encoder_current = (new_current_limit - (int32_t)CAN_DEFAULT_CURRENT_LIMIT) / CURRENT_STEP_HEX;
    
    if (new_current_limit > (int32_t)MAX_CURRENT_LIMIT_VALUE)
        new_current_limit = (int32_t)MAX_CURRENT_LIMIT_VALUE;
		encoder_current = (new_current_limit - (int32_t)CAN_DEFAULT_CURRENT_LIMIT) / CURRENT_STEP_HEX;

	curr_current_value = (uint32_t)new_current_limit;
    return curr_current_value;
}

/* Packing structure for current limit */
uint32_t pack_current_voltage(){
	/* Formula for new voltage to be send */
    int32_t new_voltage = (int32_t)CAN_DEFAULT_VOLTAGE_VALUE + encoder_voltage * (int32_t)VOLTAGE_STEP_HEX;
	
	/* Check limits */
    if (new_voltage < (int32_t)MIN_VOLTAGE_VALUE)
        new_voltage = (int32_t)MIN_VOLTAGE_VALUE;								
		encoder_voltage = (new_voltage - (int32_t)CAN_DEFAULT_VOLTAGE_VALUE) / VOLTAGE_STEP_HEX;
    
    if (new_voltage > (int32_t)MAX_VOLTAGE_VALUE)
        new_voltage = (int32_t)MAX_VOLTAGE_VALUE;
		encoder_voltage = (new_voltage - (int32_t)CAN_DEFAULT_VOLTAGE_VALUE) / VOLTAGE_STEP_HEX;

    curr_voltage_value = (uint32_t)new_voltage;

    return curr_voltage_value;
}





