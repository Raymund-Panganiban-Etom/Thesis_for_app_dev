#include <WiFi.h>
#include <WebServer.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>

// ==========================================
// INTERNAL SD CARD PINS
// ==========================================
#define SD_MMC_CMD  38
#define SD_MMC_CLK  39
#define SD_MMC_D0   40

// ==========================================
// SENSOR PINS
// ==========================================
#define SDA1_PIN 14
#define SCL1_PIN 47  
#define RX1_PIN  17 //swap rx and tx
#define TX1_PIN  18
#define FAN1_PIN 1

#define SDA2_PIN 41
#define SCL2_PIN 42
#define RX2_PIN  15 //swap rx and tx
#define TX2_PIN  16
#define FAN2_PIN 2

// ==========================================
// WI-FI (STATION MODE)
// ==========================================
const char *WIFI_SSID = "Wifi Legendary Etom 4G";
const char *WIFI_PASS = "raymundedwardednajanetedmund";
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// ==========================================
// SENSORS & VARIABLES
// ==========================================
TwoWire I2C_1 = TwoWire(0);
TwoWire I2C_2 = TwoWire(1);
Adafruit_SHT31 sht31_1 = Adafruit_SHT31(&I2C_1);
Adafruit_SHT31 sht31_2 = Adafruit_SHT31(&I2C_2);

HardwareSerial SM4_Serial1(1);
HardwareSerial SM4_Serial2(2);

float temp1 = NAN, hum1 = NAN, eth1 = NAN;
float temp2 = NAN, hum2 = NAN, eth2 = NAN;
unsigned long lastRxTime1 = 0, lastRxTime2 = 0, lastReadTime = 0;

String rawData1 = "None";
String rawData2 = "None";
int count1 = 0;
int count2 = 0;

// Fan thresholds (Ethylene ppm)
const float MIN_PPM = 5.0;    
const float MAX_PPM = 100.0;  

// ==========================================
// UART PARSER (From Datasheet)
// ==========================================
float readSM4(HardwareSerial &port, int sensorNum) {
  if (port.available() > 0) {
    uint8_t tempBuf[50];
    int len = 0;
    
    // Pull all available data
    while (port.available() > 0 && len < 50) {
      tempBuf[len++] = port.read();
    }

    if (sensorNum == 1) { count1 += len; lastRxTime1 = millis(); }
    if (sensorNum == 2) { count2 += len; lastRxTime2 = millis(); }

    // Convert to Hex for UI display
    String hexStr = "";
    for (int i = 0; i < len; i++) {
      char hex[4];
      sprintf(hex, "%02X ", tempBuf[i]);
      hexStr += hex;
    }
    if (sensorNum == 1) rawData1 = hexStr;
    if (sensorNum == 2) rawData2 = hexStr;

    // Parse Datasheet format: FF 86 [High] [Low] [ID] [0] [--] [--] [Checksum]
    for (int i = 0; i <= len - 9; i++) {
      if (tempBuf[i] == 0xFF && tempBuf[i+1] == 0x86) {
        
        // Calculate Checksum based on datasheet
        uint8_t sum = tempBuf[i+1] + tempBuf[i+2] + tempBuf[i+3] + tempBuf[i+4] + tempBuf[i+5] + tempBuf[i+6] + tempBuf[i+7];
        uint8_t expectedChecksum = (~sum) + 1;

        if (expectedChecksum == tempBuf[i+8]) {
          float ppm = (tempBuf[i+2] * 256) + tempBuf[i+3];
          ppm = ppm * 0.1; // Resolution from datasheet (0.1 ppm)
          return ppm;
        }
      }
    }
  }
  return -1.0;
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(300);

  // 1. Stabilize RX pins to stop floating noise (FE / FC)
  pinMode(RX1_PIN, INPUT_PULLUP);
  pinMode(RX2_PIN, INPUT_PULLUP);

  // 2. Init SD Card
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5);

  // 3. Init Wi-Fi (with 20-second timeout so it doesn't freeze)
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  Serial.print("Connecting to Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  WiFi.setSleep(false);

  // 4. Init Sensors
  I2C_1.begin(SDA1_PIN, SCL1_PIN);
  I2C_2.begin(SDA2_PIN, SCL2_PIN);
  sht31_1.begin(0x44);
  sht31_2.begin(0x44);
  
  SM4_Serial1.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN);
  SM4_Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);

  // 5. Init Fans
  ledcSetup(0, 25000, 8); 
  ledcAttachPin(FAN1_PIN, 0);
  ledcSetup(1, 25000, 8); 
  ledcAttachPin(FAN2_PIN, 1);

  // 6. Web Server API Route
  server.on("/api/data", []() {
    // Read SHT31s
    float t1 = sht31_1.readTemperature();
    float h1 = sht31_1.readHumidity();
    if (!isnan(t1)) { temp1 = t1; hum1 = h1; }

    float t2 = sht31_2.readTemperature();
    float h2 = sht31_2.readHumidity();
    if (!isnan(t2)) { temp2 = t2; hum2 = h2; }

    // Determine SM4 online status (active if received data in last 3 seconds)
    int s1 = (millis() - lastRxTime1 < 3000) ? 1 : 0;
    int s2 = (millis() - lastRxTime2 < 3000) ? 1 : 0;

    // Build JSON string
    char jt1[8]="null", jh1[8]="null", je1[8]="null";
    char jt2[8]="null", jh2[8]="null", je2[8]="null";

    if (!isnan(temp1)) snprintf(jt1, sizeof(jt1), "%.1f", temp1);
    if (!isnan(hum1))  snprintf(jh1, sizeof(jh1), "%.1f", hum1);
    if (!isnan(eth1))  snprintf(je1, sizeof(je1), "%.1f", eth1);
    
    if (!isnan(temp2)) snprintf(jt2, sizeof(jt2), "%.1f", temp2);
    if (!isnan(hum2))  snprintf(jh2, sizeof(jh2), "%.1f", hum2);
    if (!isnan(eth2))  snprintf(je2, sizeof(je2), "%.1f", eth2);

    char json[400];
    snprintf(json, sizeof(json),
      "{\"t1\":%s,\"h1\":%s,\"e1\":%s,\"s1\":%d,\"raw1\":\"%s (Bytes: %d)\",\"t2\":%s,\"h2\":%s,\"e2\":%s,\"s2\":%d,\"raw2\":\"%s (Bytes: %d)\"}",
      jt1, jh1, je1, s1, rawData1.c_str(), count1, jt2, jh2, je2, s2, rawData2.c_str(), count2);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
  });

  // Serve UI from SD Card
  server.serveStatic("/", SD_MMC, "/");
  server.begin();
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  server.handleClient();

  // Read UART data continuously
  float val1 = readSM4(SM4_Serial1, 1);
  if (val1 >= 0) eth1 = val1;

  float val2 = readSM4(SM4_Serial2, 2);
  if (val2 >= 0) eth2 = val2;

  // Fan Control Logic (runs every 2 seconds)
  if (millis() - lastReadTime >= 2000) {
    lastReadTime = millis();

    // Map PPM to Fan Speed (0-255)
    int fan1Speed = isnan(eth1) ? 0 : map(eth1 * 100, MIN_PPM * 100, MAX_PPM * 100, 0, 255);
    ledcWrite(0, constrain(fan1Speed, 0, 255));

    int fan2Speed = isnan(eth2) ? 0 : map(eth2 * 100, MIN_PPM * 100, MAX_PPM * 100, 0, 255);
    ledcWrite(1, constrain(fan2Speed, 0, 255));
  }
}
