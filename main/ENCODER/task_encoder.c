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
#define DEBOUNCE_DELAY_US 250
 
/*******************************************************************************
 * User Variables
 ******************************************************************************/

/* Encoder and button state */
static send_type_e current_send_flag = SEND_VOLTAGE;          /* Current parameter being adjusted (voltage or current) */
static button_pressed_e button_flag = BUTTON_NOT_PRESSED;     /* State of the encoder button */

volatile int encoder_voltage = 0;                             /* Encoder step count for voltage */
volatile int encoder_current = 0;                             /* Encoder step count for current */

/* Current CAN parameter values */
uint32_t curr_voltage_value = CAN_DEFAULT_VOLTAGE_VALUE;      /* Current voltage value */
uint32_t curr_current_value = CAN_DEFAULT_CURRENT_LIMIT;      /* Current current limit */

/* Debounce timing for encoder signals */
static int64_t last_step_time_encoder_pulse_us = 0;           /* Timestamp of last encoder pulse */
static int64_t lastDebounceTime = 0;                          /* Time from last change */
static const int64_t STEP_DEBOUNCE_INTERVAL_US = 25000;       /* Debounce interval in microseconds (~25ms) */
static int lastButtonReading = 1;                             /* Last read state of the button */

/*******************************************************************************
 * UI Object References
 ******************************************************************************/
extern ui_objects_t objects;                            /* Structure containing all UI objects */

/*******************************************************************************
 * FreeRTOS Objects
 ******************************************************************************/

/* Queues */
extern QueueHandle_t lvgl_voltage_queue;                /* Queue for voltage changes */
extern QueueHandle_t lvgl_current_queue;                /* Queue for current limit changes */
extern QueueHandle_t lvgl_bolding_update;               /* Queue for UI bolding updates */
extern QueueHandle_t lvgl_button_pressed;               /* Queue for button press events */

/* Queue set */
extern QueueSetHandle_t xQueueSetEncoder;               /* Queue set for encoder-related events */

/* Semaphores */
extern SemaphoreHandle_t encoder_semaphore;             /* Semaphore for encoder pulses */
extern SemaphoreHandle_t switch_semaphore;              /* Semaphore for encoder button presses */
extern SemaphoreHandle_t command_semaphore;             /* Semaphore for command execution */

/*******************************************************************************
 * Main function
 ******************************************************************************/

void task_encoder(void *arg) {
    uint16_t i;
    QueueSetMemberHandle_t activated_handle = NULL;

    while (1) {
        /* Wait indefinitely for one of the semaphores in the queue set to become available */
        activated_handle = xQueueSelectFromSet(xQueueSetEncoder, portMAX_DELAY);

        /* Handle encoder rotation event */
        if (activated_handle == encoder_semaphore) {
            /* Take encoder semaphore */
            xSemaphoreTake(encoder_semaphore, 0);

            /* Small delay for signal stabilization */
            for (i = 0; i < 2500; i++);

            /* Read encoder pin states */
            int clk_level = gpio_get_level(ENCODER_CLK_PIN);
            int dt_level  = gpio_get_level(ENCODER_DT_PIN);
            int step      = 0;

            /* Determine rotation direction */
            if (clk_level == 1) step = (dt_level != clk_level) ? +1 : -1;

            if (step != 0) {
				
				/* Software debounce: check time since last step */
                int64_t now_pulse = esp_timer_get_time();

                if ((now_pulse - last_step_time_encoder_pulse_us) < STEP_DEBOUNCE_INTERVAL_US)
                    continue;

                last_step_time_encoder_pulse_us = now_pulse;

                /* Check if button is not pressed */
                if (button_flag == BUTTON_NOT_PRESSED) {
                    
                    /* Select parameter to adjust based on step direction */
                    current_send_flag = (step > 0) ? SEND_CURRENT_LIMIT : SEND_VOLTAGE;
                    
                    /* Notify display update task */
                    xQueueSend(lvgl_bolding_update, &current_send_flag, 0);
                } 
                else {
                    
                    /* Update selected parameter */
                    if (current_send_flag == SEND_VOLTAGE) {
                        encoder_voltage += step;
                    
                        uint32_t new_voltage_value = pack_current_voltage();
                    
                        /* Send updated voltage to both CAN devices */
                        twai_send_voltage(CAN_SETTING_VALUES_ID_1, new_voltage_value, CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE);
            			twai_send_voltage(CAN_SETTING_VALUES_ID_2, new_voltage_value, CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE);
            			
                        /* Notify display task with new voltage */
            			xQueueSend(lvgl_voltage_queue, &new_voltage_value, 0);
                    } 
                    else {
                        encoder_current += step;
                    
                        uint32_t new_current_limit = pack_current_limit();
                    
                        /* Send updated current limit to both CAN devices */
                        twai_send_current(CAN_SETTING_VALUES_ID_1, new_current_limit, CAN_SETTING_VALUES_ON_LINE_CURRENT_LIMIT);
                       	twai_send_current(CAN_SETTING_VALUES_ID_2, new_current_limit, CAN_SETTING_VALUES_ON_LINE_CURRENT_LIMIT);
                    
                        /* Notify display task with new current limit */
                        xQueueSend(lvgl_current_queue, &new_current_limit, 0);
                    }
                }
            }
        }
        /* Handle encoder button press event */ 
        else if (activated_handle == switch_semaphore) {
            /* Take button semaphore */
			xSemaphoreTake(switch_semaphore, 0);
			
			/* Software debounce for button press */
		    int reading = gpio_get_level(ENCODER_SW_PIN);
		    int64_t now = esp_timer_get_time();
		
		    if (reading != lastButtonReading) {
		        lastDebounceTime  = now;
		        lastButtonReading = reading;
		    }
		
		    if ((now - lastDebounceTime) > DEBOUNCE_DELAY_US) {
		        int reading = gpio_get_level(ENCODER_SW_PIN);
		          
                /* On button press (active low), toggle button flag */
	            if (reading == 0) {
	                button_flag = (button_flag == BUTTON_PRESSED) ? BUTTON_NOT_PRESSED : BUTTON_PRESSED;
	                
                    /* Notify display task of button press */
	                xQueueSend(lvgl_button_pressed, &button_flag, 0);
	                
                    /* Reset lastButtonReading to avoid multiple toggles */
	                lastButtonReading = 1;
	            }
		    }
        }
        /* Handle periodic command semaphore to prevent device reset */
        else if (activated_handle == command_semaphore) {
            /* Take command semaphore */
            xSemaphoreTake(command_semaphore, 0);

            /* Pack current voltage value */
            uint32_t new_voltage_value = pack_current_voltage();
            
            /* Send current voltage periodically to CAN devices */
            twai_send_voltage(CAN_SETTING_VALUES_ID_1, new_voltage_value, CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE);
            twai_send_voltage(CAN_SETTING_VALUES_ID_2, new_voltage_value, CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE);
        }
    }
}


