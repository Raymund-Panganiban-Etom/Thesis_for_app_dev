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
#define C2H4_RX_PIN   34   // Connects to Local YYS-SM4 TX (Input-only pin)
#define C2H4_TX_PIN   -1   // Not needed for passive stream

// UART2 -> Bidirectional Link with ESP32-S3
//   RX (GPIO 35) <- Receives S3 SM4-C2H4 & Corn/Pomelo/StringBeans states
//   TX (GPIO 17) -> Sends full Banana & Calamansi readings + Fan states to ESP32-S3
#define S3_RX_PIN     35   // Connects to ESP32-S3 TX2 (GPIO 15)
#define S3_TX_PIN     17   // Connects to ESP32-S3 RX2 (GPIO 16)

// ==========================================================
// 3. SENSOR INSTANCES, ML CLASSIFIERS & CSV THRESHOLDS
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

// Instant escalation on high gas/heat/dryness; stable hold on recovery
const int   UP_DEBOUNCE_TICKS   = 1;     // Instant trigger on spike (500 ms loop)
const int   DOWN_DEBOUNCE_TICKS = 4;     // 2.0 s sustained drop before stepping down
const float ETH_HYST_PPM        = 0.20f; // Anti-chatter band around Warning/Critical thresholds
const float TEMP_HYST_C         = 0.30f;
const float HUM_HYST_PCT        = 0.50f;

enum Esp32Crop {
  CROP_BANANA = 0,
  CROP_CALAMANSI,
  ESP32_CROP_COUNT
};

// Exact CSV decision boundaries extracted from BananaClassifier.h & CalamansiClassifier.h
// Note: Humidity follows an inverse relationship (lower RH = higher spoilage risk)
struct CommodityProfile {
  const char* name;
  float tempNormMax;   // Normal: <= tempNormMax | Warning: tempNormMax..tempWarnMax | Critical: > tempWarnMax
  float tempWarnMax;
  float humNormMin;    // Normal: >  humNormMin  | Warning: humWarnMin..humNormMin   | Critical: <= humWarnMin
  float humWarnMin;
  float ethNormMax;    // Normal: <= ethNormMax  | Warning: ethNormMax..ethWarnMax   | Critical: > ethWarnMax
  float ethWarnMax;
  bool  useML;
};

static const CommodityProfile COMMODITY_PROFILES[ESP32_CROP_COUNT] = {
  // name         tNorm   tWarn   hNorm   hWarn   eNorm   eWarn   useML
  { "Banana",    31.01f, 36.01f, 71.03f, 63.50f, 3.85f,  7.85f,  true },
  { "Calamansi", 31.01f, 36.01f, 71.05f, 63.50f, 2.04f,  4.75f,  true }
};

struct CropDebounceState {
  int upCount        = 0;
  int downCount      = 0;
  int confirmedState = 0;
  int lastTState     = 0;
  int lastHState     = 0;
  int lastEState     = 0;
};

CropDebounceState debounceStates[ESP32_CROP_COUNT];

enum EvalStatus {
  EVAL_OK = 0,
  EVAL_GAS_ONLY_SHT_NA,
  EVAL_FAILSAFE_ALL,
  EVAL_FAILSAFE_ETH,
  EVAL_OUT_OF_RANGE
};

struct CropEvalResult {
  EvalStatus status            = EVAL_OK;
  int        confirmed         = 0;
  int        rawPrediction     = 0;
  bool       overrideTriggered = false;
  bool       stateChanged      = false;
};

inline int evalUpperWithHyst(float val, float normMax, float warnMax, float hyst, int prevState) {
  if (val > warnMax) return 2;
  if (prevState == 2 && val > (warnMax - hyst)) return 2;
  if (val > normMax) return 1;
  if (prevState >= 1 && val > (normMax - hyst)) return 1;
  return 0;
}

inline int evalLowerWithHyst(float val, float normMin, float warnMin, float hyst, int prevState) {
  if (val <= warnMin) return 2;
  if (prevState == 2 && val <= (warnMin + hyst)) return 2;
  if (val <= normMin) return 1;
  if (prevState >= 1 && val <= (normMin + hyst)) return 1;
  return 0;
}

