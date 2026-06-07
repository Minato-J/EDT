# 节点计数去抖（边沿滤波）

## What to build

当前 `main.c` TIM6 中断中的节点计数逻辑（约 L230）在 `current_line != last_line_status` 时直接 `count++`，没有去抖保护。场地传感器噪声或黑线边缘抖动可能导致一次过线被计为多次，所有任务的状态机都会因此跑飞。

在 `task.c` 的 `Run_Task_*` 函数分区内（或 `main.c` 的 `count++` 附近）加入去抖逻辑：检测到边沿变化后，在一定时间窗口内（如 200ms~500ms，即 10~25 个 TIM6 周期）忽略后续边沿触发，防止重复计数。

实现方式建议：引入一个 `count_debounce` 计数器，每次 `count++` 后设为初始值（如 15），每个 TIM6 周期递减，仅当 `count_debounce == 0` 时才允许新的 `count++`。

## Acceptance criteria

- [ ] 引入去抖计数器变量（如 `count_debounce`）
- [ ] 每次 `count++` 后启动去抖窗口
- [ ] 去抖窗口内的边沿变化被忽略
- [ ] 去抖窗口时长约 200ms~500ms（可调）
- [ ] 变量声明在 `task.h` 中（如需要跨文件访问），实现在 `task.c` 或 `main.c` 中

## Blocked by

None — can start immediately
