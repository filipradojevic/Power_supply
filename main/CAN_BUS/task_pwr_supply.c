/**
 * @file    task_pwr_supply.c
 * @brief   Task for sending CAN requests and parsing responses from a power
 * supply.
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
#include "driver/uart.h"
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

/* User variables */
volatile system_stats_t system_stats_1;
volatile system_stats_t system_stats_2;
volatile system_stats_t system_stats_3;

esp_err_t esp_err_pwr;

/* FreeRTOS objects */
extern QueueHandle_t lvgl_voltage_queue;
extern QueueHandle_t lvgl_current_queue;
extern QueueHandle_t lvgl_update_queue;
extern QueueHandle_t xQueueCan;

lvgl_data_t display_1 = {0};
lvgl_data_t display_2 = {0};
lvgl_data_t display_3 = {0};

lvgl_bonded_data_t screen = {0};

/* Send parsed statistics - kompaktno */
void uart_send_stats_compact(uint8_t gadget_num, system_stats_t *stats) {

	char buffer[200];
	int offset = 0;

	// Format: "1: Vin=230.5V Iin=2.5A Pin=576W | Vout=48.2V Iout=10.5A
	// Pout=506W | Eff=87.8% Tout=42C Tin=45C"
	offset += sprintf(buffer + offset, "%d: ", gadget_num);
	offset +=
		sprintf(buffer + offset, "Vin=%.1fV Iin=%.2fA Pin=%.0fW | ",
				stats->input_voltage, stats->input_current, stats->input_power);
	offset += sprintf(buffer + offset, "Vout=%.1fV Iout=%.2fA Pout=%.0fW | ",
					  stats->output_voltage, stats->output_current_1,
					  stats->output_power);
	offset += sprintf(buffer + offset, "Eff=%.1f%% Tout=%.0fC Tin=%.0fC",
					  stats->efficiency, stats->output_temp, stats->input_temp);
	offset += sprintf(buffer + offset, "\r\n");

	uart_write_bytes(UART_NUM_0, buffer, offset);
}

/* Ili još kraća verzija */
void uart_send_stats_short(uint8_t gadget_num, system_stats_t *stats) {

	char buffer[128];

	sprintf(buffer, "%d: %.1fV %.2fA %.0fW -> %.1fV %.2fA %.0fW (%.1f%%)\r\n",
			gadget_num, stats->input_voltage, stats->input_current,
			stats->input_power, stats->output_voltage, stats->output_current_1,
			stats->output_power, stats->efficiency);

	uart_write_bytes(UART_NUM_0, buffer, strlen(buffer));
}

/* Još kompaktnija verzija (ako ti treba) */
void uart_send_stats_minimal(uint8_t gadget_num, system_stats_t *stats) {
	char buffer[64];

	sprintf(buffer, "M%d: %.1fV %.1fA %.1fA\r\n", gadget_num,
			stats->output_voltage, stats->output_current_1,
			stats->output_current_max);

	uart_write_bytes(UART_NUM_0, buffer, strlen(buffer));
}

/* Provera da li su svi parametri primljeni */
bool is_stats_complete(system_stats_t *stats) {
	// Proveri da li su sve bitne vrednosti != 0 (ili postavi flag u
	// parse_statistics)
	return (stats->input_voltage > 0 && stats->output_voltage > 0 &&
			stats->efficiency > 0);
}

/*******************************************************************************
 * Main function
 ******************************************************************************/

void task_pwr_supply(void *arg) {
	twai_message_t rx_msg;
	twai_message_t request_msg;

	TickType_t gui_update_period = pdMS_TO_TICKS(250);
	TickType_t timeout_ms = pdMS_TO_TICKS(1000);
	TickType_t last_update = xTaskGetTickCount();

	static uint8_t stats_counter_1 = 0;
	static uint8_t stats_counter_2 = 0;
	static uint8_t stats_counter_3 = 0;

	// Inicijalizuj 3 različita request messaga
	can_init_msg(&request_msg, CAN_REQUEST_PARAMETERS_ID, 0, 0,
				 CAN_REQUEST_FLAG);

	for (;;) {
		/* Send request messages to all 3 power supplies */
		twai_transmit(&request_msg, pdMS_TO_TICKS(100));

		if (xQueueReceive(xQueueCan, &rx_msg, timeout_ms) == pdPASS) {
			/* Check CAN ID of gadget 1 */
			if (rx_msg.identifier == CAN_REQUEST_VALUES_ID_1 ||
				rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID_1) {

				/* PARSE THE DATA */
				parse_statistics(rx_msg.data, &system_stats_1);

				/* Check ID request for display 1 */
				if (rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID_1) {
					uart_send_stats_minimal(CAN_GADGET_1, &system_stats_1);
					update_display(&display_1, system_stats_1, CAN_GADGET_1);
				}
			}
			/* Check CAN ID of gadget 2 */
			else if (rx_msg.identifier == CAN_REQUEST_VALUES_ID_2 ||
					 rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID_2) {

				/* PARSE THE DATA */
				parse_statistics(rx_msg.data, &system_stats_2);

				/* Check ID request for display 2 */
				if (rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID_2) {
					uart_send_stats_minimal(CAN_GADGET_2, &system_stats_2);
					update_display(&display_2, system_stats_2, CAN_GADGET_2);
				}
			}
			/* Check CAN ID of gadget 3 */
			else if (rx_msg.identifier == CAN_REQUEST_VALUES_ID_3 ||
					 rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID_3) {

				/* PARSE THE DATA */
				parse_statistics(rx_msg.data, &system_stats_3);

				uart_send_stats_minimal(CAN_GADGET_3, &system_stats_3);
				/* Check ID request for display 3 */
				// if (rx_msg.identifier == CAN_END_OF_REQUEST_VALUES_ID_3) {
				update_display(&display_3, system_stats_3, CAN_GADGET_3);
				// }
			}
		}

		TickType_t now = xTaskGetTickCount();

		/* Update screen if both displays changed or timeout elapsed */
		if ((display_1.updated == 1 && display_2.updated == 1 &&
			 display_3.updated == 1) ||
			(now - last_update) > gui_update_period) {

			/* Copy display data to screen and send to LVGL task */
			screen.display_1 = display_1;
			screen.display_2 = display_2;
			screen.display_3 = display_3;
			xQueueSend(lvgl_update_queue, &screen, 0);

			/* Reset update flags */
			display_1.updated = 0;
			display_2.updated = 0;
			display_3.updated = 0;

			/* Update last refresh time */
			last_update = now;
		}
	}
}

