#include <WiFi.h>
#include <WebServer.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>

// ==========================================
// SD CARD PINS (MMC 1-bit)
// ==========================================
#define SD_MMC_CMD  38
#define SD_MMC_CLK  39
#define SD_MMC_D0   40

// ==========================================
// FANS (LEDC PWM)
// ==========================================
#define FAN1_PIN  6    // Chamber 1 · Intake
#define FAN2_PIN  7    // Chamber 1 · Exhaust
#define FAN3_PIN  4    // Chamber 2 · Intake
#define FAN4_PIN  5    // Chamber 2 · Exhaust

#define FAN_PWM_FREQ  25000
#define FAN_PWM_RES   8      // 0..255

// ==========================================
// SENSORS
// ==========================================
#define SDA1_PIN 14
#define SCL1_PIN 47
#define RX1_PIN  17
#define TX1_PIN  18

#define SDA2_PIN 41
#define SCL2_PIN 42
#define RX2_PIN  15
#define TX2_PIN  16

// ==========================================
// WI-FI — static IP
// ==========================================
const char *WIFI_SSID = "PLDTHOMEFIBR5G1ccc0_Plus";
const char *WIFI_PASS = "PLDTWIFIe2xp7";

#define USE_STATIC_IP true
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway (192, 168, 1, 1);
IPAddress subnet  (255, 255, 255, 0);

WebServer server(80);

// ==========================================
// SENSOR OBJECTS
// ==========================================
TwoWire I2C_1 = TwoWire(0);
TwoWire I2C_2 = TwoWire(1);
Adafruit_SHT31 sht31_1 = Adafruit_SHT31(&I2C_1);
Adafruit_SHT31 sht31_2 = Adafruit_SHT31(&I2C_2);

HardwareSerial SM4_Serial1(1);
HardwareSerial SM4_Serial2(2);

float temp1 = NAN, hum1 = NAN, eth1 = NAN;
float temp2 = NAN, hum2 = NAN, eth2 = NAN;
unsigned long lastRxTime1 = 0, lastRxTime2 = 0;

String rawData1 = "None";
String rawData2 = "None";

// ==========================================
// FAN STATE
// ==========================================
int fanPWM[4] = {0, 0, 0, 0};

// UI pushes every ~1 s. If it goes quiet for 15 s, drop fans to 0.
unsigned long lastCommandMs = 0;
const unsigned long STALE_MS = 15000;

// ==========================================
// UART PARSER (SM4 ethylene sensor)
// ==========================================
float readSM4(HardwareSerial &port, int sensorNum) {
  if (port.available() > 0) {
    uint8_t buf[50];
    int len = 0;
    while (port.available() > 0 && len < 50) buf[len++] = port.read();

    if (sensorNum == 1) lastRxTime1 = millis();
    if (sensorNum == 2) lastRxTime2 = millis();

    String hexStr;
    for (int i = 0; i < len; i++) { char h[4]; sprintf(h, "%02X ", buf[i]); hexStr += h; }
    if (sensorNum == 1) rawData1 = hexStr;
    if (sensorNum == 2) rawData2 = hexStr;

    for (int i = 0; i <= len - 9; i++) {
      if (buf[i] == 0xFF && buf[i+1] == 0x86) {
        uint8_t s = buf[i+1]+buf[i+2]+buf[i+3]+buf[i+4]+buf[i+5]+buf[i+6]+buf[i+7];
        uint8_t chk = (~s) + 1;
        if (chk == buf[i+8]) {
          return ((buf[i+2] << 8) | buf[i+3]) * 0.1f;
        }
      }
    }
  }
  return -1.0f;
}

// ==========================================
// WI-FI HELPERS
// ==========================================
const char* wlStr(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "SCAN_DONE";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (USE_STATIC_IP) {
    if (!WiFi.config(local_IP, gateway, subnet)) {
      Serial.println("[WIFI] !! WiFi.config() failed — falling back to DHCP");
    } else {
      Serial.printf("[WIFI] Static IP %s\n", local_IP.toString().c_str());
    }
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WIFI] Connecting to \"%s\"", WIFI_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 25000) {
    delay(500); Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] CONNECTED");
    Serial.printf("[WIFI] IP   : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] GW   : %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("[WIFI] Mask : %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("[WIFI] RSSI : %d dBm\n", WiFi.RSSI());
    if (WiFi.localIP() == IPAddress(0,0,0,0)) {
      Serial.println("[WIFI] !! IP is 0.0.0.0 — DHCP failed, try another static IP");
    }
  } else {
    Serial.printf("[WIFI] FAILED: %s\n", wlStr(WiFi.status()));
    Serial.println("[WIFI] Scanning…");
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      Serial.printf("  %-32s ch=%2d rssi=%d\n",
                    WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
    }
  }
}

