idf_component_register(SRCS 
							"APP/main.c"
							"CAN_BUS/task_can_receive.c"
							"CAN_BUS/task_pwr_supply.c" 
							"CAN_BUS/task_can_watchdog.c" 
							"ENCODER/task_encoder.c" 
							"LVGL_ILI9341/task_lvgl_ili9341.c"
							"LVGL_ILI9341/lvgl_screens.c"
                       
                       INCLUDE_DIRS
                       		 "."
                       		 "APP/."
                       		 "CAN_BUS/."
                       		 "ENCODER/."
                       		 "LVGL_ILI9341/."
                       )
