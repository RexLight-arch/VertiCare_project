# VertiCare Qt 上位机

Qt上位机负责展示环境数据和发送灌溉控制指令。

## 运行环境

```text
Qt Creator 4.12.2
Qt 5.12.9 MinGW 64-bit
JDK 8
```

## 运行目录

```text
VertiCareQt.exe
config.ini
bridge/
  verticare-bridge.jar
  bridge.properties
```

## 配置

在程序运行目录创建 `config.ini`：

```ini
[onenet]
productId=YOUR_PRODUCT_ID
deviceName=YOUR_DEVICE_NAME
productAccessKey=YOUR_PRODUCT_ACCESS_KEY
controlApiUrl=https://iot-api.heclouds.com/thingmodel/set-device-property
tokenLifetimeSeconds=3600

[bridge]
javaPath=java
jar=bridge/verticare-bridge.jar
config=bridge/bridge.properties

[ui]
pollIntervalMs=3000
mockMode=false
```

Bridge配置参照 `VertiCareBridge/bridge.properties.example`。

上位机关闭时，Bridge会自动退出，避免残留进程继续占用Failover订阅。
同一订阅不要同时启动多个上位机实例。