inline void applyAsymmetricDebounce(CropDebounceState& db, int targetState, bool& changedOut) {
  int prevConfirmed = db.confirmedState;

  if (targetState > db.confirmedState) {
    db.downCount = 0;
    db.upCount++;
    if (db.upCount >= UP_DEBOUNCE_TICKS) {
      db.confirmedState = targetState;
      db.upCount        = 0;
    }
  } else if (targetState < db.confirmedState) {
    db.upCount = 0;
    db.downCount++;
    if (db.downCount >= DOWN_DEBOUNCE_TICKS) {
      db.confirmedState = targetState;
      db.downCount      = 0;
    }
  } else {
    db.upCount   = 0;
    db.downCount = 0;
  }

  changedOut = (db.confirmedState != prevConfirmed);
}

void formatStorageAdvice(Esp32Crop c, float temp, float hum, float eth,
                         char* outBuf, size_t bufLen) {
  const CommodityProfile& p = COMMODITY_PROFILES[c];
  outBuf[0] = '\0';

  if (!isnan(temp)) {
    if (temp > p.tempWarnMax)       strncat(outBuf, " [TEMP CRIT]", bufLen - strlen(outBuf) - 1);
    else if (temp > p.tempNormMax)  strncat(outBuf, " [TEMP WARN]", bufLen - strlen(outBuf) - 1);
  }
  if (!isnan(hum)) {
    if (hum <= p.humWarnMin)        strncat(outBuf, " [HUM CRIT]",  bufLen - strlen(outBuf) - 1);
    else if (hum <= p.humNormMin)   strncat(outBuf, " [HUM WARN]",  bufLen - strlen(outBuf) - 1);
  }
  if (eth > p.ethWarnMax)           strncat(outBuf, " [C2H4 CRIT]", bufLen - strlen(outBuf) - 1);
  else if (eth > p.ethNormMax)      strncat(outBuf, " [C2H4 WARN]", bufLen - strlen(outBuf) - 1);
}

bool readSht31WithRetry(Adafruit_SHT31& sensor, bool& readyFlag, float& outTemp, float& outHum) {
  if (!readyFlag) readyFlag = sensor.begin(0x44);
  if (!readyFlag) {
    outTemp = NAN;
    outHum  = NAN;
    return false;
  }

  for (int attempt = 0; attempt < 2; attempt++) {
    outTemp = sensor.readTemperature();
    outHum  = sensor.readHumidity();
    if (!isnan(outTemp) && !isnan(outHum)) return true;
    delay(5);
  }

  readyFlag = false;
  outTemp   = NAN;
  outHum    = NAN;
  return false;
}

// ==========================================================
// 4. LOCAL RAW ETHYLENE STATE & WINDOW PEAK CAPTURE (BANANA)
// ==========================================================
const unsigned long ETH_TIMEOUT_MS    = 5000;
const unsigned long CONTROL_PERIOD_MS = 500;   // Fast 500 ms ML + PWM + UART2 sync
const unsigned long PRINT_PERIOD_MS   = 1000;  // Periodic Serial Monitor log (plus instant on state change)

float         bananaEthLatest      = 0.0f;
float         bananaEthWindowPeak  = 0.0f;
bool          bananaWindowHasFrame = false;
unsigned long lastEthMs            = 0;
unsigned long lastByteMs           = 0;
unsigned long lastControlMs        = 0;
unsigned long lastPrintMs          = 0;
bool          ethEverReceived      = false;
uint8_t       bananaLastFrame[9]   = { 0 };
bool          bananaHasFrame       = false;

void resetEthylene() {
  bananaEthLatest      = 0.0f;
  bananaEthWindowPeak  = 0.0f;
  bananaWindowHasFrame = false;
  ethEverReceived      = false;
}

bool ethyleneIsFresh() {
  return ethEverReceived && (millis() - lastEthMs < ETH_TIMEOUT_MS);
}

void acceptEthylene(float ppm) {
  bananaEthLatest = ppm;
  if (!bananaWindowHasFrame || ppm > bananaEthWindowPeak) {
    bananaEthWindowPeak  = ppm;
    bananaWindowHasFrame = true;
  }
  lastEthMs       = millis();
  ethEverReceived = true;
}

