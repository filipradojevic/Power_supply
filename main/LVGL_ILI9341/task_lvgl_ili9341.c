/**
 * @file    task_lvgl_ili9341.c
 * @brief   
 * 
 * @version 1.0.0
 * @date    30.05.2025
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

/*******************************************************************************
 * User variables
 ******************************************************************************/

/* User variables */
extern esp_err_t esp_err;
static bool led_on = false;
extern button_pressed_e button_flag;
static uint32_t last_encoder_activity_time = 0;
static const uint32_t encoder_timeout_ms = 1000;
static send_type_e last_flag_value = SEND_VOLTAGE;

/* LVGL variables */
extern lv_disp_t *global_disp; /* Global Current Active Display */
extern lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
extern lv_disp_drv_t disp_drv;      // contains callback functions
extern esp_lcd_panel_handle_t panel_handle; /* Display that indicate LVGL which one we use */
extern lv_disp_t *disp;
extern ui_objects_t objects;

/* FreeRTOS objects */
extern QueueHandle_t lvgl_voltage_queue;
extern QueueHandle_t lvgl_current_queue;
extern QueueHandle_t lvgl_update_queue;
extern QueueHandle_t lvgl_bolding_update;
extern QueueHandle_t lvgl_button_pressed;
extern QueueSetHandle_t xQueueSetLvgl;

extern SemaphoreHandle_t lvgl_mux;
extern SemaphoreHandle_t watchdog_semaphore;

/*******************************************************************************
 * Main function
 ******************************************************************************/

void task_lvgl_ili9341(void *arg)
{
    QueueSetMemberHandle_t activated_queue = NULL;
    lvgl_data_t lvgl_received;
    send_type_e voltage_current_flag = SEND_VOLTAGE;
    button_pressed_e button_pressed_flag = BUTTON_NOT_PRESSED;
    uint32_t received_value;
    char value_str[16];
    

    while (1) {
        activated_queue = xQueueSelectFromSet(xQueueSetLvgl, portMAX_DELAY);

        if (activated_queue == lvgl_voltage_queue) {
            
            if (xQueueReceive(lvgl_voltage_queue, &received_value, 0) == pdTRUE) {

			    float voltage = ((float)received_value / 1000.0f) - 1.5f;
				
				/* Take the mutex */
			    if (lvgl_lock(-1)) {
					
			        snprintf(value_str, sizeof(value_str), "%.1f", voltage);  // npr. "48.2 V"
			        
			        lv_label_set_text(objects.vol_change, value_str);
        			
        			int16_t arc_val = (int16_t)(voltage * 10.0f);  // Sačuvaj tačnost pre kastovanja
        			
        			lv_arc_set_value(objects.arc3, arc_val);
        			
        			lv_timer_handler();
			        
			        lvgl_unlock();
			    }
			}

        } else if (activated_queue == lvgl_current_queue) {
            
            if (xQueueReceive(lvgl_current_queue, &received_value, 0) == pdTRUE) {
    
				float cur_limit = received_value / 30.0f;
				
				/* Take the mutex */
			    if (lvgl_lock(-1)) {
			        
			        snprintf(value_str, sizeof(value_str), "%.1f", cur_limit);
			        
			        lv_label_set_text(objects.curr_limit_change, value_str);
			        
			        int16_t arc_val = (int16_t)(cur_limit * 10.0f);  // Sačuvaj tačnost pre kastovanja
        			
        			lv_arc_set_value(objects.arc4, arc_val);
        			
			        lv_timer_handler();
			        
			        lvgl_unlock();
			    }
			} 
			
			
        } else if (activated_queue == lvgl_update_queue) {
			  
			  if (xQueueReceive(lvgl_update_queue, &lvgl_received, 0) == pdTRUE) {
			  	
			  	 update_lvgl_display(&lvgl_received,
			  						button_pressed_flag,
			  						voltage_current_flag, 
			  						&button_pressed_flag);
			  						
    		  }
    		
		} else if (activated_queue == lvgl_bolding_update) {
			  if (xQueueReceive(lvgl_bolding_update, &voltage_current_flag, 0) == pdTRUE) {
			     if (lvgl_lock(-1)) {
					
			        update_voltage_current_labels(voltage_current_flag);
			        
			        lv_timer_handler();
			        
			        lvgl_unlock();
			    } 
			 }
			
		} else if (activated_queue == lvgl_button_pressed) {
			  if (xQueueReceive(lvgl_button_pressed, &button_pressed_flag, 0) == pdTRUE) {
			     if (lvgl_lock(-1)) {
					
			        update_voltage_current_change(button_pressed_flag, voltage_current_flag);
			       
			        lv_timer_handler();
			        
			        lvgl_unlock();
			    } 
			 }
    	} else if (activated_queue == watchdog_semaphore) {
			
			xSemaphoreTake(watchdog_semaphore, 0);
	        
	        if (lvgl_lock(-1)) {
	            
				
				display_error(&led_on);
				
	            lv_timer_handler();
	            
	            lvgl_unlock();
      	   }	
				
		} else {
			Error_Handler();
		}
    }
}

