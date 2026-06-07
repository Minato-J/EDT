#include "pid.h"

void PID_Init(PID_TypeDef *pid, float p, float i, float d, float max) {
    pid->Kp = p; pid->Ki = i; pid->Kd = d;
    pid->out_max = max;
    pid->target = 0; pid->error = 0; pid->last_error = 0;
    pid->integral = 0; pid->output = 0;
}

float PID_Compute(PID_TypeDef *pid, float target, float measured) {
    pid->target = target;
    pid->error = pid->target - measured;
    
    // ?分累加
    pid->integral += pid->error;
    
    // ??的?分抗?和（限幅）
    if (pid->integral > pid->out_max) pid->integral = pid->out_max;
    if (pid->integral < -pid->out_max) pid->integral = -pid->out_max;
    
    // ?算?出
    pid->output = (pid->Kp * pid->error) + 
                  (pid->Ki * pid->integral) + 
                  (pid->Kd * (pid->error - pid->last_error));
    
    pid->last_error = pid->error;
    
    // ?出限幅 (适配你的 ARR=100)
    if (pid->output > pid->out_max) pid->output = pid->out_max;
    if (pid->output < -pid->out_max) pid->output = -pid->out_max;
    
    return pid->output;
}