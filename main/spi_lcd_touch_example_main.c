#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

#include "ui/screens.h"


#include "esp_lcd_ili9341.h"

static esp_err_t ili9341_disp_initialization();
void Error_Handler();
esp_err_t fill_screen_color(esp_lcd_panel_handle_t panel_handle, uint16_t color);
void test_colors(esp_lcd_panel_handle_t panel_handle);

// Using SPI2 in the example
#define LCD_HOST  SPI2_HOST

#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_SCLK           19
#define EXAMPLE_PIN_NUM_MOSI           12
#define EXAMPLE_PIN_NUM_MISO           13
#define EXAMPLE_PIN_NUM_LCD_DC         21
#define EXAMPLE_PIN_NUM_LCD_RST        18
#define EXAMPLE_PIN_NUM_LCD_CS         14
#define EXAMPLE_PIN_NUM_BK_LIGHT       5
#define EXAMPLE_PIN_NUM_TOUCH_CS       15


#define EXAMPLE_LCD_H_RES              240
#define EXAMPLE_LCD_V_RES              320


#define PIN_STATE_LOW  0
#define PIN_STATE_HIGH 1

// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2


/* Colors of display */
// Color definitions
#define ILI9341_BLACK 0x0000       ///<   0,   0,   0
#define ILI9341_NAVY 0x000F        ///<   0,   0, 123
#define ILI9341_DARKGREEN 0x03E0   ///<   0, 125,   0
#define ILI9341_DARKCYAN 0x03EF    ///<   0, 125, 123
#define ILI9341_MAROON 0x7800      ///< 123,   0,   0
#define ILI9341_PURPLE 0x780F      ///< 123,   0, 123
#define ILI9341_OLIVE 0x7BE0       ///< 123, 125,   0
#define ILI9341_LIGHTGREY 0xC618   ///< 198, 195, 198
#define ILI9341_DARKGREY 0x7BEF    ///< 123, 125, 123
#define ILI9341_BLUE 0x001F        ///<   0,   0, 255
#define ILI9341_GREEN 0x07E0       ///<   0, 255,   0
#define ILI9341_CYAN 0x07FF        ///<   0, 255, 255
#define ILI9341_RED 0xF800         ///< 255,   0,   0
#define ILI9341_MAGENTA 0xF81F     ///< 255,   0, 255
#define ILI9341_YELLOW 0xFFE0      ///< 255, 255,   0
#define ILI9341_WHITE 0xFFFF       ///< 255, 255, 255
#define ILI9341_ORANGE 0xFD20      ///< 255, 165,   0
#define ILI9341_GREENYELLOW 0xAFE5 ///< 173, 255,  41
#define ILI9341_PINK 0xFC18        ///< 255, 130, 198
esp_lcd_panel_handle_t panel_handle = NULL;

// DMA-aligned statički bafer u RAM-u
// Veličina: širina * visina ekrana * 2 bajta (jer je RGB565 = 16 bita = 2 bajta)
static uint16_t color_buffer[EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES] __attribute__((aligned(4))) = {0};


esp_err_t fill_screen_color(esp_lcd_panel_handle_t panel_handle, uint16_t color)
{
    size_t pixels_count = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES;

    for (size_t i = 0; i < pixels_count; i++) {
        color_buffer[i] = color;
    }

    if (esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, EXAMPLE_LCD_H_RES,
    													 EXAMPLE_LCD_V_RES, color_buffer) != ESP_OK){
    	
    	return ESP_FAIL;
    }
    
    return ESP_OK;
}

void test_colors(esp_lcd_panel_handle_t panel_handle) {
    while (1) {
        fill_screen_color(panel_handle, ILI9341_BLACK);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_NAVY);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_DARKGREEN);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_DARKCYAN);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_MAROON);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_PURPLE);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_OLIVE);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_LIGHTGREY);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_DARKGREY);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_BLUE);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_GREEN);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_CYAN);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_RED);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_MAGENTA);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_YELLOW);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_WHITE);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_ORANGE);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_GREENYELLOW);
        vTaskDelay(pdMS_TO_TICKS(1000));

        fill_screen_color(panel_handle, ILI9341_PINK);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void app_main(void)
{
	
	if (ili9341_disp_initialization() != ESP_OK) {
        Error_Handler();
    }

    test_colors(panel_handle);
}

static esp_err_t ili9341_disp_initialization(){
	/* Initialize background light pin with driver/gpio.h */
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    
    if (gpio_config(&bk_gpio_config) != ESP_OK){
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
    
    if (spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK){
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
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
    };
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle) != ESP_OK){
		return ESP_FAIL;
	}

    /* This installs the driver for ili9341 */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    
    if (esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle) != ESP_OK){
		return ESP_FAIL;
	}
    if (esp_lcd_panel_reset(panel_handle) != ESP_OK){
		return ESP_FAIL;
	}
    if (esp_lcd_panel_init(panel_handle) != ESP_OK){
		return ESP_FAIL;
	}
    if (esp_lcd_panel_mirror(panel_handle, true, false) != ESP_OK){
		return ESP_FAIL;
	}
    if (esp_lcd_panel_disp_on_off(panel_handle, true) != ESP_OK){
		return ESP_FAIL;
	}

    /* Turning on the background light */
    if (gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL) != ESP_OK){
		return ESP_FAIL;
	}
    
	return ESP_OK;
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

