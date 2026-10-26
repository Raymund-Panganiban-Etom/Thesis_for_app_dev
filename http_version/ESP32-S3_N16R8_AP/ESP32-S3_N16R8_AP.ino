#include <WiFi.h>
#include <WebServer.h>
#include <SD_MMC.h>       // Using SD_MMC for Freenove board
#include <Wire.h>
#include <Adafruit_SHT31.h>

// ==========================================
// SD CARD PINS (From Freenove sd_read_write.h)
// ==========================================
#define SD_MMC_CMD  38 
#define SD_MMC_CLK  39 
#define SD_MMC_D0   40 

// ==========================================
// PIN DEFINITIONS & HARDWARE
// ==========================================
// Container 1
#define SDA1_PIN 8
#define SCL1_PIN 9
#define RX1_PIN 17
#define TX1_PIN 18
#define FAN1_PIN 6

// Container 2
#define SDA2_PIN 4
#define SCL2_PIN 5
#define RX2_PIN 15
#define TX2_PIN 16
#define FAN2_PIN 7

// ==========================================
// WI-FI AP SETTINGS
// ==========================================
const char *ssid_AP     = "ESP32-Storage"; // The Wi-Fi name you will connect to
const char *password_AP = "12345678";      // Minimum 8 characters

IPAddress local_IP(192, 168, 1, 100);      // The IP address you will type in the browser
IPAddress gateway(192, 168, 1, 10);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// ==========================================
// SENSOR OBJECTS & VARIABLES
// ==========================================
TwoWire I2C_1 = TwoWire(0);
TwoWire I2C_2 = TwoWire(1);
Adafruit_SHT31 sht31_1 = Adafruit_SHT31(&I2C_1);
Adafruit_SHT31 sht31_2 = Adafruit_SHT31(&I2C_2);

HardwareSerial Ethylene1(1);
HardwareSerial Ethylene2(2);

float temp1 = 0.0, hum1 = 0.0, eth1 = 0.0;
float temp2 = 0.0, hum2 = 0.0, eth2 = 0.0;
unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 2000; 

// Fan mapping (Ethylene ppm to PWM 0-255)
const float MIN_PPM = 5.0;    
const float MAX_PPM = 100.0;  
const int MIN_PWM = 0;        
const int MAX_PWM = 255;      

// ==========================================
// HELPER: Read Ethylene UART
// ==========================================
float getEthyleneReading(HardwareSerial &serialPort) {
  if (serialPort.available()) {
    String data = serialPort.readStringUntil('\n');
    data.trim();
    float val = data.toFloat();
    if (val > 0.0 && val < 500.0) return val;
  }
  return -1.0;
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(300);

  // 1. Init SD Card (SD_MMC Mode)
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  // Parameters: mountpoint, mode1bit, format_if_mount_failed, freq, max_files
  if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)) {
    Serial.println("ERROR: SD Card Mount Failed! Check wiring & format.");
  } else {
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
      Serial.println("No SD_MMC card attached");
    } else {
      Serial.println("SD Card mounted successfully.");
      Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    }
  }

  // 2. Init Wi-Fi Access Point
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  if (WiFi.softAP(ssid_AP, password_AP)) {
    Serial.println("Wi-Fi AP Ready!");
    Serial.print("Connect to Wi-Fi: "); Serial.println(ssid_AP);
    Serial.print("Then open browser to: http://"); Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Wi-Fi AP Failed!");
  }

  // 3. Init Sensors (I2C & UART)
  I2C_1.begin(SDA1_PIN, SCL1_PIN);
  I2C_2.begin(SDA2_PIN, SCL2_PIN);
  sht31_1.begin(0x44);
  sht31_2.begin(0x44);
  Ethylene1.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN);
  Ethylene2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);

  // 4. Init PWM for Fans
  ledcSetup(0, 25000, 8);
  ledcAttachPin(FAN1_PIN, 0);
  ledcWrite(0, 0);
  ledcSetup(1, 25000, 8);
  ledcAttachPin(FAN2_PIN, 1);
  ledcWrite(1, 0);

  // 5. Setup Web Server API Route
  server.on("/api/data", []() {
    // Force read sensors right before sending JSON
    float t1 = sht31_1.readTemperature();
    float h1 = sht31_1.readHumidity();
    if (!isnan(t1)) temp1 = t1;
    if (!isnan(h1)) hum1 = h1;

    float t2 = sht31_2.readTemperature();
    float h2 = sht31_2.readHumidity();
    if (!isnan(t2)) temp2 = t2;
    if (!isnan(h2)) hum2 = h2;

    // Read Ethylene UART
    float ne1 = getEthyleneReading(Ethylene1);
    if (ne1 >= 0) eth1 = ne1;
    float ne2 = getEthyleneReading(Ethylene2);
    if (ne2 >= 0) eth2 = ne2;

    // Create JSON string
    char json[150];
    snprintf(json, sizeof(json), 
      "{\"t1\":%.1f,\"h1\":%.1f,\"e1\":%.1f,\"t2\":%.1f,\"h2\":%.1f,\"e2\":%.1f}", 
      temp1, hum1, eth1, temp2, hum2, eth2);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
  });

  // 6. Serve static files (HTML, CSS, JS, Images) from SD_MMC
  server.serveStatic("/", SD_MMC, "/");

  server.begin();
  Serial.println("HTTP Server Started. Waiting for connections...");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  // Handle browser requests
  server.handleClient();

  // Fan control logic (runs every 2 seconds)
  if (millis() - lastReadTime >= READ_INTERVAL) {
    lastReadTime = millis();
    
    int fan1Speed = map(eth1 * 100, MIN_PPM * 100, MAX_PPM * 100, MIN_PWM, MAX_PWM);
    ledcWrite(0, constrain(fan1Speed, MIN_PWM, MAX_PWM));

    int fan2Speed = map(eth2 * 100, MIN_PPM * 100, MAX_PPM * 100, MIN_PWM, MAX_PWM);
    ledcWrite(1, constrain(fan2Speed, MIN_PWM, MAX_PWM));

    Serial.printf("C1: T:%.1f H:%.1f E:%.1f | C2: T:%.1f H:%.1f E:%.1f\n", 
                  temp1, hum1, eth1, temp2, hum2, eth2);
  }
}