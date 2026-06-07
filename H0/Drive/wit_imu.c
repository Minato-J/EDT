#include "wit_imu.h"

// ?例化全局?据?构体
WIT_IMU_Data_t IMU_Data = {0};

/**
 * @brief  ?特智能???字?解析??机
 * @param  data: 串口接收到的??字?
 * @note   ?此函?放入串口接收中?回?中即可
 */
void WIT_Parse_Byte(uint8_t data) 
{
    static uint8_t rx_buffer[11];
    static uint8_t rx_cnt = 0;

    if (rx_cnt == 0 && data != 0x55) {
        return;
    }

    // 2. 存入???
    rx_buffer[rx_cnt++] = data;

    // 3. 收集? 11 ?字?，?始校?和解算
    if (rx_cnt >= 11) 
    {
        rx_cnt = 0; // ?接收下一?做准?

        // 3.1 ?算校?和 (前 10 ?字??加)
        uint8_t sum = 0;
        for (int i = 0; i < 10; i++) {
            sum += rx_buffer[i];
        }
        if (sum != rx_buffer[10]) {
            return; // 校?失?，?明?据在??中受到干扰，直接??本?
        }

        // 3.2 根据包??型(第2?字?)解算?据
        switch(rx_buffer[1]) 
        {
            case 0x51: // 收到加速度包
                IMU_Data.AccX = (int16_t)((rx_buffer[3] << 8) | rx_buffer[2]) / 32768.0f * 16.0f;
                IMU_Data.AccY = (int16_t)((rx_buffer[5] << 8) | rx_buffer[4]) / 32768.0f * 16.0f;
                IMU_Data.AccZ = (int16_t)((rx_buffer[7] << 8) | rx_buffer[6]) / 32768.0f * 16.0f;
                break;

            case 0x52: // 收到角速度包 (重?！)
                IMU_Data.GyroX = (int16_t)((rx_buffer[3] << 8) | rx_buffer[2]) / 32768.0f * 2000.0f;
                IMU_Data.GyroY = (int16_t)((rx_buffer[5] << 8) | rx_buffer[4]) / 32768.0f * 2000.0f;
                IMU_Data.GyroZ = (int16_t)((rx_buffer[7] << 8) | rx_buffer[6]) / 32768.0f * 2000.0f;
                break;

            case 0x53: // 收到角度包
                IMU_Data.Roll  = (int16_t)((rx_buffer[3] << 8) | rx_buffer[2]) / 32768.0f * 180.0f;
                IMU_Data.Pitch = (int16_t)((rx_buffer[5] << 8) | rx_buffer[4]) / 32768.0f * 180.0f;
                IMU_Data.Yaw   = (int16_t)((rx_buffer[7] << 8) | rx_buffer[6]) / 32768.0f * 180.0f;
                break;
                
            default:
                break;
        }
    }
}