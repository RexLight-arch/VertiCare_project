#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>

#include "config.h"

DHT dht(DHT_PIN, DHT11);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Servo irrigationServo;

struct SensorData {
  float temperature;
  float airHumidity;
  int lightValue;
  int rainValue;
  bool rainDetected;
  bool vibrationDetected;
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
unsigned long lastVibrationMs = 0;
unsigned long maintenanceEventUntilMs = 0;

bool previousIrrigationOn = false;

void connectWiFi();
void connectMqtt();
void readSensors();
void updateControl();
void publishTelemetry();
void applyServo(bool irrigationOn);
const char *classifyLight(int value);
String buildTelemetryJson();
String buildOneNetPropertyPayload();

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(VIBRATION_PIN, INPUT);
  pinMode(RAIN_DIGITAL_PIN, INPUT);

  dht.begin();
  irrigationServo.setPeriodHertz(50);
  irrigationServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);

  latestData.temperature = 0.0;
  latestData.airHumidity = 0.0;
  latestData.lightValue = 0;
  latestData.rainValue = 0;
  latestData.rainDetected = false;
  latestData.vibrationDetected = false;
  latestState.irrigationOn = false;
  latestState.servoAngle = SERVO_CLOSE_ANGLE;
  latestState.lightStatus = "unknown";
  latestState.maintenanceEvent = false;

  applyServo(false);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
  mqttClient.setKeepAlive(60);

  Serial.println();
  Serial.println(F("VertiCare demo starting..."));
  readSensors();
  updateControl();
  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    connectMqtt();
  }
  mqttClient.loop();

  unsigned long now = millis();

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

void connectWiFi() {
  Serial.print(F("Connecting WiFi: "));
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');

    if (millis() - startMs > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println();
      Serial.println(F("WiFi connect timeout, retrying..."));
      WiFi.disconnect();
      delay(800);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      startMs = millis();
    }
  }

  Serial.println();
  Serial.print(F("WiFi connected, IP: "));
  Serial.println(WiFi.localIP());
}

void connectMqtt() {
  while (!mqttClient.connected()) {
    Serial.print(F("Connecting OneNet MQTT... "));

    bool ok = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
    if (ok) {
      Serial.println(F("connected"));
      return;
    }

    Serial.print(F("failed, state="));
    Serial.print(mqttClient.state());
    Serial.println(F(", retry in 3 seconds"));
    delay(3000);
  }
}

void readSensors() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

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

  latestData.lightValue = analogRead(LIGHT_ADC_PIN);
  latestData.rainValue = analogRead(RAIN_ADC_PIN);

  bool rainDigitalActive = digitalRead(RAIN_DIGITAL_PIN) == RAIN_DIGITAL_ACTIVE_LEVEL;
  bool rainAnalogActive = latestData.rainValue <= RAIN_WET_ADC_THRESHOLD;
  latestData.rainDetected = rainDigitalActive || rainAnalogActive;

  bool vibrationActive = digitalRead(VIBRATION_PIN) == VIBRATION_ACTIVE_LEVEL;
  latestData.vibrationDetected = vibrationActive;

  unsigned long now = millis();
  if (vibrationActive && (now - lastVibrationMs > VIBRATION_DEBOUNCE_MS)) {
    lastVibrationMs = now;
    maintenanceEventUntilMs = now + MAINTENANCE_EVENT_HOLD_MS;
    Serial.println(F("Maintenance/vibration event detected"));
  }
}

void updateControl() {
  latestState.lightStatus = classifyLight(latestData.lightValue);
  latestState.maintenanceEvent = millis() < maintenanceEventUntilMs;

  bool humidityLow = latestData.airHumidity > 0 && latestData.airHumidity < AIR_HUMIDITY_OPEN_THRESHOLD;
  bool humidityRecovered = latestData.airHumidity >= AIR_HUMIDITY_CLOSE_THRESHOLD;

  if (latestData.rainDetected || humidityRecovered) {
    latestState.irrigationOn = false;
  } else if (humidityLow) {
    latestState.irrigationOn = true;
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
  Serial.print(F(" Rain="));
  Serial.print(latestData.rainDetected ? F("yes") : F("no"));
  Serial.print(F(" Vib="));
  Serial.print(latestData.vibrationDetected ? F("yes") : F("no"));
  Serial.print(F(" Irrigation="));
  Serial.println(latestState.irrigationOn ? F("on") : F("off"));
}

void publishTelemetry() {
  String payload = buildOneNetPropertyPayload();

  Serial.print(F("Publishing to "));
  Serial.print(ONENET_PROPERTY_POST_TOPIC);
  Serial.print(F(": "));
  Serial.println(payload);

  bool ok = mqttClient.publish(ONENET_PROPERTY_POST_TOPIC, payload.c_str());
  Serial.println(ok ? F("MQTT publish ok") : F("MQTT publish failed"));
}

void applyServo(bool irrigationOn) {
  int angle = irrigationOn ? SERVO_OPEN_ANGLE : SERVO_CLOSE_ANGLE;
  irrigationServo.write(angle);
}

const char *classifyLight(int value) {
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
  json += "\"servoAngle\":" + String(latestState.servoAngle);
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
  json += "\"servoAngle\":{\"value\":" + String(latestState.servoAngle) + "}";
  json += "}}";
  return json;
}
