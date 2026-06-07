'''
实验名称：6路灰度循迹 + 官方UART2发送 (K230 Mini 最终版)
说明：2.4寸屏显示，划分6个ROI判断黑线，打包为1个Byte通过UART2(引脚11)发给STM32。
'''

import time, os, sys
import image
from machine import UART
from machine import FPIOA

from media.sensor import *
from media.display import *
from media.media import *

print("====== K230 Mini 循迹系统 (带官方串口) 启动 ======")

# ================= 1. 串口2 初始化 (完全使用你的官方例程) =================
fpioa = FPIOA()
# UART2代码: 引脚11为TX(接STM32的RX), 引脚12为RX(接STM32的TX)
fpioa.set_function(11, FPIOA.UART2_TXD)
fpioa.set_function(12, FPIOA.UART2_RXD)

# 设置串口号2和波特率 115200
uart = UART(UART.UART2, 115200)
print("串口2 初始化完成！引脚11准备发送数据...")


# ================= 2. 摄像头 & 2.4寸屏幕 初始化 =================
sensor = Sensor(width=1280, height=960)
sensor.reset()
sensor.set_framesize(width=640, height=480) # 2.4寸屏专用分辨率
sensor.set_pixformat(Sensor.RGB565)

# 驱动 ST7701 屏幕，并映射到 IDE 方便调试
Display.init(Display.ST7701, width=640, height=480, to_ide=True)

MediaManager.init()
sensor.run()
clock = time.clock()


# ================= 3. 循迹区域 (ROI) 划分 =================
roi_w = 106
roi_h = 60
roi_y = 360 # 识别框在屏幕偏下方

rois = [
    (0 * roi_w, roi_y, roi_w, roi_h),
    (1 * roi_w, roi_y, roi_w, roi_h),
    (2 * roi_w, roi_y, roi_w, roi_h),
    (3 * roi_w, roi_y, roi_w, roi_h),
    (4 * roi_w, roi_y, roi_w, roi_h),
    (5 * roi_w, roi_y, 640 - 5*roi_w, roi_h)
]

# 黑线判断阈值 (根据场地光线微调，越小越黑)
BLACK_THRESHOLD = 30


# ================= 4. 主循环 =================
try:
    while True:
        os.exitpoint()
        clock.tick()

        img = sensor.snapshot()

        line_states = [0, 0, 0, 0, 0, 0]
        send_data = 0 # 准备发送的 1 Byte 压缩数据

        for i, roi in enumerate(rois):
            stats = img.get_statistics(roi=roi)
            lightness = stats.l_mean()

            # 灰度判断
            if lightness < BLACK_THRESHOLD:
                line_states[i] = 1
                color = (0, 255, 0) # 绿色表示压线
                # 位操作：将检测到的框映射到二进制位上
                send_data |= (1 << (5 - i))
            else:
                line_states[i] = 0
                color = (255, 0, 0) # 红色表示白底

            # 在图像上画框和数字
            img.draw_rectangle(roi, color=color, thickness=2)
            img.draw_string(roi[0] + 35, roi[1] + 15, str(line_states[i]), scale=3, color=color)

        # ================= 5. 发送串口数据 =================
        # 为了让 STM32 接收不发生错位，我们打包 3 个字节：
        # 第1个字节: 0xAA (帧头1)
        # 第2个字节: 0x55 (帧头2)
        # 第3个字节: send_data (实际的循迹数据，范围 0x00 到 0x3F)
        packet = bytearray([0xAA, 0x55, send_data])
        uart.write(packet)

        # 在 IDE 终端打印，方便你肉眼监控
        print(f"状态: {line_states} -> 串口发送十六进制: {hex(send_data)} | FPS: {clock.fps()}")

        # 屏幕刷新显示
        Display.show_image(img)

except KeyboardInterrupt as e:
    print("程序已停止")
except BaseException as e:
    print(f"发生异常: '{e}'")
finally:
    if isinstance(sensor, Sensor):
        sensor.stop()
    Display.deinit()
    MediaManager.deinit()
    print("资源释放完毕。")