float consumeBananaRawPpm() {
  if (!ethyleneIsFresh()) return 0.0f;
  if (bananaWindowHasFrame) {
    float peak = bananaEthWindowPeak;
    bananaWindowHasFrame = false;
    return peak;
  }
  return bananaEthLatest;
}

// ==========================================================
// 5. REMOTE DATA FROM ESP32-S3 (NON-BLOCKING UART2 PARSER)
// ==========================================================
float         remoteS3EthPpm     = 0.0f;
bool          remoteS3EthValid   = false;
int           remoteCornState    = 0; // 0=Normal, 1=Warning, 2=Critical
int           remotePomeloState  = 0;
int           remoteBeansState   = 0;
unsigned long lastRemoteS3Ms     = 0;

char s3RxBuf[96];
size_t s3RxLen = 0;

void parseRemoteS3Line(const char* line) {
  // Expected packet from S3: S3_DATA,<ethValid>,<ethPpm>,<cornState>,<pomeloState>,<beansState>
  if (strncmp(line, "S3_DATA,", 8) == 0) {
    int validFlag = 0, cState = 0, pState = 0, bState = 0;
    float ppm = 0.0f;
    if (sscanf(line, "S3_DATA,%d,%f,%d,%d,%d",
               &validFlag, &ppm, &cState, &pState, &bState) == 5) {
      remoteS3EthValid  = (validFlag == 1);
      remoteS3EthPpm    = (ppm < 0.0f) ? 0.0f : ppm;
      remoteCornState   = constrain(cState, 0, 2);
      remotePomeloState = constrain(pState, 0, 2);
      remoteBeansState  = constrain(bState, 0, 2);
      lastRemoteS3Ms    = millis();
    }
  }
}

void pollRemoteS3UART() {
  while (Serial2.available()) {
    char ch = (char)Serial2.read();
    if (ch == '\r') continue;

    if (ch == '\n') {
      if (s3RxLen > 0) {
        s3RxBuf[s3RxLen] = '\0';
        parseRemoteS3Line(s3RxBuf);
        s3RxLen = 0;
      }
    } else if (s3RxLen < sizeof(s3RxBuf) - 1) {
      s3RxBuf[s3RxLen++] = ch;
    } else {
      s3RxLen = 0;
    }
  }
}

