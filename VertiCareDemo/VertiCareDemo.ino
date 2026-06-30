#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>
#include <SPI.h>
#include <MFRC522.h>

#include "config.h"

static const char *FIRMWARE_VERSION = "v1.5-polish";

DHT dht(DHT_PIN, DHT11);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences preferences;
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

struct SensorData {
  float temperature;
  float airHumidity;
  int lightValue;
  int mq135Value;
  int airQualityPercent;
  int rainValue;
  int rainDigitalValue;
  int vibrationDigitalValue;
  int flameDigitalValue;
  int tiltDigitalValue;
  bool rainDetected;
  bool vibrationDetected;
  bool flameDetected;
  bool tiltDetected;
  bool dhtHealthy;
  bool rainSensorHealthy;
  bool mq135Healthy;
};

struct ControlState {
  bool irrigationOn;
  int servoAngle;
  const char *lightStatus;
  const char *airQualityStatus;
  bool maintenanceEvent;
  bool safetyAlert;
  const char *lastEventType;
  const char *lastEventLevel;
  const char *lastEventMessage;
  unsigned long lastEventTimeSec;
  uint32_t eventSequence;
};

SensorData latestData;
ControlState latestState;

unsigned long lastSensorReadMs = 0;
unsigned long lastMqttPublishMs = 0;
unsigned long maintenanceEventUntilMs = 0;
unsigned long vibrationWindowStartedMs = 0;
unsigned long lastWiFiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long lastFlameEventMs = 0;
unsigned long lastTiltEventMs = 0;
unsigned long lastVibrationEventMs = 0;
unsigned long lastAirQualityEventMs = 0;
unsigned long lastRainEventMs = 0;
unsigned long lastAccessRecordEventMs = 0;
unsigned long lastUnauthorizedMaintenanceEventMs = 0;
unsigned long rfidEnrollStartedMs = 0;
unsigned long rfidAuthorizedUntilMs = 0;
unsigned long touchPressedStartedMs = 0;
unsigned long lastTouchReleasedMs = 0;

volatile uint32_t vibrationPulseCount = 0;
volatile uint32_t lastVibrationPulseUs = 0;

bool previousIrrigationOn = false;
uint8_t rainWetCandidateCount = 0;
uint8_t rainDryCandidateCount = 0;
uint8_t flameActiveCandidateCount = 0;
uint8_t flameInactiveCandidateCount = 0;
uint8_t tiltActiveCandidateCount = 0;
uint8_t tiltInactiveCandidateCount = 0;
uint8_t rainBaselineSampleCount = 0;
uint32_t rainBaselineAccumulator = 0;
int rainDryBaseline = -1;
uint8_t dhtFailureCount = 0;
uint8_t rainAdcFailureCount = 0;
uint8_t mq135AdcFailureCount = 0;
uint8_t maintenanceActivityScore = 0;
bool dhtEverValid = false;
int controlMode = CONTROL_MODE_AUTO;
bool manualIrrigation = false;
float openHumidityThreshold = AIR_HUMIDITY_OPEN_THRESHOLD;
float closeHumidityThreshold = AIR_HUMIDITY_CLOSE_THRESHOLD;
String authorizedUids[RFID_MAX_CARDS];
uint8_t authorizedCardCount = 0;
String currentOperatorId = "未认证";
String lastAccessEvent = "待机";
String lastCardUid = "";
bool rfidEnrollMode = false;
bool rfidAuthorized = false;
bool touchPressed = false;
bool touchLongHandled = false;
bool touchClearHandled = false;

void maintainConnections();
void startWiFiConnection();
void tryConnectMqtt();
void ARDUINO_ISR_ATTR onVibrationPulse();
void setupRfid();
void loadAuthorizedCards();
void saveAuthorizedCards();
void printBootSummary();
void pollTouch();
void pollRfid();
void enterEnrollMode();
void exitEnrollMode(const char *reason);
void clearAuthorizedCards();
void handleShortTouch();
void handleCard(const String &uid);
String uidToString(const MFRC522::Uid &uid);
int findAuthorizedUid(const String &uid);
void readSensors();
void updateVibrationState();
void updateControl();
void updateEvents();
void recordEvent(const char *type, const char *level, const char *message,
                 unsigned long &lastEventMs, unsigned long cooldownMs);
void publishTelemetry();
void onMqttMessage(char *topic, byte *payload, unsigned int length);
void handlePropertySet(const String &payload);
void publishPropertySetReply(const String &requestId, bool ok, const char *message);
String extractJsonString(const String &json, const char *key, const String &fallback);
bool extractJsonBool(const String &json, const char *key, bool fallback);
int extractJsonInt(const String &json, const char *key, int fallback);
float extractJsonFloat(const String &json, const char *key, float fallback);
bool jsonContainsKey(const String &json, const char *key);
void applyServo(bool irrigationOn);
void setupServoPwm();
void writeServoAngle(int angle);
int readMedianAdc(uint8_t pin, uint8_t sampleCount);
void updateRainState(bool rainCandidate, bool dryCandidate);
void updateConfirmedState(bool activeCandidate, bool &state,
                          uint8_t &activeCount, uint8_t &inactiveCount);
