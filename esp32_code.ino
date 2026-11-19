#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include "BananaClassifier.h"
#include "CalamansiClassifier.h"

// ==========================================================
// 1. CONTAINER FAN PWM PIN DEFINITIONS (UNCHANGED)
// ==========================================================
#define PWM_FREQ   25000   // 25 kHz
#define PWM_RES    8       // 8-bit resolution -> 0 to 255
#define DUTY_ON    255     // Max ON
#define DUTY_OFF   0       // OFF

// Container 1 Fan -> Assigned to: BANANA
//   Index 0 (Normal / Low)     = GPIO 4
//   Index 1 (Warning / Medium) = GPIO 5
//   Index 2 (Critical / Max)   = GPIO 13
const int CONTAINER1_PINS[3] = { 4, 5, 13 };

// Container 2 Fan -> Assigned to: CALAMANSI, CORN, POMELO, STRING BEANS
//   Col 0 (Normal / Low)     = GPIO 19, 23, 27
//   Col 1 (Warning / Medium) = GPIO 16, 25, 32
//   Col 2 (Critical / Max)   = GPIO 15, 26, 33
#define C2_GROUPS 3
const int CONTAINER2_PINS[C2_GROUPS][3] = {
  { 19, 16, 15 },
  { 23, 25, 26 },
  { 27, 32, 33 }
};

// ==========================================================
// 2. SENSOR & UART PIN DEFINITIONS (UNCHANGED)
// ==========================================================
// I2C Bus 0 -> SHT31 #1 (Assigned to: BANANA)
#define I2C1_SDA_PIN  14
#define I2C1_SCL_PIN  18

// I2C Bus 1 -> SHT31 #2 (Assigned to: CALAMANSI)
#define I2C2_SDA_PIN  21
#define I2C2_SCL_PIN  22

// UART1 -> Local YYS-SM4-C2H4 Sensor (Assigned to: BANANA)
#define C2H4_RX_PIN   34   // Connects to Local YYS-SM4 TX
#define C2H4_TX_PIN   -1   // Not needed for passive stream

// UART2 -> Bidirectional Link with ESP32-S3
//   RX (GPIO 35) <- Receives S3 SM4-C2H4 & Corn/Pomelo/StringBeans states
//   TX (GPIO 17) -> Sends full Banana & Calamansi readings + Fan states to ESP32-S3
#define S3_RX_PIN     35   // Connects to ESP32-S3 TX2 (GPIO 15)
#define S3_TX_PIN     17   // Connects to ESP32-S3 RX2 (GPIO 16)

// ==========================================================
// 3. SENSOR INSTANCES & ML CLASSIFIERS
// ==========================================================
Adafruit_SHT31 sht31_Banana    = Adafruit_SHT31(&Wire);   // SHT31 #1 -> Banana
Adafruit_SHT31 sht31_Calamansi = Adafruit_SHT31(&Wire1);  // SHT31 #2 -> Calamansi

bool shtBananaReady    = false;
bool shtCalamansiReady = false;

Eloquent::ML::Port::BananaClassifier    bananaClassifier;
Eloquent::ML::Port::CalamansiClassifier calamansiClassifier;

const char* CLASS_NAMES[]     = { "NORMAL", "WARNING", "CRITICAL" };
const char* FAN_SPEED_NAMES[] = { "Low", "Medium", "Maximum" };
const int   NUM_CLASSES       = 3;

enum Esp32Crop {
  CROP_BANANA = 0,
  CROP_CALAMANSI,
  ESP32_CROP_COUNT
};

struct CommodityProfile {
  const char* name;
  float idealTempLow;
  float idealTempHigh;
  float idealHumLow;
  float idealHumHigh;
  float ethNormalMax;
  float ethWarningMax;
  bool  useML;
};

static const CommodityProfile COMMODITY_PROFILES[ESP32_CROP_COUNT] = {
  // name         tLow   tHigh  hLow   hHigh  ethNorm ethWarn useML
  { "Banana",    13.0f, 30.0f, 62.0f, 95.0f,  1.0f,   7.85f,  true },
  { "Calamansi",  8.0f, 20.0f, 70.0f, 95.0f,  0.3f,   1.5f,   true },
};

