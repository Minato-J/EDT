# Issue 005 Plan: 任务(4) 停车次数计数 + 策略集成

## Context

任务(4) 要求小车沿 A→B→C→D→A 路线行驶一整圈，过程中**恰好停车 4 次**。路线共有 4 个节点（B、C、D、A），最自然的策略是每个节点停一次。本计划在 Issue 003（任务 3 路线逻辑）和 Issue 004（停车原语 `Task_Stop_And_Wait()`）的基础上，新增 `Run_Task_4()` 函数，集成停车计数与停车执行。

路线与停车时机：

```
A —(直行)—→ [停1] B —(右弧)—→ [停2] C —(直行)—→ [停3] D —(左弧)—→ [停4] A(终点)
```

关键依赖：
- Issue 004 提供 `Task_Stop_And_Wait()` 停车原语（减速→停稳→保持→返回完成标志）
- Issue 003 提供的路线阶段逻辑可参考复用

## 修改文件

**`H0/Drive/task.c`**（主要修改文件）
**`H0/Drive/task.h`**（新增 extern 声明）

---

### 改动 1：新增 Task 4 参数常量

在文件顶部 `// ---- Task 2 参数 ----` 区域之后新增：

```c
// ---- Task 4 参数 ----
#define TASK4_STOP_COUNT_TARGET   4      // 目标停车次数
#define TASK4_BASE_SPEED_STRAIGHT 60     // 直线段速度
#define TASK4_BASE_SPEED_CURVE    40     // 圆弧段速度
```

**说明：** 速度值与 Task 2/3 保持一致，后续调试可独立调整。

---

### 改动 2：新增 stop_count 变量

在全局变量区（`count` 声明附近）新增：

```c
uint8_t stop_count = 0;            // 任务(4) 停车计数器
```

在 `task.h` 中新增 extern 声明：

```c
extern uint8_t stop_count;
```

在 `Task_Key_Scan()` 的 start 按键处理中，与 `count = 0` 一起重置：

```c
stop_count = 0;  // 新增，每次启动重置停车计数
```

---

### 改动 3：新增 stop 状态变量（状态机辅助）

用于在 `Run_Task_4` 内部区分"行驶中"和"停车中"两个子状态：

```c
// ---- Task 4 内部状态 ----
typedef enum {
    STOP_IDLE = 0,     // 未在停车（正常行驶）
    STOP_IN_PROGRESS   // 正在执行停车原语
} StopState;

static StopState stop_state = STOP_IDLE;
```

**说明：** 使用枚举状态机而非简单 bool，方便后续扩展更多停车子阶段（如减速中、保持中、加速中）。

---

### 改动 4：实现 Run_Task_4() 函数

在 `Run_Task_3()` 之后新增完整函数：

```c
static void Run_Task_4(void)
{
    // ---- 子状态：正在停车 ----
    if (stop_state == STOP_IN_PROGRESS)
    {
        // 调用 Issue 004 的停车原语，返回 1 表示停车完成
        if (Task_Stop_And_Wait() == 1)
        {
            stop_state = STOP_IDLE;   // 停车结束，恢复行驶
            stop_count++;             // 停车计数 +1
        }
        return;  // 停车期间不执行下面的路线逻辑
    }

    // ---- 子状态：正常行驶 ----
    // 判断是否已完成所有停车，且已到终点
    if (count >= 4)
    {
        // 整圈结束，最终停车
        task_running = 0;
        base_speed = 0;
        return;
    }

    // ---- 阶段逻辑：根据 count 设置行驶参数 ----
    // count 会在 TIM6 中断中自增（节点检测逻辑在 main.c 中）
    // 每到一个节点先停车，停完再切换到下一阶段

    if (count == 0)
    {
        // A→B 直线
        target_angle = 0.0f;
        base_speed = TASK4_BASE_SPEED_STRAIGHT;
    }
    else if (count == 1)
    {
        // 到达 B，停车 1
        if (stop_count < TASK4_STOP_COUNT_TARGET)
        {
            stop_state = STOP_IN_PROGRESS;
            return;
        }
        // 停车完成后：B→C 右弧
        target_angle = RIGHT_HALF_CIRCLE;
        base_speed = TASK4_BASE_SPEED_CURVE;
    }
    else if (count == 2)
    {
        // 到达 C，停车 2
        if (stop_count < TASK4_STOP_COUNT_TARGET)
        {
            stop_state = STOP_IN_PROGRESS;
            return;
        }
        // 停车完成后：C→D 直线
        target_angle = 180.0f;
        base_speed = TASK4_BASE_SPEED_STRAIGHT;
    }
    else if (count == 3)
    {
        // 到达 D，停车 3
        if (stop_count < TASK4_STOP_COUNT_TARGET)
        {
            stop_state = STOP_IN_PROGRESS;
            return;
        }
        // 停车完成后：D→A 左弧
        target_angle = LEFT_HALF_CIRCLE;
        base_speed = TASK4_BASE_SPEED_CURVE;
    }
    else if (count >= 4)
    {
        // 到达 A（终点），停车 4（最后一次，直接结束）
        task_running = 0;
        base_speed = 0;
        stop_count++;
    }
}
```

