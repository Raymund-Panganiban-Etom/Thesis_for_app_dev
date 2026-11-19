#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include "sd_read_write.h"
#include "CornClassifier.h"
#include "PomeloClassifier.h"
#include "StringbeansClassifier.h"

// ==========================================================
// 1. WI-FI ACCESS POINT, WEB SERVER & MQTT BROKER CONFIG
// ==========================================================
const char* AP_SSID = "PostHarvest-Storage-AI";
const char* AP_PASS = "12345678";

IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

WebServer  httpServer(80);
WiFiServer mqttWsServer(9001);   // MQTT over WebSockets (for Browser UI)
WiFiServer mqttTcpServer(1883);  // Standard MQTT over TCP (optional MQTT tools)

const char* MQTT_TOPIC = "postharvest/telemetry";

// ==========================================================
// 2. PIN DEFINITIONS (ESP32-S3 N16R8 - UNCHANGED)
// ==========================================================
#define I2C1_SDA_PIN    20
#define I2C1_SCL_PIN    21

#define I2C2_SDA_PIN    8
#define I2C2_SCL_PIN    9

#define MUX_SDA_PIN        10
#define MUX_SCL_PIN        11
#define TCA9548A_ADDR      0x70
#define STRINGBEANS_MUX_CH 0

#define C2H4_RX_PIN     18
#define C2H4_TX_PIN     17

#define ESP32_RX_PIN    16
#define ESP32_TX_PIN    15

// ==========================================================
// 3. SENSOR INSTANCES & ML CLASSIFIERS
// ==========================================================
Adafruit_SHT31 sht31_Corn        = Adafruit_SHT31(&Wire);
Adafruit_SHT31 sht31_Pomelo      = Adafruit_SHT31(&Wire1);
Adafruit_SHT31 sht31_StringBeans = Adafruit_SHT31(&Wire1);

bool shtCornReady        = false;
bool shtPomeloReady      = false;
bool shtStringBeansReady = false;

Eloquent::ML::Port::CornClassifier        cornClassifier;
Eloquent::ML::Port::PomeloClassifier      pomeloClassifier;
Eloquent::ML::Port::StringbeansClassifier stringbeansClassifier;

const char* CLASS_NAMES[]     = { "NORMAL", "WARNING", "CRITICAL" };
const char* FAN_SPEED_NAMES[] = { "Low", "Medium", "Maximum" };
const int   NUM_CLASSES       = 3;

enum S3Crop {
  CROP_CORN = 0,
  CROP_POMELO,
  CROP_STRING_BEANS,
  S3_CROP_COUNT
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

static const CommodityProfile COMMODITY_PROFILES[S3_CROP_COUNT] = {
  { "Corn",          0.0f, 15.0f, 75.0f, 99.0f,  0.5f,   2.0f,   true },
  { "Pomelo",        5.0f, 20.0f, 70.0f, 95.0f,  0.5f,   2.0f,   true },
  { "String Beans",  4.0f, 15.0f, 70.0f, 95.0f,  1.0f,   4.0f,   true },
};

static const CommodityProfile REMOTE_BANANA_PROFILE    = { "Banana",    13.0f, 30.0f, 62.0f, 95.0f, 1.0f, 7.85f, true };
static const CommodityProfile REMOTE_CALAMANSI_PROFILE = { "Calamansi",  8.0f, 20.0f, 70.0f, 95.0f, 0.3f, 1.50f, true };

struct CropDebounceState {
  int consecutiveCount = 0;
  int lastRawState     = -1;
  int confirmedState   = 0;
};

CropDebounceState debounceStates[S3_CROP_COUNT];

struct LiveTelemetrySnapshot {
  float cornTemp      = NAN;
  float cornHum       = NAN;
  int   cornState     = 0;

  float pomeloTemp    = NAN;
  float pomeloHum     = NAN;
  int   pomeloState   = 0;

  float beansTemp     = NAN;
  float beansHum      = NAN;
  int   beansState    = 0;

  float s3EthPpm      = 0.0f;
  bool  s3EthValid    = false;

  int   c1FanState    = 0;
  int   c2FanState    = 0;
  bool  esp32Online   = false;
} telemetry;

inline int classifyByRules(S3Crop c, float eth) {
  const CommodityProfile& p = COMMODITY_PROFILES[c];
  if (eth <= p.ethNormalMax)  return 0;
  if (eth <= p.ethWarningMax) return 1;
  return 2;
}

inline const char* storageAdviceFromProfile(const CommodityProfile& p, float temp, float hum) {
  if (temp > p.idealTempHigh + 5.0f)  return " [TEMP HIGH]";
  if (temp < p.idealTempLow  - 5.0f)  return " [TEMP LOW]";
  if (hum  > p.idealHumHigh  + 10.0f) return " [HUM HIGH]";
  if (hum  < p.idealHumLow   - 10.0f) return " [HUM LOW]";
  return "";
}

// ==========================================================
// 4. I2C MULTIPLEXER BUS SWITCHING HELPERS
// ==========================================================
void switchWire1ToPomelo() {
  Wire1.end();
  Wire1.begin(I2C2_SDA_PIN, I2C2_SCL_PIN);
}

void switchWire1ToMux() {
  Wire1.end();
  Wire1.begin(MUX_SDA_PIN, MUX_SCL_PIN);
}

bool tcaSelect(uint8_t channel) {
  if (channel > 7) return false;
  Wire1.beginTransmission(TCA9548A_ADDR);
  Wire1.write(1 << channel);
  return (Wire1.endTransmission() == 0);
}

void tcaDisableAll() {
  Wire1.beginTransmission(TCA9548A_ADDR);
  Wire1.write(0);
  Wire1.endTransmission();
}

// ==========================================================
// 5. LOCAL ETHYLENE BUFFER & WATCHDOG
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
// 6. REMOTE READINGS RECEIVED FROM STANDARD ESP32
// ==========================================================
struct RemoteEsp32Readings {
  float bananaTemp     = NAN;
  float bananaHum      = NAN;
  float bananaEth      = -1.0f;
  int   bananaState    = 0;

