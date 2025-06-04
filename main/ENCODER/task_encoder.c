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
static send_type_e current_send_flag = SEND_VOLTAGE;   /* Current parameter being adjusted (voltage or current) */
static button_pressed_e button_flag = BUTTON_NOT_PRESSED;     /* State of the encoder button */
volatile int encoder_voltage = 0;                       /* Encoder step count for voltage */
volatile int encoder_current = 0;                       /* Encoder step count for current */

/* Current CAN parameter values */
uint32_t curr_voltage_value = CAN_DEFAULT_VOLTAGE_VALUE;   /* Current voltage value */
uint32_t curr_current_value = CAN_DEFAULT_CURRENT_LIMIT;   /* Current current limit */

/* Debounce timing for encoder signals */
static int64_t last_step_time_encoder_pulse_us = 0;     /* Timestamp of last encoder pulse */
static const int64_t STEP_DEBOUNCE_INTERVAL_US = 17500; /* Debounce interval in microseconds (~17.5ms) */
static int lastButtonReading = 1;             // poslednje očitano stanje dugmeta
static int64_t lastDebounceTime = 0;          // vreme poslednje promene

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
    uint8_t currentButtonState = 1;
    QueueSetMemberHandle_t activated_handle = NULL;

    while (1) {
        activated_handle = xQueueSelectFromSet(xQueueSetEncoder, portMAX_DELAY);

        /* Encoder rotation event */
        if (activated_handle == encoder_semaphore) {
            xSemaphoreTake(encoder_semaphore, 0);

            for (i = 0; i < 2500; i++);

            int clk_level = gpio_get_level(ENCODER_CLK_PIN);
            int dt_level = gpio_get_level(ENCODER_DT_PIN);
            int step = 0;

            if (clk_level == 1) step = (dt_level  != clk_level) ? +1 : -1;

            if (step != 0) {
				
				/* Software Debounce */
                int64_t now_pulse = esp_timer_get_time();

                if ((now_pulse - last_step_time_encoder_pulse_us) < STEP_DEBOUNCE_INTERVAL_US)
                    continue;

                last_step_time_encoder_pulse_us = now_pulse;

                if (button_flag == BUTTON_NOT_PRESSED) {
                    
                    /* Select parameter to adjust */
                    current_send_flag = (step > 0) ? SEND_CURRENT_LIMIT : SEND_VOLTAGE;
                    
                    xQueueSend(lvgl_bolding_update, &current_send_flag, 0);
                
                } else {
                    
                    /* Update selected parameter */
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
            }

        /* Encoder button press event */
        } else if (activated_handle == switch_semaphore) {
			xSemaphoreTake(switch_semaphore, 0);
			
			/* Software Debounce */
		    int reading = gpio_get_level(ENCODER_SW_PIN);
		    
		    int64_t now = esp_timer_get_time();
		
		    if (reading != lastButtonReading) {
		        lastDebounceTime = now;
		        lastButtonReading = reading;
		    }
		
		    if ((now - lastDebounceTime) > DEBOUNCE_DELAY_US) {
		        
		        int reading = gpio_get_level(ENCODER_SW_PIN);
		          
	            if (reading == 0) {
	                /* Toggle button_flag */
	                button_flag = (button_flag == BUTTON_PRESSED) ? BUTTON_NOT_PRESSED : BUTTON_PRESSED;
	                
	                xQueueSend(lvgl_button_pressed, &button_flag, 0);
	                
	                lastButtonReading = 1;
	            }
		    }
		    
		    
        /* Periodic command event to prevent device reset */
        } else if (activated_handle == command_semaphore) {
            xSemaphoreTake(command_semaphore, 0);

            uint32_t new_voltage_value = pack_current_voltage();
            twai_send_voltage(new_voltage_value, CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE);
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





