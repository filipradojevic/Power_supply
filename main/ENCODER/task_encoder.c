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
#include "lvgl_screens.h"
#include "main.h"
#include "task_can_receive.h"
#include "task_encoder.h"
#include "task_lvgl_ili9341.h"
#include "task_pwr_supply.h"

/*******************************************************************************
 * Defines
 ******************************************************************************/
#define DEBOUNCE_DELAY_US 20000			// 20 [ms] debounce for encoder signals
#define ENCODER_RATE_LIMIT_US 8000		// max ~125 Hz
#define ENCODER_FLUSH_INTERVAL_US 20000 // slanje na 20 ms

/* Timer defines */
#define TIMER_BASE_CLK 80000000 /* 80MHz clock */
#define TIMER_DIVIDER 80		/* 80 MHz / 80 = 1 000 000 Hz (1 tick = 1us) */
#define TIMER_SCALE                                                            \
	(TIMER_BASE_CLK / TIMER_DIVIDER) /* 100,000 ticks per second */
#define TIMER_INTERVAL_US                                                      \
	50000 /* interrupt interval in microseconds (50ms)                         \
		   */

/*******************************************************************************
 * User Variables
 ******************************************************************************/

/* Encoder and button state */
static send_type_e current_send_flag =
	SEND_VOLTAGE; /* Current parameter being adjusted (voltage or current) */
static button_pressed_e button_flag =
	BUTTON_NOT_PRESSED;			  /* State of the encoder button */
volatile int encoder_voltage = 0; /* Encoder step count for voltage */
volatile int encoder_current = 0; /* Encoder step count for current */

/* Current CAN parameter values */
uint32_t curr_voltage_value =
	CAN_DEFAULT_VOLTAGE_VALUE; /* Current voltage value */
uint32_t curr_current_value =
	CAN_DEFAULT_CURRENT_LIMIT; /* Current current limit */

/* Debounce timing for encoder signals */
static int64_t last_step_time_encoder_pulse_us =
	0;								 /* Timestamp of last encoder pulse */
static int lastButtonReading = 1;	 // poslednje očitano stanje dugmeta
static int64_t lastDebounceTime = 0; // vreme poslednje promene

/* Encoder accumulation */
static volatile int encoder_delta_voltage = 0;
static volatile int encoder_delta_current = 0;

/* Timing */
static int64_t last_encoder_event_us = 0;
static int64_t last_flush_us = 0;

/*******************************************************************************
 * UI Object References
 ******************************************************************************/

extern ui_objects_t objects; /* Structure containing all UI objects */

/*******************************************************************************
 * FreeRTOS Objects
 ******************************************************************************/

/* Queues */
extern QueueHandle_t lvgl_voltage_queue;  /* Queue for voltage changes */
extern QueueHandle_t lvgl_current_queue;  /* Queue for current limit changes */
extern QueueHandle_t lvgl_bolding_update; /* Queue for UI bolding updates */
extern QueueHandle_t lvgl_button_pressed; /* Queue for button press events */

/* Queue set */
extern QueueSetHandle_t
	xQueueSetEncoder; /* Queue set for encoder-related events */

/* Semaphores */
extern SemaphoreHandle_t encoder_semaphore; /* Semaphore for encoder pulses */
extern SemaphoreHandle_t
	switch_semaphore; /* Semaphore for encoder button presses */
extern SemaphoreHandle_t
	command_semaphore; /* Semaphore for command execution */

static esp_err_t timer_initialization(void);
/* Isr prototypes */
void timer_isr(void *arg);

/*******************************************************************************
 * Main function
 ******************************************************************************/
