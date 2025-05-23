/**
 * @file    main.c
 * @brief   Main application file.
 *          Initializes all peripherals: CAN, LED, and encoder pins.
 *          Handles encoder interrupt processing and starts relevant tasks.
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
#include <freertos/FreeRTOS.h>
#include "esp_err.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* Drivers header files */
#include "driver/gpio.h"
#include "driver/twai.h"
#include "driver/timer.h"
#include "hal/gpio_types.h"
#include "main.h"

/* Users header files */
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "task_encoder.h"

/*******************************************************************************
 * Defines
 ******************************************************************************/

#define CAN_QUEUE_MAX_SIZE 128
#define QUEUE_SET_LENGTH   3
#define MIN_PRIO_TASK 	   1
#define MEDIUM_PRIO_TASK   3
#define MAX_PRIO_TASK 	   5


#define TIMER_BASE_CLK        80000000                   /* 80 MHz base clock */
#define TIMER_DIVIDER         8000               /* 80 MHz / 8000 = 10,000 Hz */
#define TIMER_SCALE (TIMER_BASE_CLK/TIMER_DIVIDER) /* 10,000 ticks per second */
#define TIMER_INTERVAL_SEC    10             /* interrupt interval in seconds */

/*******************************************************************************
 * User variables
 ******************************************************************************/

/* User variables */
volatile int64_t last_event_time = 0;
extern volatile int encoderPos;
const int64_t debounce_us = 1000;
esp_err_t esp_err;

/* FreeRTOS objects */
QueueHandle_t queue_can;
QueueSetHandle_t xQueueSet;
SemaphoreHandle_t encoder_semaphore;
SemaphoreHandle_t switch_semaphore;
SemaphoreHandle_t command_semaphore;

/*******************************************************************************
 * Prototyp of functions
 ******************************************************************************/

/* Initialization prototypes */
static esp_err_t hardware_init(void);
static esp_err_t rtos_objects_init(void);
static esp_err_t timer_initialization(void);
static esp_err_t twai_initialization(void);
static esp_err_t led_initialization(void);

/* Task prototypes */
void task_pwr_supply(void *arg);
void task_can_alert(void *arg);
void task_encoder(void *arg);

/* Isr prototypes */
void timer_isr(void *arg);
void switch_isr_handler(void* arg);
void encoder_isr_handler(void* arg);

/*******************************************************************************
 * Main
 ******************************************************************************/

void app_main() {
    /** Initialize hardware peripherals */
    if (hardware_init() != ESP_OK) {
        Error_Handler();
    }

    /** Initialize FreeRTOS objects: queues, semaphores */
    if (rtos_objects_init() != ESP_OK) {
        Error_Handler();
    }

    /** Create encoder task */
    if (xTaskCreate(task_encoder, "Encode_and_send_cmd", 2048, NULL,
     MAX_PRIO_TASK, NULL) != pdPASS) {
        Error_Handler();
    }
    
    if (xTaskCreate(task_can_alert, "can_rx_task", 2048, NULL,
     MIN_PRIO_TASK, NULL) != pdPASS) {
        Error_Handler();
    }

    if (xTaskCreate(task_pwr_supply, "can_processing", 2048, NULL,
     MEDIUM_PRIO_TASK, NULL) != pdPASS) {
        Error_Handler();
    }
    
    /* Fallback loop, it should never be entered */
    while (1) {
        Error_Handler();
    }
}

/** Initialize hardware: LED GPIO, TWAI (CAN)*/
static esp_err_t hardware_init(void) {
    if (led_initialization() != ESP_OK) {
        return ESP_FAIL;
    }

    if (twai_initialization() != ESP_OK) {
        return ESP_FAIL;
    }

    if (timer_initialization() != ESP_OK) {
        return ESP_FAIL;
    }
    
    if (encoder_initialization() != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t led_initialization(void){
    if (gpio_reset_pin(GPIO_NUM_48) == ESP_FAIL) {
        return ESP_FAIL;
    }
    
    if (gpio_set_direction(GPIO_NUM_48, GPIO_MODE_OUTPUT) == ESP_FAIL) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t twai_initialization(void){
    /* Initialize the structure of a twai peripheral */
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_4, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
    twai_filter_config_t f_config = {
        .acceptance_code = (0x1081407F << 3) | 0x04,
        .acceptance_mask = (0x1FFFFFFF << 3) | 0x04,
        .single_filter = true
    };

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_FAIL) {
        return ESP_FAIL;
    }
    if (twai_start() == ESP_FAIL) {
        return ESP_FAIL;
    }

	//TODO Process other alert flags 
    uint32_t alert_flags = TWAI_ALERT_RX_DATA|
    					   TWAI_ALERT_BUS_OFF | 
    					   TWAI_ALERT_BUS_ERROR;

    if (twai_reconfigure_alerts(alert_flags, NULL) != ESP_OK) {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

static esp_err_t timer_initialization(void){
    /* Initialize the structure of a timer peripheral */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true,
    };

    /* Initialized timer */
    if (timer_init(TIMER_GROUP_0, TIMER_0, &config) != ESP_OK){
        return ESP_FAIL;
    }

    /* Setup timer to count from zero */
    if (timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL) != ESP_OK){
        return ESP_FAIL;
    }

    /* Set the alarm (interrupt) to be activated at wanted time */
    if (timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, TIMER_INTERVAL_SEC * TIMER_SCALE) != ESP_OK){
        return ESP_FAIL;
    }

    /* Enable interrupt */
    if (timer_enable_intr(TIMER_GROUP_0, TIMER_0) != ESP_OK){
        return ESP_FAIL;
    }

    /* Connect with isr handler */
    if (timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_isr,
                       NULL, ESP_INTR_FLAG_IRAM, NULL) != ESP_OK){
        return ESP_FAIL;
    }

    /* Start the timer */
    if (timer_start(TIMER_GROUP_0, TIMER_0) != ESP_OK){
        return ESP_FAIL;
    }

    return ESP_OK;
}

