#include "task.h"
#include "oled.h"
#include "led_beep.h"
extern volatile uint16_t beep_led_timer;
#include "main.h"      
#include "stdio.h"
#include "wit_imu.h"   
#include "k230_track.h" 
#include "math.h" 
#include "led_beep.h"

extern float target_angle;
extern int16_t base_speed;
extern float Yaw_Offset;
extern float current_angle;

uint16_t selected_task = 1;
uint16_t task_running = 0;
uint8_t count = 0;           
uint8_t last_line_status = 0x00;

void Task_Manager_Init(void) {
    selected_task = 1;
    task_running = 0;
    count = 0;
    last_line_status = 0x00;
}

// ====================================================================
// 按键初始化 (PC12 下拉输入)
// ====================================================================
void key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void Task_Key_Scan(void) 
{
    static uint8_t pb0_state = 0;
    static uint32_t pb0_tick = 0;

    static uint8_t pb1_state = 0;
    static uint32_t pb1_tick = 0;

    // 只有在任务未运行时才允许按键操作
    if (!task_running) 
    {
        // === 切换任务 (PB0) 终极防抖版 ===
        uint8_t pb0_pressed = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET);
        static uint32_t pb0_down_tick = 0;
        static uint8_t pb0_triggered = 0;

        if (pb0_pressed) {
            if (pb0_down_tick == 0) {
                pb0_down_tick = HAL_GetTick(); // 记录首次检测到按下的时间
            } else if (!pb0_triggered && (HAL_GetTick() - pb0_down_tick >= 20)) {
                // 持续按下超过 20ms，确认为有效实体按键，瞬间触发
                pb0_triggered = 1;
                selected_task++;
                if (selected_task > 4) selected_task = 1;
            }
        } else {
            pb0_down_tick = 0;
            pb0_triggered = 0; // 松手后复位
        }

        // === 启动任务 (PC12) 终极防抖发车版 ===
        uint8_t pc12_pressed = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_12) == GPIO_PIN_RESET);
        static uint32_t pc12_down_tick = 0;
        static uint8_t pc12_triggered = 0;

        if (pc12_pressed) {
            if (pc12_down_tick == 0) {
                pc12_down_tick = HAL_GetTick(); 
            } else if (!pc12_triggered && (HAL_GetTick() - pc12_down_tick >= 20)) {
                // 持续按下超过 20ms，果断发车
                pc12_triggered = 1;
                Yaw_Offset = IMU_Data.Yaw; 
                count = 0;
                last_line_status = (line_sensor_data != 0x00) ? 1 : 0;
                task_running = 1; 
            }
        } else {
            pc12_down_tick = 0;
            pc12_triggered = 0;
        }
    }
}

// ====================================================================
// 任务(1)：A点出发，B点停车 (直线)
// ====================================================================
static void Run_Task_1(void) 
{
    if (count == 0) 
    {
        // 刚出发，跑直线 A->B
        base_speed = 60;
    }
    else if (count >= 1) 
    {
        // 遇到第一个边沿（离开B点或者遇到B点横线），立刻停车
        base_speed = 0;
        task_running = 0;
        Led_On();
        Beep_On();
        beep_led_timer = 50;
    }
}