const char *classifyLight(int value);
int calculateAirQualityPercent(int value);
const char *classifyAirQuality(int percent);
String buildTelemetryJson();
String buildOneNetPropertyPayload();

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(VIBRATION_PIN, VIBRATION_USE_PULLUP ? INPUT_PULLUP : INPUT);
  pinMode(RAIN_DIGITAL_PIN, RAIN_DIGITAL_USE_PULLUP ? INPUT_PULLUP : INPUT);
  pinMode(FLAME_DIGITAL_PIN, FLAME_USE_PULLUP ? INPUT_PULLUP : INPUT);
  pinMode(TILT_DIGITAL_PIN, TILT_USE_PULLUP ? INPUT_PULLUP : INPUT);
  pinMode(TOUCH_PIN, TOUCH_USE_PULLDOWN ? INPUT_PULLDOWN : INPUT);
  attachInterrupt(digitalPinToInterrupt(VIBRATION_PIN), onVibrationPulse,
                  VIBRATION_ACTIVE_LEVEL == LOW ? FALLING : RISING);

  dht.begin();
  setupServoPwm();

  latestData.temperature = 0.0;
  latestData.airHumidity = 0.0;
  latestData.lightValue = 0;
  latestData.mq135Value = 0;
  latestData.airQualityPercent = 0;
  latestData.rainValue = 0;
  latestData.rainDigitalValue = HIGH;
  latestData.vibrationDigitalValue = HIGH;
  latestData.flameDigitalValue = HIGH;
  latestData.tiltDigitalValue = HIGH;
  latestData.rainDetected = false;
  latestData.vibrationDetected = false;
  latestData.flameDetected = false;
  latestData.tiltDetected = false;
  latestData.dhtHealthy = false;
  latestData.rainSensorHealthy = false;
  latestData.mq135Healthy = false;
  latestState.irrigationOn = false;
  latestState.servoAngle = SERVO_CLOSE_ANGLE;
  latestState.lightStatus = "unknown";
  latestState.airQualityStatus = "unknown";
  latestState.maintenanceEvent = false;
  latestState.safetyAlert = false;
  latestState.lastEventType = "none";
  latestState.lastEventLevel = "info";
  latestState.lastEventMessage = "系统待机";
  latestState.lastEventTimeSec = 0;
  latestState.eventSequence = 0;
  vibrationWindowStartedMs = millis();

  applyServo(false);
  preferences.begin("verticare", false);
  loadAuthorizedCards();
  setupRfid();
  printBootSummary();
  int storedRainBaseline = preferences.getInt("rainBase", -1);
  if (storedRainBaseline >= ADC_HEALTH_MIN &&
      storedRainBaseline <= ADC_HEALTH_MAX) {
    rainDryBaseline = storedRainBaseline;
    rainBaselineSampleCount = RAIN_BASELINE_CALIBRATION_SAMPLES;
    Serial.print(F("Loaded rain dry baseline: "));
    Serial.println(rainDryBaseline);
  }

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
  mqttClient.setKeepAlive(60);
  mqttClient.setCallback(onMqttMessage);

  Serial.println();
  Serial.println(F("VertiCare demo starting..."));
  Serial.print(F("Firmware version: "));
  Serial.println(FIRMWARE_VERSION);
  readSensors();
  updateControl();
  startWiFiConnection();
}

void printBootSummary() {
  Serial.println(F("[BOOT] VertiCare hardware summary"));
  Serial.print(F("[BOOT] Firmware="));
  Serial.println(FIRMWARE_VERSION);
  Serial.print(F("[BOOT] DHT="));
  Serial.print(DHT_PIN);
  Serial.print(F(" LightAO="));
  Serial.print(LIGHT_ADC_PIN);
  Serial.print(F(" RainAO="));
  Serial.print(RAIN_ADC_PIN);
  Serial.print(F(" MQ135AO="));
  Serial.println(MQ135_ADC_PIN);
  Serial.print(F("[BOOT] FlameDO="));
  Serial.print(FLAME_DIGITAL_PIN);
  Serial.print(F(" RainDO="));
  Serial.print(RAIN_DIGITAL_PIN);
  Serial.print(F(" VibDO="));
  Serial.print(VIBRATION_PIN);
  Serial.print(F(" TiltDO="));
  Serial.println(TILT_DIGITAL_PIN);
  Serial.print(F("[BOOT] Servo="));
  Serial.print(SERVO_PIN);
  Serial.print(F(" RFID SS/RST="));
  Serial.print(RFID_SS_PIN);
  Serial.print(F("/"));
  Serial.print(RFID_RST_PIN);
  Serial.print(F(" Touch="));
  Serial.println(TOUCH_PIN);
  Serial.print(F("[BOOT] MQTT publish interval ms="));
  Serial.print(MQTT_PUBLISH_INTERVAL_MS);
  Serial.print(F(" sensor read interval ms="));
  Serial.println(SENSOR_READ_INTERVAL_MS);
  Serial.print(F("[BOOT] Authorized RFID cards="));
  Serial.println(authorizedCardCount);
}

void loop() {
  unsigned long now = millis();
  maintainConnections();
  pollTouch();
  pollRfid();
  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  if (now - lastSensorReadMs >= SENSOR_READ_INTERVAL_MS) {
    lastSensorReadMs = now;
    readSensors();
    updateControl();
  }

  if (now - lastMqttPublishMs >= MQTT_PUBLISH_INTERVAL_MS) {
    lastMqttPublishMs = now;
    publishTelemetry();
  }
}

void maintainConnections() {
  unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long retryInterval =
        max(WIFI_CONNECT_TIMEOUT_MS, WIFI_RECONNECT_INTERVAL_MS);
    if (now - lastWiFiAttemptMs >= retryInterval) {
      startWiFiConnection();
    }
    return;
  }

  if (!mqttClient.connected() &&
      now - lastMqttAttemptMs >= MQTT_RECONNECT_INTERVAL_MS) {
    tryConnectMqtt();
  }
}

