/*
 * ============================================================================
 * 跳蛋振动控制器 - BLE客户端（接收端）
 * Vibration Egg Controller - BLE Client
 * ============================================================================
 * 
 * 角色：BLE Client（客户端）
 * 功能：主动扫描并连接手镯端，接收LEVEL指令并执行振动
 * 
 * 硬件：ESP32 + 震动马达（GPIO4）
 * 
 * 作者：AI助手
 * 日期：2026年4月5日
 * ============================================================================
 */

#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEUtils.h>

// ================= 硬件配置 =================
#define MOTOR_PIN         4
#define LED_PIN           2

// ================= BLE配置 =================
#define TARGET_DEVICE_NAME    "ESP32_FusionCtrl"
#define SERVICE_UUID          "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ================= 振动参数 =================
#define PWM_FREQ          5000
#define PWM_RES           8
#define DUTY_OFF          30
#define DUTY_LOW          80
#define DUTY_MED          150
#define DUTY_HIGH         250

// ================= 全局变量 =================
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteChar = nullptr;
bool connected = false;
uint8_t currentLevel = 3;
uint8_t lastLevel = 255;

BLEAdvertisedDevice* myDevice = nullptr;
bool doConnect = false;

// ================= 振动控制 =================
void setVibration(uint8_t level) {
  uint32_t duty;
  switch (level) {
    case 3: duty = DUTY_OFF; break;
    case 2: duty = DUTY_LOW; break;
    case 1: duty = DUTY_MED; break;
    case 0: duty = DUTY_HIGH; break;
    default: return;
  }
  currentLevel = level;
  ledcWrite(MOTOR_PIN, duty);
  Serial.printf("▶ 振动: LEVEL:%d (%d%%)\n", level, (duty*100)/255);
}

// ================= 解析指令 =================
void parseLevelCommand(String value) {
  if (value.startsWith("LEVEL:")) {
    int level = value.substring(6).toInt();
    if (level >= 0 && level <= 3 && level != lastLevel) {
      lastLevel = level;
      setVibration(level);
      digitalWrite(LED_PIN, (level < 3) ? HIGH : LOW);
    }
  }
}

// ================= BLE通知回调 =================
void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  String value = "";
  for (size_t i = 0; i < length; i++) value += (char)pData[i];
  Serial.print("← ");
  Serial.println(value);
  parseLevelCommand(value);
}

// ================= 连接设备 =================
bool connectToServer() {
  if (myDevice == nullptr) return false;
  
  Serial.print("[BLE] 连接到 ");
  Serial.println(myDevice->getName().c_str());
  
  pClient = BLEDevice::createClient();
  
  if (!pClient->connect(myDevice)) {
    Serial.println("[BLE] 连接失败！");
    return false;
  }
  
  Serial.println("[BLE] ✓ 已连接");
  
  BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (pRemoteService == nullptr) {
    Serial.println("[BLE] 未找到服务");
    pClient->disconnect();
    return false;
  }
  
  pRemoteChar = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
  if (pRemoteChar == nullptr) {
    Serial.println("[BLE] 未找到特征");
    pClient->disconnect();
    return false;
  }
  
  if (pRemoteChar->canNotify()) {
    pRemoteChar->registerForNotify(notifyCallback);
  }
  
  connected = true;
  digitalWrite(LED_PIN, HIGH);
  return true;
}

// ================= 扫描回调 =================
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) {
    if (device.haveName() && device.getName() == TARGET_DEVICE_NAME) {
      Serial.print("[BLE] 找到: ");
      Serial.println(device.getName().c_str());
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(device);
      doConnect = true;
    }
  }
};

// ================= 初始化 =================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println(" 跳蛋振动控制器 v1.3");
  Serial.println("========================================\n");
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // PWM初始化
  ledcAttach(MOTOR_PIN, PWM_FREQ, PWM_RES);
  ledcWrite(MOTOR_PIN, DUTY_OFF);
  
  // 开机测试
  Serial.println("[自检] 马达振动 2秒...");
  ledcWrite(MOTOR_PIN, 255);
  delay(2000);
  ledcWrite(MOTOR_PIN, 0);
  Serial.println("[自检] 完成");
  
  // BLE初始化
  BLEDevice::init("Vibration_Egg");
  
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pScan->setActiveScan(true);
  
  Serial.println("[BLE] 扫描中...");
  pScan->start(0, false);
}

// ================= 主循环 =================
void loop() {
  // 发现设备后连接
  if (doConnect) {
    doConnect = false;
    if (connectToServer()) {
      Serial.println("[状态] 已连接，等待指令...");
    } else {
      Serial.println("[状态] 连接失败，重新扫描...");
      BLEDevice::getScan()->start(0, false);
    }
  }
  
  // 检查断线
  if (connected && pClient && !pClient->isConnected()) {
    connected = false;
    digitalWrite(LED_PIN, LOW);
    Serial.println("[BLE] 断开，重新扫描...");
    BLEDevice::getScan()->start(0, false);
  }
  
  // 心跳
  static unsigned long lastHeart = 0;
  if (millis() - lastHeart > 5000) {
    lastHeart = millis();
    Serial.printf("[状态] %s LEVEL:%d\n", connected ? "已连接" : "未连接", currentLevel);
  }
  
  delay(100);
}
