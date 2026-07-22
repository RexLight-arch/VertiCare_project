# VertiCare

VertiCare is an IoT-based vertical greening management system built on
ESP32-C6, OneNet, Java Bridge, and a Qt desktop dashboard. It collects
environmental data from a plant wall, reports telemetry to the cloud, detects
safety and maintenance events, and supports remote irrigation control.

## What It Does

- Monitors temperature, humidity, light, rain, vibration, air quality, flame,
  tilt, and RFID maintenance status.
- Uploads device telemetry to OneNet through MQTT OneJSON property reports.
- Receives cloud-to-device commands through OneNet property setting messages.
- Uses a Java Bridge to consume OneNet service-side subscription messages and
  forward normalized JSON data to the Qt dashboard.
- Provides a Qt 5 desktop dashboard for live monitoring, irrigation control,
  RFID status, event history, trend charts, settings, and CSV export.
- Stores telemetry and events locally with SQLite so the dashboard can show the
  last known data when cloud data is temporarily unavailable.

## System Architecture

```text
ESP32-C6
  -> MQTT OneJSON property report
  -> OneNet thing model
  -> OneNet service-side subscription
  -> Java Bridge
  -> Qt desktop dashboard

Qt desktop dashboard
  -> OneNet HTTP property set API
  -> OneNet MQTT property set message
  -> ESP32-C6
```

The ESP32-C6 and the Qt dashboard do not communicate directly. OneNet acts as
the cloud relay for both telemetry and control commands.

## Main Components

```text
VertiCareDemo/      ESP32-C6 Arduino firmware and OneNet thing model
VertiCareBridge/    Java bridge for OneNet service-side subscription messages
VertiCareQt/        Qt 5.12 desktop dashboard
docs/               Wiring, architecture notes, reports, slides, and assets
```

## Hardware Modules

Current firmware supports the following modules:

- ESP32-C6 development board
- DHT11 temperature and humidity sensor
- Photoresistor light sensor
- MH-RD rain sensor module
- SW-420 vibration sensor
- MQ-135 air quality sensor
- Flame sensor
- SW-520D tilt sensor
- RC522 RFID module
- TTP223 capacitive touch sensor
- SG90 servo

See [docs/FINAL_WIRING.md](docs/FINAL_WIRING.md) for the detailed wiring table.

## Software Stack

| Layer | Technology |
| --- | --- |
| Device firmware | Arduino IDE, ESP32 Arduino Core |
| Cloud platform | OneNet thing model, MQTT OneJSON, HTTP property set API |
| Subscription bridge | Java, Maven, OneNet Pulsar service subscription |
| Desktop dashboard | Qt 5.12.9 MinGW 64-bit |
| Local storage | SQLite |

## Quick Start

### 1. Configure OneNet

Import the thing model:

```text
VertiCareDemo/onenet/verticare_thing_model_control.json
```

Create a OneNet product device, enable MQTT access, and configure a
service-side subscription for telemetry forwarding.

### 2. Configure and Flash the ESP32-C6 Firmware

Copy the firmware configuration template:

```powershell
Copy-Item VertiCareDemo/config.h.example VertiCareDemo/config.h
```

Edit `VertiCareDemo/config.h` with Wi-Fi credentials, OneNet product
information, device name, and MQTT token. Then open
`VertiCareDemo/VertiCareDemo.ino` in Arduino IDE and flash it to the ESP32-C6.

### 3. Build the Java Bridge

Copy the bridge configuration template:

```powershell
Copy-Item VertiCareBridge/bridge.properties.example VertiCareBridge/bridge.properties
```

Fill in the OneNet consumer group, subscription, product, and device settings.
Then build the bridge:

```powershell
mvn -f VertiCareBridge/pom.xml -DskipTests package
```

### 4. Run the Qt Dashboard

Open the Qt project:

```text
VertiCareQt/VertiCareQt.pro
```

Build it with Qt 5.12.9 MinGW 64-bit. Copy the dashboard configuration template
to the application runtime directory:

```powershell
Copy-Item VertiCareQt/config.ini.example path\to\runtime\config.ini
```

Set `mockMode=false`, fill in the OneNet product information, and set
`autoStartBridge=true` if the dashboard should launch the Java Bridge
automatically.

## Runtime Files

The application expects these runtime files when running outside Qt Creator:

```text
VertiCareQt.exe
config.ini
bridge/
  verticare-bridge.jar
  bridge.properties
sqldrivers/
  qsqlite.dll
```

For a packaged Windows build, use Qt `windeployqt` and verify that the SQLite
driver exists under `sqldrivers/`.

## Configuration Safety

The real configuration files are intentionally ignored by Git:

- `VertiCareDemo/config.h`
- `VertiCareBridge/bridge.properties`
- `VertiCareQt/config.ini`

Do not commit Wi-Fi credentials, OneNet access keys, MQTT tokens, device
secrets, or consumer group keys.

## Documentation

- [Wiring](docs/FINAL_WIRING.md)
- [OneNet Architecture](docs/ONENET_ARCHITECTURE.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Acceptance Checklist](docs/ACCEPTANCE_CHECKLIST.md)
- [Release Notes](docs/RELEASE_NOTES.md)
- [GitHub Submission Notes](docs/GITHUB_SUBMISSION.md)

## Verified Environment

```text
ESP32 Arduino Core 3.2.0
Arduino IDE
Qt Creator 4.12.2
Qt 5.12.9 MinGW 64-bit
JDK 8
Maven 3.9.16
OneNet OneJSON
```

