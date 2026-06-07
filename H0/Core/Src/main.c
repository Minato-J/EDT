/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Motor.h"
#include "Encoder.h"
#include "pid.h"
#include "stdio.h"
#include "wit_imu.h"
#include <math.h>
#include "k230_track.h"
#include "task.h"
#include "oled.h"
#include "led_beep.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// ==== Uart2 & Uart3 ===
uint8_t rx_data;

// ==== Pid para === 
PID_TypeDef pid_left;
PID_TypeDef pid_right;  

// ====  pid ==== 
int16_t target_v_left = 0;  // 左轮独立目标
int16_t target_v_right = 0; // 右轮独立目标
int16_t current_v_left = 0;
int16_t current_v_right = 0; 

//=== 角度环 === 
PID_TypeDef pid_angle;      
float Yaw_Offset = 0;        
float target_angle = 0;    
float current_angle = 0;
int16_t base_speed = 0;    
float turn_out;
volatile uint16_t beep_led_timer = 0;
extern uint8_t lap_count;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
    //HAL_Delay(500);  // == 角度校准
	Encoder_Init();
	Motor_Init();
	//Motor_SetSpeed_B(-10);
	//Motor_SetSpeed_A(-10);
	OLED_Init();
	OLED_Clear();
	OLED_ShowString(0, 0, (uint8_t *)"Task:", 16, 1);
	OLED_ShowString(0, 16, (uint8_t *)"startflag:", 16, 1);
	OLED_ShowString(0, 32, (uint8_t *)"count:", 16, 1);
	OLED_Refresh();
	PID_Init(&pid_left,0.9f, 0.12f, 0.0f, 500.0f);  // == lelf
	PID_Init(&pid_right, 0.8f, 0.12f, 0.0f, 500.0f);  // == right
	PID_Init(&pid_angle, 0.8f, 0.0f, 0.9f, 40.0f); // == angle
	led_beep_init();
	Task_Manager_Init();
	key_Init();
	HAL_UART_Receive_IT(&huart2, &rx_data, 1);    
   HAL_UART_Receive_IT(&huart3, &k230_rx_data, 1);
   HAL_TIM_Base_Start_IT(&htim6);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  // == 串口打印数据进行测试
	               //printf("%.2f,%d,%d\n", current_angle, current_v_left, current_v_right);
				  //printf("%.2f\n",IMU_Data.Yaw);
					//HAL_Delay(10)；
	   //printf("xrf%.2f,%.2f\n",IMU_Data.Yaw,current_angle);
	  // HAL_Delay(50);
	  // == 
	   // 1. (按键扫描已移至 TIM6 中断，确保绝对灵敏)
      // Task_Key_Scan();
      // 2. 任务分发，跑对应的状态机 (实现在 task.c 中)
      Task_Dispatcher();
	  // OLED 显示任务状态信息
	  OLED_ShowSignedNum(48, 0,selected_task, 4, 16, 1);
	  
	  // 在第一行空白处 (x=80) 打印当前任务的状态
	  extern int task3_state;
	  extern int task4_state;
      
	  if (selected_task == 3) {
	      OLED_ShowString(80, 0, (uint8_t *)"S:", 16, 1);
	      OLED_ShowSignedNum(96, 0, task3_state, 1, 16, 1);
          OLED_ShowString(104, 0, (uint8_t *)"  ", 16, 1); // 覆盖残留字符
	  } else if (selected_task == 4) {
	      OLED_ShowString(80, 0, (uint8_t *)"S:", 16, 1);
          OLED_ShowSignedNum(96, 0, task4_state, 2, 16, 1); // 显示 0~16
	  } else {
          OLED_ShowString(80, 0, (uint8_t *)"      ", 16, 1); // 清除
      }
	  
	  OLED_ShowSignedNum(80, 16, task_running, 4, 16, 1);
	  OLED_ShowSignedNum(48, 32, count, 4, 16, 1);
	  
	  char oled_buf[24];
	  // %-7.2f 表示左对齐，总宽7个字符，保留2位小数。刚好覆盖掉旧的字符
	  sprintf(oled_buf, "%-7.2f %-7.2f", current_angle, target_angle);
	  OLED_ShowString(0, 48, (uint8_t *)oled_buf, 16, 1);
	  
	  OLED_Refresh();
	  //printf("%d\r\n",selected_task);
      HAL_Delay(10);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// 判断是不是我们用来做 PID 的 TIM6
    if (htim->Instance == TIM6) 
    {
        // 放在 20ms 中断里扫描按键，彻底无视 OLED 造成的延迟，保证 100% 灵敏度！
        Task_Key_Scan();
        
		if (beep_led_timer > 0) {
			beep_led_timer--;
			if (beep_led_timer == 0) {
				Led_Off();
				Beep_Off();
			}
		}
		// == 中断测试 == 
		//printf("test");
		// == 判断计数 == 
		if (task_running == 1)
        {
            uint8_t current_line = (line_sensor_data != 0x00);
            static uint8_t count_debounce = 0;
            
            // 冷却时间递减
            if (count_debounce > 0) {
                count_debounce--;
            } else {
                // 双边沿检测：只要黑白状态改变，立刻计数（白变黑、黑变白都算）
                if (last_line_status != current_line)
                {
                    count++;
                    count_debounce = 15; // 300ms 的冷却时间 (15 * 20ms)
                }
            }
            last_line_status = current_line;
        }
        // ================= 1. 数据清洗：角度突变过滤器 =================
        static float last_valid_angle = 0;
        
        // 减去 Yaw_Offset，让发车时按下按键后的零点生效！
        float raw_yaw = IMU_Data.Yaw - Yaw_Offset; 
        float diff = raw_yaw - last_valid_angle;
        while (diff > 180.0f)  diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        if (fabs(diff) > 30.0f && last_valid_angle != 0) {
            current_angle = last_valid_angle; 
        } else {
            current_angle = raw_yaw;         
            last_valid_angle = raw_yaw;
        }
        // ================= 2. 核心大脑：控制权分配 (双模切换) =================
        if (task_running == 1)
        {
            // 判断当前看到黑线了吗？
            // 【全局修复】：如果车在原地打转 (base_speed == 0)，强制屏蔽摄像头，纯听陀螺仪！
            if (line_sensor_data == 0x00 || base_speed == 0) 
            {
                // ---- 【模式 A：盲开模式】没有黑线，全白！听陀螺仪的，死死保住目标角度 ----
                float angle_error = target_angle - current_angle;
                while (angle_error > 180.0f)  angle_error -= 360.0f;
                while (angle_error < -180.0f) angle_error += 360.0f;
                turn_out = PID_Compute(&pid_angle, angle_error, 0);
            }
            else 
            {
                // ---- 【模式 B：循迹模式】看到黑线了！抛弃陀螺仪，听摄像头的！ ----
                // 【陀螺仪保护】：同步目标角度，防止脱线切回盲开瞬间产生剧烈抽搐
                if (selected_task == 3 || selected_task == 4) {
                    target_angle = current_angle;
                }
                turn_out = K230_Get_Turn_Speed(line_sensor_data);
            }
            // ================= 3. turn_out 平滑滤波 (防抖) =================
            static float turn_out_smooth = 0;
            // 降低新数据的权重 (原为0.7, 现改为0.3)，让切入黑线时的瞬间大幅度转向变得平滑
            turn_out_smooth = 0.7f * turn_out_smooth + 0.3f * turn_out;

            target_v_left  = base_speed - (int16_t)turn_out_smooth;
            target_v_right = base_speed + (int16_t)turn_out_smooth;
        }
        else 
        {
            target_v_left = 0;
            target_v_right = 0;
        }
        // ================= 3. 底层肌肉：速度环 PID =================
        // === 左轮 ===
        int16_t raw_l = Encoder_Get_Count_Left();
        static float fil_l = 0; 
        fil_l = 0.7f * fil_l + 0.3f * (float)raw_l; 
        float out_l = PID_Compute(&pid_left, (float)target_v_left, fil_l);
        Motor_SetSpeed_A((int16_t)out_l);
        current_v_left = (int16_t)fil_l;

        // === 右轮 ===
        int16_t raw_r = Encoder_Get_Count_Right();
        static float fil_r = 0; 
        fil_r = 0.7f * fil_r + 0.3f * (float)raw_r; 
        float out_r = PID_Compute(&pid_right, (float)target_v_right, fil_r);
        Motor_SetSpeed_B((int16_t)out_r);
        current_v_right = (int16_t)fil_r;
		//printf("%d,%d\r\n",raw_l,raw_r);
    }
}

//uart
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART2) 
    {
        WIT_Parse_Byte(rx_data);
        HAL_UART_Receive_IT(&huart2, &rx_data, 1); 
    }
    
    if(huart->Instance == USART3) 
    {
        // 把收到的 1 个字节交给封装好的状态机
        K230_Parse_Byte(k230_rx_data);
        // 重新开启串口 3 接收中断
        HAL_UART_Receive_IT(&huart3, &k230_rx_data, 1); 
		//
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