void startWiFiConnection() {
  lastWiFiAttemptMs = millis();
  Serial.print(F("Connecting WiFi: "));
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void tryConnectMqtt() {
  lastMqttAttemptMs = millis();
  Serial.print(F("Connecting OneNet MQTT... "));

  bool ok = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
  if (ok) {
    Serial.println(F("connected"));
    bool subscribed = mqttClient.subscribe(ONENET_PROPERTY_SET_TOPIC);
    Serial.print(F("Subscribed property set topic: "));
    Serial.println(subscribed ? F("ok") : F("failed"));
    return;
  }

  Serial.print(F("failed, state="));
  Serial.println(mqttClient.state());
}

void ARDUINO_ISR_ATTR onVibrationPulse() {
  uint32_t nowUs = micros();
  if (nowUs - lastVibrationPulseUs >= VIBRATION_PULSE_DEBOUNCE_US) {
    lastVibrationPulseUs = nowUs;
    ++vibrationPulseCount;
  }
}

void setupRfid() {
  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
  rfid.PCD_Init();
  delay(50);
  Serial.println(F("RFID reader ready"));
}

void loadAuthorizedCards() {
  authorizedCardCount = preferences.getUChar("rfidCount", 0);
  if (authorizedCardCount > RFID_MAX_CARDS) {
    authorizedCardCount = RFID_MAX_CARDS;
  }
  for (uint8_t i = 0; i < authorizedCardCount; ++i) {
    String key = String("rfid") + String(i);
    authorizedUids[i] = preferences.getString(key.c_str(), "");
  }
  Serial.print(F("Loaded authorized RFID cards: "));
  Serial.println(authorizedCardCount);
}

void saveAuthorizedCards() {
  preferences.putUChar("rfidCount", authorizedCardCount);
  for (uint8_t i = 0; i < RFID_MAX_CARDS; ++i) {
    String key = String("rfid") + String(i);
    if (i < authorizedCardCount) {
      preferences.putString(key.c_str(), authorizedUids[i]);
    } else {
      preferences.remove(key.c_str());
    }
  }
}

void pollTouch() {
  unsigned long now = millis();
  bool active = digitalRead(TOUCH_PIN) == TOUCH_ACTIVE_LEVEL;
  if (active && !touchPressed) {
    touchPressed = true;
    touchLongHandled = false;
    touchClearHandled = false;
    touchPressedStartedMs = now;
  }

  if (active && touchPressed) {
    unsigned long held = now - touchPressedStartedMs;
    if (!touchClearHandled && held >= TOUCH_CLEAR_HOLD_MS) {
      touchClearHandled = true;
      touchLongHandled = true;
      clearAuthorizedCards();
      return;
    }
    if (!touchLongHandled && held >= TOUCH_ENROLL_HOLD_MS) {
      touchLongHandled = true;
      if (rfidEnrollMode) {
        exitEnrollMode("手动退出录卡模式");
      } else {
        enterEnrollMode();
      }
    }
  }

  if (!active && touchPressed) {
    unsigned long held = now - touchPressedStartedMs;
    touchPressed = false;
    if (!touchLongHandled && held >= TOUCH_SHORT_MIN_MS &&
        now - lastTouchReleasedMs >= TOUCH_RELEASE_DEBOUNCE_MS) {
      lastTouchReleasedMs = now;
      handleShortTouch();
    }
  }

  if (rfidEnrollMode && now - rfidEnrollStartedMs >= RFID_ENROLL_TIMEOUT_MS) {
    exitEnrollMode("录卡超时");
  }

  if (rfidAuthorized && now > rfidAuthorizedUntilMs) {
    rfidAuthorized = false;
    currentOperatorId = "未认证";
    lastAccessEvent = "授权已过期";
  }
}

void pollRfid() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }
  String uid = uidToString(rfid.uid);
  handleCard(uid);
  updateControl();
  publishTelemetry();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void enterEnrollMode() {
  rfidEnrollMode = true;
  rfidEnrollStartedMs = millis();
  lastAccessEvent = "进入录卡模式";
  recordEvent("rfid", "info", "进入录卡模式",
              lastAccessRecordEventMs, 0);
  Serial.println(F("RFID enroll mode entered"));
}

void exitEnrollMode(const char *reason) {
  rfidEnrollMode = false;
  lastAccessEvent = reason;
  recordEvent("rfid", "info", reason, lastAccessRecordEventMs, 0);
  Serial.print(F("RFID enroll mode exited: "));
  Serial.println(reason);
}

void clearAuthorizedCards() {
  authorizedCardCount = 0;
  for (uint8_t i = 0; i < RFID_MAX_CARDS; ++i) {
    authorizedUids[i] = "";
  }
  saveAuthorizedCards();
  rfidEnrollMode = false;
  rfidAuthorized = false;
  currentOperatorId = "未认证";
  lastAccessEvent = "已清空授权卡";
  recordEvent("rfid", "warning", "已清空授权卡",
              lastAccessRecordEventMs, 0);
  Serial.println(F("All authorized RFID cards cleared"));
}

void handleShortTouch() {
  controlMode = controlMode == CONTROL_MODE_AUTO
                    ? CONTROL_MODE_MANUAL
                    : CONTROL_MODE_AUTO;
  if (controlMode == CONTROL_MODE_MANUAL) {
    manualIrrigation = latestState.irrigationOn;
    lastAccessEvent = "已切换手动模式";
  } else {
    lastAccessEvent = "已切换自动模式";
  }
  recordEvent("touch", "info", controlMode == CONTROL_MODE_AUTO
              ? "触摸切换到自动模式"
              : "触摸切换到手动模式",
              lastAccessRecordEventMs, 0);
  updateControl();
  publishTelemetry();
}

void handleCard(const String &uid) {
  lastCardUid = uid;
  int index = findAuthorizedUid(uid);
  if (rfidEnrollMode) {
    if (index >= 0) {
      currentOperatorId = "Operator " + String(index + 1);
      rfidAuthorized = true;
      rfidAuthorizedUntilMs = millis() + RFID_AUTH_VALID_MS;
      lastAccessEvent = currentOperatorId + " 已存在";
      exitEnrollMode("卡片已存在，录卡结束");
      return;
    }
    if (authorizedCardCount >= RFID_MAX_CARDS) {
      lastAccessEvent = "授权卡已满";
      exitEnrollMode("授权卡已满");
      return;
    }
    authorizedUids[authorizedCardCount] = uid;
    currentOperatorId = "Operator " + String(authorizedCardCount + 1);
    ++authorizedCardCount;
    saveAuthorizedCards();
    rfidAuthorized = true;
    rfidAuthorizedUntilMs = millis() + RFID_AUTH_VALID_MS;
    lastAccessEvent = currentOperatorId + " 录入成功";
    exitEnrollMode("维护人员录入成功");
    return;
  }

  if (index >= 0) {
    currentOperatorId = "Operator " + String(index + 1);
    rfidAuthorized = true;
    rfidAuthorizedUntilMs = millis() + RFID_AUTH_VALID_MS;
    lastAccessEvent = currentOperatorId + " 已认证";
    recordEvent("rfid", "info", "维护人员已认证",
                lastAccessRecordEventMs, 0);
  } else {
    currentOperatorId = "未认证";
    rfidAuthorized = false;
    lastAccessEvent = "未授权卡片";
    recordEvent("rfid", "warning", "检测到未授权卡片",
                lastAccessRecordEventMs, 0);
  }
}

String uidToString(const MFRC522::Uid &uid) {
  String text;
  for (byte i = 0; i < uid.size; ++i) {
    if (uid.uidByte[i] < 0x10) {
      text += "0";
    }
    text += String(uid.uidByte[i], HEX);
  }
  text.toUpperCase();
  return text;
}

