#ifndef __WIT_IMU_H
#define __WIT_IMU_H

#include "main.h"

// 定?一??构体?集中存放解算后的物理量
typedef struct {
    float AccX, AccY, AccZ;       // 加速度 (g)
    float GyroX, GyroY, GyroZ;    // 角速度 (度/秒) —— 你的?向?最需要的?据！
    float Roll, Pitch, Yaw;       // ?拉角 (度)
} WIT_IMU_Data_t;

// 暴露出全局?构体供 main.c ?取
extern WIT_IMU_Data_t IMU_Data;

// 解析函??明
void WIT_Parse_Byte(uint8_t data);

#endif