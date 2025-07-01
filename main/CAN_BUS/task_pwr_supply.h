/**
 * @file    task_pwr_supply.h
 * @brief   Task pwr_supply monitors with RG4850G2 power supply 
 * We have CAN Bus between our uC and pwr supply and communicate via messages 
 * We have commands for:  
 * Current limit on/off line
 * Voltage on/off line
 * And we have request message for data temperature, voltage, current, power
 * efieciency and frequency both input and output things
 * @version 1.0.0
 * @date    14.05.2025
 * @author  LisumLab
 */


#ifndef TASK_PWR_SUPPLY_H
#define TASK_PWR_SUPPLY_H

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************* 
 * Includes 
 ******************************************************************************/
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/twai.h"

/******************************************************************************* 
 * Defines 
 ******************************************************************************/

/* CAN Message IDs */
#define CAN_REQUEST_PARAMETERS_ID_1                       0x108040FE
#define CAN_SETTING_VALUES_ID_1                           0x108180FE
#define CAN_REQUEST_VALUES_ID_1                           0x1081407F
#define CAN_END_OF_REQUEST_VALUES_ID_1                    0x1081407E
#define CAN_SETTING_VALUES_ACK_ID_1                       0x1081807E

/* CAN Message IDs 2*/
#define CAN_REQUEST_PARAMETERS_ID_2                       0x108040FE
#define CAN_SETTING_VALUES_ID_2                           0x108280FE
#define CAN_REQUEST_VALUES_ID_2                           0x1082407F
#define CAN_END_OF_REQUEST_VALUES_ID_2                    0x1082407E
#define CAN_SETTING_VALUES_ACK_ID_2                       0x1082807E


/* Can Answer parameters IDs*/
#define CAN_ANSWER_PARAMETERS_INPUT_POWER_ID              0X70
#define CAN_ANSWER_PARAMETERS_INPUT_FREQ_ID               0X71
#define CAN_ANSWER_PARAMETERS_INPUT_CURRENT_ID            0X72
#define CAN_ANSWER_PARAMETERS_OUTPUT_POWER_ID             0X73
#define CAN_ANSWER_PARAMETERS_EFFICIENCY_ID               0X74
#define CAN_ANSWER_PARAMETERS_OUTPUT_VOLTAGE_ID           0X75
#define CAN_ANSWER_PARAMETERS_OUTPUT_CURR_MAX_ID          0X76
#define CAN_ANSWER_PARAMETERS_INPUT_VOLTAGE_ID            0X78
#define CAN_ANSWER_PARAMETERS_OUTPUT_TEMP_ID              0X7F
#define CAN_ANSWER_PARAMETERS_OUTPUT_CURR_1_ID            0X81
#define CAN_ANSWER_PARAMETERS_OUTPUT_CURR_2_ID            0X82
#define CAN_ANSWER_PARAMETERS_UNKNOWN_ID                  0X83

/* CAN Commands */
#define CAN_SETTING_VALUES_ON_LINE_OUTPUT_VOLTAGE         0x01000000
#define CAN_SETTING_VALUES_OFF_LINE_OUTPUT_VOLTAGE        0x01010000
#define CAN_SETTING_VALUES_OVERVOLTAGE_PROTECTION         0x01020000
#define CAN_SETTING_VALUES_ON_LINE_CURRENT_LIMIT          0x01030000
#define CAN_SETTING_VALUES_OFF_LINE_DEFAULT_CURRENT_LIMIT 0x01040000

/* Default Settings */
#define CAN_DEFAULT_VOLTAGE_VALUE                         0x0000C738  // 49.8 V
#define CAN_DEFAULT_CURRENT_LIMIT                         0x0000003C  // 2    A
#define VOLTAGE_STEP_HEX                                  0x66        // 0.1  V step (102 Decimal)
#define CURRENT_STEP_HEX 								  0x03        // 0.1  A step (3 × 0.034 ≈ 0.102 A)

/* Range ov voltage and current */
#define MAX_VOLTAGE_VALUE 								  0xE91E      // 58.5 V
#define MIN_VOLTAGE_VALUE 								  0xA55A      // 41.5 V
#define MAX_CURRENT_LIMIT_VALUE							  0x06E5  	  // 60   A
#define MIN_CURRENT_LIMIT_VALUE							  0x0000  	  // 0    A

/* CAN Flags */
#define CAN_REQUEST_FLAG                                  0
#define CAN_COMMAND_FLAG                                  1

/******************************************************************************* 
 * Typedefs 
 ******************************************************************************/

/**
 * @brief Structure holding various power system statistics.
 */
typedef struct {
    float     input_power;
    float     input_freq;
    float     input_current;
    float     output_power;
    float     efficiency;
    float     output_voltage;
    float     output_current_max;
    float     input_voltage;
    float     output_temp;
    float     output_current_1;
    float     output_current_2;
    float     input_temp;
    uint32_t  unknown;
} system_stats_t;


typedef enum {
    CAN_GADGET_1 = 0,
    CAN_GADGET_2 = 1
} can_gadget_e;

typedef struct {
    float voltage;
    float current;
    float limit;
    float temp;
    float power;
    float efficiency;
    can_gadget_e display_id;  // 1 za display_1, 2 za display_2
    uint8_t updated;
} lvgl_data_t;

typedef struct{
	lvgl_data_t display_1;
	lvgl_data_t display_2;
} lvgl_bonded_data_t;


/******************************************************************************* 
 * Global Variables 
 ******************************************************************************/


/******************************************************************************* 
 * Function Prototypes 
 ******************************************************************************/

/**
 * @brief Updates the display structure with current system statistics.
 *
 * Copies relevant data from the provided system statistics into the display
 * structure and marks the display as updated. Also assigns the given display ID.
 *
 * @param display      Pointer to the display data structure to update.
 * @param system_stats The system statistics to copy data from.
 * @param display_id   Identifier for the display (e.g., gadget ID).
 */

static inline void update_display(lvgl_data_t *display, system_stats_t system_stats, uint8_t display_id){
    display->voltage    = system_stats.output_voltage;
    display->current    = system_stats.output_current_1;
    display->limit      = system_stats.output_current_max;
    display->temp       = system_stats.output_temp;
    display->power      = system_stats.output_voltage * system_stats.output_current_1;
    display->efficiency = system_stats.efficiency;
    display->display_id = display_id;
    display->updated    = 1;
}

/**
 * @brief Callback triggered by CAN receive interrupt.
 *
 * @param[in] arg     User-defined argument (optional).
 */

void IRAM_ATTR my_can_rx_callback(void* arg);

/**
 * @brief Parses raw CAN data into the system statistics structure.
 *
 * @param[in] data  Pointer to received CAN payload.
 * @param system_stats structure in which we parse the data.
 */

void parse_statistics(uint8_t *data, volatile system_stats_t *system_stats);

/**
 * @brief Initializes a CAN message structure with specified parameters.
 *
 * @param[out] can_structure  Pointer to CAN message structure to be filled.
 * @param[in]  device_id      CAN device ID.
 * @param[in]  command        Command or request identifier.
 * @param[in]  value  Voltage/current value to be sent.
 * @param[in]  flag           CAN_REQUEST_FLAG or CAN_COMMAND_FLAG.
 */

void can_init_msg(twai_message_t *msg, uint32_t id, uint32_t command, uint32_t value, uint8_t flag);


#ifdef __cplusplus
}
#endif

#endif /* TASK_PWR_SUPPLY_H */
