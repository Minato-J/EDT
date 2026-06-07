#ifndef __PID_H
#define __PID_H

#include "main.h"

// PID ?构体定?
typedef struct {
    float target;     // 目?值
    float Kp, Ki, Kd; // ??
    float error;      // ?前偏差
    float last_error; // 上次偏差
    float integral;   // ?分?
    float output;     // ?出值
    float out_max;    // ?出最大限幅
} PID_TypeDef;

// 函??明
void PID_Init(PID_TypeDef *pid, float p, float i, float d, float max);
float PID_Compute(PID_TypeDef *pid, float target, float measured);

#endif