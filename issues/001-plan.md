# Issue 001 Plan: 启用任务(2) A→B→C→D→A

## Context

`Run_Task_2()` 已有部分框架代码但存在缺陷：缺少 `count==0` 阶段（A→B 直线），且在 `Task_Dispatcher` 中被注释。目标是补全阶段切换逻辑并启用该任务函数。

路线：A(起点) → B(直线) → C(右半圆弧) → D(左半圆弧) → A(左半圆弧回起点)

## 修改文件

**`H0/Drive/task.c`**（唯一需要修改的文件）

---

### 改动 1：在文件顶部添加任务 2 参数常量

```c
// ---- Task 2 参数 ----
#define BASE_SPEED_STRAIGHT   45
#define BASE_SPEED_CURVE      32
#define RIGHT_HALF_CIRCLE  (-185.0f)  // 右半圆目标角（B→C，顺时针）
#define LEFT_HALF_CIRCLE   (+185.0f)  // 左半圆目标角（D→A，逆时针）
#define YAW_THRESHOLD         8.0f    // 退出阈值（度）
```

**说明：**
- `±185°` 比理论值 180° 多 5° 作为过冲裕量，确保 IMU 能检测到弧段完成
- `yaw_threshold = 8°` 表示当前角度与目标角度差在 ±8° 内时认为弧段完成（可用于后续弧段退出判断）
- 角度归一化逻辑在 main.c 中处理（normalize to ±180°），实际运行时 ±185° 归一化后在边界附近仍能正确驱动 PID

---

### 改动 2：`Run_Task_2()` 补全阶段逻辑

保留原有代码不动，在函数体中补全完整阶段逻辑：

```c
static void Run_Task_2(void)  
{
    if (count == 0) 
    {
        // A→B: 直线段，保持初始朝向 0°
        target_angle = 0.0f;
        base_speed = BASE_SPEED_STRAIGHT;
    }
    else if (count == 1) 
    {
        // B→C: 右半圆弧 (顺时针 185°)
        target_angle = RIGHT_HALF_CIRCLE;  // -185°
        base_speed = BASE_SPEED_CURVE;     // 32，降速防甩出
    }
    else if (count == 2) 
    {
        // C→D: 底部直线段，朝向 ~180°
        target_angle = 180.0f;
        base_speed = BASE_SPEED_STRAIGHT;
    }
    else if (count == 3) 
    {
        // D→A: 左半圆弧 (逆时针 185°)
        target_angle = LEFT_HALF_CIRCLE;   // +185°
        base_speed = BASE_SPEED_CURVE;
    }
    else if (count >= 4) 
    {
        // 回到 A，停车
        task_running = 0;  
        base_speed = 0;
    }
}
```

---

### 改动 3：`Task_Dispatcher()` 取消注释

```c
case 2: Run_Task_2(); break;   // 取消 /* */ 注释
```

---

### 改动 4：`Task_Key_Scan()` 启动时重置 count

当前 `count` 在启动时未重置（是 bug），在 start 按键处理中增加一行：

```c
count = 0;  // 新增，与 current_state / last_line_status 一起重置
```

---

## 验证方式

1. Keil 编译无错误/无警告
2. 烧录后按键选 Task 2 → Start：
   - count==0：A→B 直线，base_speed=45，target_angle=0°
   - count==1：B→C 右弧，base_speed=32，target_angle=-185°
   - count==2：C→D 直线，base_speed=45，target_angle=180°
   - count==3：D→A 左弧，base_speed=32，target_angle=+185°
   - count>=4：停车
3. 各 `target_angle` 数值需在实际场地调试微调

## 参数速查

| 常量               | 值      | 用途             |
|--------------------|---------|-----------------|
| BASE_SPEED_STRAIGHT| 45      | 直线段基础速度    |
| BASE_SPEED_CURVE   | 32      | 圆弧段基础速度    |
| RIGHT_HALF_CIRCLE  | -185.0f | 右半圆目标角度    |
| LEFT_HALF_CIRCLE   | +185.0f | 左半圆目标角度    |
| YAW_THRESHOLD      | 8.0f    | 角度退出阈值(度)  |