/* ============================ User Functions ============================= */

void can_init_msg(twai_message_t *msg, uint32_t id, uint32_t command,
				  uint32_t value, uint8_t flag) {
	msg->identifier = id;	   /* Set CAN message identifier      */
	msg->extd = 1;			   /* Use extended frame format       */
	msg->rtr = 0;			   /* No remote transmission request  */
	msg->data_length_code = 8; /* Data length is fixed to 8 bytes */

	/* If the flag indicates a command, fill data with command and value */
	if (flag == CAN_COMMAND_FLAG) {
		/* Pack command into first 4 bytes (big endian) */
		msg->data[0] = (command >> 24) & 0xFF;
		msg->data[1] = (command >> 16) & 0xFF;
		msg->data[2] = (command >> 8) & 0xFF;
		msg->data[3] = (command >> 0) & 0xFF;

		/* Pack value into next 4 bytes (big endian) */
		msg->data[4] = (value >> 24) & 0xFF;
		msg->data[5] = (value >> 16) & 0xFF;
		msg->data[6] = (value >> 8) & 0xFF;
		msg->data[7] = (value >> 0) & 0xFF;
	} else {
		/* If not a command, clear all data bytes */
		memset(msg->data, 0, 8);
	}
}

void parse_statistics(uint8_t *data, volatile system_stats_t *system_stats) {
	/* Extract command identifier from data */
	uint8_t command = data[1];

	/* Extract 32-bit value from data bytes (big endian) */
	uint32_t value = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
					 ((uint32_t)data[6] << 8) | ((uint32_t)data[7] << 0);

	/* Update corresponding field in system statistics based on command */
	switch (command) {
	case CAN_ANSWER_PARAMETERS_INPUT_POWER_ID:
		/* Input power scaled by 1024 */
		system_stats->input_power = value / 1024.0f;
		break;

	case CAN_ANSWER_PARAMETERS_INPUT_FREQ_ID:
		/* Input frequency scaled by 1024 */
		system_stats->input_freq = value / 1024.0f;
		break;

	case CAN_ANSWER_PARAMETERS_INPUT_CURRENT_ID:
		/* Input current scaled by 1024 */
		system_stats->input_current = value / 1024.0f;
		break;

	case CAN_ANSWER_PARAMETERS_OUTPUT_POWER_ID:
		/* Output power scaled by 1024 */
		system_stats->output_power = value / 1024.0f;
		break;

	case CAN_ANSWER_PARAMETERS_EFFICIENCY_ID:
		/* Efficiency scaled by 1024 and converted to percentage */
		system_stats->efficiency = (value / 1024.0f) * 100.0f;
		break;

	case CAN_ANSWER_PARAMETERS_OUTPUT_VOLTAGE_ID:
		/* Output voltage scaled by 1024 */
		system_stats->output_voltage = value / 1024.0f;
		break;

	case CAN_ANSWER_PARAMETERS_OUTPUT_CURR_MAX_ID:
		/* Maximum output current scaled by 30 */
		system_stats->output_current_max = value / 30.0f;
		break;

	case CAN_ANSWER_PARAMETERS_INPUT_VOLTAGE_ID:
		/* Input voltage scaled by 1024 */
		system_stats->input_voltage = value / 1024.0f;
		break;

	case CAN_ANSWER_PARAMETERS_OUTPUT_TEMP_ID:
		/* Output temperature scaled by 1024 */
		system_stats->output_temp = value / 1024.0f;
		break;

	case CAN_ANSWER_PARAMETERS_OUTPUT_CURR_1_ID:
		/* Output current 1 scaled by 1024 */
		system_stats->output_current_1 = value / 1024.0f;
		break;

	case CAN_ANSWER_PARAMETERS_OUTPUT_CURR_2_ID:
		/* Output current 2 scaled by 1024 */
		system_stats->output_current_2 = value / 1024.0f;
		break;

	case CAN_ANSWER_PARAMETERS_UNKNOWN_ID:
		/* Unknown parameter stored as-is */
		system_stats->unknown = value;
		break;

	default:
		/* Unknown command - optionally log or handle error */
		break;
	}
}