void task_encoder(void *arg) {

	QueueSetMemberHandle_t activated_handle = NULL;

	if (timer_initialization() != ESP_OK) {
		// return ESP_FAIL;
	}

	while (1) {

		activated_handle = xQueueSelectFromSet(xQueueSetEncoder, portMAX_DELAY);

		/* ================= ROTATION EVENT ================= */
		if (activated_handle == encoder_semaphore) {

			xSemaphoreTake(encoder_semaphore, 0);

			int64_t now = esp_timer_get_time();

			/* Debounce - ignore pulses that come too quickly */
			if ((now - last_encoder_event_us) < DEBOUNCE_DELAY_US)
				continue;

			last_encoder_event_us = now;

			int clk = gpio_get_level(ENCODER_CLK_PIN);
			int dt = gpio_get_level(ENCODER_DT_PIN);
			int step = 0;

			if (clk == 1)
				step = (dt != clk) ? +1 : -1;

			if (step == 0)
				continue;

			/* -------- PARAMETER SELECT -------- */
			if (button_flag == BUTTON_NOT_PRESSED) {

				current_send_flag =
					(step > 0) ? SEND_CURRENT_LIMIT : SEND_VOLTAGE;

				xQueueSend(lvgl_bolding_update, &current_send_flag, 0);

				/* -------- VALUE CHANGE (ACCUMULATE) -------- */
			} else {

				if (current_send_flag == SEND_VOLTAGE)
					encoder_delta_voltage += step;
				else
					encoder_delta_current += step;
			}
		}

		/* ================= BUTTON EVENT ================= */
		else if (activated_handle == switch_semaphore) {

			xSemaphoreTake(switch_semaphore, 0);

			int reading = gpio_get_level(ENCODER_SW_PIN);
			int64_t now = esp_timer_get_time();

			if (reading != lastButtonReading) {
				lastDebounceTime = now;
				lastButtonReading = reading;
			}

			if ((now - lastDebounceTime) > DEBOUNCE_DELAY_US) {

				if (reading == 0) {

					button_flag = (button_flag == BUTTON_PRESSED)
									  ? BUTTON_NOT_PRESSED
									  : BUTTON_PRESSED;

					/* KADA SE DUGME OTPUSTI - pošalji finalne vrednosti */
					if (button_flag == BUTTON_NOT_PRESSED) {

						/* ---- Voltage update - pošalji finalnu vrednost ----
						 */
						uint32_t v = pack_current_voltage();
						twai_send_voltage(
							v, CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE);
						xQueueSend(lvgl_voltage_queue, &v, 0);

						/* ---- Current update - pošalji finalnu vrednost ----
						 */
						uint32_t c = pack_current_limit();
						twai_send_current(
							c, CAN_SETTING_VALUES_ON_LINE_CURRENT_LIMIT);
						xQueueSend(lvgl_current_queue, &c, 0);
					}

					xQueueSend(lvgl_button_pressed, &button_flag, 0);

					lastButtonReading = 1;
				}
			}
		}

		/* ================= PERIODIC FLUSH - samo za preview na ekranu
		   ================= */
		else if (activated_handle == command_semaphore) {

			xSemaphoreTake(command_semaphore, 0);

			int64_t now = esp_timer_get_time();

			if ((now - last_flush_us) < ENCODER_FLUSH_INTERVAL_US)
				continue;

			last_flush_us = now;

			/* Ažuriraj prikaz samo ako je dugme pritisnuto i ima promena */
			if (button_flag == BUTTON_PRESSED) {

				/* ---- Voltage preview ---- */
				if (encoder_delta_voltage != 0) {

					encoder_voltage += encoder_delta_voltage;
					encoder_delta_voltage = 0;

					uint32_t v = pack_current_voltage();
					xQueueSend(lvgl_voltage_queue, &v, 0);
				}

				/* ---- Current preview ---- */
				if (encoder_delta_current != 0) {

					encoder_current += encoder_delta_current;
					encoder_delta_current = 0;

					uint32_t c = pack_current_limit();
					xQueueSend(lvgl_current_queue, &c, 0);
				}
			}
		}
	}
}

/* Initialize hardware: Timer */
static esp_err_t timer_initialization(void) {
	/* Initialize the structure of a timer peripheral */
	timer_config_t config = {
		.divider = TIMER_DIVIDER,
		.counter_dir = TIMER_COUNT_UP,
		.counter_en = TIMER_PAUSE,
		.alarm_en = TIMER_ALARM_EN,
		.auto_reload = true,
	};

	/* Initialized timer */
	if (timer_init(TIMER_GROUP_0, TIMER_0, &config) != ESP_OK) {
		return ESP_FAIL;
	}

	/* Setup timer to count from zero */
	if (timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL) !=
		ESP_OK) {
		return ESP_FAIL;
	}

	/* Set the alarm (interrupt) to be activated at wanted time */
	if (timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, TIMER_INTERVAL_US) !=
		ESP_OK) {
		return ESP_FAIL;
	}

	/* Enable interrupt */
	if (timer_enable_intr(TIMER_GROUP_0, TIMER_0) != ESP_OK) {
		return ESP_FAIL;
	}

	/* Connect with isr handler */
	if (timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_isr, NULL,
						   ESP_INTR_FLAG_IRAM, NULL) != ESP_OK) {
		return ESP_FAIL;
	}

	/* Start the timer */
	if (timer_start(TIMER_GROUP_0, TIMER_0) != ESP_OK) {
		return ESP_FAIL;
	}

	return ESP_OK;
}

