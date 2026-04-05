/*
 * ============================================================================
 * ESP32 统一传感器控制系统
 * Unified Sensor Control System - BLE + WiFi Dual Channel
 * ============================================================================
 * 
 * 功能整合：
 * - 真实传感器数据采集（MAX30105心率 + FSR402压力）
 * - BLE通道：向跳蛋端发送振动档位指令
 * - WiFi通道：向Web服务器同步心率、压力、档位数据
 * 
 * 硬件：ESP32-S3 + MAX30105 + FSR402
 * 
 * 作者：AI助手
 * 日期：2026年4月5日
 * ============================================================================
 */

#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ================= 硬件引脚配置 =================
#define FSR_PIN 4
#define LED_PIN 2
#define I2C_SDA 2
#define I2C_SCL 1

// ================= 压力阈值配置 =================
#define PRESSURE_L0 500
#define PRESSURE_L1 1500
#define PRESSURE_L2 3000
#define PRESSURE_L3 4000

// ================= 心率阈值配置 =================
#define HEART_RATE_L1 60
#define HEART_RATE_L2 80
#define HEART_RATE_L3 100

// ================= 滤波参数 =================
#define EMA_ALPHA 0.3f
#define SAMPLE_MS 20
#define RATE_SIZE 10

// ================= BLE 配置 =================
#define BLE_DEVICE_NAME "ESP32_FusionCtrl"
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ================= WiFi 配置 =================
const char* WIFI_SSID = "liuyu";           // WiFi名称（需修改为实际网络）
const char* WIFI_PASSWORD = "12345678";    // WiFi密码（需修改为实际密码）
const char* SERVER_URL = "http://192.168.50.50:5001/api/data";  // Web服务器URL
const int WIFI_SEND_INTERVAL = 5000;       // WiFi数据发送间隔(毫秒)

// ================= 全局对象 =================
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
MAX30105 particleSensor;

// ================= 连接状态 =================
bool bleDeviceConnected = false;
bool oldBleDeviceConnected = false;
bool wifiConnected = false;

// ================= 传感器数据 =================
float filteredPressure = 0.0;
uint8_t pressureLevel = 3;
long irValue = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 0;
byte heartRates[RATE_SIZE];
byte heartRateSpot = 0;
byte heartRateCount = 0;
uint8_t finalLevel = 3;
uint8_t lastSentBleLevel = 255;

// ================= 时间戳 =================
unsigned long lastSendTime = 0;
unsigned long lastSampleTime = 0;
unsigned long lastDebugTime = 0;
unsigned long lastWifiSendTime = 0;

// ================= 压力映射函数 =================
// 档位定义：3=最低档(最弱/无动作)，2=中低，1=中高，0=最高档(最强)
uint8_t mapPressureToLevel(float value) {
  if (value <= PRESSURE_L0) return 0;   // 轻压 → 最强振
  if (value <= PRESSURE_L1) return 1;   // 中压 → 中强振
  if (value <= PRESSURE_L2) return 2;   // 重压 → 中弱振
  return 3;                              // 极重压 → 停止
}

// ================= 心率映射函数 =================
uint8_t mapHeartRateToLevel(float bpm) {
  if (bpm < HEART_RATE_L1) return 3;
  if (bpm < HEART_RATE_L2) return 2;
  if (bpm < HEART_RATE_L3) return 1;
  return 0;
}

// ================= 核心融合逻辑 =================
uint8_t computeFinalLevel() {
  if (pressureLevel <= 2) {
    return pressureLevel;
  } else {
    return mapHeartRateToLevel(beatsPerMinute);
  }
}

// ================= BLE 回调 =================
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleDeviceConnected = true;
    Serial.println("[BLE] ✓ 跳蛋端已连接");
    digitalWrite(LED_PIN, HIGH);
  };
  void onDisconnect(BLEServer* pServer) {
    bleDeviceConnected = false;
    Serial.println("[BLE] ✗ 跳蛋端已断开");
    digitalWrite(LED_PIN, LOW);
  }
};

// ================= BLE 发送档位 =================
void sendBleLevel(uint8_t level) {
  if (!bleDeviceConnected) return;
  if (level == lastSentBleLevel && (millis() - lastSendTime) < 500) {
    return;
  }
  
  char buf[32];
  snprintf(buf, sizeof(buf), "LEVEL:%d", level);
  pCharacteristic->setValue(buf);
  pCharacteristic->notify();
  
  Serial.printf("[BLE发送] LEVEL:%d | 压档:%d | BPM:%.0f\n", 
                level, pressureLevel, beatsPerMinute);
  
  lastSentBleLevel = level;
  lastSendTime = millis();
}

// ================= WiFi 连接 =================
void connectToWiFi() {
  Serial.printf("[WiFi] 正在连接: %s\n", WIFI_SSID);
  
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n[WiFi] ✓ 连接成功!");
    Serial.printf("[WiFi] IP地址: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] 信号强度: %d dBm\n", WiFi.RSSI());
  } else {
    wifiConnected = false;
    Serial.println("\n[WiFi] ✗ 连接失败，稍后重试");
  }
}