/** Initialize FreeRTOS objects: CAN queue, semaphores, queue set */
static esp_err_t rtos_objects_init(void) {
    queue_can = xQueueCreate(CAN_QUEUE_MAX_SIZE, sizeof(twai_message_t));
    if (queue_can == NULL) {
        gpio_set_level(GPIO_NUM_48, PIN_STATE_HIGH);
        return ESP_FAIL;
    }

    encoder_semaphore = xSemaphoreCreateBinary();
    if (encoder_semaphore == NULL) {
        gpio_set_level(GPIO_NUM_48, PIN_STATE_HIGH);
        return ESP_FAIL;
    }

    switch_semaphore = xSemaphoreCreateBinary();
    if (switch_semaphore == NULL) {
        gpio_set_level(GPIO_NUM_48, PIN_STATE_HIGH);
        return ESP_FAIL;
    }

    command_semaphore = xSemaphoreCreateBinary();
    if (command_semaphore == NULL){
        gpio_set_level(GPIO_NUM_48, PIN_STATE_HIGH);
        return ESP_FAIL;
    }

    xQueueSet = xQueueCreateSet(QUEUE_SET_LENGTH);
    
    if (xQueueSet == NULL) {
        return ESP_FAIL;
    }
    
    if (xQueueAddToSet(encoder_semaphore, xQueueSet) != pdPASS) {
        return ESP_FAIL;
    }
    
    if (xQueueAddToSet(switch_semaphore, xQueueSet) != pdPASS) {
        return ESP_FAIL;
    }
    
    if (xQueueAddToSet(command_semaphore, xQueueSet) != pdPASS) {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/** Initialize hardware: Rotary encoder */
esp_err_t encoder_initialization(void) {
    esp_err_t ret;

    /* Set switch pin as input */
    ret = gpio_set_direction(ENCODER_SW_PIN, GPIO_MODE_INPUT);
    if (ret != ESP_OK) return ret;

    /* Set DT pin as input */
    ret = gpio_set_direction(ENCODER_DT_PIN, GPIO_MODE_INPUT);
    if (ret != ESP_OK) return ret;

    /* Set CLK pin as input */
    ret = gpio_set_direction(ENCODER_CLK_PIN, GPIO_MODE_INPUT);
    if (ret != ESP_OK) return ret;

    /* Enable internal pull-up resistors for encoder pins */
    ret = gpio_pullup_en(ENCODER_DT_PIN);
    if (ret != ESP_OK) return ret;
    
    ret = gpio_pullup_en(ENCODER_CLK_PIN);
    if (ret != ESP_OK) return ret;
    
    ret = gpio_pullup_en(ENCODER_SW_PIN);
    if (ret != ESP_OK) return ret;

    /* Install GPIO ISR service with default configuration */
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK) return ret;

    /* Configure interrupt on rising edge for encoder CLK pin */
    ret = gpio_set_intr_type(ENCODER_CLK_PIN, GPIO_INTR_POSEDGE);
    if (ret != ESP_OK) return ret;

    /* Attach ISR handler for encoder CLK pin */
    ret = gpio_isr_handler_add(ENCODER_CLK_PIN, encoder_isr_handler, NULL);
    if (ret != ESP_OK) return ret;

    /* Configure interrupt on falling edge for encoder switch pin */
    ret = gpio_set_intr_type(ENCODER_SW_PIN, GPIO_INTR_NEGEDGE);
    if (ret != ESP_OK) return ret;

    /* Attach ISR handler for encoder switch pin */
    ret = gpio_isr_handler_add(ENCODER_SW_PIN, switch_isr_handler, NULL);
    if (ret != ESP_OK) return ret;

    /* If all initialization steps succeeded, return ESP_OK */
    return ESP_OK;
}

/* Isr  handler for encoder */
void IRAM_ATTR encoder_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    xSemaphoreGiveFromISR(encoder_semaphore, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/* Isr  handler for switch */
void IRAM_ATTR switch_isr_handler(void* arg) {

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    xSemaphoreGiveFromISR(switch_semaphore, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/* This function is periodicly sending command, only because our pwr supply 
    would be reseted to default value if it does not receive any command for 
    some time */
void IRAM_ATTR timer_isr(void *arg)
{
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

/* Error handler*/
void Error_Handler(){
	while (1) { 
		gpio_set_level(GPIO_NUM_48, PIN_STATE_LOW);
		vTaskDelay(pdMS_TO_TICKS(200)); 
		gpio_set_level(GPIO_NUM_48, PIN_STATE_HIGH);
		vTaskDelay(pdMS_TO_TICKS(200)); 
	}
}
