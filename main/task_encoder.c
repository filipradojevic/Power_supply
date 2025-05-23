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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* Drivers header files */
#include "driver/twai.h"
#include "hal/twai_types.h"

/* Users header files */
#include "main.h"
#include "task_pwr_supply.h"
#include "task_can_receive.h"
#include "task_encoder.h"
#include "esp_timer.h"

/*******************************************************************************
 * Defines
 ******************************************************************************/

 
 /*******************************************************************************
 * User variables
 ******************************************************************************/

/* User ENUM's */
typedef enum {
    SEND_VOLTAGE = 0,
    SEND_CURRENT_LIMIT
} send_type_e;

/* User variables */
static send_type_e current_send_flag = SEND_VOLTAGE;
volatile int encoderPos = 0;
uint32_t curr_voltage_value = CAN_DEFAULT_VOLTAGE_VALUE;
uint32_t curr_current_value = CAN_DEFAULT_CURRENT_LIMIT;

/* FreeRTOS objects */
extern SemaphoreHandle_t encoder_semaphore;
extern SemaphoreHandle_t switch_semaphore;
extern SemaphoreHandle_t command_semaphore;
extern QueueSetHandle_t xQueueSet;

/*******************************************************************************
 * Main function
 ******************************************************************************/

void task_encoder(void *arg) {
    uint16_t i;
    uint8_t currentButtonState = 1;
    QueueSetMemberHandle_t activated_handle = NULL;

    while (1) {
        activated_handle = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);

        /* We process encoder changes and send the right commands */
        if (activated_handle == encoder_semaphore) {
            xSemaphoreTake(encoder_semaphore, 0);

            /* Wait for debounce */
            for (i = 0; i < 1000; i++);

            int a = gpio_get_level(ENCODER_CLK_PIN);
            int b = gpio_get_level(ENCODER_DT_PIN);

            int step = 0;

            /* Check if still active */
            if (a == 1) {
                step = (b != a) ? +1 : -1;
            }

            if (step != 0) {
                encoderPos += step;

                /* Check the type of the command voltage/current */
                if (current_send_flag == SEND_VOLTAGE) {
                    uint32_t new_voltage_value = pack_current_voltage();

                    twai_send_voltage(new_voltage_value, CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE);
                }else {
                    uint32_t new_current_limit = pack_current_limit();

                    twai_send_current(new_current_limit, CAN_SETTING_VALUES_ON_LINE_CURRENT_LIMIT);
                }
            }

        /* We process switch changes and set the right commands */
        }else if (activated_handle == switch_semaphore) {
            xSemaphoreTake(switch_semaphore, 0);

            for (i = 0; i < 1000; i++);

            /* Read button state again to verify rising edge of button signal */
            currentButtonState = gpio_get_level(ENCODER_SW_PIN);

            if (currentButtonState == 0) {
                current_send_flag = (current_send_flag == SEND_VOLTAGE) ? SEND_CURRENT_LIMIT : SEND_VOLTAGE;
            }

        /* Timer interrupt triggers this; send command to prevent power supply reset */
        }else if (activated_handle == command_semaphore){
            xSemaphoreTake(command_semaphore, 0);
            
			uint32_t current_voltage_value = pack_current_voltage();
			
            twai_send_voltage(current_voltage_value, CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE);
        
        }else {
            Error_Handler();
        }
    }
}

void twai_send_current(uint32_t new_current_limit, uint32_t command){
    twai_message_t msg;
    
    can_init_msg(&msg,
                    CAN_SETTING_VALUES_ID,
                    command,
                    new_current_limit,
                    CAN_COMMAND_FLAG);

    twai_transmit(&msg, pdMS_TO_TICKS(100));
}

void twai_send_voltage(uint32_t new_voltage_value, uint32_t command){
    twai_message_t msg;

    can_init_msg(&msg,
                    CAN_SETTING_VALUES_ID,
                    command,
                    new_voltage_value,
                    CAN_COMMAND_FLAG);

    twai_transmit(&msg, pdMS_TO_TICKS(100));
}

uint32_t pack_current_limit(){
	int32_t new_current_limit = (int32_t)CAN_DEFAULT_CURRENT_LIMIT + encoderPos * (int32_t)CURRENT_STEP_HEX;

    // ograničenja za struju
    if (new_current_limit < (int32_t)MIN_CURRENT_LIMIT_VALUE)
        new_current_limit = (int32_t)MIN_CURRENT_LIMIT_VALUE;
		encoderPos = (new_current_limit - (int32_t)CAN_DEFAULT_CURRENT_LIMIT) / CURRENT_STEP_HEX;
    
    if (new_current_limit > (int32_t)MAX_CURRENT_LIMIT_VALUE)
        new_current_limit = (int32_t)MAX_CURRENT_LIMIT_VALUE;
		encoderPos = (new_current_limit - (int32_t)CAN_DEFAULT_CURRENT_LIMIT) / CURRENT_STEP_HEX;

	curr_current_value = (uint32_t)new_current_limit;
    return curr_current_value;
}

uint32_t pack_current_voltage(){
	/* Formula for new voltage to be send */
    int32_t new_voltage = (int32_t)CAN_DEFAULT_VOLTAGE_VALUE + encoderPos * (int32_t)VOLTAGE_STEP_HEX;
	
	/* Check limits */
    if (new_voltage < (int32_t)MIN_VOLTAGE_VALUE)
        new_voltage = (int32_t)MIN_VOLTAGE_VALUE;								
		encoderPos = (new_voltage - (int32_t)CAN_DEFAULT_VOLTAGE_VALUE) / VOLTAGE_STEP_HEX;
    
    if (new_voltage > (int32_t)MAX_VOLTAGE_VALUE)
        new_voltage = (int32_t)MAX_VOLTAGE_VALUE;
		encoderPos = (new_voltage - (int32_t)CAN_DEFAULT_VOLTAGE_VALUE) / VOLTAGE_STEP_HEX;

    curr_voltage_value = (uint32_t)new_voltage;

    return curr_voltage_value;
}





