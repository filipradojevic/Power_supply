/**
 * @file    lvgl_screens.h
 * @brief   
 * @version 1.0.0
 * @date    30.05.2025
 * @author  LisumLab
 */


#ifndef LVGL_SCREENS_H_
#define LVGL_SCREENS_H_


/******************************************************************************* 
 * Includes 
 ******************************************************************************/

/* LVGL Library */
#include "lvgl.h"
/******************************************************************************* 
 * Defines 
 ******************************************************************************/

/******************************************************************************* 
 * Structures
 ******************************************************************************/
typedef struct {
	lv_obj_t *intro;
	lv_obj_t *main;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
	lv_obj_t *obj3;
    lv_obj_t *voltage_label;
    lv_obj_t *curr_limit_label;
    lv_obj_t *vol_change;
    lv_obj_t *power_label;
    lv_obj_t *power_value;
    lv_obj_t *temp_label;
    lv_obj_t *temp_value;
    lv_obj_t *curr_limit_change;
    lv_obj_t *curr_change;
    lv_obj_t *voltage_label_1;
    lv_obj_t *curr_limit_label_1;
    lv_obj_t *slider_efficiency;
    lv_obj_t *slider_power;
    lv_obj_t *slider_temp;
    lv_obj_t *slider_limit_curr;
    lv_obj_t *led;
    lv_obj_t *effieciency;
} ui_objects_t;

/******************************************************************************* 
 * Prototypes
 ******************************************************************************/


#endif /* LVGL_SCREENS_H_ */