// ==========================================
// FAN HELPERS
// ==========================================
void applyFans() {
  bool stale = (lastCommandMs == 0) || (millis() - lastCommandMs > STALE_MS);
  for (int i = 0; i < 4; i++) {
    int v = stale ? 0 : constrain(fanPWM[i], 0, 255);
    switch (i) {
      case 0: ledcWrite(FAN1_PIN, v); break;
      case 1: ledcWrite(FAN2_PIN, v); break;
      case 2: ledcWrite(FAN3_PIN, v); break;
      case 3: ledcWrite(FAN4_PIN, v); break;
    }
  }
}

// ==========================================
// CORS helper — called at the top of every handler
// ==========================================
void addCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Max-Age", "600");
  server.sendHeader("Cache-Control", "no-store");
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n=== Edge AI Storage · ESP32-S3 ===");
  Serial.printf("Chip: %s rev %d\n", ESP.getChipModel(), ESP.getChipRevision());

  pinMode(RX1_PIN, INPUT_PULLUP);
  pinMode(RX2_PIN, INPUT_PULLUP);

  // SD card
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  bool sdOk = SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5);
  Serial.printf("[SD] Mount %s\n", sdOk ? "OK" : "FAILED");

  // Wi-Fi
  connectWiFi();

  // Fans first
  ledcAttach(FAN1_PIN, FAN_PWM_FREQ, FAN_PWM_RES);
  ledcAttach(FAN2_PIN, FAN_PWM_FREQ, FAN_PWM_RES);
  ledcAttach(FAN3_PIN, FAN_PWM_FREQ, FAN_PWM_RES);
  ledcAttach(FAN4_PIN, FAN_PWM_FREQ, FAN_PWM_RES);
  applyFans();

  // Sensors
  I2C_1.begin(SDA1_PIN, SCL1_PIN);
  I2C_2.begin(SDA2_PIN, SCL2_PIN);
  sht31_1.begin(0x44);
  sht31_2.begin(0x44);

  SM4_Serial1.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN);
  SM4_Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);

  // ==================================================
  // ROUTES
  // ==================================================

  // ---------- /ping ----------
  server.on("/ping", []() {
    addCors();
    server.send(200, "text/plain", "pong");
  });

  // ---------- /diag ----------
  server.on("/diag", []() {
    addCors();
    String j = "{";
    j += "\"ssid\":\""     + String(WIFI_SSID) + "\",";
    j += "\"status\":\""   + String(wlStr(WiFi.status())) + "\",";
    j += "\"ip\":\""       + WiFi.localIP().toString()     + "\",";
    j += "\"gw\":\""       + WiFi.gatewayIP().toString()   + "\",";
    j += "\"mask\":\""     + WiFi.subnetMask().toString()  + "\",";
    j += "\"rssi\":"       + String(WiFi.RSSI())           + ",";
    j += "\"ch\":"         + String(WiFi.channel())        + ",";
    j += "\"uptimeMs\":"   + String(millis())              + ",";
    j += "\"heap\":"       + String(ESP.getFreeHeap())     + ",";
    j += "\"fanPWM\":["    + String(fanPWM[0]) + "," + String(fanPWM[1]) + ","
                           + String(fanPWM[2]) + "," + String(fanPWM[3]) + "]";
    j += "}";
    server.send(200, "application/json", j);
  });

  // ---------- /raw ----------
  server.on("/raw", []() {
    addCors();
    String j = "{";
    j += "\"raw1\":\"" + rawData1 + "\",";
    j += "\"raw2\":\"" + rawData2 + "\",";
    j += "\"s1\":"     + String(millis() - lastRxTime1 < 3000 ? 1 : 0) + ",";
    j += "\"s2\":"     + String(millis() - lastRxTime2 < 3000 ? 1 : 0);
    j += "}";
    server.send(200, "application/json", j);
  });

  // ---------- /api/fans — OPTIONS preflight ----------
  server.on("/api/fans", HTTP_OPTIONS, []() {
    addCors();
    server.send(204);
  });

  // ---------- /api/fans — POST command ----------
  server.on("/api/fans", HTTP_POST, []() {
    addCors();

    if (!server.hasArg("plain")) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"error\":\"body required: f1,f2,f3,f4\"}");
      return;
    }

    String body = server.arg("plain");
    int vals[4] = {0, 0, 0, 0};
    int idx = 0;
    for (unsigned int i = 0; i < body.length() && idx < 4; i++) {
      char c = body[i];
      if (c >= '0' && c <= '9') vals[idx] = vals[idx] * 10 + (c - '0');
      else if (c == ',')        idx++;
    }
    for (int i = 0; i < 4; i++) fanPWM[i] = constrain(vals[i], 0, 255);
    lastCommandMs = millis();
    applyFans();

    char j[80];
    snprintf(j, sizeof(j),
             "{\"ok\":true,\"f\":[%d,%d,%d,%d]}",
             fanPWM[0], fanPWM[1], fanPWM[2], fanPWM[3]);
    server.send(200, "application/json", j);
  });

  // ---------- /api/data — full sensor snapshot (UI does not use this now) ----------
  server.on("/api/data", []() {
    addCors();

    float t1 = sht31_1.readTemperature();
    float h1 = sht31_1.readHumidity();
    if (!isnan(t1)) { temp1 = t1; hum1 = h1; }

    float t2 = sht31_2.readTemperature();
    float h2 = sht31_2.readHumidity();
    if (!isnan(t2)) { temp2 = t2; hum2 = h2; }

    int s1 = (millis() - lastRxTime1 < 3000) ? 1 : 0;
    int s2 = (millis() - lastRxTime2 < 3000) ? 1 : 0;

    auto evalBanana = [&](float t, float h, float e) -> int {
      if (t > 16.0f || h < 85.0f || h > 95.0f || e > 1.00f) return 2;
      if (t > 14.5f || h < 90.0f || h > 95.0f || e > 0.10f) return 1;
      return 0;
    };
    auto evalMixed = [&](float t, float h, float e) -> int {
      if (t > 9.0f  || h < 85.0f || h > 95.0f || e > 0.50f) return 2;
      if (t > 7.5f  || h < 85.0f || h > 95.0f || e > 0.10f) return 1;
      return 0;
    };

    float bE = isnan(eth1) ? 0.0f : eth1;
    float mE = isnan(eth2) ? 0.0f : eth2;
    float bT = isnan(temp1) ? 0.0f : temp1;
    float bH = isnan(hum1)  ? 0.0f : hum1;
    float mT = isnan(temp2) ? 0.0f : temp2;
    float mH = isnan(hum2)  ? 0.0f : hum2;

    int bS = evalBanana(bT, bH, bE);
    int mS = evalMixed(mT, mH, mE);

    const int PWM_TABLE[3] = {51, 128, 204};
    int f1 = PWM_TABLE[bS];
    int f2 = PWM_TABLE[bS];
    int f3 = PWM_TABLE[mS];
    int f4 = PWM_TABLE[mS];

    if (millis() - lastCommandMs <= STALE_MS) {
      f1 = fanPWM[0]; f2 = fanPWM[1]; f3 = fanPWM[2]; f4 = fanPWM[3];
    } else {
      fanPWM[0] = f1; fanPWM[1] = f2; fanPWM[2] = f3; fanPWM[3] = f4;
      applyFans();
    }

    auto fmt = [](float v, int d) -> String {
      if (isnan(v)) return String("null");
      return String(v, d);
    };

    String json = "{";

    json += "\"b\":{";
    json += "\"t\":" + fmt(bT, 2) + ",";
    json += "\"h\":" + fmt(bH, 2) + ",";
    json += "\"e\":" + fmt(bE, 3) + ",";
    json += "\"s\":" + String(bS) + ",";
    json += "\"pwm\":" + String(f1);
    json += "},";

    json += "\"m\":{";
    json += "\"e\":" + fmt(mE, 3) + ",";
    json += "\"s\":" + String(mS) + ",";
    json += "\"pwm\":" + String(f3);
    json += "},";

    struct CropLimit { const char* id; float tWarn, tHigh; };
    CropLimit crops[] = {
      {"sitaw",     7.5f, 9.0f},
      {"pomelo",   12.0f,14.0f},
      {"kalamansi",12.0f,14.0f},
      {"corn",      2.0f, 5.0f}
    };
    json += "\"c\":[";
    for (int i = 0; i < 4; i++) {
      if (i) json += ",";
      int cs;
      if (mT > crops[i].tHigh || mH < 85.0f || mH > 95.0f || mE > 0.50f) cs = 2;
      else if (mT > crops[i].tWarn || mH < 88.0f || mH > 93.0f || mE > 0.10f) cs = 1;
      else cs = 0;
      json += "{\"id\":\"" + String(crops[i].id) + "\",";
      json += "\"t\":"    + fmt(mT, 2) + ",";
      json += "\"h\":"    + fmt(mH, 2) + ",";
      json += "\"s\":"    + String(cs) + "}";
    }
    json += "],";

    json += "\"f\":[" + String(f1) + "," + String(f2) + ","
                     + String(f3) + "," + String(f4) + "]";
    json += "}";

    server.send(200, "application/json", json);
  });

  // ---------- static files from SD ----------
  server.serveStatic("/", SD_MMC, "/");

  server.begin();
  Serial.println("[HTTP] Listening on port 80");
  Serial.printf("[HTTP] Open http://%s/  (or /ping to test)\n",
                WiFi.localIP().toString().c_str());
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  server.handleClient();

  // Read both SM4 sensors
  float v1 = readSM4(SM4_Serial1, 1);
  if (v1 >= 0) eth1 = v1;
  float v2 = readSM4(SM4_Serial2, 2);
  if (v2 >= 0) eth2 = v2;

  // Refresh fan outputs once a second (picks up the fail-safe)
  static unsigned long lastApply = 0;
  if (millis() - lastApply > 1000) {
    lastApply = millis();
    applyFans();
  }
}