int findAuthorizedUid(const String &uid) {
  for (uint8_t i = 0; i < authorizedCardCount; ++i) {
    if (authorizedUids[i] == uid) {
      return i;
    }
  }
  return -1;
}

void readSensors() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  bool dhtReadOk = !isnan(humidity) && !isnan(temperature);

  if (!isnan(humidity)) {
    latestData.airHumidity = humidity;
  } else {
    Serial.println(F("DHT humidity read failed, keeping last value"));
  }

  if (!isnan(temperature)) {
    latestData.temperature = temperature;
  } else {
    Serial.println(F("DHT temperature read failed, keeping last value"));
  }
  if (dhtReadOk) {
    dhtFailureCount = 0;
    dhtEverValid = true;
    latestData.dhtHealthy = true;
  } else {
    if (dhtFailureCount < DHT_FAILURE_LIMIT) {
      ++dhtFailureCount;
    }
    latestData.dhtHealthy =
        dhtEverValid && dhtFailureCount < DHT_FAILURE_LIMIT;
  }

  latestData.lightValue = analogRead(LIGHT_ADC_PIN);
  latestData.mq135Value = readMedianAdc(MQ135_ADC_PIN, MQ135_ADC_SAMPLE_COUNT);
  latestData.airQualityPercent =
      calculateAirQualityPercent(latestData.mq135Value);
  bool mq135AdcValid = latestData.mq135Value >= ADC_HEALTH_MIN &&
                       latestData.mq135Value <= ADC_HEALTH_MAX;
  if (mq135AdcValid) {
    mq135AdcFailureCount = 0;
  } else if (mq135AdcFailureCount < MQ135_ADC_FAILURE_LIMIT) {
    ++mq135AdcFailureCount;
  }
  latestData.mq135Healthy =
      mq135AdcFailureCount < MQ135_ADC_FAILURE_LIMIT;

  latestData.rainValue = readMedianAdc(RAIN_ADC_PIN, RAIN_ADC_SAMPLE_COUNT);
  bool rainAdcValid = latestData.rainValue >= ADC_HEALTH_MIN &&
                      latestData.rainValue <= ADC_HEALTH_MAX;
  if (rainAdcValid) {
    rainAdcFailureCount = 0;
  } else if (rainAdcFailureCount < RAIN_ADC_FAILURE_LIMIT) {
    ++rainAdcFailureCount;
  }
  latestData.rainSensorHealthy =
      (!RAIN_USE_ANALOG ||
       rainAdcFailureCount < RAIN_ADC_FAILURE_LIMIT) &&
      (!RAIN_USE_ANALOG ||
       rainBaselineSampleCount >= RAIN_BASELINE_CALIBRATION_SAMPLES);

  latestData.rainDigitalValue = digitalRead(RAIN_DIGITAL_PIN);
  latestData.vibrationDigitalValue = digitalRead(VIBRATION_PIN);
  latestData.flameDigitalValue = digitalRead(FLAME_DIGITAL_PIN);
  latestData.tiltDigitalValue = digitalRead(TILT_DIGITAL_PIN);

  updateConfirmedState(latestData.flameDigitalValue == FLAME_ACTIVE_LEVEL,
                       latestData.flameDetected, flameActiveCandidateCount,
                       flameInactiveCandidateCount);
  updateConfirmedState(latestData.tiltDigitalValue == TILT_ACTIVE_LEVEL,
                       latestData.tiltDetected, tiltActiveCandidateCount,
                       tiltInactiveCandidateCount);

  bool rainDigitalActive = latestData.rainDigitalValue == RAIN_DIGITAL_ACTIVE_LEVEL;
  if (RAIN_USE_ANALOG &&
      rainBaselineSampleCount < RAIN_BASELINE_CALIBRATION_SAMPLES) {
    rainBaselineAccumulator += latestData.rainValue;
    ++rainBaselineSampleCount;
    latestData.rainDetected = false;
    if (rainBaselineSampleCount == RAIN_BASELINE_CALIBRATION_SAMPLES) {
      rainDryBaseline =
          rainBaselineAccumulator / RAIN_BASELINE_CALIBRATION_SAMPLES;
      preferences.putInt("rainBase", rainDryBaseline);
      latestData.rainSensorHealthy = true;
      Serial.print(F("Rain dry baseline calibrated: "));
      Serial.println(rainDryBaseline);
    }
  }

  int rainWetThreshold = RAIN_ANALOG_WET_WHEN_LOW
                             ? rainDryBaseline - RAIN_WET_DROP_FROM_BASELINE
                             : rainDryBaseline + RAIN_WET_DROP_FROM_BASELINE;
  int rainDryThreshold = RAIN_ANALOG_WET_WHEN_LOW
                             ? rainDryBaseline - RAIN_DRY_DROP_FROM_BASELINE
                             : rainDryBaseline + RAIN_DRY_DROP_FROM_BASELINE;
  bool rainAnalogReady =
      !RAIN_USE_ANALOG ||
      rainBaselineSampleCount >= RAIN_BASELINE_CALIBRATION_SAMPLES;
  bool rainAnalogActive =
      rainAnalogReady &&
      (RAIN_ANALOG_WET_WHEN_LOW
           ? latestData.rainValue <= rainWetThreshold
           : latestData.rainValue >= rainWetThreshold);
  bool rainAnalogDry =
      rainAnalogReady &&
      (RAIN_ANALOG_WET_WHEN_LOW
           ? latestData.rainValue >= rainDryThreshold
           : latestData.rainValue <= rainDryThreshold);
  bool rainCandidate = (RAIN_USE_DIGITAL && rainDigitalActive) ||
                       (RAIN_USE_ANALOG && rainAnalogActive);
  bool dryCandidate = (!RAIN_USE_DIGITAL || !rainDigitalActive) &&
                      (!RAIN_USE_ANALOG || rainAnalogDry);
  if (rainAnalogReady) {
    updateRainState(rainCandidate, dryCandidate);
  }

  updateVibrationState();
}

