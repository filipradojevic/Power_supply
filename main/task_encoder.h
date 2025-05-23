/**
 * @file    task_encoder.h
 * @brief   Header file for rotary encoder handling.
 *          Declares encoder initialization, ISR handler, and encoder task functions.
 *          Defines GPIO pins used for encoder signals and related constants.
 * 
 * @version 1.0.0
 * @date    20.05.2025
 * @author  LisumLab
 */

#ifndef MAIN_TASK_ENCODER_H_
#define MAIN_TASK_ENCODER_H_


/*******************************************************************************
 * Includes
 ******************************************************************************/

#include <stdio.h>
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "main.h"
#include "rom/ets_sys.h"


/*******************************************************************************
 * Defines
 ******************************************************************************/

/* Detecting impulse */
#define ENCODER_CLK_PIN GPIO_NUM_15
/* Detecting angular */
#define ENCODER_DT_PIN  GPIO_NUM_16 
/* Switch */
#define ENCODER_SW_PIN  GPIO_NUM_17

/* If it's needed for encoder to limit it*/
#define PCNT_UNIT       PCNT_UNIT_0
#define PCNT_H_LIM_VAL  10000
#define PCNT_L_LIM_VAL -10000

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/

/**
 * @brief GPIO interrupt handler for the rotary encoder.
 *
 * This ISR is triggered by a GPIO event (e.g., rising/falling edge) from the encoder.
 * It signals the encoder task using a semaphore to process the rotation or button press
 * outside the interrupt context.
 *
 * @param[in] arg Optional argument passed during ISR registration (usually unused).
 */
void encoder_isr_handler(void* arg);

/**
 * @brief Initializes the rotary encoder hardware and related components.
 *
 * Configures GPIO pins for encoder channels (CLK, DT) and switch (SW),
 * sets up GPIO interrupts for the encoder and button, and creates
 * synchronization primitives such as semaphores.
 *
 * @return ESP_OK if initialization succeeds, otherwise an appropriate error code.
 */
esp_err_t encoder_initialization(void);

/**
 * @brief FreeRTOS task that handles encoder input and sends CAN messages accordingly.
 *
 * This task waits for events from a queue set (encoder rotation or button press),
 * updates the internal encoder position, and sends the corresponding voltage or
 * current limit value over CAN, depending on the current mode.
 *
 * @param[in] arg Optional task argument (typically unused).
 */
void task_encoder(void *arg);

/**
 * @brief Prepares and returns the current limit value to be sent over CAN.
 *
 * Converts the current encoder position or other logic into a 32-bit packed
 * format representing the desired current limit.
 *
 * @return 32-bit value representing the packed current limit.
 */
uint32_t pack_current_limit();

/**
 * @brief Prepares and returns the output voltage value to be sent over CAN.
 *
 * Converts the current encoder position or other logic into a 32-bit packed
 * format representing the desired output voltage.
 *
 * @return 32-bit value representing the packed voltage value.
 */
uint32_t pack_current_voltage();

/**
 * @brief Sends current limit command over TWAI.
 */
void twai_send_current(uint32_t new_current_limit, uint32_t command);

/**
 * @brief Sends voltage command over TWAI.
 */
void twai_send_voltage(uint32_t new_voltage_value, uint32_t command);

#endif /* MAIN_TASK_ENCODER_H_ */
