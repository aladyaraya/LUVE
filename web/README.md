项目结构
web/
├── __pycache__/                  # Python 编译缓存
│   └── config.cpython-312.pyc
├── static/                       # 静态资源文件夹
│   └── bgm/                      # 背景音乐资源
│       ├── donggan/              # "动感" 分类音乐
│       │   └── dont-stop.mp3
│       ├── jianjin/              # "渐进" 分类音乐
│       │   └── timing.mp3
│       └── pingjing/             # "平静" 分类音乐
│           └── float-sturling.mp3
├── app.py                        # 主程序入口 (后端逻辑)
├── config.py                     # 配置文件
├── favicon.ico                   # 网站图标
├── front-end.html                # 主前端页面入口
├── three.core.js                 # Three.js 核心库
└── three.module.js               # Three.js 模块化扩展

运行app.py  启动后端服务，接收硬件传输信号
启动html 接收来自后端的信号，实现UI不同主题切换