/* Sending command for a voltage ON/OFF Line */
void twai_send_voltage(uint32_t new_voltage_value, uint32_t command) {
	twai_message_t msg;

	can_init_msg(&msg, CAN_SETTING_VALUES_ID_1, command, new_voltage_value,
				 CAN_COMMAND_FLAG);

	twai_transmit(&msg, pdMS_TO_TICKS(100));

	can_init_msg(&msg, CAN_SETTING_VALUES_ID_2, command, new_voltage_value,
				 CAN_COMMAND_FLAG);

	twai_transmit(&msg, pdMS_TO_TICKS(100));

	can_init_msg(&msg, CAN_SETTING_VALUES_ID_3, command, new_voltage_value,
				 CAN_COMMAND_FLAG);

	twai_transmit(&msg, pdMS_TO_TICKS(100));
}

/* Sending command for a Current LIMIT ON/OFF Line */
void twai_send_current(uint32_t new_current_limit, uint32_t command) {
	twai_message_t msg;

	can_init_msg(&msg, CAN_SETTING_VALUES_ID_1, command, new_current_limit,
				 CAN_COMMAND_FLAG);

	twai_transmit(&msg, pdMS_TO_TICKS(100));

	can_init_msg(&msg, CAN_SETTING_VALUES_ID_2, command, new_current_limit,
				 CAN_COMMAND_FLAG);

	twai_transmit(&msg, pdMS_TO_TICKS(100));

	can_init_msg(&msg, CAN_SETTING_VALUES_ID_3, command, new_current_limit,
				 CAN_COMMAND_FLAG);

	twai_transmit(&msg, pdMS_TO_TICKS(100));
}

/* Packing structure for voltage */
uint32_t pack_current_limit() {
	/* Formula for new current limit to be send */
	int32_t new_current_limit = (int32_t)CAN_DEFAULT_CURRENT_LIMIT +
								encoder_current * (int32_t)CURRENT_STEP_HEX;

	/* Check limits */
	if (new_current_limit < (int32_t)MIN_CURRENT_LIMIT_VALUE)
		new_current_limit = (int32_t)MIN_CURRENT_LIMIT_VALUE;
	encoder_current = (new_current_limit - (int32_t)CAN_DEFAULT_CURRENT_LIMIT) /
					  CURRENT_STEP_HEX;

	if (new_current_limit > (int32_t)MAX_CURRENT_LIMIT_VALUE)
		new_current_limit = (int32_t)MAX_CURRENT_LIMIT_VALUE;
	encoder_current = (new_current_limit - (int32_t)CAN_DEFAULT_CURRENT_LIMIT) /
					  CURRENT_STEP_HEX;

	curr_current_value = (uint32_t)new_current_limit;
	return curr_current_value;
}

/* Packing structure for current limit */
uint32_t pack_current_voltage() {
	/* Formula for new voltage to be send */
	int32_t new_voltage = (int32_t)CAN_DEFAULT_VOLTAGE_VALUE +
						  encoder_voltage * (int32_t)VOLTAGE_STEP_HEX;

	/* Check limits */
	if (new_voltage < (int32_t)MIN_VOLTAGE_VALUE)
		new_voltage = (int32_t)MIN_VOLTAGE_VALUE;
	encoder_voltage =
		(new_voltage - (int32_t)CAN_DEFAULT_VOLTAGE_VALUE) / VOLTAGE_STEP_HEX;

	if (new_voltage > (int32_t)MAX_VOLTAGE_VALUE)
		new_voltage = (int32_t)MAX_VOLTAGE_VALUE;
	encoder_voltage =
		(new_voltage - (int32_t)CAN_DEFAULT_VOLTAGE_VALUE) / VOLTAGE_STEP_HEX;

	curr_voltage_value = (uint32_t)new_voltage;

	return curr_voltage_value;
}

/* Periodically sends command to prevent power supply reset to default */
void IRAM_ATTR timer_isr(void *arg) {
	/* Clear flag of interrupt */
	timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);

	/* Start timer again */
	timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	xSemaphoreGiveFromISR(command_semaphore, &xHigherPriorityTaskWoken);

	if (xHigherPriorityTaskWoken) {
		portYIELD_FROM_ISR();
	}
}