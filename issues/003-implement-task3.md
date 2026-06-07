# 实现任务(3) A→C→B→D→A

## What to build

在 `task.c` 的 `Run_Task_3()` 空函数体内实现完整的 A→C→B→D→A 路线控制逻辑。

路线特点（与任务2不同）：

- A→C: 直线段（对角线穿越场地）
- C→B: 右半圆弧
- B→D: 直线段（对角线穿越场地）
- D→A: 左半圆弧

按 `count` 值分阶段设置 `target_angle` 和 `base_speed`：

- count==0: 直线 A→C，`target_angle` 设为对角线方向角度（需根据场地几何计算，约 -45° 或 +135°，待调）
- count==1: 到达 C，切换进入右半圆弧 C→B，`target_angle` 切换
- count==2: 到达 B，切换进入直线 B→D，`target_angle` 设为另一条对角线方向
- count==3: 到达 D，切换进入左半圆弧 D→A
- count>=4: 到达 A，`task_running=0`，停车

关键挑战：

1. 直线段 A→C 和 B→D 是对角线穿越，`target_angle` 与任务2的直线段（A→B 水平）不同
2. 需要在任务2调试完成后复用其半圆弧控制参数
3. 赛题要求"以尽可能少的停车次数完成一整圈"，因此本任务应尽量不停车连续行驶

在 `Task_Dispatcher` 中取消 `Run_Task_3()` 的注释，启用 case 3。

## Acceptance criteria

- [ ] `Run_Task_3()` 函数体内有完整的阶段切换逻辑
- [ ] `Task_Dispatcher` 中 case 3 被取消注释
- [ ] 各阶段 `target_angle` 和 `base_speed` 有合理初值
- [ ] count>=4 时正确停车
- [ ] 保留原有空函数体注释（如有）

## Blocked by

- Issue 001 (任务2完成，复用半圆弧参数)
- Issue 002 (节点计数去抖，防止 count 跑飞)
