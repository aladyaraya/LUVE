## ✨ 主要特性

- **3D 渲染**: 集成 Three.js 核心库，实现流畅的 WebGL 图形渲染。
- **音乐分类管理**: 静态资源按风格分类，包含：
  - **动感 (Donggan)**: 节奏强烈的音乐。
  - **渐进 (Jianjin)**: 循序渐进的氛围音乐。
  - **平静 (Pingjing)**: 舒缓放松的背景音。
- **轻量级后端**: 基于 Python 的原生服务或轻量框架，易于部署。
- **响应式前端**: 纯 HTML/JS 实现，无复杂构建工具依赖，开箱即用。

## 技术栈

- **后端**: Python 3.12及以上版本
- **前端**: HTML, JavaScript, CSS
- **图形库**: Three.js (Core & Modules)
- **资源**: MP3 音频流

## 快速开始

运行后端程序：python3 app.py
浏览器访问，如http://127.0.0.1:5500

## 项目结构

web/
├── static/
│ └── bgm/ # 存放所有背景音乐资源
│ ├── donggan/ # 动感音乐分类
│ ├── jianjin/ # 渐进音乐分类
│ └── pingjing/ # 平静音乐分类
├── app.py # 后端主入口
├── config.py # 配置文件
├── front-end.html # 前端主页面
├── three.core.js # Three.js 核心库
└── three.module.js # Three.js 模块库