  float calamansiTemp  = NAN;
  float calamansiHum   = NAN;
  float calamansiEth   = -1.0f;
  int   calamansiState = 0;

  int   c1FanState     = 0;
  int   c2FanState     = 0;

  unsigned long lastUpdateMs = 0;
  bool  valid = false;
} remoteEsp32;

void pollRemoteESP32UART() {
  while (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();

    if (line.startsWith("ESP32_DATA,")) {
      float tBan, hBan, eBan, tCal, hCal, eCal;
      int bState, c1State, calState, c2State;
      if (sscanf(line.c_str(), "ESP32_DATA,%f,%f,%f,%d,%d,%f,%f,%f,%d,%d",
                 &tBan, &hBan, &eBan, &bState, &c1State,
                 &tCal, &hCal, &eCal, &calState, &c2State) == 10) {
        remoteEsp32.bananaTemp     = (tBan <= -900.0f) ? NAN : tBan;
        remoteEsp32.bananaHum      = (hBan <= -900.0f) ? NAN : hBan;
        remoteEsp32.bananaEth      = eBan;
        remoteEsp32.bananaState    = constrain(bState, 0, 2);
        remoteEsp32.c1FanState     = constrain(c1State, 0, 2);

        remoteEsp32.calamansiTemp  = (tCal <= -900.0f) ? NAN : tCal;
        remoteEsp32.calamansiHum   = (hCal <= -900.0f) ? NAN : hCal;
        remoteEsp32.calamansiEth   = eCal;
        remoteEsp32.calamansiState = constrain(calState, 0, 2);
        remoteEsp32.c2FanState     = constrain(c2State, 0, 2);

        remoteEsp32.lastUpdateMs   = millis();
        remoteEsp32.valid          = true;
      }
    }
  }
}

// ==========================================================
// 7. YYS SM4-C2H4 BINARY FRAME PARSER
// ==========================================================
void pollLocalC2H4() {
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
// 8. EMBEDDED MQTT 3.1.1 BROKER (WEBSOCKETS :9001 + TCP :1883)
// ==========================================================
#define MAX_MQTT_CLIENTS 4

struct MqttWsClient {
  WiFiClient sock;
  bool wsHandshakeDone = false;
  bool mqttSubscribed  = false;
};

struct MqttTcpClient {
  WiFiClient sock;
  bool mqttSubscribed = false;
};

MqttWsClient  wsClients[MAX_MQTT_CLIENTS];
MqttTcpClient tcpClients[MAX_MQTT_CLIENTS];

// Portable SHA-1 & Base64 for RFC 6455 WebSocket Handshake
static inline uint32_t sha1Rol(uint32_t value, uint32_t bits) {
  return (value << bits) | (value >> (32 - bits));
}

void computeSha1(const uint8_t* data, size_t len, uint8_t hash[20]) {
  uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
  size_t newLen = len + 1;
  while ((newLen % 64) != 56) newLen++;
  uint8_t* msg = (uint8_t*)calloc(newLen + 8, 1);
  if (!msg) return;
  memcpy(msg, data, len);
  msg[len] = 0x80;
  uint64_t bitLen = (uint64_t)len * 8;
  for (int i = 0; i < 8; i++) msg[newLen + i] = (bitLen >> ((7 - i) * 8)) & 0xFF;

  for (size_t offset = 0; offset < newLen + 8; offset += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
      w[i] = ((uint32_t)msg[offset + i * 4] << 24) |
             ((uint32_t)msg[offset + i * 4 + 1] << 16) |
             ((uint32_t)msg[offset + i * 4 + 2] << 8) |
             ((uint32_t)msg[offset + i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++) {
      w[i] = sha1Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
    for (int i = 0; i < 80; i++) {
      uint32_t f, k;
      if (i < 20)      { f = (b & c) | ((~b) & d);           k = 0x5A827999; }
      else if (i < 40) { f = b ^ c ^ d;                      k = 0x6ED9EBA1; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d);    k = 0x8F1BBCDC; }
      else             { f = b ^ c ^ d;                      k = 0xCA62C1D6; }
      uint32_t temp = sha1Rol(a, 5) + f + e + k + w[i];
      e = d; d = c; c = sha1Rol(b, 30); b = a; a = temp;
    }
    h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
  }
  free(msg);
  uint32_t hs[5] = { h0, h1, h2, h3, h4 };
  for (int i = 0; i < 5; i++) {
    hash[i * 4]     = (hs[i] >> 24) & 0xFF;
    hash[i * 4 + 1] = (hs[i] >> 16) & 0xFF;
    hash[i * 4 + 2] = (hs[i] >> 8)  & 0xFF;
    hash[i * 4 + 3] =  hs[i]        & 0xFF;
  }
}

String base64EncodeBytes(const uint8_t* data, size_t len) {
  static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t v = (uint32_t)data[i] << 16;
    if (i + 1 < len) v |= (uint32_t)data[i + 1] << 8;
    if (i + 2 < len) v |= (uint32_t)data[i + 2];
    out += B64[(v >> 18) & 0x3F];
    out += B64[(v >> 12) & 0x3F];
    out += (i + 1 < len) ? B64[(v >> 6) & 0x3F] : '=';
    out += (i + 2 < len) ? B64[v & 0x3F] : '=';
  }
  return out;
}

void sendWsBinaryFrame(WiFiClient& client, const uint8_t* payload, size_t len) {
  if (!client.connected()) return;
  uint8_t hdr[4];
  size_t hdrLen = 0;
  hdr[0] = 0x82; // FIN + Binary opcode
  if (len <= 125) {
    hdr[1] = (uint8_t)len;
    hdrLen = 2;
  } else {
    hdr[1] = 126;
    hdr[2] = (len >> 8) & 0xFF;
    hdr[3] = len & 0xFF;
    hdrLen = 4;
  }
  client.write(hdr, hdrLen);
  client.write(payload, len);
}

size_t buildMqttPublishPacket(uint8_t* outBuf, size_t maxBuf, const char* topic, const char* payload) {
  size_t topicLen   = strlen(topic);
  size_t payloadLen = strlen(payload);
  size_t remLen     = 2 + topicLen + payloadLen;
  if (remLen + 5 > maxBuf) return 0;

  size_t idx = 0;
  outBuf[idx++] = 0x30; // MQTT PUBLISH (QoS 0)
  size_t x = remLen;
  do {
    uint8_t b = x % 128;
    x /= 128;
    if (x > 0) b |= 128;
    outBuf[idx++] = b;
  } while (x > 0);

  outBuf[idx++] = (topicLen >> 8) & 0xFF;
  outBuf[idx++] = topicLen & 0xFF;
  memcpy(outBuf + idx, topic, topicLen);
  idx += topicLen;
  memcpy(outBuf + idx, payload, payloadLen);
  idx += payloadLen;
  return idx;
}

void formatFloatOrNull(char* out, size_t len, float val) {
  if (isnan(val)) {
    snprintf(out, len, "null");
  } else {
    snprintf(out, len, "%.2f", val);
  }
}

void buildTelemetryJson(char* json, size_t maxLen) {
  char tBan[16], hBan[16], eBan[16];
  char tCal[16], hCal[16], eCal[16];
  char tCorn[16], hCorn[16];
  char tPom[16], hPom[16];
  char tBean[16], hBean[16];
  char eS3[16];

  formatFloatOrNull(tBan,  sizeof(tBan),  telemetry.esp32Online ? remoteEsp32.bananaTemp    : NAN);
  formatFloatOrNull(hBan,  sizeof(hBan),  telemetry.esp32Online ? remoteEsp32.bananaHum     : NAN);
  formatFloatOrNull(eBan,  sizeof(eBan),  (telemetry.esp32Online && remoteEsp32.bananaEth >= 0.0f) ? remoteEsp32.bananaEth : NAN);

  formatFloatOrNull(tCal,  sizeof(tCal),  telemetry.esp32Online ? remoteEsp32.calamansiTemp : NAN);
  formatFloatOrNull(hCal,  sizeof(hCal),  telemetry.esp32Online ? remoteEsp32.calamansiHum  : NAN);
  formatFloatOrNull(eCal,  sizeof(eCal),  (telemetry.esp32Online && remoteEsp32.calamansiEth >= 0.0f) ? remoteEsp32.calamansiEth : NAN);

  formatFloatOrNull(tCorn, sizeof(tCorn), telemetry.cornTemp);
  formatFloatOrNull(hCorn, sizeof(hCorn), telemetry.cornHum);
  formatFloatOrNull(tPom,  sizeof(tPom),  telemetry.pomeloTemp);
  formatFloatOrNull(hPom,  sizeof(hPom),  telemetry.pomeloHum);
  formatFloatOrNull(tBean, sizeof(tBean), telemetry.beansTemp);
  formatFloatOrNull(hBean, sizeof(hBean), telemetry.beansHum);
  formatFloatOrNull(eS3,   sizeof(eS3),   telemetry.s3EthValid ? telemetry.s3EthPpm : NAN);

  snprintf(json, maxLen,
    "{"
      "\"esp32Link\":%s,"
      "\"b\":{\"t\":%s,\"h\":%s,\"e\":%s,\"s\":%d},"
      "\"m\":{\"e\":%s},"
      "\"c\":["
        "{\"id\":\"kalamansi\",\"t\":%s,\"h\":%s,\"e\":%s,\"s\":%d},"
        "{\"id\":\"corn\",\"t\":%s,\"h\":%s,\"e\":%s,\"s\":%d},"
        "{\"id\":\"pomelo\",\"t\":%s,\"h\":%s,\"e\":%s,\"s\":%d},"
        "{\"id\":\"sitaw\",\"t\":%s,\"h\":%s,\"e\":%s,\"s\":%d}"
      "],"
      "\"fans\":{"
        "\"c1\":%d,\"c1Label\":\"%s\","
        "\"c2\":%d,\"c2Label\":\"%s\""
      "}"
    "}",
    telemetry.esp32Online ? "true" : "false",
    tBan, hBan, eBan, telemetry.esp32Online ? remoteEsp32.bananaState : 0,
    eS3,
    tCal, hCal, eCal, telemetry.esp32Online ? remoteEsp32.calamansiState : 0,
    tCorn, hCorn, eS3, telemetry.cornState,
    tPom, hPom, eS3, telemetry.pomeloState,
    tBean, hBean, eS3, telemetry.beansState,
    telemetry.c1FanState, FAN_SPEED_NAMES[telemetry.c1FanState],
    telemetry.c2FanState, FAN_SPEED_NAMES[telemetry.c2FanState]
  );
}

void publishMqttTelemetryToAll() {
  char json[768];
  buildTelemetryJson(json, sizeof(json));

  uint8_t mqttPkt[820];
  size_t pktLen = buildMqttPublishPacket(mqttPkt, sizeof(mqttPkt), MQTT_TOPIC, json);
  if (pktLen == 0) return;

  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (wsClients[i].sock && wsClients[i].sock.connected() &&
        wsClients[i].wsHandshakeDone && wsClients[i].mqttSubscribed) {
      sendWsBinaryFrame(wsClients[i].sock, mqttPkt, pktLen);
    }
    if (tcpClients[i].sock && tcpClients[i].sock.connected() &&
        tcpClients[i].mqttSubscribed) {
      tcpClients[i].sock.write(mqttPkt, pktLen);
    }
  }
}

void processMqttBytes(const uint8_t* buf, size_t len, bool isWs, WiFiClient& sock, bool& subscribedFlag) {
  size_t pos = 0;
  while (pos < len) {
    uint8_t header = buf[pos++];
    uint8_t pktType = header & 0xF0;

    size_t remLen = 0, mult = 1;
    uint8_t encByte = 0;
    do {
      if (pos >= len) return;
      encByte = buf[pos++];
      remLen += (encByte & 127) * mult;
      mult *= 128;
    } while ((encByte & 128) != 0);

    if (pos + remLen > len) break;

    if (pktType == 0x10) {
      // CONNECT -> reply CONNACK (Accepted)
      uint8_t connack[4] = { 0x20, 0x02, 0x00, 0x00 };
      if (isWs) sendWsBinaryFrame(sock, connack, 4);
      else      sock.write(connack, 4);
    } else if (pktType == 0x80 && remLen >= 2) {
      // SUBSCRIBE -> reply SUBACK (QoS 0) + push immediate snapshot
      uint8_t suback[5] = { 0x90, 0x03, buf[pos], buf[pos + 1], 0x00 };
      if (isWs) sendWsBinaryFrame(sock, suback, 5);
      else      sock.write(suback, 5);
      subscribedFlag = true;

      char json[768];
      buildTelemetryJson(json, sizeof(json));
      uint8_t mqttPkt[820];
      size_t pktLen = buildMqttPublishPacket(mqttPkt, sizeof(mqttPkt), MQTT_TOPIC, json);
      if (pktLen > 0) {
        if (isWs) sendWsBinaryFrame(sock, mqttPkt, pktLen);
        else      sock.write(mqttPkt, pktLen);
      }
    } else if (pktType == 0xC0) {
      // PINGREQ -> reply PINGRESP
      uint8_t pingresp[2] = { 0xD0, 0x00 };
      if (isWs) sendWsBinaryFrame(sock, pingresp, 2);
      else      sock.write(pingresp, 2);
    } else if (pktType == 0xE0) {
      // DISCONNECT
      sock.stop();
      return;
    }

    pos += remLen;
  }
}

void pollMqttBroker() {
  // 1. Accept new WebSocket MQTT clients (port 9001)
  if (mqttWsServer.hasClient()) {
    WiFiClient newClient = mqttWsServer.available();
    bool placed = false;
    for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
      if (!wsClients[i].sock || !wsClients[i].sock.connected()) {
        wsClients[i].sock = newClient;
        wsClients[i].wsHandshakeDone = false;
        wsClients[i].mqttSubscribed  = false;
        placed = true;
        break;
      }
    }
    if (!placed) newClient.stop();
  }