void updateVibrationState() {
  unsigned long now = millis();
  if (now - vibrationWindowStartedMs < VIBRATION_WINDOW_MS) {
    return;
  }

  noInterrupts();
  uint32_t pulses = vibrationPulseCount;
  vibrationPulseCount = 0;
  interrupts();

  vibrationWindowStartedMs = now;
  bool activeWindow = pulses >= VIBRATION_MIN_PULSES;
  if (activeWindow) {
    if (maintenanceActivityScore < MAINTENANCE_CONFIRM_WINDOWS) {
      ++maintenanceActivityScore;
    }
  } else if (maintenanceActivityScore > 0) {
    --maintenanceActivityScore;
  }
  latestData.vibrationDetected =
      maintenanceActivityScore >= MAINTENANCE_CONFIRM_WINDOWS;

  Serial.print(F("Vibration pulses in window: "));
  Serial.print(pulses);
  Serial.print(F(" MaintenanceScore="));
  Serial.println(maintenanceActivityScore);

  if (latestData.vibrationDetected) {
    maintenanceEventUntilMs = now + MAINTENANCE_EVENT_HOLD_MS;
    Serial.println(F("Maintenance activity confirmed"));
  }
}

int readMedianAdc(uint8_t pin, uint8_t sampleCount) {
  const uint8_t maxSamples = 15;
  int samples[maxSamples];
  uint8_t count = constrain(sampleCount, 1, maxSamples);

  for (uint8_t i = 0; i < count; ++i) {
    samples[i] = analogRead(pin);
    delayMicroseconds(200);
  }

  for (uint8_t i = 1; i < count; ++i) {
    int value = samples[i];
    int8_t j = i - 1;
    while (j >= 0 && samples[j] > value) {
      samples[j + 1] = samples[j];
      --j;
    }
    samples[j + 1] = value;
  }

  return samples[count / 2];
}

void updateRainState(bool rainCandidate, bool dryCandidate) {
  if (rainCandidate) {
    if (rainWetCandidateCount < RAIN_STATE_CONFIRM_SAMPLES) {
      ++rainWetCandidateCount;
    }
    rainDryCandidateCount = 0;
    if (rainWetCandidateCount >= RAIN_STATE_CONFIRM_SAMPLES) {
      latestData.rainDetected = true;
    }
  } else if (dryCandidate) {
    if (rainDryCandidateCount < RAIN_STATE_CONFIRM_SAMPLES) {
      ++rainDryCandidateCount;
    }
    rainWetCandidateCount = 0;
    if (rainDryCandidateCount >= RAIN_STATE_CONFIRM_SAMPLES) {
      latestData.rainDetected = false;
    }
  } else {
    rainWetCandidateCount = 0;
    rainDryCandidateCount = 0;
  }
}

void updateConfirmedState(bool activeCandidate, bool &state,
                          uint8_t &activeCount, uint8_t &inactiveCount) {
  if (activeCandidate) {
    if (activeCount < SAFETY_ACTIVE_CONFIRM_SAMPLES) {
      ++activeCount;
    }
    inactiveCount = 0;
    if (activeCount >= SAFETY_ACTIVE_CONFIRM_SAMPLES) {
      state = true;
    }
    return;
  }

  if (inactiveCount < SAFETY_CLEAR_CONFIRM_SAMPLES) {
    ++inactiveCount;
  }
  activeCount = 0;
  if (inactiveCount >= SAFETY_CLEAR_CONFIRM_SAMPLES) {
    state = false;
  }
}

void updateControl() {
  latestState.lightStatus = classifyLight(latestData.lightValue);
  latestState.airQualityStatus =
      classifyAirQuality(latestData.airQualityPercent);
  latestState.maintenanceEvent = millis() < maintenanceEventUntilMs;
  latestState.safetyAlert =
      latestData.flameDetected || latestData.tiltDetected ||
      latestData.airQualityPercent >= AIR_QUALITY_ALERT_PERCENT;
  updateEvents();
  bool sensorHealthy =
      latestData.dhtHealthy && latestData.rainSensorHealthy &&
      latestData.mq135Healthy;

  bool humidityLow = latestData.airHumidity > 0 && latestData.airHumidity < openHumidityThreshold;
  bool humidityRecovered = latestData.airHumidity >= closeHumidityThreshold;

  if (controlMode == CONTROL_MODE_MANUAL) {
    latestState.irrigationOn = manualIrrigation;
  } else if (!sensorHealthy) {
    latestState.irrigationOn = false;
  } else {
    if (latestData.rainDetected || humidityRecovered) {
      latestState.irrigationOn = false;
    } else if (humidityLow) {
      latestState.irrigationOn = true;
    }
  }

  latestState.servoAngle = latestState.irrigationOn ? SERVO_OPEN_ANGLE : SERVO_CLOSE_ANGLE;

  if (latestState.irrigationOn != previousIrrigationOn) {
    previousIrrigationOn = latestState.irrigationOn;
    applyServo(latestState.irrigationOn);
  }

  Serial.print(F("T="));
  Serial.print(latestData.temperature, 1);
  Serial.print(F("C H="));
  Serial.print(latestData.airHumidity, 1);
  Serial.print(F("% Light="));
  Serial.print(latestData.lightValue);
  Serial.print(F(" MQ135="));
  Serial.print(latestData.mq135Value);
  Serial.print(F(" AQ="));
  Serial.print(latestData.airQualityPercent);
  Serial.print(F("%/"));
  Serial.print(latestState.airQualityStatus);
  Serial.print(F(" RainADC="));
  Serial.print(latestData.rainValue);
  Serial.print(F(" RainBase="));
  Serial.print(rainDryBaseline);
  Serial.print(F(" RainDO="));
  Serial.print(latestData.rainDigitalValue);
  Serial.print(F(" Rain="));
  Serial.print(latestData.rainDetected ? F("yes") : F("no"));
  Serial.print(F(" VibDO="));
  Serial.print(latestData.vibrationDigitalValue);
  Serial.print(F(" Vib="));
  Serial.print(latestData.vibrationDetected ? F("yes") : F("no"));
  Serial.print(F(" FlameDO="));
  Serial.print(latestData.flameDigitalValue);
  Serial.print(F(" Flame="));
  Serial.print(latestData.flameDetected ? F("yes") : F("no"));
  Serial.print(F(" TiltDO="));
  Serial.print(latestData.tiltDigitalValue);
  Serial.print(F(" Tilt="));
  Serial.print(latestData.tiltDetected ? F("yes") : F("no"));
  Serial.print(F(" Irrigation="));
  Serial.print(latestState.irrigationOn ? F("on") : F("off"));
  Serial.print(F(" Mode="));
  Serial.print(controlMode == CONTROL_MODE_MANUAL ? F("manual") : F("auto"));
  Serial.print(F(" RFID="));
  Serial.print(rfidEnrollMode ? F("enroll") : (rfidAuthorized ? F("authorized") : F("idle")));
  Serial.print(F(" Cards="));
  Serial.print(authorizedCardCount);
  Serial.print(F(" Operator="));
  Serial.print(currentOperatorId);
  Serial.print(F(" Sensors="));
  Serial.println(sensorHealthy ? F("ok") : F("fault"));
}

