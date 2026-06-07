#include "k230_track.h"

uint8_t line_sensor_data = 0x00;
uint8_t k230_rx_data = 0;

void K230_Parse_Byte(uint8_t byte) 
{
    static uint8_t state = 0; 
    
    switch(state) 
    {
        case 0:
            if (byte == 0xAA) state = 1; 
            else state = 0;
            break;
        case 1:
            if (byte == 0x55) state = 2; 
            else state = 0;
            break;
        case 2:
            line_sensor_data = byte;  
            state = 0;             
            break;
        default:
            state = 0;
            break;
    }
}

int16_t K230_Get_Turn_Speed(uint8_t sensor_val) 
{
    static int16_t last_turn = 0; // ??�W�@����?�V�A???�ϩR��
    int16_t turn_speed = 0;
    switch(sensor_val) 
    {
    case 0x0C: turn_speed = 0;   break;     // 001100 正中
    
    // 微偏
    case 0x08: turn_speed = 2;   break;     // 001000
    case 0x04: turn_speed = -2;  break;     // 000100
    case 0x18: turn_speed = 3;   break;     // 011000
    case 0x06: turn_speed = -3;  break;     // 000110
    
    // 中度偏
    case 0x1C: turn_speed = 5;   break;     // 011100
    case 0x0E: turn_speed = -5;  break;     // 001110
    case 0x38: turn_speed = 6;  break;     // 111000 
    case 0x07: turn_speed = -6; break;     // 000111 
    
    // 重度偏（单侧内路）
    case 0x10: turn_speed = 8;  break;     // 010000 (削弱防过冲)
    case 0x02: turn_speed = -8; break;     // 000010 (削弱防过冲)
    
    // 极度偏（最外侧边缘）
    case 0x30: turn_speed = 10;  break;     // 110000 👈 (大幅削弱防过冲，原14)
    case 0x20: turn_speed = 12;  break;     // 100000 👈 (大幅削弱防过冲，原18)
    case 0x03: turn_speed = -10; break;     // 000011 👈 (大幅削弱防过冲，原-14)
    case 0x01: turn_speed = -12; break;     // 000001 👈 (大幅削弱防过冲，原-18)
    
    default: 
        turn_speed = last_turn; // 如果读到没在表里的数据（比如噪点、太宽的线），保持上次的转向，而不是突然回正！
        break;

    }
    
    last_turn = turn_speed; 
    return turn_speed;
}