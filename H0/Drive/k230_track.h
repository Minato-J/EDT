#ifndef __K230_TRACK_H
#define __K230_TRACK_H

#include "stdint.h"

extern uint8_t line_sensor_data; 
extern uint8_t k230_rx_data;      
void K230_Parse_Byte(uint8_t byte);
int16_t K230_Get_Turn_Speed(uint8_t sensor_val);

#endif