// Per-crop debounce state (3 consecutive matches to confirm; defaults to 0 = NORMAL)
struct CropDebounceState {
  int consecutiveCount = 0;
  int lastRawState     = -1;
  int confirmedState   = 0;
};

CropDebounceState debounceStates[ESP32_CROP_COUNT];

inline int classifyByRules(Esp32Crop c, float eth) {
  const CommodityProfile& p = COMMODITY_PROFILES[c];
  if (eth <= p.ethNormalMax)  return 0;
  if (eth <= p.ethWarningMax) return 1;
  return 2;
}

inline const char* storageAdvice(Esp32Crop c, float temp, float hum) {
  const CommodityProfile& p = COMMODITY_PROFILES[c];
  if (temp > p.idealTempHigh + 5.0f)  return " [TEMP HIGH]";
  if (temp < p.idealTempLow  - 5.0f)  return " [TEMP LOW]";
  if (hum  > p.idealHumHigh  + 10.0f) return " [HUM HIGH]";
  if (hum  < p.idealHumLow   - 10.0f) return " [HUM LOW]";
  return "";
}

// ==========================================================
// 4. LOCAL ETHYLENE BUFFER & WATCHDOG (BANANA SM4-C2H4)
// ==========================================================
const int ETH_AVG_N = 5;
float ethHistory[ETH_AVG_N] = {0};
int   ethIndex = 0;
int   ethCount = 0;
float ethLast  = 0.0f;

const unsigned long ETH_TIMEOUT_MS  = 5000;
const unsigned long PRINT_PERIOD_MS = 1000;
unsigned long lastEthMs       = 0;
unsigned long lastByteMs      = 0;
unsigned long lastPrintMs     = 0;
bool          ethEverReceived = false;

void pushEthylene(float v) {
  ethHistory[ethIndex] = v;
  ethIndex = (ethIndex + 1) % ETH_AVG_N;
  if (ethCount < ETH_AVG_N) ethCount++;
}

float getSmoothedEthylene() {
  if (ethCount == 0) return ethLast;
  float s = 0;
  for (int i = 0; i < ethCount; i++) s += ethHistory[i];
  return s / ethCount;
}

void resetEthylene() {
  for (int i = 0; i < ETH_AVG_N; i++) ethHistory[i] = 0.0f;
  ethIndex = 0;
  ethCount = 0;
  ethLast  = 0.0f;
  ethEverReceived = false;
}

bool ethyleneIsFresh() {
  return ethEverReceived && (millis() - lastEthMs < ETH_TIMEOUT_MS);
}

void acceptEthylene(float ppm) {
  ethLast = ppm;
  pushEthylene(ppm);
  lastEthMs = millis();
  ethEverReceived = true;
}

// ==========================================================
// 5. REMOTE DATA FROM ESP32-S3 (C2H4 + CORN/POMELO/BEANS STATES)
// ==========================================================
float         remoteS3EthPpm     = 0.0f;
bool          remoteS3EthValid   = false;
int           remoteCornState    = 0; // 0=Normal, 1=Warning, 2=Critical
int           remotePomeloState  = 0;
int           remoteBeansState   = 0;
unsigned long lastRemoteS3Ms     = 0;

void pollRemoteS3UART() {
  while (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();

    // Expected packet from S3: S3_DATA,<ethValid>,<ethPpm>,<cornState>,<pomeloState>,<beansState>
    if (line.startsWith("S3_DATA,")) {
      int validFlag = 0, cState = 0, pState = 0, bState = 0;
      float ppm = 0.0f;
      if (sscanf(line.c_str(), "S3_DATA,%d,%f,%d,%d,%d",
                 &validFlag, &ppm, &cState, &pState, &bState) == 5) {
        remoteS3EthValid  = (validFlag == 1);
        remoteS3EthPpm    = ppm;
        remoteCornState   = constrain(cState, 0, 2);
        remotePomeloState = constrain(pState, 0, 2);
        remoteBeansState  = constrain(bState, 0, 2);
        lastRemoteS3Ms    = millis();
      }
    }
  }
}

