# 启用任务(2) A→B→C→D→A

## What to build

取消 `Task_Dispatcher` 中 `Run_Task_2()` 的注释，使其生效。保留 `Run_Task_2` 内现有注释代码作为参考，在其下方补全 A→B→C→D→A 的阶段切换逻辑。

路线：A(起点) → B(直线) → C(右半圆弧) → D(左半圆弧) → A(左半圆弧回起点)

按 `count` 值分阶段：

- count==0: 直线 A→B，`target_angle=0`，`base_speed` 适中
- count==1: 到达 B，切换 `target_angle` 进入右半圆弧 B→C
- count==2: 到达 C，切换 `target_angle` 进入左半圆弧 C→D
- count==3: 到达 D，切换 `target_angle` 进入左半圆弧 D→A
- count>=4: 到达 A，`task_running=0`，`base_speed=0` 停车

半圆弧的 `target_angle` 具体数值需在实际场地调试，先填合理初值并用注释标注"待调"。

## Acceptance criteria

- [ ] `Run_Task_2()` 在 `Task_Dispatcher` 中被取消注释
- [ ] 保留原有注释代码不删除
- [ ] 各阶段 `target_angle` 和 `base_speed` 有合理初值
- [ ] count>=4 时正确停车

## Blocked by

None — can start immediately