// ====================================================================
// 任务(2)：A->B->C->D->A 全场完整测试 (极简 count 闭环版)
// 核心：全程使用 count 切换状态。在 C 点执行“停车 1 度高精度校准发车”。
// ====================================================================
static void Run_Task_2(void)
{
    static int wait_tick = 0;   
    static int last_count = -1; 
    static uint8_t c_aligned = 0; // 记录 C 点是否完成高精度对准

    // 只要到达新的边缘，就重置计时器和状态
    if (count != last_count)
    {
        wait_tick = 0;
        if (count == 2) c_aligned = 0; // 到达 C 点时重置对准标志
        last_count = count;
        if (count > 0 && count <= 4) {
            Led_On();
            Beep_On();
            beep_led_timer = 50;
        }
    }

    wait_tick++;

    if (count == 0)
    {
        // 【第 0 段】：A -> B 直线段
        base_speed = 60; 
        target_angle = 0.0f; 
    }
    else if (count == 1)
    {
        // 【第 1 段】：到达 B 点，进入 B -> C 弯道
        base_speed = 30;     
        target_angle = -90.0f; 
    }
    else if (count == 2)
    {
        // 【第 2 段】：到达 C 点，停车对准，然后发车 (C -> D 直线)
        target_angle = -180.0f; // 目标物理角度
        
        if (c_aligned == 0) 
        {
            // 还没对准，强制把车停死，靠角度环扭转车头
            base_speed = 0;
            
            // 计算当前角度与 -180 度的真实误差，完美处理 +-180度突变
            float err = current_angle - (-180.0f);
            while (err > 180.0f) err -= 360.0f;
            while (err < -180.0f) err += 360.0f;
            
            // 误差绝对值小于 1 度时，视为完美对准！
            if (err > -1.0f && err < 1.0f) {
                c_aligned = 1; // 锁定对准状态，防止后续因行驶晃动又停车
            }
        }
        else 
        {
            // 误差小于 1 度，校准完毕，果断发车！全力冲刺 C->D
            base_speed = 60;
        }
    }
    else if (count == 3)
    {
        // 【第 3 段】：到达 D 点，进入 D -> A 弯道
        base_speed = 30;
        target_angle = -270.0f; // 再次向右弯曲，引导角 -270
    }
    else if (count >= 4)
    {
        // 【第 4 段】：回到起点 A 点！一圈完美闭环，彻底停车。
        base_speed = 0;
        task_running = 0;
        count = 0;
        wait_tick = 0;
        c_aligned = 0;
    }
}


// ====================================================================
// 任务(3)：单边沿检测测试 (动态角度误差极速发车版)
// 核心：使用 current_state 进行流转，利用 count >= N 的单边沿触发。
// 优化：原地转向不再死等 700ms，只要误差进入 1.5 度瞬间立刻发车，大幅节约时间。
// ====================================================================
int task3_state = 0;

