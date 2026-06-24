#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>

#include "config.h"

DHT dht(DHT_PIN, DHT11);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences preferences;

struct SensorData {
  float temperature;
  float airHumidity;
  int lightValue;
  int rainValue;
  int rainDigitalValue;
  int vibrationDigitalValue;
  bool rainDetected;
  bool vibrationDetected;
  bool dhtHealthy;
  bool rainSensorHealthy;
};

struct ControlState {
  bool irrigationOn;
  int servoAngle;
  const char *lightStatus;
  bool maintenanceEvent;
};

SensorData latestData;
ControlState latestState;

unsigned long lastSensorReadMs = 0;
unsigned long lastMqttPublishMs = 0;
unsigned long maintenanceEventUntilMs = 0;
unsigned long vibrationWindowStartedMs = 0;
unsigned long lastWiFiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;

volatile uint32_t vibrationPulseCount = 0;
volatile uint32_t lastVibrationPulseUs = 0;

bool previousIrrigationOn = false;
uint8_t rainWetCandidateCount = 0;
uint8_t rainDryCandidateCount = 0;
uint8_t rainBaselineSampleCount = 0;
uint32_t rainBaselineAccumulator = 0;
int rainDryBaseline = -1;
uint8_t dhtFailureCount = 0;
uint8_t rainAdcFailureCount = 0;
bool dhtEverValid = false;
int controlMode = CONTROL_MODE_AUTO;
bool manualIrrigation = false;
float openHumidityThreshold = AIR_HUMIDITY_OPEN_THRESHOLD;
float closeHumidityThreshold = AIR_HUMIDITY_CLOSE_THRESHOLD;

void maintainConnections();
void startWiFiConnection();
void tryConnectMqtt();
void ARDUINO_ISR_ATTR onVibrationPulse();
void readSensors();
void updateVibrationState();
void updateControl();
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
const char *classifyLight(int value);
String buildTelemetryJson();
String buildOneNetPropertyPayload();

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(VIBRATION_PIN, VIBRATION_USE_PULLUP ? INPUT_PULLUP : INPUT);
  pinMode(RAIN_DIGITAL_PIN, RAIN_DIGITAL_USE_PULLUP ? INPUT_PULLUP : INPUT);
  attachInterrupt(digitalPinToInterrupt(VIBRATION_PIN), onVibrationPulse,
                  VIBRATION_ACTIVE_LEVEL == LOW ? FALLING : RISING);

  dht.begin();
  setupServoPwm();

  latestData.temperature = 0.0;
  latestData.airHumidity = 0.0;
  latestData.lightValue = 0;
  latestData.rainValue = 0;
  latestData.rainDigitalValue = HIGH;
  latestData.vibrationDigitalValue = HIGH;
  latestData.rainDetected = false;
  latestData.vibrationDetected = false;
  latestData.dhtHealthy = false;
  latestData.rainSensorHealthy = false;
  latestState.irrigationOn = false;
  latestState.servoAngle = SERVO_CLOSE_ANGLE;
  latestState.lightStatus = "unknown";
  latestState.maintenanceEvent = false;
  vibrationWindowStartedMs = millis();

  applyServo(false);
  preferences.begin("verticare", false);
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
  readSensors();
  updateControl();
  startWiFiConnection();
}

