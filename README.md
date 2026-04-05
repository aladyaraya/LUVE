# ESP32 双端联动控制系统

> 基于 BLE 的传感器-振动器联动系统，主机采集数据并决策，从机执行振动反馈

---

## 项目结构

```
├── esp32_unified_system/     # 主机端 - 传感器采集 + BLE服务端
│   ├── esp32_unified_system.ino
│   └── README_unified_system.md
│
├── esp32_comm_reserver/      # 从机端 - BLE客户端 + 振动控制
│   ├── esp32_comm_reserver.ino
│   └── README_comm_reserver.md
│
├── web_server/               # Web可视化端 - Flask + Socket.IO
│   ├── app.py                # Flask 后端服务
│   ├── config.py             # 配置文件
│   ├── front-end.html        # 前端交互界面
│   ├── three.module.js       # Three.js 3D渲染库
│   └── static/bgm/           # 背景音乐资源
│       ├── donggan/          # 动感音乐
│       ├── jianjin/          # 渐进音乐
│       └── pingjing/         # 平静音乐
│
└── README.md                 # 本文件
```

---

## 系统架构

```
┌───────────────────────────────────────────┐
│              主机端 (ESP32-S3)             │
│                                            │
│  ┌─────────┐  ┌──────────────┐            │
│  │MAX30105 │  │   FSR402     │            │
│  │心率传感器│  │  压力传感器   │            │
│  └────┬────┘  └──────┬───────┘            │
│       │              │                     │
│       ▼              ▼                     │
│  ┌─────────────────────────┐              │
│  │  数据融合 + 档位决策     │              │
│  └───────┬─────────────────┘              │
│          │                                 │
│    ┌─────┴─────┐                           │
│    ▼           ▼                           │
│ ┌────────┐  ┌────────┐                     │
│ │  BLE   │  │  WiFi  │                     │
│ │ Server │  │ Client │                     │
│ └───┬────┘  └───┬────┘                     │
└─────┼───────────┼──────────────────────────┘
      │           │
      │ BLE       │ WiFi HTTP POST
      ▼           ▼
┌──────────────┐  ┌──────────────────────────────┐
│ 从机端(ESP32) │  │      Web 可视化服务器         │
│  ┌────────┐  │  │                               │
│  │振动马达 │  │  │  ┌─────────────────────────┐  │
│  └────────┘  │  │  │   Flask + Socket.IO     │  │
└──────────────┘  │  │   实时数据推送          │  │
                  │  └───────────┬─────────────┘  │
                  │              │                │
                  │  ┌───────────▼─────────────┐  │
                  │  │   Three.js 3D 情动球    │  │
                  │  │   + 动态背景音乐        │  │
                  │  └─────────────────────────┘  │
                  └──────────────────────────────┘
```

---

## 快速上手

### 1️⃣ 硬件准备

**主机端需要：**
- ESP32-S3 开发板（需支持 BLE 5.0）
- MAX30105 心率传感器
- FSR402 压力传感器

**从机端需要：**
- ESP32 开发板
- 振动马达 + N-MOS 驱动电路

### 2️⃣ 配置修改

**主机端** (`esp32_unified_system.ino`)：
```cpp
// WiFi 配置
const char* WIFI_SSID = "你的WiFi名称";
const char* WIFI_PASSWORD = "你的密码";

// Web 服务器配置
const char* SERVER_URL = "http://你的服务器IP:5001/api/data";
```

**从机端** (`esp32_comm_reserver.ino`)：默认配置即可，会自动搜索主机设备

### 3️⃣ Web 服务器数据接口

主机端会实时向 Web 服务器发送档位数据（HTTP POST）：

**请求格式：**
```
POST /api/data
Content-Type: application/json

{
  "heartRate": 75,      // 当前心率 BPM
  "pressure": 1234,     // 压力传感器原始值
  "level": 1,           // 当前振动档位 (0-3)
  "timestamp": 1712275200
}
```

**服务端功能：**
- 接收传感器数据并通过 Socket.IO 实时推送到前端
- 前端根据档位自动切换视觉主题与背景音乐

### 4️⃣ 上传运行

1. 先上传主机端代码，等待启动完成
2. 再上传从机端代码，观察串口日志确认连接成功
3. 启动 Web 服务器（见下方说明）
4. 按压压力传感器或改变心率，从机将自动调整振动强度，Web 端同步切换视觉与音乐

---

## Web 可视化端

### 技术栈

| 组件 | 技术 | 说明 |
|------|------|------|
| 后端 | Flask + Flask-SocketIO | 处理 HTTP 请求，实时推送硬件状态 |
| 前端 | HTML5 + Socket.IO Client | 接收实时数据，控制界面切换 |
| 3D渲染 | Three.js | 情动球动态波浪效果 |
| 音频 | HTML5 Audio | 三档背景音乐无缝切换 |

### 启动方式

```bash
# 安装依赖
pip install flask flask-socketio

# 启动服务器
cd web_server
python app.py
```

服务启动后访问 `http://localhost:5001` 即可看到可视化界面。

### 功能特性

- **实时同步**：通过 Socket.IO 接收硬件档位，零延迟响应
- **三档视觉主题**：
  - 🍃 **平静** (Level 3) - 清新蓝绿渐变，柔和波浪
  - 🌅 **渐进** (Level 1-2) - 温暖橙黄渐变，中等起伏
  - 🔥 **动感** (Level 0) - 热烈粉红渐变，剧烈波动
- **动态音乐**：每档对应专属背景音乐，带淡入淡出过渡
- **3D 情动球**：Three.js 渲染的动态波浪平面，振幅随档位变化

### 目录结构

```
web_server/
├── app.py                 # Flask 主程序（端口 5001）
├── config.py              # 调试模式与串口配置
├── front-end.html         # 前端页面（可直接浏览器打开测试）
├── three.module.js        # Three.js ES Module 版本
├── three.core.js          # Three.js 核心模块
├── favicon.ico            # 网站图标
└── static/bgm/            # 背景音乐资源
    ├── donggan/           # 动感音乐 - dont-stop.mp3
    ├── jianjin/           # 渐进音乐 - timing.mp3
    └── pingjing/          # 平静音乐 - float-sturling.mp3
```

---

## 振动档位说明

| 指令 | 强度 | 触发条件 | Web 端响应 |
|-----|------|---------|-----------|
| `LEVEL:0` | 最高 | 轻压 或 高心率(>100 BPM) | 🔥 动感主题 + dont-stop |
| `LEVEL:1` | 中高 | 中压 或 中高心率(80-100 BPM) | 🌅 渐进主题 + timing |
| `LEVEL:2` | 中低 | 重压 或 中低心率(60-80 BPM) | 🌅 渐进主题 + timing |
| `LEVEL:3` | 最低 | 极重压 或 低心率(<60 BPM) | 🍃 平静主题 + float-sturling |

---

## 详细文档

- [主机端完整文档](./esp32_unified_system/README_unified_system.md) - 硬件接线、配置参数、融合算法详解
- [从机端完整文档](./esp32_comm_reserver/README_comm_reserver.md) - 振动控制、PWM配置、断线重连

---

## 作者

LUVE随心所动  
日期：2026年4月5日