void updateEvents() {
  if (latestData.flameDetected) {
    recordEvent("flame", "critical", "检测到火焰，请立即检查设备周边",
                lastFlameEventMs, EVENT_CRITICAL_COOLDOWN_MS);
  }
  if (latestData.tiltDetected) {
    recordEvent("tilt", "warning", "绿化架发生倾斜，请检查固定结构",
                lastTiltEventMs, EVENT_WARNING_COOLDOWN_MS);
  }
  if (latestData.vibrationDetected) {
    if (rfidAuthorized) {
      recordEvent("maintenance", "notice", "授权维护中",
                  lastVibrationEventMs, EVENT_WARNING_COOLDOWN_MS);
    } else {
      recordEvent("maintenance", "warning", "未授权维护活动",
                  lastUnauthorizedMaintenanceEventMs, EVENT_WARNING_COOLDOWN_MS);
    }
  }
  if (latestData.airQualityPercent >= AIR_QUALITY_ALERT_PERCENT) {
    recordEvent("air", "warning", "空气状态较差，建议通风",
                lastAirQualityEventMs, EVENT_WARNING_COOLDOWN_MS);
  }
  if (latestData.rainDetected) {
    recordEvent("rain", "notice", "检测到雨滴，自动灌溉将保持关闭",
                lastRainEventMs, EVENT_NOTICE_COOLDOWN_MS);
  }
}

void recordEvent(const char *type, const char *level, const char *message,
                 unsigned long &lastEventMs, unsigned long cooldownMs) {
  unsigned long now = millis();
  if (lastEventMs != 0 && now - lastEventMs < cooldownMs) {
    return;
  }
  lastEventMs = now;
  latestState.lastEventType = type;
  latestState.lastEventLevel = level;
  latestState.lastEventMessage = message;
  latestState.lastEventTimeSec = now / 1000;
  ++latestState.eventSequence;
  Serial.print(F("Event recorded: "));
  Serial.print(type);
  Serial.print(F(" / "));
  Serial.print(level);
  Serial.print(F(" / "));
  Serial.println(message);
}

void publishTelemetry() {
  if (!mqttClient.connected()) {
    Serial.println(F("MQTT offline, telemetry skipped"));
    return;
  }

  String payload = buildOneNetPropertyPayload();

  Serial.print(F("Publishing to "));
  Serial.print(ONENET_PROPERTY_POST_TOPIC);
  Serial.print(F(": "));
  Serial.println(payload);

  bool ok = mqttClient.publish(ONENET_PROPERTY_POST_TOPIC, payload.c_str());
  Serial.println(ok ? F("MQTT publish ok") : F("MQTT publish failed"));
}

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  String topicText = String(topic);
  String payloadText;
  payloadText.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    payloadText += (char)payload[i];
  }

  Serial.print(F("MQTT message from "));
  Serial.print(topicText);
  Serial.print(F(": "));
  Serial.println(payloadText);

  if (topicText == ONENET_PROPERTY_SET_TOPIC) {
    handlePropertySet(payloadText);
  }
}

void handlePropertySet(const String &payload) {
  String requestId = extractJsonString(payload, "id", String(millis()));
  bool changed = false;

  if (jsonContainsKey(payload, "controlMode")) {
    controlMode = constrain(extractJsonInt(payload, "controlMode", controlMode), CONTROL_MODE_AUTO, CONTROL_MODE_MANUAL);
    changed = true;
  }

  if (jsonContainsKey(payload, "manualIrrigation")) {
    manualIrrigation = extractJsonBool(payload, "manualIrrigation", manualIrrigation);
    changed = true;
  }

  if (jsonContainsKey(payload, "openThreshold")) {
    openHumidityThreshold = constrain(extractJsonFloat(payload, "openThreshold", openHumidityThreshold), 0.0f, 100.0f);
    changed = true;
  }

  if (jsonContainsKey(payload, "closeThreshold")) {
    closeHumidityThreshold = constrain(extractJsonFloat(payload, "closeThreshold", closeHumidityThreshold), 0.0f, 100.0f);
    changed = true;
  }

  if (openHumidityThreshold > closeHumidityThreshold) {
    float temp = openHumidityThreshold;
    openHumidityThreshold = closeHumidityThreshold;
    closeHumidityThreshold = temp;
  }

  updateControl();
  publishPropertySetReply(requestId, changed, changed ? "success" : "no supported property");
  publishTelemetry();
}

void publishPropertySetReply(const String &requestId, bool ok, const char *message) {
  String reply = "{";
  reply += "\"id\":\"" + requestId + "\",";
  reply += "\"code\":" + String(ok ? 200 : 400) + ",";
  reply += "\"msg\":\"" + String(message) + "\"";
  reply += "}";

  Serial.print(F("Publishing set reply: "));
  Serial.println(reply);
  mqttClient.publish(ONENET_PROPERTY_SET_REPLY_TOPIC, reply.c_str());
}

String extractJsonString(const String &json, const char *key, const String &fallback) {
  String marker = "\"" + String(key) + "\"";
  int keyIndex = json.indexOf(marker);
  if (keyIndex < 0) {
    return fallback;
  }

  int colonIndex = json.indexOf(':', keyIndex + marker.length());
  if (colonIndex < 0) {
    return fallback;
  }

  int quoteStart = json.indexOf('"', colonIndex + 1);
  if (quoteStart < 0) {
    return fallback;
  }

  int quoteEnd = json.indexOf('"', quoteStart + 1);
  if (quoteEnd < 0) {
    return fallback;
  }

  return json.substring(quoteStart + 1, quoteEnd);
}