void loop() {
  unsigned long now = millis();
  maintainConnections();
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
  latestData.vibrationDetected = pulses >= VIBRATION_MIN_PULSES;

  Serial.print(F("Vibration pulses in window: "));
  Serial.println(pulses);

  if (latestData.vibrationDetected) {
    maintenanceEventUntilMs = now + MAINTENANCE_EVENT_HOLD_MS;
    Serial.println(F("Maintenance/vibration event detected"));
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

void updateControl() {
  latestState.lightStatus = classifyLight(latestData.lightValue);
  latestState.maintenanceEvent = millis() < maintenanceEventUntilMs;
  bool sensorHealthy =
      latestData.dhtHealthy && latestData.rainSensorHealthy;

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
  Serial.print(F(" Irrigation="));
  Serial.print(latestState.irrigationOn ? F("on") : F("off"));
  Serial.print(F(" Mode="));
  Serial.print(controlMode == CONTROL_MODE_MANUAL ? F("manual") : F("auto"));
  Serial.print(F(" Sensors="));
  Serial.println(sensorHealthy ? F("ok") : F("fault"));
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

String buildTelemetryJson() {
  String json = "{";
  json += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
  json += "\"temperature\":" + String(latestData.temperature, 1) + ",";
  json += "\"airHumidity\":" + String(latestData.airHumidity, 1) + ",";
  json += "\"lightValue\":" + String(latestData.lightValue) + ",";
  json += "\"lightStatus\":\"" + String(latestState.lightStatus) + "\",";
  json += "\"rainDetected\":" + String(latestData.rainDetected ? "true" : "false") + ",";
  json += "\"vibrationDetected\":" + String(latestData.vibrationDetected ? "true" : "false") + ",";
  json += "\"maintenanceEvent\":" + String(latestState.maintenanceEvent ? "true" : "false") + ",";
  json += "\"irrigationState\":" + String(latestState.irrigationOn ? "true" : "false") + ",";
  json += "\"servoAngle\":" + String(latestState.servoAngle) + ",";
  json += "\"controlMode\":" + String(controlMode) + ",";
  json += "\"manualIrrigation\":" + String(manualIrrigation ? "true" : "false") + ",";
  json += "\"openThreshold\":" + String(openHumidityThreshold, 1) + ",";
  json += "\"closeThreshold\":" + String(closeHumidityThreshold, 1) + ",";
  json += "\"dhtHealthy\":" + String(latestData.dhtHealthy ? "true" : "false") + ",";
  json += "\"rainSensorHealthy\":" + String(latestData.rainSensorHealthy ? "true" : "false") + ",";
  json += "\"sensorHealthy\":" +
          String((latestData.dhtHealthy && latestData.rainSensorHealthy)
                     ? "true"
                     : "false");
  json += "}";
  return json;
}

String buildOneNetPropertyPayload() {
  String id = String(millis());
  String json = "{";
  json += "\"id\":\"" + id + "\",";
  json += "\"version\":\"1.0\",";
  json += "\"params\":{";
  json += "\"temperature\":{\"value\":" + String(latestData.temperature, 1) + "},";
  json += "\"airHumidity\":{\"value\":" + String(latestData.airHumidity, 1) + "},";
  json += "\"lightValue\":{\"value\":" + String(latestData.lightValue) + "},";
  json += "\"lightStatus\":{\"value\":\"" + String(latestState.lightStatus) + "\"},";
  json += "\"rainDetected\":{\"value\":" + String(latestData.rainDetected ? "true" : "false") + "},";
  json += "\"vibrationDetected\":{\"value\":" + String(latestData.vibrationDetected ? "true" : "false") + "},";
  json += "\"maintenanceEvent\":{\"value\":" + String(latestState.maintenanceEvent ? "true" : "false") + "},";
  json += "\"irrigationState\":{\"value\":" + String(latestState.irrigationOn ? "true" : "false") + "},";
  json += "\"servoAngle\":{\"value\":" + String(latestState.servoAngle) + "},";
  json += "\"controlMode\":{\"value\":" + String(controlMode) + "},";
  json += "\"manualIrrigation\":{\"value\":" + String(manualIrrigation ? "true" : "false") + "},";
  json += "\"openThreshold\":{\"value\":" + String(openHumidityThreshold, 1) + "},";
  json += "\"closeThreshold\":{\"value\":" + String(closeHumidityThreshold, 1) + "},";
  json += "\"dhtHealthy\":{\"value\":" + String(latestData.dhtHealthy ? "true" : "false") + "},";
  json += "\"rainSensorHealthy\":{\"value\":" + String(latestData.rainSensorHealthy ? "true" : "false") + "},";
  json += "\"sensorHealthy\":{\"value\":" +
          String((latestData.dhtHealthy && latestData.rainSensorHealthy)
                     ? "true"
                     : "false") +
          "}";
  json += "}}";
  return json;
}