void display_error(bool *led_on){
	/* Set LED color to yellow */
	lv_led_set_color(objects.led, lv_color_hex(0xFFFF00));
	
	/* Blink the LED by toggling brightness */
	if (*led_on) {
	    lv_led_set_brightness(objects.led, 255);  /* Full brightness */
	    lv_label_set_text(objects.curr_limit_label_1, "CAN BUS ERROR");
	    lv_label_set_text(objects.effieciency, "");
	} else {
	    lv_led_set_brightness(objects.led, 50);   /* Dimmed brightness */
	    lv_label_set_text(objects.curr_limit_label_1, "");
	    lv_label_set_text(objects.effieciency, "");
	}
	
	/* Toggle LED state */
	*led_on = !(*led_on);
	
}

void update_lvgl_display(const lvgl_data_t *data, button_pressed_e button_flag,
 send_type_e activity_encoder, button_pressed_e *button_pressed_flag) {
    char value_str[32];
    static bool led_state = false;

	/* Take the mutex */
    if (lvgl_lock(-1)) {
        
        if (*button_pressed_flag == BUTTON_NOT_PRESSED){
			snprintf(value_str, sizeof(value_str), "%.1f", data->voltage);
            
            lv_label_set_text(objects.vol_change, value_str);
            
            lv_arc_set_value(objects.obj0, (int)(data->voltage * 10));
            
            snprintf(value_str, sizeof(value_str), "%.1f", data->limit);
       		
       		lv_label_set_text(objects.curr_limit_change, value_str);
           
		}
		/* Power slider */
		int power_value = (int)(data->power);
		lv_color_t power_color = get_scaled_color(power_value, 0, 2900);
		
		lv_slider_set_value(objects.slider_power, power_value, LV_ANIM_OFF);
		lv_obj_set_style_bg_color(objects.slider_power, power_color, LV_PART_INDICATOR);
		lv_obj_set_style_bg_color(objects.slider_power, power_color, LV_PART_KNOB);


		/* Temperature slider */
		int temp_value = (int)(data->temp);
		lv_color_t temp_color = get_scaled_color(temp_value, 10, 70);
		
		lv_slider_set_value(objects.slider_temp, temp_value, LV_ANIM_OFF);
		lv_obj_set_style_bg_color(objects.slider_temp, temp_color, LV_PART_INDICATOR);
		lv_obj_set_style_bg_color(objects.slider_temp, temp_color, LV_PART_KNOB);


		/* Update voltage arc value*/
		lv_arc_set_value(objects.obj0, (int)(data->voltage * 10));
		
        /* Current value*/
        snprintf(value_str, sizeof(value_str), "%.1f", data->current);
        lv_label_set_text(objects.curr_change, value_str);
        lv_arc_set_value(objects.obj1, (int)(data->current * 10));

        /* Temperature value*/
        snprintf(value_str, sizeof(value_str), "%d", (int)data->temp);
        lv_label_set_text(objects.temp_value, value_str);
        lv_slider_set_value(objects.slider_temp, (int)(data->temp), LV_ANIM_OFF);

        /* Power value*/
        snprintf(value_str, sizeof(value_str), "%d", (int)data->power);
        lv_label_set_text(objects.power_value, value_str);
        lv_slider_set_value(objects.slider_power, (int)(data->power), LV_ANIM_OFF);

        /* Efficiency value*/
        snprintf(value_str, sizeof(value_str), "%.1f", data->efficiency);
        lv_label_set_text(objects.curr_limit_label_1, "Efficiency [%]");
        lv_label_set_text(objects.effieciency, value_str);

        /* --- LED TOGGLE --- */
        if (led_state) {
        
            lv_led_set_brightness(objects.led, 180);
        
        } else {
        
            lv_led_set_brightness(objects.led, 0);
        }
        
        /* When it works properly it need to be green color */
        lv_led_set_color(objects.led, lv_color_hex(0xff00ff26));
        
        led_state = !led_state;
		
	    uint32_t now = lv_tick_get();
	
	    if (button_flag == BUTTON_NOT_PRESSED && activity_encoder == last_flag_value) {
	        
	        /* Is the time past over timeout logic? */
	        if ((now - last_encoder_activity_time) > encoder_timeout_ms) {
	            
				/* Deselect the labels */
	            lv_obj_set_style_text_color(objects.voltage_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	            lv_obj_set_style_bg_color(objects.voltage_label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	            lv_obj_set_style_bg_opa(objects.voltage_label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
	            lv_obj_set_style_pad_all(objects.voltage_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	
	            lv_obj_set_style_text_color(objects.voltage_label_1, lv_color_hex(0xFFFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	            lv_obj_set_style_bg_color(objects.voltage_label_1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	            lv_obj_set_style_bg_opa(objects.voltage_label_1, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
	            lv_obj_set_style_pad_all(objects.voltage_label_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	
	            /* Save the time */
	            last_encoder_activity_time = now;
	        }
	    } else {
			
	        /* Update the changes*/
	        last_encoder_activity_time = lv_tick_get();  // Nova "početna" tačka
	        last_flag_value = activity_encoder;
	    }
	
		/* Update the screen */
        lv_timer_handler();
        
        /* Freed up mutex */
        lvgl_unlock();
    }
}

lv_color_t get_scaled_color(int value, int min, int max) {
    if (value < min) value = min;
    if (value > max) value = max;

    // Normalizacija u opsegu 0.0 do 1.0
    float ratio = (float)(value - min) / (float)(max - min);

    // Linearna interpolacija između zelene i crvene
    uint8_t red = (uint8_t)(ratio * 255);
    uint8_t green = (uint8_t)((1.0f - ratio) * 255);
    uint8_t blue = 0;

    return lv_color_make(red, green, blue);
}


void update_voltage_current_change(button_pressed_e flag_change, send_type_e flag_label) {
    if (flag_change == BUTTON_PRESSED) {
		if (flag_label == SEND_VOLTAGE) {    
	        
	        /* Selected voltage - green */
	        lv_obj_set_style_text_color(objects.vol_change, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
	        // Voltage label: selektovan (tamna siva)
	        lv_obj_set_style_text_color(objects.voltage_label, lv_color_hex(0xFFFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	        lv_obj_set_style_bg_color(objects.voltage_label, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DEFAULT);
	        lv_obj_set_style_bg_opa(objects.voltage_label, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
	        lv_obj_set_style_pad_all(objects.voltage_label, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
			}
		else {
			
			/* Selected voltage - green */
	        lv_obj_set_style_text_color(objects.curr_limit_change, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
	        // Current Limit label: selektovan (tamna siva)
	        lv_obj_set_style_text_color(objects.voltage_label_1, lv_color_hex(0xFFFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	        lv_obj_set_style_bg_color(objects.voltage_label_1, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DEFAULT);
	        lv_obj_set_style_bg_opa(objects.voltage_label_1, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
	        lv_obj_set_style_pad_all(objects.voltage_label_1, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
		}  
    } else{
    		
    		/* Deselected voltage - white */
	        lv_obj_set_style_text_color(objects.vol_change, lv_color_hex(0xFFFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	        
	        /* Deselected current - white */
	        lv_obj_set_style_text_color(objects.curr_limit_change, lv_color_hex(0xFFFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    } 
}

void update_voltage_current_labels(send_type_e flag) {
    if (flag == SEND_VOLTAGE) {
        // Voltage label: selektovan (tamna siva)
        lv_obj_set_style_text_color(objects.voltage_label, lv_color_hex(0xFFFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.voltage_label, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(objects.voltage_label, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(objects.voltage_label, 6, LV_PART_MAIN | LV_STATE_DEFAULT);


        lv_obj_set_style_text_color(objects.voltage_label_1, lv_color_hex(0xFFFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.voltage_label_1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(objects.voltage_label_1, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(objects.voltage_label_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (flag == SEND_CURRENT_LIMIT) {
        // Voltage label: deselektovan (svetla siva)
		lv_obj_set_style_text_color(objects.voltage_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.voltage_label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(objects.voltage_label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(objects.voltage_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	
        // Current Limit label: selektovan (tamna siva)
        lv_obj_set_style_text_color(objects.voltage_label_1, lv_color_hex(0xFFFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.voltage_label_1, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(objects.voltage_label_1, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(objects.voltage_label_1, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void set_arc_value_and_color(lv_obj_t *arc, float value, int min, int max) {
    int16_t scaled_value = (int16_t)(value * 10.0f);
    lv_arc_set_value(arc, scaled_value);

    // Clamp vrednost
    if (scaled_value < min) scaled_value = min;
    if (scaled_value > max) scaled_value = max;

    // Izračunaj odnos
    float ratio = (float)(scaled_value - min) / (float)(max - min);

    // Zelena → Žuta → Crvena
    uint8_t r, g;
    if (ratio < 0.5f) {
        r = (uint8_t)(ratio * 2 * 255);
        g = 255;
    } else {
        r = 255;
        g = (uint8_t)((1.0f - (ratio - 0.5f) * 2) * 255);
    }
    lv_color_t arc_color = lv_color_make(r, g, 0);

    // Postavi boju arka
    lv_obj_set_style_arc_color(arc, arc_color, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // Knob boja (opciono)
    lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(arc, 3, LV_PART_KNOB | LV_STATE_DEFAULT);
}

bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    
    lv_disp_flush_ready(disp_driver);
    
    return false;
}

void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    
    /* copy a buffer's content to a specific area of the display */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
void lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) 
    {
    case LV_DISP_ROT_NONE:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);

        break;
    case LV_DISP_ROT_90:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);

        break;
    case LV_DISP_ROT_180:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);

        break;
    case LV_DISP_ROT_270:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);

    	break;    
    default:
    	break;
    }
}



void increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

bool lvgl_lock(int timeout_ms)
{
    /* Convert timeout in milliseconds to FreeRTOS ticks */
    /* If `timeout_ms` is set to -1, the program will block until the condition is met */
    
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}