  // 2. Accept new TCP MQTT clients (port 1883)
  if (mqttTcpServer.hasClient()) {
    WiFiClient newClient = mqttTcpServer.available();
    bool placed = false;
    for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
      if (!tcpClients[i].sock || !tcpClients[i].sock.connected()) {
        tcpClients[i].sock = newClient;
        tcpClients[i].mqttSubscribed = false;
        placed = true;
        break;
      }
    }
    if (!placed) newClient.stop();
  }

  // 3. Service active WebSocket MQTT clients
  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (!wsClients[i].sock || !wsClients[i].sock.connected()) continue;
    int avail = wsClients[i].sock.available();
    if (avail <= 0) continue;

    if (!wsClients[i].wsHandshakeDone) {
      String req = "";
      unsigned long t0 = millis();
      while (wsClients[i].sock.connected() && millis() - t0 < 250) {
        while (wsClients[i].sock.available()) {
          req += (char)wsClients[i].sock.read();
        }
        if (req.indexOf("\r\n\r\n") >= 0) break;
      }
      int kIdx = req.indexOf("Sec-WebSocket-Key:");
      if (kIdx >= 0) {
        int lineEnd = req.indexOf("\r\n", kIdx);
        String key = req.substring(kIdx + 18, lineEnd);
        key.trim();
        String magic = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        uint8_t sha[20];
        computeSha1((const uint8_t*)magic.c_str(), magic.length(), sha);
        String acceptKey = base64EncodeBytes(sha, 20);

        String resp = "HTTP/1.1 101 Switching Protocols\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Accept: " + acceptKey + "\r\n";
        if (req.indexOf("Sec-WebSocket-Protocol:") >= 0) {
          resp += "Sec-WebSocket-Protocol: mqtt\r\n";
        }
        resp += "\r\n";
        wsClients[i].sock.print(resp);
        wsClients[i].wsHandshakeDone = true;
      } else {
        wsClients[i].sock.stop();
      }
    } else {
      // Decode incoming WebSocket frame(s)
      while (wsClients[i].sock.available() >= 2) {
        uint8_t b0 = wsClients[i].sock.read();
        uint8_t b1 = wsClients[i].sock.read();
        uint8_t opcode = b0 & 0x0F;
        bool masked = (b1 & 0x80) != 0;
        size_t payLen = b1 & 0x7F;
        if (payLen == 126) {
          if (wsClients[i].sock.available() < 2) break;
          payLen = ((size_t)wsClients[i].sock.read() << 8) | wsClients[i].sock.read();
        }
        uint8_t mask[4] = {0};
        if (masked) {
          if (wsClients[i].sock.available() < 4) break;
          wsClients[i].sock.readBytes(mask, 4);
        }
        if (payLen > 512 || wsClients[i].sock.available() < (int)payLen) break;

        uint8_t decoded[512];
        wsClients[i].sock.readBytes(decoded, payLen);
        if (masked) {
          for (size_t k = 0; k < payLen; k++) decoded[k] ^= mask[k % 4];
        }

        if (opcode == 0x08) {
          wsClients[i].sock.stop();
          break;
        } else if (opcode == 0x09) {
          uint8_t pong[2] = { 0x8A, 0x00 };
          wsClients[i].sock.write(pong, 2);
        } else if (opcode == 0x01 || opcode == 0x02) {
          processMqttBytes(decoded, payLen, true, wsClients[i].sock, wsClients[i].mqttSubscribed);
        }
      }
    }
  }

  // 4. Service active TCP MQTT clients
  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (!tcpClients[i].sock || !tcpClients[i].sock.connected()) continue;
    int avail = tcpClients[i].sock.available();
    if (avail <= 0) continue;
    if (avail > 512) avail = 512;
    uint8_t buf[512];
    int n = tcpClients[i].sock.readBytes(buf, avail);
    if (n > 0) {
      processMqttBytes(buf, n, false, tcpClients[i].sock, tcpClients[i].mqttSubscribed);
    }
  }
}

