DEBUG = True           # True=使用随机波形模拟，False=使用真实硬件
SERIAL_PORT = '/dev/tty.usbmodem1101' # 你的 XIAO 连接后的串口号
BAUD_RATE = 115200    # ESP32 默认通讯速率

# 音乐映射参数
WINDOW_SIZE = 20      # 滑动窗口大小 (分析多少个采样点)
PRESSURE_THRESHOLD = 50 # 区分“轻按”与“重按”的门槛