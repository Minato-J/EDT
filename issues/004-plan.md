# Issue 004 Plan: 精确停车控制（编码器零速判定 + 短停）

## Context

当前 task.c 中的停车方式是硬切：
```c
task_running = 0;
base_speed = 0;
```
没有减速过程，没有判断车辆是否真正停稳，也没有控制停车时长。需要实现一个可在 Run_Task_3 / Run_Task_4 中复用的"减速 -> 停稳 -> 保持 -> 再启动"动作原语。

核心思路：在 task.c 中增加一个有限状态机函数 `Task_Stop_And_Wait()`，由 `Task_Dispatcher()` 每次调用时推进一拍（约 10ms 一拍）。状态机分四个阶段：减速、等停稳、保持停车、完成。调用方检查返回值，为 0 表示还在停，为 1 表示停车流程结束可以进入下一阶段。

## 修改文件

**`H0/Drive/task.c`** — 新增状态机函数 + 修改调用方式
**`H0/Drive/task.h`** — 导出新函数声明

---

### 改动 1：task.c 顶部新增停车状态机定义和参数

在 `extern` 声明块之后、`#define` 块之前插入：

```c
// ---- 精确停车状态机 ----
typedef enum {
    STOP_IDLE = 0,      // 空闲，未触发
    STOP_DECEL,         // 减速阶段：base_speed 线性递减至 0
    STOP_WAIT_STILL,    // 等待停稳：检测编码器速度接近 0
    STOP_HOLD,          // 保持停车：base_speed=0 持续指定时长
    STOP_DONE           // 完成，等待调用方读取后复位
} StopPhase;

#define STOP_DECEL_STEP       3   // 每拍（~10ms）base_speed 递减量
#define STOP_VEL_THRESHOLD    5   // 编码器速度阈值，低于此值认为已停稳
#define STOP_HOLD_TICKS      80   // 默认停车保持拍数，80拍 x 10ms = 800ms

static StopPhase  stop_phase       = STOP_IDLE;
static int16_t    stop_saved_speed = 0;   // 触发停车前的速度，供恢复时参考
static uint16_t   stop_tick_cnt    = 0;   // 保持阶段计数器
static uint16_t   stop_hold_target = STOP_HOLD_TICKS;  // 保持目标拍数
```

**说明：**
- `STOP_DECEL_STEP = 3`：从 base_speed=60 减到 0 需要 20 拍 = 200ms，平滑且不拖沓
- `STOP_VEL_THRESHOLD = 5`：编码器滤波后读数 < 5 认为轮子已停（实测可微调）
- `STOP_HOLD_TICKS = 80`：默认 800ms 停车保持，调用方可通过参数覆盖

---

### 改动 2：新增 `Task_Stop_Begin()` 和 `Task_Stop_And_Wait()` 函数

放在 `Task_Dispatcher()` 之前：

```c
/**
 * @brief  触发停车序列（只需调用一次）
 * @param  hold_ms  停车保持时间（毫秒），传 0 则使用默认值 800ms
 */
void Task_Stop_Begin(uint16_t hold_ms)
{
    stop_saved_speed = base_speed;
    stop_tick_cnt    = 0;
    stop_hold_target = (hold_ms > 0) ? (hold_ms / 10) : STOP_HOLD_TICKS;
    stop_phase       = STOP_DECEL;
}

/**
 * @brief  停车状态机推进一拍，由 Task_Dispatcher() 每次调用
 * @retval 0 = 停车流程进行中；1 = 停车流程已完成，可以恢复行驶
 */
uint8_t Task_Stop_And_Wait(void)
{
    extern int16_t current_v_left;
    extern int16_t current_v_right;

    switch (stop_phase)
    {
        case STOP_IDLE:
            return 1;

        // -------- 阶段 1：线性减速 --------
        case STOP_DECEL:
            if (base_speed > STOP_DECEL_STEP)
            {
                base_speed -= STOP_DECEL_STEP;
            }
            else
            {
                base_speed = 0;
                stop_phase = STOP_WAIT_STILL;
            }
            return 0;

        // -------- 阶段 2：等待编码器确认停稳 --------
        case STOP_WAIT_STILL:
            base_speed = 0;
            if (current_v_left  > -STOP_VEL_THRESHOLD &&
                current_v_left  <  STOP_VEL_THRESHOLD &&
                current_v_right > -STOP_VEL_THRESHOLD &&
                current_v_right <  STOP_VEL_THRESHOLD)
            {
                stop_tick_cnt = 0;
                stop_phase    = STOP_HOLD;
            }
            return 0;

        // -------- 阶段 3：保持停车 --------
        case STOP_HOLD:
            base_speed = 0;
            stop_tick_cnt++;
            if (stop_tick_cnt >= stop_hold_target)
            {
                stop_phase = STOP_DONE;
            }
            return 0;

        // -------- 阶段 4：完成 --------
        case STOP_DONE:
            return 1;

        default:
            stop_phase = STOP_IDLE;
            return 1;
    }
}

/**
 * @brief  重置停车状态机（任务启动时或需要中断停车流程时调用）
 */
void Task_Stop_Reset(void)
{
    stop_phase = STOP_IDLE;
    stop_tick_cnt = 0;
}
```