// ==========================================================
// 9. HTTP STATIC FILE SERVER (SD_MMC /Downloads/Index.html)
// ==========================================================
String getContentType(const String& path) {
  String lower = path;
  lower.toLowerCase();
  if (lower.endsWith(".html")) return "text/html; charset=utf-8";
  if (lower.endsWith(".css"))  return "text/css; charset=utf-8";
  if (lower.endsWith(".js"))   return "application/javascript; charset=utf-8";
  if (lower.endsWith(".json")) return "application/json";
  if (lower.endsWith(".png"))  return "image/png";
  if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) return "image/jpeg";
  if (lower.endsWith(".svg"))  return "image/svg+xml";
  if (lower.endsWith(".ico"))  return "image/x-icon";
  return "text/plain";
}

bool streamFromSd(const String& sdPath) {
  if (!SD_MMC.exists(sdPath)) return false;
  File file = SD_MMC.open(sdPath, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) file.close();
    return false;
  }

  String lower = sdPath;
  lower.toLowerCase();
  if (lower.endsWith(".html")) {
    httpServer.sendHeader("Cache-Control", "no-cache");
  } else {
    httpServer.sendHeader("Cache-Control", "public, max-age=3600");
  }
  httpServer.sendHeader("Connection", "close");
  httpServer.streamFile(file, getContentType(sdPath));
  file.close();
  return true;
}

