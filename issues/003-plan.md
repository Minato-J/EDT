# Issue 003 Plan: 实现任务(3) A→C→B→D→A

## Context

`Run_Task_3()` 当前为空函数体，`Task_Dispatcher` 中 case 3 被注释。目标是实现完整的 A→C→B→D→A 路线控制逻辑并启用该任务。

路线与任务(2)不同——先走两条对角线穿越场地，再走两个半圆弧连接：
- A→C: 对角线直线（左上→右下），target_angle 约 -45°（即顺时针 45°）
- C→B: 右半圆弧（顺时针，复用 `RIGHT_HALF_CIRCLE`）
- B→D: 对角线直线（右上→左下），target_angle 约 -135°（即顺时针 135°，归一化后等效 -135°）
- D→A: 左半圆弧（逆时针，复用 `LEFT_HALF_CIRCLE`）

几何推导：
- 场地 220cm×120cm，A(左上角), B(右上角), C(右下角), D(左下角)
- A→C 方向：从左上到右下，水平分量 +220cm、垂直分量 -120cm，atan2(-120, 220) ≈ -28.8°；但循迹车是沿着黑线跑，黑线铺设的实际角度需要根据场地实测确定，初值设为 -45°，调试时再修正
- B→D 方向：从右上到左下，水平分量 -220cm、垂直分量 -120cm，角度约 180°+28.8° = -151.2°（归一化后）；初值设为 -135°，待调试

## 修改文件

**`H0/Drive/task.c`**（唯一需要修改的文件）

---

### 改动 1：在参数常量区域新增任务 3 对角线角度常量

在现有的 `#define YAW_THRESHOLD` 之后追加：

```c
// ---- Task 3 参数 ----
#define DIAG_AC_ANGLE   (-45.0f)   // A→C 对角线方向（左上→右下），待实测修正
#define DIAG_BD_ANGLE   (-135.0f)  // B→D 对角线方向（右上→左下），待实测修正
```

**说明：**
- 两个对角线角度的初值基于场地几何估算，实际值必须在场地实测后修正
- A→C 理论计算约 -28.8°，此处用 -45° 作为保守初值（K230 循迹模式下灰度传感器会辅助纠偏）
- B→D 理论计算约 -151.2°，此处用 -135° 作为初值
- 可与任务 2 共用 `BASE_SPEED_STRAIGHT`、`BASE_SPEED_CURVE`、`RIGHT_HALF_CIRCLE`、`LEFT_HALF_CIRCLE`

---

### 改动 2：实现 `Run_Task_3()` 函数体

替换现有的空函数体：

```c
static void Run_Task_3(void)  
{
    if (count == 0)
    {
        // A→C: 对角线直线段（左上→右下）
        target_angle = DIAG_AC_ANGLE;   // -45°，待实测
        base_speed = BASE_SPEED_STRAIGHT;
    }
    else if (count == 1)
    {
        // C→B: 右半圆弧（顺时针 185°）
        target_angle = RIGHT_HALF_CIRCLE;  // -185°
        base_speed = BASE_SPEED_CURVE;
    }
    else if (count == 2)
    {
        // B→D: 对角线直线段（右上→左下）
        target_angle = DIAG_BD_ANGLE;   // -135°，待实测
        base_speed = BASE_SPEED_STRAIGHT;
    }
    else if (count == 3)
    {
        // D→A: 左半圆弧（逆时针 185°）
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

**逻辑说明：**
- `count` 由 main.c TIM6 中断中的边沿检测逻辑自动递增（`last_line_status` 状态翻转时 count++）
- 圆弧段复用任务 2 的 `RIGHT_HALF_CIRCLE` / `LEFT_HALF_CIRCLE` 参数，因为 C→B 和 D→A 的弧段形状与任务 2 相同
- 直线段速度用 `BASE_SPEED_STRAIGHT`(60)，圆弧段降速为 `BASE_SPEED_CURVE`(40)，与任务 2 保持一致

---

### 改动 3：`Task_Dispatcher()` 取消 case 3 的注释

将：
```c
case 3: /* Run_Task_3(); */ break;
```

改为：
```c
case 3: Run_Task_3(); break;
```

---

## 验证方式

1. Keil 编译无错误/无警告
2. 烧录后按键选 Task 3 → Start：
   - count==0：A→C 对角线，base_speed=60，target_angle=-45°
   - count==1：C→B 右弧，base_speed=40，target_angle=-185°
   - count==2：B→D 对角线，base_speed=60，target_angle=-135°
   - count==3：D→A 左弧，base_speed=40，target_angle=+185°
   - count>=4：停车，task_running=0
3. 重点观察：
   - 对角线段 target_angle 是否需要实测修正（特别是 -45° 和 -135° 的实际偏差）
   - 圆弧段参数是否与任务 2 表现一致（应该一致，因为弧段完全相同）
   - 节点计数是否正确触发（黑线交叉点 count 递增）
4. 调试建议：先低速（降低 BASE_SPEED）单独测试对角线角度，确认无误后再恢复全速

## 参数速查

| 常量               | 值       | 用途                        |
|--------------------|----------|----------------------------|
| BASE_SPEED_STRAIGHT| 60       | 直线段基础速度               |
| BASE_SPEED_CURVE   | 40       | 圆弧段基础速度               |
| DIAG_AC_ANGLE      | -45.0f   | A→C 对角线方向角度（待实测）  |
| DIAG_BD_ANGLE      | -135.0f  | B→D 对角线方向角度（待实测）  |
| RIGHT_HALF_CIRCLE  | -185.0f  | 右半圆目标角度               |
| LEFT_HALF_CIRCLE   | +185.0f  | 左半圆目标角度               |
| YAW_THRESHOLD      | 8.0f     | 角度退出阈值(度)             |
