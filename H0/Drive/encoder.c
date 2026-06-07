#include "encoder.h"
#include "tim.h"

void Encoder_Init(void) {
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL); 
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL); 
}

int16_t Encoder_Get_Count_Left(void) {
    int16_t cnt;
    cnt = (int16_t)__HAL_TIM_GET_COUNTER(&htim2);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    return cnt;
}

int16_t Encoder_Get_Count_Right(void) {
    int16_t cnt;
    cnt = (int16_t)__HAL_TIM_GET_COUNTER(&htim4);
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    return cnt;
}