/* Sending command for a voltage ON/OFF Line */
void twai_send_voltage(uint32_t can_id, uint32_t new_voltage_value, uint32_t command){
	twai_message_t can_msg;

    /* Initialize CAN message with given parameters */
    can_init_msg(&can_msg, can_id, command, new_voltage_value, CAN_COMMAND_FLAG);

    /* Transmit the CAN message with a timeout of 100 ms */
    twai_transmit(&can_msg, pdMS_TO_TICKS(100));
}

/* Sending command for a Current LIMIT ON/OFF Line */
void twai_send_current(uint32_t can_id, uint32_t new_current_limit, uint32_t command){
	twai_message_t can_msg;
    
    /* Initialize CAN message with given parameters */
    can_init_msg(&can_msg, can_id, command, new_current_limit, CAN_COMMAND_FLAG);

    /* Transmit the CAN message with a timeout of 100 ms */
    twai_transmit(&can_msg, pdMS_TO_TICKS(100));
}

/* Packing structure for current limit*/
uint32_t pack_current_limit(){
    /* Calculate new current limit based on encoder steps */
	int32_t new_current_limit = (int32_t)CAN_DEFAULT_CURRENT_LIMIT + encoder_current * (int32_t)CURRENT_STEP_HEX;

    /* Clamp new current limit within defined bounds */
    if (new_current_limit < (int32_t)MIN_CURRENT_LIMIT_VALUE){
        new_current_limit = (int32_t)MIN_CURRENT_LIMIT_VALUE;
		encoder_current = (new_current_limit - (int32_t)CAN_DEFAULT_CURRENT_LIMIT) / CURRENT_STEP_HEX;
    }
    if (new_current_limit > (int32_t)MAX_CURRENT_LIMIT_VALUE){
        new_current_limit = (int32_t)MAX_CURRENT_LIMIT_VALUE;
		encoder_current = (new_current_limit - (int32_t)CAN_DEFAULT_CURRENT_LIMIT) / CURRENT_STEP_HEX;
    }

    /* Update global current limit value */
	curr_current_value = (uint32_t)new_current_limit;
    
    return curr_current_value;
}

/* Packing structure for voltage */
uint32_t pack_current_voltage(){
	/* Calculate new voltage based on encoder steps */
    int32_t new_voltage = (int32_t)CAN_DEFAULT_VOLTAGE_VALUE + encoder_voltage * (int32_t)VOLTAGE_STEP_HEX;
	
	/* Clamp new voltage within defined bounds */
    if (new_voltage < (int32_t)MIN_VOLTAGE_VALUE){
        new_voltage = (int32_t)MIN_VOLTAGE_VALUE;								
		encoder_voltage = (new_voltage - (int32_t)CAN_DEFAULT_VOLTAGE_VALUE) / VOLTAGE_STEP_HEX;
    }
    if (new_voltage > (int32_t)MAX_VOLTAGE_VALUE){
        new_voltage = (int32_t)MAX_VOLTAGE_VALUE;
		encoder_voltage = (new_voltage - (int32_t)CAN_DEFAULT_VOLTAGE_VALUE) / VOLTAGE_STEP_HEX;
    }

    /* Update global voltage value */
    curr_voltage_value = (uint32_t)new_voltage;

    return curr_voltage_value;
}





