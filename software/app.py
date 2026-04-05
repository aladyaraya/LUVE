from flask import Flask, render_template, request
from flask_socketio import SocketIO
import time

app = Flask(__name__)
# 禁用冗余日志
import logging
log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)

socketio = SocketIO(app, cors_allowed_origins="*")

@app.route('/')
def index():
    # 渲染最新的 v15.html
    return render_template('v15.html')

@app.route('/api/data', methods=['POST'])
def receive_data():
    try:
        # 获取 JSON 数据
        payload = request.get_json()
        if not payload:
            return "Invalid JSON", 400

        # 提取 level 字段
        level = payload.get('level', 1)
        
        # 终端打印，用于黑客松现场快速除错
        print(f"[{time.strftime('%H:%M:%S')}] 🟢 硬件触发 Level: {level}")

        # 发送给前端 Socket 监听器
        socketio.emit('hardware_update', {'level': level})

        return "OK", 200
    except Exception as e:
        print(f"❌ 后端异常: {e}")
        return str(e), 500

if __name__ == '__main__':
    # 使用 5001 端口避开系统占用
    socketio.run(app, host='0.0.0.0', port=5001, debug=False)

    print("\n" + "="*40)
    print(f"🚀 记仇小猫服务器已启动!")
    print(f"💻 网页访问: http://localhost:5001")
    print(f"📡 ESP32 填写的 URL: http://{local_ip}:5001/api/data")
    print("="*40 + "\n")