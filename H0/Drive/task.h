#ifndef __TASK_H
#define __TASK_H

#include "stdint.h"

// ���S? main.c �M��L���ϥΪ�����?�q
extern uint16_t selected_task;
extern uint16_t task_running;
extern uint8_t current_state;
extern uint8_t last_line_status;
extern uint8_t count;
extern float current_angle;

// �֤ߨ�??��
void Task_Manager_Init(void); // ��?�޲z����l�� (?��?�Τ@��)
void Task_Key_Scan(void);
void key_Init(void);     // ��??�y��? (��b while(1) ��)
void Task_Dispatcher(void);   // ��?��?�� (��b while(1) ��)

#endif