// ==========================================================
// 6. YYS SM4-C2H4 BINARY FRAME PARSER (CODE 4 ALGORITHM)
// ==========================================================
void pollBananaEthyleneUART() {
  while (Serial1.available() >= 9) {
    if (Serial1.peek() != 0xFF) {
      Serial1.read();
      continue;
    }

    uint8_t frame[9];
    Serial1.readBytes(frame, 9);
    lastByteMs = millis();

    uint8_t sum = 0;
    for (int i = 1; i <= 7; i++) sum += frame[i];
    uint8_t expected = (uint8_t)((~sum) + 1);
    if (frame[8] != expected) continue;
    if (frame[1] != 0x86)     continue;

    uint16_t raw = ((uint16_t)frame[2] << 8) | frame[3];
    float ppm = raw * 0.1f;
    if (ppm < 0.0f) ppm = 0.0f;
    acceptEthylene(ppm);
  }
  if (Serial1.available()) lastByteMs = millis();
}

// ==========================================================
// 7. FAN PWM OUTPUT CONTROLLERS (ESP32 Core 3.x API)
// ==========================================================
void setContainer1Fan(int state) {
  state = constrain(state, 0, 2);
  for (int s = 0; s < 3; s++) {
    ledcWrite(CONTAINER1_PINS[s], (s == state) ? DUTY_ON : DUTY_OFF);
  }
}

void setContainer2Fan(int state) {
  state = constrain(state, 0, 2);
  for (int g = 0; g < C2_GROUPS; g++) {
    for (int s = 0; s < 3; s++) {
      ledcWrite(CONTAINER2_PINS[g][s], (s == state) ? DUTY_ON : DUTY_OFF);
    }
  }
}