// ================= WiFi 数据发送 =================
void sendToWebServer() {
  if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] 未连接，尝试重连...");
    connectToWiFi();
    return;
  }
  
  HTTPClient http;
  
  if (!http.begin(SERVER_URL)) {
    Serial.println("[WiFi] HTTP初始化失败");
    http.end();
    return;
  }
  
  // 构建JSON数据包
  char jsonPayload[256];
  snprintf(jsonPayload, sizeof(jsonPayload), 
    "{\"heartRate\":%.0f,\"pressure\":%.0f,\"level\":%d,\"timestamp\":%lu}",
    beatsPerMinute, filteredPressure, finalLevel, millis());
  
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    Serial.printf("[WiFi] 发送成功 → HTTP %d | BPM:%.0f 压力:%.0f 档位:%d\n", 
                  httpResponseCode, beatsPerMinute, filteredPressure, finalLevel);
  } else {
    Serial.printf("[WiFi] 发送失败: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  
  http.end();
}

// ================= 初始化 MAX30105 =================
bool initMAX30105() {
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("[错误] MAX30105 未找到！");
    return false;
  }
  particleSensor.setup(0x1F, 4, 2, 100, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x0A);
  Serial.println("[OK] MAX30105 心率传感器初始化成功");
  return true;
}

// ================= 初始化 BLE =================
void initBLE() {
  BLEDevice::init(BLE_DEVICE_NAME);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("LEVEL:3");
  
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  
  BLEDevice::startAdvertising();
  
  Serial.printf("[OK] BLE广播中: %s\n", BLE_DEVICE_NAME);
}

// ================= 初始化 WiFi =================
void initWiFi() {
  connectToWiFi();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n");
  Serial.println("╔════════════════════════════════════════════╗");
  Serial.println("║   ESP32 统一传感器控制系统 v1.0            ║");
  Serial.println("║   BLE + WiFi 双通道数据同步                ║");
  Serial.println("╠════════════════════════════════════════════╣");
  Serial.println("║   档位: 3=最低 2=中低 1=中高 0=最高        ║");
  Serial.println("║   通道: BLE→跳蛋振动  WiFi→Web服务器       ║");
  Serial.println("╚════════════════════════════════════════════╝");
  Serial.println();
  
  // 初始化硬件
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(FSR_PIN, INPUT);
  analogSetAttenuation(ADC_11db);
  
  // 初始化传感器
  bool hrAvailable = initMAX30105();
  if (!hrAvailable) {
    Serial.println("[警告] 心率传感器不可用，仅用压力模式");
  }
  
  // 初始化通信模块
  initBLE();
  initWiFi();
  
  Serial.println("\n[就绪] 系统启动完成");
  Serial.println("├─ BLE: 等待跳蛋端连接...");
  Serial.println("└─ WiFi: 定时向服务器推送数据\n");
}

void loop() {
  unsigned long now = millis();
  
  // ====== BLE 连接管理 ======
  if (!bleDeviceConnected && oldBleDeviceConnected) {
    delay(500);
    pServer->getAdvertising()->start();
    Serial.println("[BLE] 重新广播中...");
    oldBleDeviceConnected = bleDeviceConnected;
  }
  if (bleDeviceConnected && !oldBleDeviceConnected) {
    oldBleDeviceConnected = bleDeviceConnected;
  }
  
  // ====== WiFi 连接管理 ======
  if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    wifiConnected = false;
    Serial.println("[WiFi] 连接断开，下次发送时重连");
  }
  
  // ====== 传感器采样 (20ms周期) ======
  if (now - lastSampleTime >= SAMPLE_MS) {
    lastSampleTime = now;
    
    // 读取压力
    uint16_t rawPressure = analogRead(FSR_PIN);
    filteredPressure = EMA_ALPHA * rawPressure + (1 - EMA_ALPHA) * filteredPressure;
    pressureLevel = mapPressureToLevel(filteredPressure);
    
    // 读取心率
    irValue = particleSensor.getIR();
    if (checkForBeat(irValue) == true) {
      long delta = now - lastBeat;
      lastBeat = now;
      if (delta > 0) {
        beatsPerMinute = 60.0 / (delta / 1000.0);
        if (beatsPerMinute > 20 && beatsPerMinute < 200) {
          heartRates[heartRateSpot] = (byte)beatsPerMinute;
          heartRateSpot = (heartRateSpot + 1) % RATE_SIZE;
          if (heartRateCount < RATE_SIZE) heartRateCount++;
          beatAvg = 0;
          for (int i = 0; i < heartRateCount; i++) beatAvg += heartRates[i];
          beatAvg /= heartRateCount;
        }
      }
    }
    
    // 融合计算最终档位
    finalLevel = computeFinalLevel();
    
    // BLE发送（实时）
    sendBleLevel(finalLevel);
  }
  
  // ====== WiFi 定时发送 (5秒周期) ======
  if (now - lastWifiSendTime >= WIFI_SEND_INTERVAL) {
    lastWifiSendTime = now;
    sendToWebServer();
  }
  
  // ====== 调试输出 (1秒周期) ======
  if (now - lastDebugTime >= 1000) {
    lastDebugTime = now;
    Serial.printf("[状态] BLE:%s | WiFi:%s | BPM:%.0f | 压力:%.0f | 档位:%d\n",
                  bleDeviceConnected ? "已连接" : "未连接",
                  wifiConnected ? "已连接" : "未连接",
                  beatsPerMinute, filteredPressure, finalLevel);
  }
  
  delay(5);
}
