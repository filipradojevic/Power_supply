/**
 * @file    main.c
 * @brief   Main application file.
 *          Initializes all peripherals: CAN, LED, Encoder, Ili9341 display and
 * LVGL. Handles encoder interrupt, processing and starts relevant tasks.
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
#include "lvgl_screens.h"
#include "main.h"
#include "task_can_receive.h"
#include "task_encoder.h"
#include "task_lvgl_ili9341.h"
#include "task_pwr_supply.h"

/*******************************************************************************
 * Defines
 ******************************************************************************/

/* FreeRTOS defines */
#define CAN_QUEUE_MAX_SIZE 128
#define QUEUE_SET_LENGTH 3
#define QUEUE_SET_LVGL_LENGHT 3
#define MIN_PRIO_TASK 1
#define MEDIUM_LOW_PRIO_TASK 2
#define MEDIUM_PRIO_TASK 3
#define MEDIUM_HIGH_PRIO_TASK 4
#define MAX_PRIO_TASK 5

/* UART Configuration */
#define UART_NUM UART_NUM_0
#define UART_TX_PIN GPIO_NUM_43
#define UART_RX_PIN GPIO_NUM_44
#define UART_BAUD_RATE 115200
#define UART_BUF_SIZE 1024

/**************************
 *	***************************************************** User variables
 ******************************************************************************/

/* User variables */
esp_err_t esp_err;

/* LVGL variables */
lv_disp_t *global_disp; /* Global Current Active Display */
lv_disp_draw_buf_t
	disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
lv_disp_drv_t disp_drv; // contains callback functions
lv_disp_t *disp = NULL;
ui_objects_t objects;

/* Display that indicate LVGL which one we use */
esp_lcd_panel_handle_t panel_handle = NULL;

/* FreeRTOS objects */
QueueHandle_t xQueueCan = NULL;
QueueSetHandle_t xQueueSetEncoder = NULL;
QueueSetHandle_t xQueueSetLvgl = NULL;
QueueHandle_t lvgl_voltage_queue = NULL;
QueueHandle_t lvgl_current_queue = NULL;
QueueHandle_t lvgl_update_queue = NULL;
QueueHandle_t lvgl_bolding_update = NULL;
QueueHandle_t lvgl_button_pressed = NULL;
SemaphoreHandle_t lvgl_mux = NULL;
SemaphoreHandle_t encoder_semaphore = NULL;
SemaphoreHandle_t switch_semaphore = NULL;
SemaphoreHandle_t command_semaphore = NULL;
SemaphoreHandle_t watchdog_semaphore = NULL;

/*******************************************************************************
 * Prototyp of functions
 ******************************************************************************/

///* Initialization prototypes */
static esp_err_t hardware_init(void);
static esp_err_t rtos_objects_init(void);
static esp_err_t led_initialization(void);
static esp_err_t twai_initialization(void);
static esp_err_t encoder_initialization(void);
static esp_err_t ili9341_disp_initialization(void);
static esp_err_t lvgl_initialization(void);
static esp_err_t uart_init(void);

/* LVGL Screens */
extern void create_intro_ui(lv_disp_t *disp);
extern void create_main_ui(lv_disp_t *disp);

/* LVGL functions */
extern bool lvgl_lock(int timeout_ms);
extern void lvgl_unlock(void);

/* Task prototypes */
void task_pwr_supply(void *arg);
void task_can_receive(void *arg);
void task_encoder(void *arg);
void task_lvgl_ili9341(void *arg);
void task_can_watchdog(void *arg);

void switch_isr_handler(void *arg);
void encoder_isr_handler(void *arg);

/*******************************************************************************
 * Main function
 ******************************************************************************/

void app_main() {
	/** Initialize hardware peripherals */
	if (hardware_init() != ESP_OK) {
		Error_Handler();
	}

	/** Initialize FreeRTOS objects: queues, semaphores, mutexs */
	if (rtos_objects_init() != ESP_OK) {
		Error_Handler();
	}

	/* Lock the mutex due to the LVGL APIs are not thread-safe */
	if (lvgl_lock(-1)) {
		create_main_ui(disp);
		lvgl_unlock();
	}

	/* Calling LVGL handler to update the screen */
	if (lvgl_lock(-1)) {
		lv_timer_handler();
		lvgl_unlock();
	}

	/* Creating RTOS TASKS */
	if (xTaskCreate(task_lvgl_ili9341, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE,
					NULL, MEDIUM_PRIO_TASK, NULL) != pdPASS) {
		Error_Handler();
	}

	if (xTaskCreate(task_encoder, "Encoder sending Commands", 2048, NULL,
					MEDIUM_PRIO_TASK, NULL) != pdPASS) {
		Error_Handler();
	}

	if (xTaskCreate(task_can_receive, "Can Receive Task", 2048, NULL,
					MEDIUM_LOW_PRIO_TASK, NULL) != pdPASS) {
		Error_Handler();
	}

	if (xTaskCreate(task_pwr_supply, "CAN Processing", 8192, NULL,
					MEDIUM_LOW_PRIO_TASK, NULL) != pdPASS) {
		Error_Handler();
	}

	if (xTaskCreate(task_can_watchdog, "Watchdog Can Task", 2048, NULL,
					MEDIUM_HIGH_PRIO_TASK, NULL) != pdPASS) {
		Error_Handler();
	}

	/* Fallback loop, it should never be entered */
	while (1) {
		Error_Handler();
	}
}