// ====================================================================
// 任务(3)：单边沿检测测试 (动态角度误差极速发车版)
// ====================================================================
static void Run_Task_3(void)
{
    static int wait_tick = 0;
    static uint8_t last_running = 0;

    // 当刚启动任务时复位状态
    if (task_running && !last_running) {
        task3_state = 0;
        wait_tick = 0;
        count = 0;
    }
    last_running = task_running;

    if (!task_running) return;

    switch(task3_state)	
    {
        // ------------------------------------------------------------
        // 状态 0: 起点校准，原地转向 -39.0 度
        // ------------------------------------------------------------
        case 0:
            base_speed = 0;         
            target_angle = -39.0f;  

            wait_tick++;
            float err0 = current_angle - target_angle;
            while (err0 > 180.0f) err0 -= 360.0f;
            while (err0 < -180.0f) err0 += 360.0f;

            if ((err0 > -2.0f && err0 < 2.0f) || wait_tick >= 50)
            {
                count = 0;         
                task3_state = 1;  
                wait_tick = 0;
            }
            break;
            
        // ------------------------------------------------------------
        // 状态 1: 直行 60，直到检测到边沿 (count >= 1)
        // ------------------------------------------------------------
        case 1:
            base_speed = 60;         
            
            if (count >= 1) 
            {  
                base_speed = 0;
                task3_state = 2;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // ------------------------------------------------------------
        // 状态 2: 停车原地转向 0.0 度 (准备进入 B->C 循迹)
        // ------------------------------------------------------------
        case 2:
            base_speed = 0;
            target_angle = 0.0f;    

            wait_tick++;
            float err2 = current_angle - target_angle;
            while (err2 > 180.0f) err2 -= 360.0f;
            while (err2 < -180.0f) err2 += 360.0f;

            // 循迹前不需要太高精度，放宽到 5.0 度，极速发车！增加 2秒超时防卡死
            if ((err2 > -5.0f && err2 < 5.0f) || wait_tick >= 200)
            {
                count = 1;       
                task3_state = 3;  
                wait_tick = 0;
            }
            break;

        // ------------------------------------------------------------
        // 状态 3: 速度 30 循迹，直到检测到边沿 (count >= 2)
        // ------------------------------------------------------------
        case 3:
            base_speed = 30;        
            // 动态将 target_angle 设为“当前车头再往右偏 15 度”，防卡顿
            target_angle = current_angle - 15.0f;

            if (count >= 2) 
            {   
                base_speed = 0;
                task3_state = 4;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // ------------------------------------------------------------
        // 状态 4: 停车原地转向 -144.0 度 (准备盲开冲刺)
        // ------------------------------------------------------------
        case 4:
            base_speed = 0;
            target_angle = -144.0f; 

            wait_tick++;
            float err4 = current_angle - target_angle;
            while (err4 > 180.0f) err4 -= 360.0f;
            while (err4 < -180.0f) err4 += 360.0f;

            // 盲开前必须保证高精度，维持 2.0 度
            if ((err4 > -2.0f && err4 < 2.0f) || wait_tick >= 50)
            {
                count = 2;
                task3_state = 5;  
                wait_tick = 0;
            }
            break;

        // ------------------------------------------------------------
        // 状态 5: 直行 60，直到检测到边沿 (count >= 3)
        // ------------------------------------------------------------
        case 5:
            base_speed = 60;
            if (count >= 3) 
            {
                base_speed = 0;
                task3_state = 6;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // ------------------------------------------------------------
        // 状态 6: 停车原地转向 180.0 度 (准备进入 D->A 循迹)
        // ------------------------------------------------------------
        case 6:
            base_speed = 0;
            target_angle = 180.0f;  

            wait_tick++;
            float err6 = current_angle - target_angle;
            while (err6 > 180.0f) err6 -= 360.0f;
            while (err6 < -180.0f) err6 += 360.0f;

            // 同样，循迹前放宽到 5.0 度，极速发车！增加 2秒超时防卡死
            if ((err6 > -5.0f && err6 < 5.0f) || wait_tick >= 200)
            {
                count = 3;
                task3_state = 7;  
                wait_tick = 0;
            }
            break;

        // ------------------------------------------------------------
        // 状态 7: 速度 30 循迹，直到返回起点 (count >= 4)
        // ------------------------------------------------------------
        case 7:
            base_speed = 30;
            // 同样，在弯道循迹中动态更新找线引导角
            target_angle = current_angle - 15.0f;
            
            if (count >= 4) 
            {
                task3_state = 8;
            
                Led_On();
                Beep_On();
                beep_led_timer = 50;}
            break;

        // ------------------------------------------------------------
        // 状态 8: 任务结束，彻底停车
        // ------------------------------------------------------------
        case 8:
        default:
            base_speed = 0;
            task_running = 0;       
            task3_state = 0;      
            count = 0;
            wait_tick = 0;
            break;
    }
}


// ====================================================================
int task4_state = 0;
// task4_lap 已经不需要了，因为我们使用展开状态机

// ====================================================================
// 任务(4)：多圈平铺测试 (完全解开循环，16个独立状态供你自由微调)
// ====================================================================
static void Run_Task_4(void)
{
    static int wait_tick = 0;
    static uint8_t last_running = 0;

    if (task_running && !last_running) {
        task4_state = 0;
        wait_tick = 0;
        count = 0;
    }
    last_running = task_running;

    if (!task_running) return;

    switch(task4_state)	
    {
        // ======================= 【第一圈】 =======================
        // ------------------------------------------------------------
        // 状态 0: 起点 A 偏转
        // ------------------------------------------------------------
        case 0:
            base_speed = 0;         
            target_angle = -39.0f;  // 第一圈的发车角，你可以微调
            wait_tick++;
            float err0 = current_angle - target_angle;
            while (err0 > 180.0f) err0 -= 360.0f;
            while (err0 < -180.0f) err0 += 360.0f;
            if ((err0 > -2.0f && err0 < 2.0f) || wait_tick >= 200) {
                count = 0; // 强制清零，防止转弯时误触发       
                task4_state = 1;  
                wait_tick = 0;
            }
            break;
            
        // 状态 1: A -> B 盲开
        case 1:
            base_speed = 60;         
            if (count >= 1) {  
                base_speed = 0;
                task4_state = 2;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 2: B 点偏转 (准备进入弯道)
        case 2:
            base_speed = 0;
            target_angle = 0.0f;    // 第一圈 B 点偏转角
            wait_tick++;
            float err2 = current_angle - target_angle;
            while (err2 > 180.0f) err2 -= 360.0f;
            while (err2 < -180.0f) err2 += 360.0f;
            if ((err2 > -5.0f && err2 < 5.0f) || wait_tick >= 200) {
                count = 1; // 强制复位，擦除原地转弯可能产生的误判       
                task4_state = 3;  
                wait_tick = 0;
            }
            break;

        // 状态 3: B -> C 循迹
        case 3:
            base_speed = 30;        
            target_angle = current_angle - 15.0f; 
            if (count >= 2) {   
                base_speed = 0;
                task4_state = 4;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 4: C 点偏转 (准备冲刺)
        case 4:
            base_speed = 0;
            target_angle = -144.0f; 
            wait_tick++;
            float err4 = current_angle - target_angle;
            while (err4 > 180.0f) err4 -= 360.0f;
            while (err4 < -180.0f) err4 += 360.0f;
            if ((err4 > -2.0f && err4 < 2.0f) || wait_tick >= 200) {
                count = 2; // 强制复位
                task4_state = 5;  
                wait_tick = 0;
            }
            break;

        // 状态 5: C -> D 盲开
        case 5:
            base_speed = 60;
            if (count >= 3) {
                base_speed = 0;
                task4_state = 6;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 6: D 点偏转 (准备进入 D->A)
        case 6:
            base_speed = 0;
            target_angle = 180.0f;  
            wait_tick++;
            float err6 = current_angle - target_angle;
            while (err6 > 180.0f) err6 -= 360.0f;
            while (err6 < -180.0f) err6 += 360.0f;
            if ((err6 > -5.0f && err6 < 5.0f) || wait_tick >= 200) {
                count = 3; // 强制复位
                task4_state = 7;  
                wait_tick = 0;
            }
            break;

        // 状态 7: D -> A 循迹
        case 7:
            base_speed = 30;
            target_angle = current_angle - 15.0f; 
            if (count >= 4) { // 到达起点 A，第一圈完成！
                base_speed = 0;
                task4_state = 8; // 直接进入第二圈！
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // ======================= 【第二圈】 =======================
        // ------------------------------------------------------------
        // 状态 8: 起点 A 偏转 (第二圈)
        // ------------------------------------------------------------
        case 8:
            base_speed = 0;         
            target_angle = -36.0f;  // 这里你可以给第二圈单独设置不同的角度！
            wait_tick++;
            float err8 = current_angle - target_angle;
            while (err8 > 180.0f) err8 -= 360.0f;
            while (err8 < -180.0f) err8 += 360.0f;
            if ((err8 > -2.0f && err8 < 2.0f) || wait_tick >= 200) {
                count = 4; // 跑完第一圈 count 是 4。这里强制校准防止误判！
                task4_state = 9;  
                wait_tick = 0;
            }
            break;
            
        // 状态 9: A -> B 盲开 (第二圈)
        case 9:
            base_speed = 60;         
            if (count >= 5) {  
                base_speed = 0;
                task4_state = 10;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 10: B 点偏转 (第二圈)
        case 10:
            base_speed = 0;
            target_angle = 0.0f;    
            wait_tick++;
            float err10 = current_angle - target_angle;
            while (err10 > 180.0f) err10 -= 360.0f;
            while (err10 < -180.0f) err10 += 360.0f;
            if ((err10 > -5.0f && err10 < 5.0f) || wait_tick >= 200) {
                count = 5; 
                task4_state = 11;  
                wait_tick = 0;
            }
            break;

        // 状态 11: B -> C 循迹 (第二圈)
        case 11:
            base_speed = 30;        
            target_angle = current_angle - 15.0f; 
            if (count >= 6) {   
                base_speed = 0;
                task4_state = 12;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 12: C 点偏转 (第二圈)
        case 12:
            base_speed = 0;
            target_angle = -145.0f; 
            wait_tick++;
            float err12 = current_angle - target_angle;
            while (err12 > 180.0f) err12 -= 360.0f;
            while (err12 < -180.0f) err12 += 360.0f;
            if ((err12 > -2.0f && err12 < 2.0f) || wait_tick >= 200) {
                count = 6; 
                task4_state = 13;  
                wait_tick = 0;
            }
            break;

        // 状态 13: C -> D 盲开 (第二圈)
        case 13:
            base_speed = 60;
            if (count >= 7) {
                base_speed = 0;
                task4_state = 14;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 14: D 点偏转 (第二圈)
        case 14:
            base_speed = 0;
            target_angle = 180.0f;  
            wait_tick++;
            float err14 = current_angle - target_angle;
            while (err14 > 180.0f) err14 -= 360.0f;
            while (err14 < -180.0f) err14 += 360.0f;
            if ((err14 > -5.0f && err14 < 5.0f) || wait_tick >= 200) {
                count = 7; 
                task4_state = 15;  
                wait_tick = 0;
            }
            break;

        // 状态 15: D -> A 循迹 (第二圈)
        case 15:
            base_speed = 30;
            target_angle = current_angle - 15.0f; 
            if (count >= 8) { // 第二圈到达 A 点
                base_speed = 0;
                task4_state = 16; // 直接进入第三圈！
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // ======================= 【第三圈】 =======================
        // ------------------------------------------------------------
        // 状态 16: 起点 A 偏转 (第三圈)
        // ------------------------------------------------------------
        case 16:
            base_speed = 0;         
            target_angle = -38.0f;  // 第三圈 A 点发车角
            wait_tick++;
            float err16 = current_angle - target_angle;
            while (err16 > 180.0f) err16 -= 360.0f;
            while (err16 < -180.0f) err16 += 360.0f;
            if ((err16 > -2.0f && err16 < 2.0f) || wait_tick >= 200) {
                count = 8; // 强行校准 count
                task4_state = 17;  
                wait_tick = 0;
            }
            break;
            
        // 状态 17: A -> B 盲开 (第三圈)
        case 17:
            base_speed = 60;         
            if (count >= 9) {  
                base_speed = 0;
                task4_state = 18;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 18: B 点偏转 (第三圈)
        case 18:
            base_speed = 0;
            target_angle = 0.0f;    
            wait_tick++;
            float err18 = current_angle - target_angle;
            while (err18 > 180.0f) err18 -= 360.0f;
            while (err18 < -180.0f) err18 += 360.0f;
            if ((err18 > -5.0f && err18 < 5.0f) || wait_tick >= 200) {
                count = 9; 
                task4_state = 19;  
                wait_tick = 0;
            }
            break;

        // 状态 19: B -> C 循迹 (第三圈)
        case 19:
            base_speed = 30;        
            target_angle = current_angle - 15.0f; 
            if (count >= 10) {   
                base_speed = 0;
                task4_state = 20;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 20: C 点偏转 (第三圈)
        case 20:
            base_speed = 0;
            target_angle = -146.0f; 
            wait_tick++;
            float err20 = current_angle - target_angle;
            while (err20 > 180.0f) err20 -= 360.0f;
            while (err20 < -180.0f) err20 += 360.0f;
            if ((err20 > -2.0f && err20 < 2.0f) || wait_tick >= 200) {
                count = 10; 
                task4_state = 21;  
                wait_tick = 0;
            }
            break;

        // 状态 21: C -> D 盲开 (第三圈)
        case 21:
            base_speed = 60;
            if (count >= 11) {
                base_speed = 0;
                task4_state = 22;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 22: D 点偏转 (第三圈)
        case 22:
            base_speed = 0;
            target_angle = 180.0f;  
            wait_tick++;
            float err22 = current_angle - target_angle;
            while (err22 > 180.0f) err22 -= 360.0f;
            while (err22 < -180.0f) err22 += 360.0f;
            if ((err22 > -5.0f && err22 < 5.0f) || wait_tick >= 200) {
                count = 11; 
                task4_state = 23;  
                wait_tick = 0;
            }
            break;

        // 状态 23: D -> A 循迹 (第三圈)
        case 23:
            base_speed = 30;
            target_angle = current_angle - 15.0f; 
            if (count >= 12) { // 第三圈跑完，进入第四圈！
                base_speed = 0;
                task4_state = 24;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // ======================= 【第四圈】 =======================
        // ------------------------------------------------------------
        // 状态 24: 起点 A 偏转 (第四圈)
        // ------------------------------------------------------------
        case 24:
            base_speed = 0;         
            target_angle = -39.0f;  
            wait_tick++;
            float err24 = current_angle - target_angle;
            while (err24 > 180.0f) err24 -= 360.0f;
            while (err24 < -180.0f) err24 += 360.0f;
            if ((err24 > -2.0f && err24 < 2.0f) || wait_tick >= 200) {
                count = 12; // 强行校准 count
                task4_state = 25;  
                wait_tick = 0;
            }
            break;
            
        // 状态 25: A -> B 盲开 (第四圈)
        case 25:
            base_speed = 60;         
            if (count >= 13) {  
                base_speed = 0;
                task4_state = 26;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 26: B 点偏转 (第四圈)
        case 26:
            base_speed = 0;
            target_angle = 0.0f;    
            wait_tick++;
            float err26 = current_angle - target_angle;
            while (err26 > 180.0f) err26 -= 360.0f;
            while (err26 < -180.0f) err26 += 360.0f;
            if ((err26 > -5.0f && err26 < 5.0f) || wait_tick >= 100) {
                count = 13; 
                task4_state = 27;  
                wait_tick = 0;
            }
            break;

        // 状态 27: B -> C 循迹 (第四圈)
        case 27:
            base_speed = 30;        
            target_angle = current_angle - 15.0f; 
            if (count >= 14) {   
                base_speed = 0;
                task4_state = 28;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 28: C 点偏转 (第四圈)
        case 28:
            base_speed = 0;
            target_angle = -146.0f; 
            wait_tick++;
            float err28 = current_angle - target_angle;
            while (err28 > 180.0f) err28 -= 360.0f;
            while (err28 < -180.0f) err28 += 360.0f;
            if ((err28 > -2.0f && err28 < 2.0f) || wait_tick >= 200) {
                count = 14; 
                task4_state = 29;  
                wait_tick = 0;
            }
            break;

        // 状态 29: C -> D 盲开 (第四圈)
        case 29:
            base_speed = 60;
            if (count >= 15) {
                base_speed = 0;
                task4_state = 30;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // 状态 30: D 点偏转 (第四圈)
        case 30:
            base_speed = 0;
            target_angle = 180.0f;  
            wait_tick++;
            float err30 = current_angle - target_angle;
            while (err30 > 180.0f) err30 -= 360.0f;
            while (err30 < -180.0f) err30 += 360.0f;
            if ((err30 > -5.0f && err30 < 5.0f) || wait_tick >= 200) {
                count = 15; 
                task4_state = 31;  
                wait_tick = 0;
            }
            break;

        // 状态 31: D -> A 循迹 (第四圈)
        case 31:
            base_speed = 30;
            target_angle = current_angle - 15.0f; 
            if (count >= 16) { // 四圈彻底跑完！
                base_speed = 0;
                task4_state = 32;   
                wait_tick = 0;
                Led_On();
                Beep_On();
                beep_led_timer = 50;
            }
            break;

        // ------------------------------------------------------------
        // 状态 32: 任务结束，彻底停车
        // ------------------------------------------------------------
        case 32:
        default:
            base_speed = 0;
            task_running = 0;       
            task4_state = 0;      
            count = 0;
            wait_tick = 0;
            break;
    }
}



// ====================================================================
// 任务调度器
// ====================================================================
void Task_Dispatcher(void) 
{
    if (task_running == 1) 
    {
        switch (selected_task) 
        {
            case 1: Run_Task_1(); break;
            case 2: Run_Task_2(); break;
            case 3: Run_Task_3(); break;
            case 4: Run_Task_4(); break;
            default: 
                task_running = 0; 
                base_speed = 0;
                break;
        }
    }
    else 
    {
        base_speed = 0; // 没启动时强制停车
    }
}