// ==========================================================
// 8. CLASSIFY & REPORT SINGLE CROP
// ==========================================================
int evaluateAndPrintCrop(Esp32Crop crop, float temp, float hum,
                         bool ethValid, bool bytesComing,
                         unsigned long staleMs, float eth,
                         const char* containerLabel, int activeContainerFanState) {
  const CommodityProfile& profile = COMMODITY_PROFILES[crop];
  CropDebounceState& db = debounceStates[crop];

  // Temp/Humidity unavailable -> Fail-safe to NORMAL (0)
  if (isnan(temp) || isnan(hum)) {
    db.consecutiveCount = 0;
    db.lastRawState     = -1;
    db.confirmedState   = 0;

    if (ethValid)
      Serial.printf("[%-9s] SHT31: N/A | C2H4=%.2f ppm -> NORMAL (Fail-Safe) | %s: %s\n",
                    profile.name, eth, containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    else
      Serial.printf("[%-9s] SHT31: N/A | C2H4: DISCONNECTED -> NORMAL (Fail-Safe) | %s: %s\n",
                    profile.name, containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    return db.confirmedState;
  }

  // Ethylene invalid -> Fail-safe to NORMAL (0)
  if (!ethValid) {
    db.consecutiveCount = 0;
    db.lastRawState     = -1;
    db.confirmedState   = 0;

    if (bytesComing)
      Serial.printf("[%-9s] T=%.1f C | H=%.1f %% | C2H4: INVALID FRAME (%.1f s) -> NORMAL (Fail-Safe) | %s: %s\n",
                    profile.name, temp, hum, staleMs / 1000.0,
                    containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    else
      Serial.printf("[%-9s] T=%.1f C | H=%.1f %% | C2H4: DISCONNECTED (%.1f s) -> NORMAL (Fail-Safe) | %s: %s\n",
                    profile.name, temp, hum, staleMs / 1000.0,
                    containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    return db.confirmedState;
  }

  // Classify using trained ML header
  int rawPrediction;
  if (profile.useML) {
    float features[3] = { temp, hum, eth }; // [temp, hum, eth]
    if (crop == CROP_BANANA) {
      rawPrediction = bananaClassifier.predict(features);
    } else {
      rawPrediction = calamansiClassifier.predict(features);
    }
  } else {
    rawPrediction = classifyByRules(crop, eth);
  }

  if (rawPrediction < 0 || rawPrediction >= NUM_CLASSES) {
    Serial.printf("[%-9s] T=%.1f C | H=%.1f %% | C2H4=%.2f ppm -> OUT-OF-RANGE(%d) | %s: %s\n",
                  profile.name, temp, hum, eth, rawPrediction,
                  containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    return db.confirmedState;
  }

  // Debounce (3 consecutive matches to confirm)
  if (rawPrediction == db.lastRawState) {
    if (db.consecutiveCount < 100) db.consecutiveCount++;
    if (db.consecutiveCount >= 3) db.confirmedState = rawPrediction;
  } else {
    db.consecutiveCount = 1;
    db.lastRawState     = rawPrediction;
  }

  const char* advice = storageAdvice(crop, temp, hum);
  const char* method = profile.useML ? "ML" : "RULE";
  Serial.printf("[%-9s] T=%.1f C | H=%.1f %% | C2H4=%.2f ppm -> %-8s (%s) | Crop Fan: %-7s | %s: %-7s%s\n",
                profile.name, temp, hum, eth,
                CLASS_NAMES[db.confirmedState], method,
                FAN_SPEED_NAMES[db.confirmedState],
                containerLabel, FAN_SPEED_NAMES[activeContainerFanState],
                advice);

  return db.confirmedState;
}

// ==========================================================
// SETUP (ESP32 Core 3.0.0+)
// ==========================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32: Banana & Calamansi Spoilage Monitor + Container Fan PWM ===");

  // 1. Setup Container 1 Fan (3 pins) & Container 2 Fan (9 pins)
  for (int s = 0; s < 3; s++) {
    ledcAttach(CONTAINER1_PINS[s], PWM_FREQ, PWM_RES);
  }
  for (int g = 0; g < C2_GROUPS; g++) {
    for (int s = 0; s < 3; s++) {
      ledcAttach(CONTAINER2_PINS[g][s], PWM_FREQ, PWM_RES);
    }
  }

  // Start both fans at Normal (Low)
  setContainer1Fan(0);
  setContainer2Fan(0);

  // 2. Setup Two Independent I2C Buses
  //    SHT31 #1 (GPIO 14/18) -> Banana
  //    SHT31 #2 (GPIO 21/22) -> Calamansi
  Wire.begin(I2C1_SDA_PIN, I2C1_SCL_PIN);
  Wire1.begin(I2C2_SDA_PIN, I2C2_SCL_PIN);

  shtBananaReady    = sht31_Banana.begin(0x44);
  shtCalamansiReady = sht31_Calamansi.begin(0x44);
  Serial.printf("SHT31 #1 Banana (GPIO 14/18): %s | SHT31 #2 Calamansi (GPIO 21/22): %s\n",
                shtBananaReady    ? "OK" : "NOT FOUND",
                shtCalamansiReady ? "OK" : "NOT FOUND");

  // 3. Setup UART1 (Banana YYS-SM4-C2H4) & UART2 (Link with ESP32-S3)
  Serial1.begin(9600, SERIAL_8N1, C2H4_RX_PIN, C2H4_TX_PIN);
  Serial1.setTimeout(50);
  Serial2.begin(115200, SERIAL_8N1, S3_RX_PIN, S3_TX_PIN);
  Serial2.setTimeout(50);
}

// ==========================================================
// LOOP
// ==========================================================
void loop() {
  unsigned long now = millis();

  pollBananaEthyleneUART();
  pollRemoteS3UART();

  if (now - lastPrintMs < PRINT_PERIOD_MS) return;
  lastPrintMs = now;

  // --- Read SHT31 #1 (Banana) ---
  if (!shtBananaReady) shtBananaReady = sht31_Banana.begin(0x44);
  float tBanana = NAN, hBanana = NAN;
  if (shtBananaReady) {
    tBanana = sht31_Banana.readTemperature();
    hBanana = sht31_Banana.readHumidity();
    if (isnan(tBanana) || isnan(hBanana)) shtBananaReady = false;
  }

  // --- Read SHT31 #2 (Calamansi) ---
  if (!shtCalamansiReady) shtCalamansiReady = sht31_Calamansi.begin(0x44);
  float tCalamansi = NAN, hCalamansi = NAN;
  if (shtCalamansiReady) {
    tCalamansi = sht31_Calamansi.readTemperature();
    hCalamansi = sht31_Calamansi.readHumidity();
    if (isnan(tCalamansi) || isnan(hCalamansi)) shtCalamansiReady = false;
  }

  // --- Banana SM4-C2H4 Freshness (Local UART1) ---
  bool bananaEthValid = ethyleneIsFresh();
  if (!bananaEthValid && ethEverReceived) resetEthylene();
  float bananaEth = bananaEthValid ? getSmoothedEthylene() : 0.0f;
  bool bananaBytesComing = (now - lastByteMs < ETH_TIMEOUT_MS);
  unsigned long bananaStaleMs = bananaBytesComing ? (now - lastEthMs) : (now - lastByteMs);

  // --- Calamansi SM4-C2H4 & S3 Crops Freshness (Received via UART2 from ESP32-S3) ---
  bool s3LinkFresh = (now - lastRemoteS3Ms < ETH_TIMEOUT_MS);
  if (!s3LinkFresh) {
    remoteS3EthValid  = false;
    remoteCornState   = 0; // Fail-safe to Normal
    remotePomeloState = 0;
    remoteBeansState  = 0;
  }
  bool calamansiEthValid = remoteS3EthValid && s3LinkFresh;
  float calamansiEth     = calamansiEthValid ? remoteS3EthPpm : 0.0f;
  unsigned long calamansiStaleMs = now - lastRemoteS3Ms;

  // --- Determine Highest Severity for Container 2 Fan (4 Crops) ---
  int calamansiCurrentState = debounceStates[CROP_CALAMANSI].confirmedState;
  int container2FanState = max(calamansiCurrentState,
                           max(remoteCornState,
                           max(remotePomeloState, remoteBeansState)));

  Serial.println("====================================================================================");
  int bananaState = evaluateAndPrintCrop(
      CROP_BANANA, tBanana, hBanana,
      bananaEthValid, bananaBytesComing, bananaStaleMs, bananaEth,
      "Container 1 Fan", debounceStates[CROP_BANANA].confirmedState);

  int calamansiState = evaluateAndPrintCrop(
      CROP_CALAMANSI, tCalamansi, hCalamansi,
      calamansiEthValid, false, calamansiStaleMs, calamansiEth,
      "Container 2 Fan", container2FanState);

  // Recompute Container 2 Fan state with latest Calamansi state & apply PWM outputs
  container2FanState = max(calamansiState,
                       max(remoteCornState,
                       max(remotePomeloState, remoteBeansState)));

  setContainer1Fan(bananaState);
  setContainer2Fan(container2FanState);

  Serial.printf(">> ACTIVE FANS | Container 1 Fan (Banana): %-7s (GPIO %d=255) | Container 2 Fan (4 Crops): %-7s\n",
                FAN_SPEED_NAMES[bananaState], CONTAINER1_PINS[bananaState],
                FAN_SPEED_NAMES[container2FanState]);

  // Send full Banana & Calamansi readings + Fan states to ESP32-S3 over UART2
  // Use -999.0 for NaN SHT31 readings and -1.0 for invalid/disconnected C2H4
  float txTBan = isnan(tBanana)    ? -999.0f : tBanana;
  float txHBan = isnan(hBanana)    ? -999.0f : hBanana;
  float txEBan = bananaEthValid    ? bananaEth : -1.0f;

  float txTCal = isnan(tCalamansi) ? -999.0f : tCalamansi;
  float txHCal = isnan(hCalamansi) ? -999.0f : hCalamansi;
  float txECal = calamansiEthValid ? calamansiEth : -1.0f;

  char txPacket[128];
  snprintf(txPacket, sizeof(txPacket),
           "ESP32_DATA,%.2f,%.2f,%.2f,%d,%d,%.2f,%.2f,%.2f,%d,%d",
           txTBan, txHBan, txEBan, bananaState, bananaState,
           txTCal, txHCal, txECal, calamansiState, container2FanState);
  Serial2.println(txPacket);
}