// ==========================================================
// 6. YYS SM4-C2H4 BINARY FRAME PARSER (PURE RAW DATA, UNCAPPED)
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

    memcpy(bananaLastFrame, frame, 9);
    bananaHasFrame = true;

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
// 8. RESPONSIVE HYBRID CLASSIFY (CSV ML + SINGLE-SENSOR SAFETY)
// ==========================================================
CropEvalResult evaluateCrop(Esp32Crop crop, float temp, float hum,
                            bool ethValid, float eth) {
  const CommodityProfile& profile = COMMODITY_PROFILES[crop];
  CropDebounceState& db = debounceStates[crop];
  CropEvalResult res;

  // Both SHT31 and C2H4 unavailable -> Fail-safe to NORMAL (0)
  if ((isnan(temp) || isnan(hum)) && !ethValid) {
    bool changed = (db.confirmedState != 0);
    db.upCount        = 0;
    db.downCount      = 0;
    db.confirmedState = 0;
    db.lastTState     = 0;
    db.lastHState     = 0;
    db.lastEState     = 0;
    res.status        = EVAL_FAILSAFE_ALL;
    res.confirmed     = 0;
    res.stateChanged  = changed;
    return res;
  }

  // SHT31 unavailable, but C2H4 IS valid -> Gas-only evaluation (still detects WARNING & CRITICAL)
  if (isnan(temp) || isnan(hum)) {
    db.lastTState = 0;
    db.lastHState = 0;
    int eState = evalUpperWithHyst(eth, profile.ethNormMax, profile.ethWarnMax, ETH_HYST_PPM, db.lastEState);
    db.lastEState = eState;

    applyAsymmetricDebounce(db, eState, res.stateChanged);
    res.status        = EVAL_GAS_ONLY_SHT_NA;
    res.rawPrediction = eState;
    res.confirmed     = db.confirmedState;
    return res;
  }

  // Ethylene invalid -> Fail-safe to NORMAL (0)
  if (!ethValid) {
    bool changed = (db.confirmedState != 0);
    db.upCount        = 0;
    db.downCount      = 0;
    db.confirmedState = 0;
    db.lastEState     = 0;
    res.status        = EVAL_FAILSAFE_ETH;
    res.confirmed     = 0;
    res.stateChanged  = changed;
    return res;
  }

  // Evaluate exact CSV per-sensor rules with hysteresis on pure raw values
  int tState = evalUpperWithHyst(temp, profile.tempNormMax, profile.tempWarnMax, TEMP_HYST_C,  db.lastTState);
  int hState = evalLowerWithHyst(hum,  profile.humNormMin,  profile.humWarnMin,  HUM_HYST_PCT, db.lastHState);
  int eState = evalUpperWithHyst(eth,  profile.ethNormMax,  profile.ethWarnMax,  ETH_HYST_PPM, db.lastEState);
  db.lastTState = tState;
  db.lastHState = hState;
  db.lastEState = eState;
  int ruleState = max(tState, max(hState, eState));

  int rawPrediction = ruleState;
  if (profile.useML) {
    float features[3] = { temp, hum, eth };
    int mlPrediction = (crop == CROP_BANANA)
                         ? bananaClassifier.predict(features)
                         : calamansiClassifier.predict(features);

    if (mlPrediction < 0 || mlPrediction >= NUM_CLASSES) {
      res.status        = EVAL_OUT_OF_RANGE;
      res.rawPrediction = mlPrediction;
      res.confirmed     = db.confirmedState;
      return res;
    }

    if (ruleState > mlPrediction) {
      rawPrediction         = ruleState;
      res.overrideTriggered = true;
    } else {
      rawPrediction         = mlPrediction;
      res.overrideTriggered = false;
    }
  }

  applyAsymmetricDebounce(db, rawPrediction, res.stateChanged);
  res.status        = EVAL_OK;
  res.rawPrediction = rawPrediction;
  res.confirmed     = db.confirmedState;
  return res;
}