bool extractJsonBool(const String &json, const char *key, bool fallback) {
  String marker = "\"" + String(key) + "\"";
  int keyIndex = json.indexOf(marker);
  if (keyIndex < 0) {
    return fallback;
  }

  int colonIndex = json.indexOf(':', keyIndex + marker.length());
  if (colonIndex < 0) {
    return fallback;
  }

  int valueIndex = colonIndex + 1;
  while (valueIndex < (int)json.length() && isspace((unsigned char)json[valueIndex])) {
    valueIndex++;
  }

  if (json[valueIndex] == '{') {
    int nestedValue = json.indexOf("\"value\"", valueIndex);
    if (nestedValue > 0) {
      int nestedColon = json.indexOf(':', nestedValue);
      if (nestedColon > 0) {
        valueIndex = nestedColon + 1;
      }
    }
  }

  while (valueIndex < (int)json.length() && isspace((unsigned char)json[valueIndex])) {
    valueIndex++;
  }

  if (json.startsWith("true", valueIndex)) {
    return true;
  }
  if (json.startsWith("false", valueIndex)) {
    return false;
  }
  return fallback;
}

int extractJsonInt(const String &json, const char *key, int fallback) {
  return (int)extractJsonFloat(json, key, fallback);
}

float extractJsonFloat(const String &json, const char *key, float fallback) {
  String marker = "\"" + String(key) + "\"";
  int keyIndex = json.indexOf(marker);
  if (keyIndex < 0) {
    return fallback;
  }

  int colonIndex = json.indexOf(':', keyIndex + marker.length());
  if (colonIndex < 0) {
    return fallback;
  }

  int valueStart = colonIndex + 1;
  while (valueStart < (int)json.length() && isspace((unsigned char)json[valueStart])) {
    valueStart++;
  }

  if (json[valueStart] == '{') {
    int nestedValue = json.indexOf("\"value\"", valueStart);
    if (nestedValue > 0) {
      int nestedColon = json.indexOf(':', nestedValue);
      if (nestedColon > 0) {
        valueStart = nestedColon + 1;
      }
    }
  }

  while (valueStart < (int)json.length() && isspace((unsigned char)json[valueStart])) {
    valueStart++;
  }

  int valueEnd = valueStart;
  while (valueEnd < (int)json.length()) {
    char c = json[valueEnd];
    if (!(isdigit((unsigned char)c) || c == '-' || c == '+' || c == '.')) {
      break;
    }
    valueEnd++;
  }

  if (valueEnd <= valueStart) {
    return fallback;
  }

  return json.substring(valueStart, valueEnd).toFloat();
}

bool jsonContainsKey(const String &json, const char *key) {
  String marker = "\"" + String(key) + "\"";
  return json.indexOf(marker) >= 0;
}

void applyServo(bool irrigationOn) {
  int angle = irrigationOn ? SERVO_OPEN_ANGLE : SERVO_CLOSE_ANGLE;
  writeServoAngle(angle);
}

void setupServoPwm() {
  ledcAttach(SERVO_PIN, SERVO_PWM_FREQ_HZ, SERVO_PWM_RESOLUTION_BITS);
}

void writeServoAngle(int angle) {
  angle = constrain(angle, 0, 180);
  int pulseUs = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  uint32_t maxDuty = (1UL << SERVO_PWM_RESOLUTION_BITS) - 1;
  uint32_t duty = (uint32_t)((uint64_t)pulseUs * maxDuty / SERVO_PWM_PERIOD_US);

  ledcWrite(SERVO_PIN, duty);
}

const char *classifyLight(int value) {
  if (LIGHT_ADC_HIGH_MEANS_DARK) {
    if (value >= LIGHT_DARK_THRESHOLD) {
      return "low";
    }
    if (value <= LIGHT_BRIGHT_THRESHOLD) {
      return "high";
    }
    return "normal";
  }

  if (value < LIGHT_LOW_THRESHOLD) {
    return "low";
  }
  if (value > LIGHT_HIGH_THRESHOLD) {
    return "high";
  }
  return "normal";
}

int calculateAirQualityPercent(int value) {
  int low = min(MQ135_CLEAN_RAW, MQ135_POOR_RAW);
  int high = max(MQ135_CLEAN_RAW, MQ135_POOR_RAW);
  int percent = (value - low) * 100 / max(1, high - low);
  return constrain(percent, 0, 100);
}

const char *classifyAirQuality(int percent) {
  if (percent >= AIR_QUALITY_ALERT_PERCENT) {
    return "poor";
  }
  if (percent >= AIR_QUALITY_WARN_PERCENT) {
    return "warning";
  }
  return "good";
}