void handleSdStaticFiles() {
  String path = httpServer.uri();

  // Support exact requested path (e.g. /Downloads/Index.html or /Downloads/index.html)
  if (streamFromSd(path)) return;

  // Fallback for lowercase /Downloads/index.html if user typed /Downloads/Index.html
  if (path.equalsIgnoreCase("/Downloads/Index.html")) {
    if (streamFromSd("/Downloads/index.html")) return;
  }

  Serial.printf("[HTTP 404] Not found on SD: %s\n", path.c_str());
  httpServer.sendHeader("Connection", "close");
  httpServer.send(404, "text/plain", "File not found on SD card: " + path);
}

// ==========================================================
// 10. SERIAL MONITOR REPORTING HELPERS
// ==========================================================
void printRemoteCrop(const CommodityProfile& profile, bool linkFresh,
                     float temp, float hum, float eth, int state,
                     const char* containerLabel, int activeContainerFanState) {
  if (!linkFresh) {
    Serial.printf("[%-12s] ESP32 UART LINK DISCONNECTED -> NORMAL (Fail-Safe) | %s: %s\n",
                  profile.name, containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    return;
  }

  if (isnan(temp) || isnan(hum)) {
    if (eth >= 0.0f)
      Serial.printf("[%-12s] SHT31: N/A | C2H4=%.2f ppm -> NORMAL (Fail-Safe) | %s: %s\n",
                    profile.name, eth, containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    else
      Serial.printf("[%-12s] SHT31: N/A | C2H4: DISCONNECTED -> NORMAL (Fail-Safe) | %s: %s\n",
                    profile.name, containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    return;
  }

  if (eth < 0.0f) {
    Serial.printf("[%-12s] T=%.1f C | H=%.1f %% | C2H4: DISCONNECTED -> NORMAL (Fail-Safe) | %s: %s\n",
                  profile.name, temp, hum, containerLabel, FAN_SPEED_NAMES[activeContainerFanState]);
    return;
  }

  const char* advice = storageAdviceFromProfile(profile, temp, hum);
  Serial.printf("[%-12s] T=%.1f C | H=%.1f %% | C2H4=%.2f ppm -> %-8s (ESP32) | Crop Fan: %-7s | %s: %-7s%s\n",
                profile.name, temp, hum, eth,
                CLASS_NAMES[state],
                FAN_SPEED_NAMES[state],
                containerLabel, FAN_SPEED_NAMES[activeContainerFanState],
                advice);
}

int evaluateAndPrintCrop(S3Crop crop, float temp, float hum,
                         bool ethValid, bool bytesComing,
                         unsigned long staleMs, float eth,
                         int activeContainer2FanState) {
  const CommodityProfile& profile = COMMODITY_PROFILES[crop];
  CropDebounceState& db = debounceStates[crop];

  if (isnan(temp) || isnan(hum)) {
    db.consecutiveCount = 0;
    db.lastRawState     = -1;
    db.confirmedState   = 0;

    if (ethValid)
      Serial.printf("[%-12s] SHT31: N/A | C2H4=%.2f ppm -> NORMAL (Fail-Safe) | Container 2 Fan: %s\n",
                    profile.name, eth, FAN_SPEED_NAMES[activeContainer2FanState]);
    else
      Serial.printf("[%-12s] SHT31: N/A | C2H4: DISCONNECTED -> NORMAL (Fail-Safe) | Container 2 Fan: %s\n",
                    profile.name, FAN_SPEED_NAMES[activeContainer2FanState]);
    return db.confirmedState;
  }

  if (!ethValid) {
    db.consecutiveCount = 0;
    db.lastRawState     = -1;
    db.confirmedState   = 0;

    if (bytesComing)
      Serial.printf("[%-12s] T=%.1f C | H=%.1f %% | C2H4: INVALID FRAME (%.1f s) -> NORMAL (Fail-Safe) | Container 2 Fan: %s\n",
                    profile.name, temp, hum, staleMs / 1000.0,
                    FAN_SPEED_NAMES[activeContainer2FanState]);
    else
      Serial.printf("[%-12s] T=%.1f C | H=%.1f %% | C2H4: DISCONNECTED (%.1f s) -> NORMAL (Fail-Safe) | Container 2 Fan: %s\n",
                    profile.name, temp, hum, staleMs / 1000.0,
                    FAN_SPEED_NAMES[activeContainer2FanState]);
    return db.confirmedState;
  }

  int rawPrediction;
  if (profile.useML) {
    float features[3] = { temp, hum, eth };
    switch (crop) {
      case CROP_CORN:
        rawPrediction = cornClassifier.predict(features);
        break;
      case CROP_POMELO:
        rawPrediction = pomeloClassifier.predict(features);
        break;
      case CROP_STRING_BEANS:
        rawPrediction = stringbeansClassifier.predict(features);
        break;
      default:
        rawPrediction = classifyByRules(crop, eth);
        break;
    }
  } else {
    rawPrediction = classifyByRules(crop, eth);
  }

  if (rawPrediction < 0 || rawPrediction >= NUM_CLASSES) {
    Serial.printf("[%-12s] T=%.1f C | H=%.1f %% | C2H4=%.2f ppm -> OUT-OF-RANGE(%d) | Container 2 Fan: %s\n",
                  profile.name, temp, hum, eth, rawPrediction,
                  FAN_SPEED_NAMES[activeContainer2FanState]);
    return db.confirmedState;
  }

  if (rawPrediction == db.lastRawState) {
    if (db.consecutiveCount < 100) db.consecutiveCount++;
    if (db.consecutiveCount >= 3) db.confirmedState = rawPrediction;
  } else {
    db.consecutiveCount = 1;
    db.lastRawState     = rawPrediction;
  }

  const char* advice = storageAdviceFromProfile(profile, temp, hum);
  const char* method = profile.useML ? "ML" : "RULE";
  Serial.printf("[%-12s] T=%.1f C | H=%.1f %% | C2H4=%.2f ppm -> %-8s (%s) | Crop Fan: %-7s | Container 2 Fan: %-7s%s\n",
                profile.name, temp, hum, eth,
                CLASS_NAMES[db.confirmedState], method,
                FAN_SPEED_NAMES[db.confirmedState],
                FAN_SPEED_NAMES[activeContainer2FanState],
                advice);

  return db.confirmedState;
}

// ==========================================================
// SETUP
// ==========================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32-S3 N16R8: 5-Crop Edge AI Hub + AP Web & MQTT Broker ===");

  // 1. Initialize Integrated SD_MMC Card (CLK=39, CMD=38, D0=40 @ 20MHz)
  sdmmcInit();
  listDir(SD_MMC, "/Downloads", 1);

  // 2. Start Wi-Fi Access Point (Channel 6, Visible, Max 4 clients, No Modem Sleep)
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASS, 6, 0, 4);
  delay(200);

  Serial.printf("Wi-Fi AP Started | SSID: %s | Password: %s\n", AP_SSID, AP_PASS);
  Serial.printf("Web UI URL       : http://%s/Downloads/Index.html\n",
                WiFi.softAPIP().toString().c_str());
  Serial.printf("MQTT Broker      : ws://%s:9001/mqtt (Topic: %s)\n",
                WiFi.softAPIP().toString().c_str(), MQTT_TOPIC);

  // 3. Start HTTP Static File Server & MQTT Brokers
  httpServer.onNotFound(handleSdStaticFiles);
  httpServer.begin();
  mqttWsServer.begin();
  mqttTcpServer.begin();

  // 4. Setup SHT31 #3 (Corn -> GPIO 20/21) & SHT31 #4 (Pomelo -> GPIO 8/9)
  Wire.begin(I2C1_SDA_PIN, I2C1_SCL_PIN);
  Wire1.begin(I2C2_SDA_PIN, I2C2_SCL_PIN);

  shtCornReady   = sht31_Corn.begin(0x44);
  shtPomeloReady = sht31_Pomelo.begin(0x44);

  // 5. Setup SHT31 #5 (String Beans -> I2C Multiplexer GPIO 10/11, Channel 0)
  switchWire1ToMux();
  if (tcaSelect(STRINGBEANS_MUX_CH)) {
    shtStringBeansReady = sht31_StringBeans.begin(0x44);
  } else {
    shtStringBeansReady = false;
  }
  tcaDisableAll();

  Serial.printf("SHT31 #3 Corn (GPIO 20/21): %s | SHT31 #4 Pomelo (GPIO 8/9): %s | SHT31 #5 String Beans (Mux CH0): %s\n",
                shtCornReady        ? "OK" : "NOT FOUND",
                shtPomeloReady      ? "OK" : "NOT FOUND",
                shtStringBeansReady ? "OK" : "NOT FOUND");

  // 6. Setup UART1 (Local YYS-SM4-C2H4) & UART2 (Link with Standard ESP32)
  Serial1.begin(9600, SERIAL_8N1, C2H4_RX_PIN, C2H4_TX_PIN);
  Serial1.setTimeout(50);
  Serial2.begin(115200, SERIAL_8N1, ESP32_RX_PIN, ESP32_TX_PIN);
  Serial2.setTimeout(50);
}

// ==========================================================
// LOOP
// ==========================================================
void loop() {
  httpServer.handleClient();
  pollMqttBroker();
  pollLocalC2H4();
  pollRemoteESP32UART();

  unsigned long now = millis();
  if (now - lastPrintMs < PRINT_PERIOD_MS) return;
  lastPrintMs = now;

  // --- Local SM4-C2H4 Freshness ---
  bool ethValid = ethyleneIsFresh();
  if (!ethValid && ethEverReceived) resetEthylene();
  float eth = ethValid ? getSmoothedEthylene() : 0.0f;
  bool bytesComing = (now - lastByteMs < ETH_TIMEOUT_MS);
  unsigned long staleMs = bytesComing ? (now - lastEthMs) : (now - lastByteMs);

  // --- Read SHT31 #3 (Corn -> GPIO 20/21) ---
  if (!shtCornReady) shtCornReady = sht31_Corn.begin(0x44);
  float tCorn = NAN, hCorn = NAN;
  if (shtCornReady) {
    tCorn = sht31_Corn.readTemperature();
    hCorn = sht31_Corn.readHumidity();
    if (isnan(tCorn) || isnan(hCorn)) shtCornReady = false;
  }

  // --- Read SHT31 #4 (Pomelo -> GPIO 8/9) ---
  switchWire1ToPomelo();
  if (!shtPomeloReady) shtPomeloReady = sht31_Pomelo.begin(0x44);
  float tPomelo = NAN, hPomelo = NAN;
  if (shtPomeloReady) {
    tPomelo = sht31_Pomelo.readTemperature();
    hPomelo = sht31_Pomelo.readHumidity();
    if (isnan(tPomelo) || isnan(hPomelo)) shtPomeloReady = false;
  }

  // --- Read SHT31 #5 (String Beans -> Multiplexer GPIO 10/11, Channel 0) ---
  switchWire1ToMux();
  float tBeans = NAN, hBeans = NAN;
  if (tcaSelect(STRINGBEANS_MUX_CH)) {
    if (!shtStringBeansReady) shtStringBeansReady = sht31_StringBeans.begin(0x44);
    if (shtStringBeansReady) {
      tBeans = sht31_StringBeans.readTemperature();
      hBeans = sht31_StringBeans.readHumidity();
      if (isnan(tBeans) || isnan(hBeans)) shtStringBeansReady = false;
    }
  } else {
    shtStringBeansReady = false;
  }
  tcaDisableAll();

  // --- Check ESP32 Link Freshness ---
  bool esp32LinkFresh = remoteEsp32.valid && (now - remoteEsp32.lastUpdateMs < ETH_TIMEOUT_MS);
  if (!esp32LinkFresh) {
    remoteEsp32.bananaState    = 0;
    remoteEsp32.calamansiState = 0;
    remoteEsp32.c1FanState     = 0;
  }

  int container2FanState = max(remoteEsp32.calamansiState,
                           max(debounceStates[CROP_CORN].confirmedState,
                           max(debounceStates[CROP_POMELO].confirmedState,
                               debounceStates[CROP_STRING_BEANS].confirmedState)));

  // --- Display All 5 Crops on ESP32-S3 Serial Monitor ---
  Serial.println("=======================================================================================");
  printRemoteCrop(REMOTE_BANANA_PROFILE, esp32LinkFresh,
                  remoteEsp32.bananaTemp, remoteEsp32.bananaHum, remoteEsp32.bananaEth,
                  remoteEsp32.bananaState, "Container 1 Fan", remoteEsp32.c1FanState);

  printRemoteCrop(REMOTE_CALAMANSI_PROFILE, esp32LinkFresh,
                  remoteEsp32.calamansiTemp, remoteEsp32.calamansiHum, remoteEsp32.calamansiEth,
                  remoteEsp32.calamansiState, "Container 2 Fan", container2FanState);

  int cornState   = evaluateAndPrintCrop(CROP_CORN, tCorn, hCorn, ethValid, bytesComing, staleMs, eth, container2FanState);
  int pomeloState = evaluateAndPrintCrop(CROP_POMELO, tPomelo, hPomelo, ethValid, bytesComing, staleMs, eth, container2FanState);
  int beansState  = evaluateAndPrintCrop(CROP_STRING_BEANS, tBeans, hBeans, ethValid, bytesComing, staleMs, eth, container2FanState);

  container2FanState = max(remoteEsp32.calamansiState,
                       max(cornState,
                       max(pomeloState, beansState)));

  telemetry.cornTemp    = tCorn;
  telemetry.cornHum     = hCorn;
  telemetry.cornState   = cornState;

  telemetry.pomeloTemp  = tPomelo;
  telemetry.pomeloHum   = hPomelo;
  telemetry.pomeloState = pomeloState;

  telemetry.beansTemp   = tBeans;
  telemetry.beansHum    = hBeans;
  telemetry.beansState  = beansState;

  telemetry.s3EthPpm    = eth;
  telemetry.s3EthValid  = ethValid;

  telemetry.c1FanState  = remoteEsp32.c1FanState;
  telemetry.c2FanState  = container2FanState;
  telemetry.esp32Online = esp32LinkFresh;

  // Publish live telemetry via MQTT to all connected UI clients
  publishMqttTelemetryToAll();

  Serial.printf(">> CONTAINER 1 FAN: %-7s (Banana=%s) | CONTAINER 2 FAN: %-7s (Calamansi=%s, Corn=%s, Pomelo=%s, StringBeans=%s) | AP Clients: %d\n",
                FAN_SPEED_NAMES[remoteEsp32.c1FanState],
                CLASS_NAMES[remoteEsp32.bananaState],
                FAN_SPEED_NAMES[container2FanState],
                CLASS_NAMES[remoteEsp32.calamansiState],
                CLASS_NAMES[cornState],
                CLASS_NAMES[pomeloState],
                CLASS_NAMES[beansState],
                WiFi.softAPgetStationNum());

  // --- Send SM4-C2H4 + S3 Crop States to Standard ESP32 via UART2 ---
  char txPacket[64];
  snprintf(txPacket, sizeof(txPacket), "S3_DATA,%d,%.2f,%d,%d,%d",
           ethValid ? 1 : 0, eth, cornState, pomeloState, beansState);
  Serial2.println(txPacket);
}