/* Initialize hardware: LED GPIO, TWAI (CAN) */
static esp_err_t hardware_init(void) {
	if (led_initialization() != ESP_OK) {
		return ESP_FAIL;
	}

	if (twai_initialization() != ESP_OK) {
		return ESP_FAIL;
	}

	if (encoder_initialization() != ESP_OK) {
		return ESP_FAIL;
	}

	if (ili9341_disp_initialization() != ESP_OK) {
		return ESP_FAIL;
	}

	if (lvgl_initialization() != ESP_OK) {
		return ESP_FAIL;
	}

	if (uart_init() != ESP_OK) {
		return ESP_FAIL;
	}

	return ESP_OK;
}

static esp_err_t uart_init(void) {

	uart_config_t uart_config = {
		.baud_rate = UART_BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));

	// GPIO 43 = TX, GPIO 44 = RX
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN,
								 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

	ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE, UART_BUF_SIZE,
										0, NULL, 0));

	// ESP_LOGI("UART", "Initialized on TX:%d RX:%d at %d baud", UART_TX_PIN,
	// 		 UART_RX_PIN, UART_BAUD_RATE);

	return ESP_OK;
}

/* Initialize FreeRTOS objects: CAN queue, semaphores, queue set */
static esp_err_t rtos_objects_init(void) {
	/* CAN Queue */
	xQueueCan = xQueueCreate(CAN_QUEUE_MAX_SIZE, sizeof(twai_message_t));
	if (xQueueCan == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	/* Encoder Queues */
	encoder_semaphore = xSemaphoreCreateBinary();
	if (encoder_semaphore == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	switch_semaphore = xSemaphoreCreateBinary();
	if (switch_semaphore == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	command_semaphore = xSemaphoreCreateBinary();
	if (command_semaphore == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	/* LVGL Queues */
	lvgl_voltage_queue = xQueueCreate(CAN_QUEUE_MAX_SIZE, sizeof(uint32_t));
	if (lvgl_voltage_queue == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	lvgl_current_queue = xQueueCreate(CAN_QUEUE_MAX_SIZE, sizeof(uint32_t));
	if (lvgl_current_queue == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	lvgl_update_queue =
		xQueueCreate(CAN_QUEUE_MAX_SIZE, sizeof(lvgl_bonded_data_t));
	if (lvgl_update_queue == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	lvgl_bolding_update = xQueueCreate(CAN_QUEUE_MAX_SIZE, sizeof(send_type_e));
	if (lvgl_bolding_update == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	lvgl_button_pressed =
		xQueueCreate(CAN_QUEUE_MAX_SIZE, sizeof(button_pressed_e));
	if (lvgl_button_pressed == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	watchdog_semaphore = xSemaphoreCreateBinary();
	if (watchdog_semaphore == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	/* Creating Recursive Mutex bcs LVGL library is not THREAD-SAFE! */
	lvgl_mux = xSemaphoreCreateRecursiveMutex();
	if (lvgl_mux == NULL) {
		gpio_set_level(48, PIN_STATE_HIGH);
		return ESP_FAIL;
	}

	/* CAN Queue Set */
	xQueueSetEncoder = xQueueCreateSet(QUEUE_SET_LENGTH);

	if (xQueueSetEncoder == NULL) {
		return ESP_FAIL;
	}

	if (xQueueAddToSet(encoder_semaphore, xQueueSetEncoder) != pdPASS) {
		return ESP_FAIL;
	}

	if (xQueueAddToSet(switch_semaphore, xQueueSetEncoder) != pdPASS) {
		return ESP_FAIL;
	}

	if (xQueueAddToSet(command_semaphore, xQueueSetEncoder) != pdPASS) {
		return ESP_FAIL;
	}

	/* L Queue Set */
	xQueueSetLvgl = xQueueCreateSet(QUEUE_SET_LVGL_LENGHT);

	if (xQueueSetLvgl == NULL) {
		return ESP_FAIL;
	}

	if (xQueueAddToSet(lvgl_voltage_queue, xQueueSetLvgl) != pdPASS) {
		return ESP_FAIL;
	}

	if (xQueueAddToSet(lvgl_current_queue, xQueueSetLvgl) != pdPASS) {
		return ESP_FAIL;
	}

	if (xQueueAddToSet(lvgl_update_queue, xQueueSetLvgl) != pdPASS) {
		return ESP_FAIL;
	}

	if (xQueueAddToSet(watchdog_semaphore, xQueueSetLvgl) != pdPASS) {
		return ESP_FAIL;
	}

	if (xQueueAddToSet(lvgl_bolding_update, xQueueSetLvgl) != pdPASS) {
		return ESP_FAIL;
	}

	if (xQueueAddToSet(lvgl_button_pressed, xQueueSetLvgl) != pdPASS) {
		return ESP_FAIL;
	}

	return ESP_OK;
}

/* Initialize hardware: Led */
static esp_err_t led_initialization(void) {
	if (gpio_reset_pin(48) == ESP_FAIL) {
		return ESP_FAIL;
	}

	if (gpio_set_direction(48, GPIO_MODE_OUTPUT) == ESP_FAIL) {
		return ESP_FAIL;
	}

	return ESP_OK;
}

/* Initialize hardware: Twai */
static esp_err_t twai_initialization(void) {
	/* Initialize the structure of a twai peripheral */
	twai_general_config_t g_config =
		TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_7, GPIO_NUM_6, TWAI_MODE_NORMAL);
	twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
	twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

	if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_FAIL) {
		return ESP_FAIL;
	}
	if (twai_start() == ESP_FAIL) {
		return ESP_FAIL;
	}

	// TODO Process other alert flags
	uint32_t alert_flags =
		TWAI_ALERT_RX_DATA | TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_ERROR;

	if (twai_reconfigure_alerts(alert_flags, NULL) != ESP_OK) {
		return ESP_FAIL;
	}

	return ESP_OK;
}

/* Initialize hardware: Rotary encoder */
static esp_err_t encoder_initialization(void) {
	esp_err_t ret;

	/* Set switch pin as input */
	ret = gpio_set_direction(ENCODER_SW_PIN, GPIO_MODE_INPUT);
	if (ret != ESP_OK)
		return ret;

	/* Set DT pin as input */
	ret = gpio_set_direction(ENCODER_DT_PIN, GPIO_MODE_INPUT);
	if (ret != ESP_OK)
		return ret;

	/* Set CLK pin as input */
	ret = gpio_set_direction(ENCODER_CLK_PIN, GPIO_MODE_INPUT);
	if (ret != ESP_OK)
		return ret;

	/* Enable internal pull-up resistors for encoder pins */
	ret = gpio_pullup_en(ENCODER_DT_PIN);
	if (ret != ESP_OK)
		return ret;

	ret = gpio_pullup_en(ENCODER_CLK_PIN);
	if (ret != ESP_OK)
		return ret;

	ret = gpio_pullup_en(ENCODER_SW_PIN);
	if (ret != ESP_OK)
		return ret;

	/* Install GPIO ISR service with default configuration */
	ret = gpio_install_isr_service(0);
	if (ret != ESP_OK)
		return ret;

	/* Configure interrupt on rising edge for encoder CLK pin */
	ret = gpio_set_intr_type(ENCODER_CLK_PIN, GPIO_INTR_POSEDGE);
	if (ret != ESP_OK)
		return ret;

	/* Attach ISR handler for encoder CLK pin */
	ret = gpio_isr_handler_add(ENCODER_CLK_PIN, encoder_isr_handler, NULL);
	if (ret != ESP_OK)
		return ret;

	/* Configure interrupt on falling edge for encoder switch pin */
	ret = gpio_set_intr_type(ENCODER_SW_PIN, GPIO_INTR_NEGEDGE);
	if (ret != ESP_OK)
		return ret;

	/* Attach ISR handler for encoder switch pin */
	ret = gpio_isr_handler_add(ENCODER_SW_PIN, switch_isr_handler, NULL);
	if (ret != ESP_OK)
		return ret;

	/* If all initialization steps succeeded, return ESP_OK */
	return ESP_OK;
}

/* Initialize hardware: Ili9341 display */
static esp_err_t ili9341_disp_initialization(void) {
	/* Initialize background light pin with driver/gpio.h */
	gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT,
									.pin_bit_mask =
										1ULL << EXAMPLE_PIN_NUM_BK_LIGHT};

	if (gpio_config(&bk_gpio_config) != ESP_OK) {
		return ESP_FAIL;
	}

	/* Initialize SPI BUS magistral with driver/spi_master*/
	spi_bus_config_t buscfg = {
		.sclk_io_num = EXAMPLE_PIN_NUM_SCLK,
		.mosi_io_num = EXAMPLE_PIN_NUM_MOSI,
		.miso_io_num = EXAMPLE_PIN_NUM_MISO,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),
	};

	if (spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) {
		return ESP_FAIL;
	}

	/* Initialize SPI Panel with esp_lcd */
	/* This initialize DC/CS pins SPI mode, how big are cmd
	   bits, how big are parameter bits etc... */
	esp_lcd_panel_io_handle_t io_handle = NULL;
	esp_lcd_panel_io_spi_config_t io_config = {
		.dc_gpio_num = EXAMPLE_PIN_NUM_LCD_DC,
		.cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
		.pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
		.lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
		.lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
		.spi_mode = 0,
		.trans_queue_depth = 10,
		.on_color_trans_done = notify_lvgl_flush_ready,
		.user_ctx = &disp_drv,
	};
	if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config,
								 &io_handle) != ESP_OK) {
		return ESP_FAIL;
	}

	/* This installs the driver for ili9341 */
	esp_lcd_panel_dev_config_t panel_config = {
		.reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
		.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
		.bits_per_pixel = 16,
	};

	if (esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle) !=
		ESP_OK) {
		return ESP_FAIL;
	}
	if (esp_lcd_panel_reset(panel_handle) != ESP_OK) {
		return ESP_FAIL;
	}
	if (esp_lcd_panel_init(panel_handle) != ESP_OK) {
		return ESP_FAIL;
	}
	if (esp_lcd_panel_mirror(panel_handle, true, false) != ESP_OK) {
		return ESP_FAIL;
	}
	if (esp_lcd_panel_disp_on_off(panel_handle, true) != ESP_OK) {
		return ESP_FAIL;
	}

	/* Turning on the background light */
	if (gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT,
					   EXAMPLE_LCD_BK_LIGHT_ON_LEVEL) != ESP_OK) {
		return ESP_FAIL;
	}

	return ESP_OK;
}

/* Initialize hardware: LVGL */
static esp_err_t lvgl_initialization(void) {

	/* Initialize the LVGL library */
	lv_init();

	/* alloc draw buffers used by LVGL */
	/* it's recommended to choose the size of the draw buffer(s) to be at
	 * least 1/10 screen sized */
	lv_color_t *buf1 = heap_caps_malloc(
		EXAMPLE_LCD_H_RES * 30 * sizeof(lv_color_t), MALLOC_CAP_DMA);
	assert(buf1);
	lv_color_t *buf2 = heap_caps_malloc(
		EXAMPLE_LCD_H_RES * 30 * sizeof(lv_color_t), MALLOC_CAP_DMA);
	assert(buf2);

	/* initialize LVGL draw buffers */
	lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 30);

	/* Initialize the driver with callbacks, sizes of screen, buffers,
	 * handlers
	 */
	lv_disp_drv_init(&disp_drv);
	disp_drv.hor_res = EXAMPLE_LCD_H_RES;
	disp_drv.ver_res = EXAMPLE_LCD_V_RES;
	disp_drv.flush_cb = lvgl_flush_cb;
	disp_drv.drv_update_cb = lvgl_port_update_callback;
	disp_drv.draw_buf = &disp_buf;
	disp_drv.user_data = panel_handle;
	disp = lv_disp_drv_register(&disp_drv);

	/* Installing LVGL timer */
	/* Tick interface for LVGL (using esp_timer to generate 2ms periodic
	 * event)
	 */
	const esp_timer_create_args_t lvgl_tick_timer_args = {
		.callback = &increase_lvgl_tick, .name = "lvgl_tick"};

	/* Initialize timer handler */
	esp_timer_handle_t lvgl_tick_timer = NULL;
	if (esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer) != ESP_OK) {
		return ESP_FAIL;
	}

	if (esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS *
													  1000) != ESP_OK) {
		return ESP_FAIL;
	}

	return ESP_OK;
}

/* Isr  handler for encoder */
void IRAM_ATTR encoder_isr_handler(void *arg) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	xSemaphoreGiveFromISR(encoder_semaphore, &xHigherPriorityTaskWoken);

	if (xHigherPriorityTaskWoken) {
		portYIELD_FROM_ISR();
	}
}

/* Isr  handler for switch */
void IRAM_ATTR switch_isr_handler(void *arg) {

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	xSemaphoreGiveFromISR(switch_semaphore, &xHigherPriorityTaskWoken);

	if (xHigherPriorityTaskWoken) {
		portYIELD_FROM_ISR();
	}
}

/* Error handler*/
void Error_Handler() {
	while (1) {
		gpio_set_level(48, PIN_STATE_LOW);
		vTaskDelay(pdMS_TO_TICKS(200));
		gpio_set_level(48, PIN_STATE_HIGH);
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}