---

### 改动 3：在 `Task_Dispatcher()` 中集成停车状态机

将当前的 `Task_Dispatcher()` 修改为：当停车流程进行中时，跳过任务函数调用，只推进停车状态机。

```c
void Task_Dispatcher(void)
{
    if (task_running == 1)
    {
        // 如果正在执行停车序列，优先推进停车状态机
        if (Task_Stop_And_Wait() == 0)
        {
            return;  // 停车进行中，跳过任务函数
        }

        switch(selected_task)
        {
            case 1: Run_Task_1(); break;
            case 2: Run_Task_2(); break;
            case 3: /* Run_Task_3(); */ break;
            case 4: /* Run_Task_4(); */ break;
        }
    }
    else
    {
        base_speed = 0;
    }
}
```

**说明：** 停车流程进行中时直接 return，不再调用 Run_Task_X()，避免任务函数覆盖 base_speed。停车完成后（返回 1），正常调用任务函数，由任务函数设置新的 base_speed 恢复行驶。

---

### 改动 4：在 `Task_Key_Scan()` 启动时重置停车状态机

在 start 按键处理中，`count = 0;` 之后增加一行：

```c
count = 0;
Task_Stop_Reset();  // 新增：确保启动时停车状态机处于空闲
task_running = 1;
```

---

### 改动 5：示例 -- 在 Run_Task_2 终点使用停车原语

将 `Run_Task_2()` 中 `count >= 4` 分支从硬切改为调用停车原语：

```c
    else if (count >= 4)
    {
        // 回到 A，精确停车（800ms 保持）
        // Task_Dispatcher 中的 Task_Stop_And_Wait() 会接管控制权
        // 减速→停稳→保持完成后才再次进入本函数
        Task_Stop_Begin(800);
        task_running = 0;  // 停车序列完成后生效，车已停稳
    }
```

**Run_Task_3 / Run_Task_4 的推荐用法（在需要中途停车的节点处）：**

```c
static void Run_Task_3(void)
{
    if (count == 0)
    {
        target_angle = 0.0f;
        base_speed = BASE_SPEED_STRAIGHT;
    }
    else if (count == 1)
    {
        // 到达第一个停车点，精确停 500ms
        Task_Stop_Begin(500);
        // 停车序列完成后 Task_Dispatcher 再次调用本函数
        // count 已经由 TIM6 中断递增，自动进入下一阶段
    }
    else if (count == 2)
    {
        // 停车后恢复行驶
        target_angle = 0.0f;
        base_speed = BASE_SPEED_STRAIGHT;
    }
    // ... 后续阶段
}
```

---

## 改动 6：task.h 新增导出声明

在 `#endif` 之前添加：

```c
// 精确停车控制
void    Task_Stop_Begin(uint16_t hold_ms);
uint8_t Task_Stop_And_Wait(void);
void    Task_Stop_Reset(void);
```

---

## 验证方式

1. Keil 编译无错误 / 无警告
2. 仅改动 task.c / task.h，不修改 main.c 和 TIM6 中断，确认不影响现有 Task 1 / Task 2 行为
3. 功能验证步骤：
   - 烧录后选 Task 2 -> Start
   - 观察 count >= 4 到达终点时：base_speed 从 60 逐步降到 0（约 200ms），不是瞬间跳零
   - 编码器读数归零后保持约 800ms 不动，然后 task_running 归 0
4. 边界测试：运行中按 Start 复位，确认 `Task_Stop_Reset()` 正确清空状态机，不会卡在 STOP_HOLD
5. 代码审查：确认 `Task_Dispatcher` 中停车期间 return 不会阻塞按键扫描（按键在 while(1) 中独立调用，不受影响）

## 参数速查

| 常量 / 参数            | 默认值  | 说明                                   |
|------------------------|---------|---------------------------------------|
| STOP_DECEL_STEP        | 3       | 每拍 base_speed 递减量（60->0 约 200ms） |
| STOP_VEL_THRESHOLD     | 5       | 编码器速度阈值，低于此值判为已停稳       |
| STOP_HOLD_TICKS        | 80      | 默认保持拍数（80 x 10ms = 800ms）       |
| hold_ms（调用参数）     | 800     | 调用方可自定义停车保持时长（毫秒）       |