**设计说明：**

1. **停车触发机制**：利用 `count` 在 TIM6 中断中自增的特性。当 `Run_Task_4()` 在主循环中被调用时，若检测到 `count` 已变为新值且 `stop_count` 未达标，立即进入 `STOP_IN_PROGRESS` 子状态。
2. **双层状态机**：外层由 `count` 驱动路线阶段，内层由 `stop_state` 驱动停车/行驶切换。
3. **count == 4 的特殊处理**：到达终点 A 时直接结束任务（第 4 次停车即终点停车），不调用停车原语，因为 `task_running=0` 本身就会切断动力。
4. **安全阀**：`stop_count < TASK4_STOP_COUNT_TARGET` 条件确保最多只停 4 次，即使调试中 count 跑飞也不会无限停车。

---

### 改动 5：Task_Dispatcher 启用 case 4

```c
case 4: Run_Task_4(); break;   // 取消注释
```

---

### 改动 6：OLED 显示停车次数（调试用）

在 `main.c` 的 `while(1)` 主循环中，OLED 显示区域新增一行：

```c
// 在已有的 OLED 显示代码之后追加：
OLED_ShowString(0, 32, (uint8_t *)"Stop:", 16, 1);
OLED_ShowSignedNum(40, 32, stop_count, 4, 16, 1);
```

同时可选开启串口调试打印（在 `Run_Task_4` 的停车触发处）：

```c
// printf("Stop #%d at node %d\r\n", stop_count + 1, count);
```

---

## 验证方式

1. **编译验证**：Keil 编译无错误/无警告
2. **功能验证**：
   - 按键选 Task 4 → Start
   - 观察 OLED 第三行显示 `Stop: 0`
   - 小车从 A 出发，到达 B 时停车，OLED 显示 `Stop: 1`
   - 停车保持约 500ms~1000ms 后自动恢复行驶
   - 到达 C 停车 2 次、D 停车 3 次
   - 到达 A 终点，任务结束，`Stop: 4`
3. **边界验证**：
   - 全程恰好停车 4 次（不多不少）
   - 如果中途 count 异常跳变，`stop_count` 不会超过 4
   - 停车期间小车完全静止（编码器读数接近 0）
4. **串口调试**：开启 `printf` 观察每次停车的节点编号和计数

## 参数速查

| 常量/变量                  | 值/类型      | 用途                        |
|---------------------------|-------------|----------------------------|
| TASK4_STOP_COUNT_TARGET   | 4           | 目标停车次数                 |
| TASK4_BASE_SPEED_STRAIGHT | 60          | 直线段基础速度               |
| TASK4_BASE_SPEED_CURVE    | 40          | 圆弧段基础速度               |
| stop_count                | uint8_t     | 当前已停车次数（全局）        |
| stop_state                | StopState   | 停车子状态机（函数内 static） |
| STOP_IDLE                 | 0           | 未在停车                     |
| STOP_IN_PROGRESS          | 1           | 正在执行停车原语              |
| RIGHT_HALF_CIRCLE         | -185.0f     | 右半圆目标角度（复用 Task 2） |
| LEFT_HALF_CIRCLE          | +185.0f     | 左半圆目标角度（复用 Task 2） |
