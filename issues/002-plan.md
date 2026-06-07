# Issue 002 Plan: 节点计数去抖（边沿滤波）

## Context

当前 `main.c` TIM6 中断（约 L229-237）在 `current_line != last_line_status` 时直接 `count++`，没有去抖保护。场地传感器噪声或黑线边缘抖动可能导致一次过线被计为多次。

TIM6 中断周期约 20ms，需要在 `count++` 后引入一个去抖窗口（约 200ms~500ms），窗口内的边沿变化全部忽略。以 20ms 为一个递减周期计算，初始值 15 对应 300ms 去抖窗口。

## 修改文件

- **`H0/Drive/task.h`** — 声明去抖计数器变量（供 main.c 访问）
- **`H0/Drive/task.c`** — 定义去抖计数器变量和常量，按键启动时重置
- **`H0/Core/Src/main.c`** — 在 TIM6 中断中实现去抖逻辑

---

### 改动 1：`task.h` 新增去抖计数器声明

在现有 `extern uint8_t count;` 下方添加：

```c
extern uint8_t count_debounce;   // 节点计数去抖窗口计数器（每 TIM6 周期减 1）
```

---

### 改动 2：`task.c` 新增去抖计数器定义与初始值常量

在 `uint8_t count = 0;` 下方添加变量定义和常量：

```c
// ---- 节点计数去抖 ----
#define DEBOUNCE_INIT  15           // 去抖初始值，TIM6=20ms → 15×20ms = 300ms
uint8_t count_debounce = 0;        // 0 = 允许新边沿，>0 = 去抖窗口中
```

同时在 `Task_Key_Scan()` 的 start 按键处理中（`count = 0;` 下方）重置：

```c
count_debounce = 0;   // 新增，与 count 一起重置
```

---

### 改动 3：`main.c` TIM6 中断中用去抖逻辑替换原有直接计数

将 L229-237 的：

```c
if (task_running == 1) 
{
    uint8_t current_line = (line_sensor_data != 0x00);
    if (current_line != last_line_status) 
    {
        last_line_status = current_line;
        count++;
        base_speed = 0;
    }
}
```

替换为：

```c
if (task_running == 1) 
{
    // 去抖计数器递减
    if (count_debounce > 0) 
    {
        count_debounce--;
    }

    uint8_t current_line = (line_sensor_data != 0x00);
    if (current_line != last_line_status) 
    {
        last_line_status = current_line;
        if (count_debounce == 0) 
        {
            count++;                      // 仅去抖窗口外才计数
            count_debounce = DEBOUNCE_INIT; // 启动去抖窗口
        }
        base_speed = 0;                   // 无论是否计数，都切断动力
    }
}
```

**要点：**
- `count_debounce` 每个 TIM6 周期（20ms）递减 1，递减到 0 后才允许下一次 `count++`
- `base_speed = 0` 保留在外层，即使边沿被去抖忽略也切断动力（安全优先）
- `DEBOUNCE_INIT` 值可在 task.c 中调整：10=200ms，15=300ms，25=500ms

---

### 改动 4：确认 main.c 已 include task.h

确认 main.c 顶部已有 `#include "task.h"`，这样 `count_debounce` 和 `DEBOUNCE_INIT` 可被访问。若缺少则需添加。

---

## 验证方式

1. Keil 编译无错误/无警告
2. 烧录后观察：
   - 正常过线时 `count` 正确递增（每次过线只 +1）
   - 在 300ms 去抖窗口内传感器抖动不会导致 `count` 多次增加
   - 快速连续触发两次边沿（间隔 < 300ms），`count` 只 +1
   - 快速连续触发两次边沿（间隔 > 300ms），`count` 正确 +2
3. 可通过 OLED 或串口打印 `count` 值验证

## 参数速查

| 常量/变量         | 默认值 | 位置         | 用途                       |
|-------------------|--------|-------------|---------------------------|
| DEBOUNCE_INIT     | 15     | task.c 顶部  | 去抖窗口长度（×20ms = 300ms）|
| count_debounce    | 0      | task.c 定义  | 当前去抖剩余计数            |
| TIM6 周期         | ~20ms  | main.c 配置  | 去抖时间基准                |
