// EV Battery & Charging Monitor - ESP32 (mô phỏng trên Wokwi)
// Đọc điện áp / dòng / nhiệt độ pin, tính SOC, bảo vệ quá áp - thấp áp - quá dòng - quá nhiệt,
// gửi dữ liệu qua MQTT và nhận lệnh điều khiển sạc từ web.

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ---------- Cấu hình mạng / MQTT ----------
// Đổi TOPIC_BASE thành chuỗi riêng của bạn (broker công cộng, ai cũng thấy topic).
// Phải trùng với BASE trong dashboard/index.html.
const char* WIFI_SSID = "Wokwi-GUEST";   // mạng Wi-Fi mặc định của Wokwi
const char* WIFI_PASS = "";
const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;

#define TOPIC_BASE "evbms-uth-demo"
const char* TOPIC_TELEMETRY  = TOPIC_BASE "/telemetry";
const char* TOPIC_CMD_CHARGE = TOPIC_BASE "/cmd/charge";  // "on" | "off"
const char* TOPIC_CMD_LIMIT  = TOPIC_BASE "/cmd/limit";   // 50..100 (%)

// ---------- Chân ----------
const int PIN_VOLT   = 34;  // biến trở: điện áp pack
const int PIN_CURR   = 35;  // biến trở: dòng (+ sạc, - xả)
const int PIN_TEMP   = 32;  // cảm biến NTC
const int PIN_LED    = 25;  // LED báo lỗi
const int PIN_BUZZER = 26;  // còi
const int PIN_RELAY  = 27;  // contactor sạc

// ---------- Mô hình pack 3S (đơn giản hóa) ----------
const float V_RANGE_MIN = 9.0f,  V_RANGE_MAX = 13.0f;  // khoảng biến trở quét
const float I_RANGE_MIN = -20.0f, I_RANGE_MAX = 20.0f;
const float V_EMPTY = 9.3f, V_FULL = 12.6f;             // 0% và 100% SOC
const float I_LIMIT = 15.0f;                            // quá dòng (A)
const float T_LIMIT = 45.0f;                            // quá nhiệt (°C)

WiFiClient wifi;
PubSubClient mqtt(wifi);

bool chargeEnabled = true;
int  limitSoc = 80;

int readAvg(int pin) {
  long sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(pin);
  return sum / 16;
}

float readTempC() {
  const float BETA = 3950.0f;
  int raw = constrain(readAvg(PIN_TEMP), 1, 4094);
  return 1.0f / (log(1.0f / (4095.0f / raw - 1.0f)) / BETA + 1.0f / 298.15f) - 273.15f;
}

const char* checkFault(float v, float i, float t) {
  if (t > T_LIMIT)        return "OVER_TEMP";
  if (v > V_FULL)         return "OVER_VOLT";
  if (v < V_EMPTY)        return "UNDER_VOLT";
  if (fabsf(i) > I_LIMIT) return "OVER_CURRENT";
  return "";
}

void onMessage(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  msg.trim();
  String t(topic);
  if (t == TOPIC_CMD_CHARGE)      chargeEnabled = (msg == "on");
  else if (t == TOPIC_CMD_LIMIT)  limitSoc = constrain(msg.toInt(), 50, 100);
  Serial.printf("[CMD] %s = %s\n", topic, msg.c_str());
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("Dang ket noi Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  while (WiFi.status() != WL_CONNECTED) { delay(250); Serial.print("."); }
  Serial.println(" OK");
}

void connectMqtt() {
  while (!mqtt.connected()) {
    String id = "esp32-evbms-" + String((uint32_t)esp_random(), HEX);
    Serial.print("Dang ket noi MQTT...");
    if (mqtt.connect(id.c_str())) {
      Serial.println(" OK");
      mqtt.subscribe(TOPIC_CMD_CHARGE);
      mqtt.subscribe(TOPIC_CMD_LIMIT);
    } else {
      Serial.printf(" loi rc=%d, thu lai sau 2s\n", mqtt.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  analogReadResolution(12);
  ledcSetup(0, 2000, 8);
  ledcAttachPin(PIN_BUZZER, 0);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(512);
}

void loop() {
  connectWifi();
  connectMqtt();
  mqtt.loop();

  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();

  float v = V_RANGE_MIN + readAvg(PIN_VOLT) / 4095.0f * (V_RANGE_MAX - V_RANGE_MIN);
  float i = I_RANGE_MIN + readAvg(PIN_CURR) / 4095.0f * (I_RANGE_MAX - I_RANGE_MIN);
  float t = readTempC();
  float soc = constrain((v - V_EMPTY) / (V_FULL - V_EMPTY) * 100.0f, 0.0f, 100.0f);

  const char* fault = checkFault(v, i, t);
  bool hasFault = fault[0] != '\0';
  bool relayOn = chargeEnabled && !hasFault && soc < limitSoc;

  digitalWrite(PIN_RELAY, relayOn ? HIGH : LOW);
  digitalWrite(PIN_LED, hasFault ? HIGH : LOW);
  ledcWriteTone(0, hasFault ? 2000 : 0);

  JsonDocument doc;
  doc["v"] = roundf(v * 100) / 100;
  doc["i"] = roundf(i * 10) / 10;
  doc["t"] = roundf(t * 10) / 10;
  doc["soc"] = roundf(soc);
  doc["relay"] = relayOn;
  doc["charge_enabled"] = chargeEnabled;
  doc["limit"] = limitSoc;
  doc["fault"] = fault;
  char buf[256];
  size_t n = serializeJson(doc, buf);
  mqtt.publish(TOPIC_TELEMETRY, (const uint8_t*)buf, n, false);
  Serial.println(buf);
}