void printCropReport(Esp32Crop crop, const CropEvalResult& res,
                     float temp, float hum, bool ethValid,
                     bool bytesComing, unsigned long staleMs, float eth,
                     const char* containerLabel, int activeContainerFanState) {
  const CommodityProfile& profile = COMMODITY_PROFILES[crop];

  if (res.status == EVAL_FAILSAFE_ALL) {
    Serial.printf("[%-9s] SHT31: N/A | C2H4: DISCONNECTED -> NORMAL (Fail-Safe) | %s: %s\n",
                  profile.name, containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    return;
  }

  if (res.status == EVAL_GAS_ONLY_SHT_NA) {
    char advice[48];
    formatStorageAdvice(crop, NAN, NAN, eth, advice, sizeof(advice));
    Serial.printf("[%-9s] SHT31: N/A | C2H4=%.2f ppm -> %-8s (GAS-ONLY) | Crop Fan: %-7s | %s: %-7s%s\n",
                  profile.name, eth,
                  CLASS_NAMES[res.confirmed],
                  FAN_SPEED_NAMES[res.confirmed],
                  containerLabel, FAN_SPEED_NAMES[activeContainerFanState],
                  advice);
    return;
  }

  if (res.status == EVAL_FAILSAFE_ETH) {
    if (bytesComing)
      Serial.printf("[%-9s] T=%.1f C | H=%.1f %% | C2H4: INVALID FRAME (%.1f s) -> NORMAL (Fail-Safe) | %s: %s\n",
                    profile.name, temp, hum, staleMs / 1000.0,
                    containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    else
      Serial.printf("[%-9s] T=%.1f C | H=%.1f %% | C2H4: WAITING/DISCONNECTED (%.1f s) -> NORMAL (Fail-Safe) | %s: %s\n",
                    profile.name, temp, hum, staleMs / 1000.0,
                    containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    return;
  }

  if (res.status == EVAL_OUT_OF_RANGE) {
    Serial.printf("[%-9s] T=%.1f C | H=%.1f %% | C2H4=%.2f ppm -> OUT-OF-RANGE(%d) | %s: %s\n",
                  profile.name, temp, hum, eth, res.rawPrediction,
                  containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    return;
  }

  char advice[48];
  formatStorageAdvice(crop, temp, hum, eth, advice, sizeof(advice));
  const char* method = !profile.useML ? "RULE" : (res.overrideTriggered ? "ML+SAFE" : "ML");

  Serial.printf("[%-9s] T=%.1f C | H=%.1f %% | C2H4=%.2f ppm -> %-8s (%-7s) | Crop Fan: %-7s | %s: %-7s%s\n",
                profile.name, temp, hum, eth,
                CLASS_NAMES[res.confirmed], method,
                FAN_SPEED_NAMES[res.confirmed],
                containerLabel, FAN_SPEED_NAMES[activeContainerFanState],
                advice);
}

// ==========================================================
// SETUP (ESP32.DEV / ESP32 Arduino Core 3.0.0+)
// ==========================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32.DEV: Banana & Calamansi Edge AI Monitor (CSV THRESHOLDS + RAW GAS) ===");

  // 1. Setup Container 1 Fan (3 pins) & Container 2 Fan (9 pins) -> 12 LEDC channels total
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

  // 2. Setup Two Independent I2C Buses (100 kHz + 50 ms timeout)
  Wire.begin(I2C1_SDA_PIN, I2C1_SCL_PIN);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  Wire1.begin(I2C2_SDA_PIN, I2C2_SCL_PIN);
  Wire1.setClock(100000);
  Wire1.setTimeOut(50);

  shtBananaReady    = sht31_Banana.begin(0x44);
  shtCalamansiReady = sht31_Calamansi.begin(0x44);
  Serial.printf("SHT31 #1 Banana (GPIO 14/18): %s | SHT31 #2 Calamansi (GPIO 21/22): %s\n",
                shtBananaReady    ? "OK" : "NOT FOUND",
                shtCalamansiReady ? "OK" : "NOT FOUND");

  // 3. Setup UART1 (Banana YYS-SM4-C2H4) & UART2 (Link with ESP32-S3)
  Serial1.setRxBufferSize(256);
  Serial1.begin(9600, SERIAL_8N1, C2H4_RX_PIN, C2H4_TX_PIN);
  Serial1.setTimeout(50);
  Serial2.setRxBufferSize(256);
  Serial2.begin(115200, SERIAL_8N1, S3_RX_PIN, S3_TX_PIN);
}

// ==========================================================
// LOOP (500 ms Fast Control & UART2 Sync)
// ==========================================================
void loop() {
  unsigned long now = millis();

  pollBananaEthyleneUART();
  pollRemoteS3UART();

  if (now - lastControlMs < CONTROL_PERIOD_MS) return;
  lastControlMs = now;

  // --- Read SHT31 #1 (Banana) & SHT31 #2 (Calamansi) with retry ---
  float tBanana = NAN, hBanana = NAN;
  readSht31WithRetry(sht31_Banana, shtBananaReady, tBanana, hBanana);

  float tCalamansi = NAN, hCalamansi = NAN;
  readSht31WithRetry(sht31_Calamansi, shtCalamansiReady, tCalamansi, hCalamansi);

  // --- Banana SM4-C2H4 Freshness (Window Peak Raw UART1) ---
  bool bananaEthValid = ethyleneIsFresh();
  if (!bananaEthValid && ethEverReceived) resetEthylene();
  float bananaEth = bananaEthValid ? consumeBananaRawPpm() : 0.0f;
  bool bananaBytesComing = (now - lastByteMs < ETH_TIMEOUT_MS);
  unsigned long bananaStaleMs = bananaBytesComing ? (now - lastEthMs) : (now - lastByteMs);

  // --- Calamansi SM4-C2H4 & S3 Crops Freshness (Received via UART2 from ESP32-S3) ---
  bool s3LinkFresh = (lastRemoteS3Ms != 0) && (now - lastRemoteS3Ms < ETH_TIMEOUT_MS);
  if (!s3LinkFresh) {
    remoteS3EthValid  = false;
    remoteCornState   = 0; // Fail-safe to Normal
    remotePomeloState = 0;
    remoteBeansState  = 0;
  }
  bool calamansiEthValid = remoteS3EthValid && s3LinkFresh;
  float calamansiEth     = calamansiEthValid ? remoteS3EthPpm : 0.0f;
  unsigned long calamansiStaleMs = (lastRemoteS3Ms == 0) ? now : (now - lastRemoteS3Ms);

  // --- Step 1: Run Responsive Hybrid CSV ML + Safety Classification ---
  CropEvalResult bananaRes    = evaluateCrop(CROP_BANANA,    tBanana,    hBanana,    bananaEthValid,    bananaEth);
  CropEvalResult calamansiRes = evaluateCrop(CROP_CALAMANSI, tCalamansi, hCalamansi, calamansiEthValid, calamansiEth);

  int bananaState    = bananaRes.confirmed;
  int calamansiState = calamansiRes.confirmed;

  // --- Step 2: Compute Active Container Fan States & Apply PWM Immediately ---
  int container1FanState = bananaState;
  int container2FanState = max(calamansiState,
                           max(remoteCornState,
                           max(remotePomeloState, remoteBeansState)));

  setContainer1Fan(container1FanState);
  setContainer2Fan(container2FanState);

  // --- Step 3: Send Full Telemetry & Fan States to ESP32-S3 over UART2 (Every 500 ms) ---
  float txTBan = isnan(tBanana)    ? -999.0f : tBanana;
  float txHBan = isnan(hBanana)    ? -999.0f : hBanana;
  float txEBan = bananaEthValid    ? bananaEth : -1.0f;

  float txTCal = isnan(tCalamansi) ? -999.0f : tCalamansi;
  float txHCal = isnan(hCalamansi) ? -999.0f : hCalamansi;
  float txECal = calamansiEthValid ? calamansiEth : -1.0f;

  char txPacket[128];
  snprintf(txPacket, sizeof(txPacket),
           "ESP32_DATA,%.2f,%.2f,%.2f,%d,%d,%.2f,%.2f,%.2f,%d,%d",
           txTBan, txHBan, txEBan, bananaState, container1FanState,
           txTCal, txHCal, txECal, calamansiState, container2FanState);
  Serial2.println(txPacket);

  // --- Step 4: Print Status Report (Every 1000 ms OR immediately on any state transition) ---
  bool anyChanged = bananaRes.stateChanged || calamansiRes.stateChanged;
  if (anyChanged || (now - lastPrintMs >= PRINT_PERIOD_MS)) {
    lastPrintMs = now;

    Serial.println("====================================================================================");
    printCropReport(CROP_BANANA, bananaRes, tBanana, hBanana,
                    bananaEthValid, bananaBytesComing, bananaStaleMs, bananaEth,
                    "Container 1 Fan", container1FanState);

    printCropReport(CROP_CALAMANSI, calamansiRes, tCalamansi, hCalamansi,
                    calamansiEthValid, false, calamansiStaleMs, calamansiEth,
                    "Container 2 Fan", container2FanState);

    if (bananaHasFrame) {
      Serial.printf(">> BANANA C2H4    | Raw=%.2f ppm | Frame=%02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                    bananaEth,
                    bananaLastFrame[0], bananaLastFrame[1], bananaLastFrame[2], bananaLastFrame[3],
                    bananaLastFrame[4], bananaLastFrame[5], bananaLastFrame[6], bananaLastFrame[7],
                    bananaLastFrame[8]);
    } else {
      Serial.printf(">> BANANA C2H4    | Raw=%.2f ppm | Frame=NONE\n", bananaEth);
    }

    Serial.printf(">> CALAMANSI C2H4 | From S3 UART2: %s | Raw=%.2f ppm (S3 States: Corn=%s, Pomelo=%s, Beans=%s)\n",
                  calamansiEthValid ? "OK" : "WAITING/DISCONNECTED",
                  calamansiEth,
                  CLASS_NAMES[remoteCornState],
                  CLASS_NAMES[remotePomeloState],
                  CLASS_NAMES[remoteBeansState]);

    Serial.printf(">> ACTIVE FANS    | Container 1 Fan (Banana): %-7s (GPIO %d=255) | Container 2 Fan (4 Crops): %-7s\n",
                  FAN_SPEED_NAMES[container1FanState], CONTAINER1_PINS[container1FanState],
                  FAN_SPEED_NAMES[container2FanState]);
  }
}