String buildTelemetryJson() {
  long authRemainingSec =
      (rfidAuthorized && rfidAuthorizedUntilMs > millis())
          ? (long)((rfidAuthorizedUntilMs - millis()) / 1000UL)
          : 0L;
  String json = "{";
  json += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
  json += "\"temperature\":" + String(latestData.temperature, 1) + ",";
  json += "\"airHumidity\":" + String(latestData.airHumidity, 1) + ",";
  json += "\"lightValue\":" + String(latestData.lightValue) + ",";
  json += "\"lightStatus\":\"" + String(latestState.lightStatus) + "\",";
  json += "\"mq135Value\":" + String(latestData.mq135Value) + ",";
  json += "\"airQualityPercent\":" + String(latestData.airQualityPercent) + ",";
  json += "\"airQualityStatus\":\"" + String(latestState.airQualityStatus) + "\",";
  json += "\"rainDetected\":" + String(latestData.rainDetected ? "true" : "false") + ",";
  json += "\"vibrationDetected\":" + String(latestData.vibrationDetected ? "true" : "false") + ",";
  json += "\"flameDetected\":" + String(latestData.flameDetected ? "true" : "false") + ",";
  json += "\"tiltDetected\":" + String(latestData.tiltDetected ? "true" : "false") + ",";
  json += "\"safetyAlert\":" + String(latestState.safetyAlert ? "true" : "false") + ",";
  json += "\"maintenanceEvent\":" + String(latestState.maintenanceEvent ? "true" : "false") + ",";
  json += "\"lastEventType\":\"" + String(latestState.lastEventType) + "\",";
  json += "\"lastEventLevel\":\"" + String(latestState.lastEventLevel) + "\",";
  json += "\"lastEventMessage\":\"" + String(latestState.lastEventMessage) + "\",";
  json += "\"lastEventTime\":" + String(latestState.lastEventTimeSec) + ",";
  json += "\"eventSequence\":" + String(latestState.eventSequence) + ",";
  json += "\"irrigationState\":" + String(latestState.irrigationOn ? "true" : "false") + ",";
  json += "\"servoAngle\":" + String(latestState.servoAngle) + ",";
  json += "\"controlMode\":" + String(controlMode) + ",";
  json += "\"manualIrrigation\":" + String(manualIrrigation ? "true" : "false") + ",";
  json += "\"openThreshold\":" + String(openHumidityThreshold, 1) + ",";
  json += "\"closeThreshold\":" + String(closeHumidityThreshold, 1) + ",";
  json += "\"rfidEnrollMode\":" + String(rfidEnrollMode ? "true" : "false") + ",";
  json += "\"rfidAuthorized\":" + String(rfidAuthorized ? "true" : "false") + ",";
  json += "\"authorizedCardCount\":" + String(authorizedCardCount) + ",";
  json += "\"currentOperatorId\":\"" + currentOperatorId + "\",";
  json += "\"lastAccessEvent\":\"" + lastAccessEvent + "\",";
  json += "\"lastCardUid\":\"" + lastCardUid + "\",";
  json += "\"authRemainingSec\":" + String(authRemainingSec) + ",";
  json += "\"dhtHealthy\":" + String(latestData.dhtHealthy ? "true" : "false") + ",";
  json += "\"rainSensorHealthy\":" + String(latestData.rainSensorHealthy ? "true" : "false") + ",";
  json += "\"mq135Healthy\":" + String(latestData.mq135Healthy ? "true" : "false") + ",";
  json += "\"sensorHealthy\":" +
          String((latestData.dhtHealthy && latestData.rainSensorHealthy &&
                  latestData.mq135Healthy)
                     ? "true"
                     : "false");
  json += "}";
  return json;
}

String buildOneNetPropertyPayload() {
  long authRemainingSec =
      (rfidAuthorized && rfidAuthorizedUntilMs > millis())
          ? (long)((rfidAuthorizedUntilMs - millis()) / 1000UL)
          : 0L;
  String id = String(millis());
  String json = "{";
  json += "\"id\":\"" + id + "\",";
  json += "\"version\":\"1.0\",";
  json += "\"params\":{";
  json += "\"temperature\":{\"value\":" + String(latestData.temperature, 1) + "},";
  json += "\"airHumidity\":{\"value\":" + String(latestData.airHumidity, 1) + "},";
  json += "\"lightValue\":{\"value\":" + String(latestData.lightValue) + "},";
  json += "\"lightStatus\":{\"value\":\"" + String(latestState.lightStatus) + "\"},";
  json += "\"mq135Value\":{\"value\":" + String(latestData.mq135Value) + "},";
  json += "\"airQualityPercent\":{\"value\":" + String(latestData.airQualityPercent) + "},";
  json += "\"airQualityStatus\":{\"value\":\"" + String(latestState.airQualityStatus) + "\"},";
  json += "\"rainDetected\":{\"value\":" + String(latestData.rainDetected ? "true" : "false") + "},";
  json += "\"vibrationDetected\":{\"value\":" + String(latestData.vibrationDetected ? "true" : "false") + "},";
  json += "\"flameDetected\":{\"value\":" + String(latestData.flameDetected ? "true" : "false") + "},";
  json += "\"tiltDetected\":{\"value\":" + String(latestData.tiltDetected ? "true" : "false") + "},";
  json += "\"safetyAlert\":{\"value\":" + String(latestState.safetyAlert ? "true" : "false") + "},";
  json += "\"maintenanceEvent\":{\"value\":" + String(latestState.maintenanceEvent ? "true" : "false") + "},";
  json += "\"lastEventType\":{\"value\":\"" + String(latestState.lastEventType) + "\"},";
  json += "\"lastEventLevel\":{\"value\":\"" + String(latestState.lastEventLevel) + "\"},";
  json += "\"lastEventMessage\":{\"value\":\"" + String(latestState.lastEventMessage) + "\"},";
  json += "\"lastEventTime\":{\"value\":" + String(latestState.lastEventTimeSec) + "},";
  json += "\"eventSequence\":{\"value\":" + String(latestState.eventSequence) + "},";
  json += "\"irrigationState\":{\"value\":" + String(latestState.irrigationOn ? "true" : "false") + "},";
  json += "\"servoAngle\":{\"value\":" + String(latestState.servoAngle) + "},";
  json += "\"controlMode\":{\"value\":" + String(controlMode) + "},";
  json += "\"manualIrrigation\":{\"value\":" + String(manualIrrigation ? "true" : "false") + "},";
  json += "\"openThreshold\":{\"value\":" + String(openHumidityThreshold, 1) + "},";
  json += "\"closeThreshold\":{\"value\":" + String(closeHumidityThreshold, 1) + "},";
  json += "\"rfidEnrollMode\":{\"value\":" + String(rfidEnrollMode ? "true" : "false") + "},";
  json += "\"rfidAuthorized\":{\"value\":" + String(rfidAuthorized ? "true" : "false") + "},";
  json += "\"authorizedCardCount\":{\"value\":" + String(authorizedCardCount) + "},";
  json += "\"currentOperatorId\":{\"value\":\"" + currentOperatorId + "\"},";
  json += "\"lastAccessEvent\":{\"value\":\"" + lastAccessEvent + "\"},";
  json += "\"lastCardUid\":{\"value\":\"" + lastCardUid + "\"},";
  json += "\"authRemainingSec\":{\"value\":" + String(authRemainingSec) + "},";
  json += "\"dhtHealthy\":{\"value\":" + String(latestData.dhtHealthy ? "true" : "false") + "},";
  json += "\"rainSensorHealthy\":{\"value\":" + String(latestData.rainSensorHealthy ? "true" : "false") + "},";
  json += "\"mq135Healthy\":{\"value\":" + String(latestData.mq135Healthy ? "true" : "false") + "},";
  json += "\"sensorHealthy\":{\"value\":" +
          String((latestData.dhtHealthy && latestData.rainSensorHealthy &&
                  latestData.mq135Healthy)
                     ? "true"
                     : "false") +
          "}";
  json += "}}";
  return json;
}
