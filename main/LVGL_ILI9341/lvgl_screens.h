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
	lv_obj_t *label1;
	lv_obj_t *label2;
	lv_obj_t *power1_label;
	lv_obj_t *power1_value;
	lv_obj_t *power2_label;
	lv_obj_t *power2_value;
	lv_obj_t *power3_label;
	lv_obj_t *power3_value;
	lv_obj_t *temp1_label;
	lv_obj_t *temp2_label;
	lv_obj_t *temp3_label;
	lv_obj_t *temp1_value;
	lv_obj_t *temp2_value;
	lv_obj_t *temp3_value;
	lv_obj_t *curr1_label;
	lv_obj_t *curr1_value;
	lv_obj_t *curr2_label;
	lv_obj_t *curr2_value;
	lv_obj_t *curr3_label;
	lv_obj_t *curr3_value;
	lv_obj_t *curr_limit_change;
	lv_obj_t *curr_change;
	lv_obj_t *voltage_label_1;
	lv_obj_t *curr_limit_label_1;
	lv_obj_t *slider_efficiency;
	lv_obj_t *slider_power;
	lv_obj_t *slider_temp;
	lv_obj_t *slider_limit_curr;
	lv_obj_t *led1;
	lv_obj_t *led2;
	lv_obj_t *led3;
	lv_obj_t *effieciency;
	lv_obj_t *arc3; // kazaljka za prvi luk
	lv_obj_t *arc4; // kazaljka za drugi luk
} ui_objects_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

#endif /* LVGL_SCREENS_H_ */
