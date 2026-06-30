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
restartIntervalMs=5000

[ui]
pollIntervalMs=3000
dataStaleTimeoutMs=15000
controlTimeoutMs=10000
mockMode=false
autoStartBridge=true
```

Bridge配置参照 `VertiCareBridge/bridge.properties.example`。

上位机关闭时，Bridge会自动退出，避免残留进程继续占用Failover订阅。
同一订阅不要同时启动多个上位机实例。

上位机会显示实时数据、数据超时和关键传感器异常状态；控制请求期间
按钮会暂时锁定，并显示设备执行成功、失败或超时结果。

## 本地数据

v1.6开始，上位机会把收到的遥测数据保存到本地SQLite数据库：

```text
程序运行目录/data/verticare.db
```

趋势页显示最近30分钟的温度、湿度、亮度和通风指数；点击“导出CSV”
可以导出最近最多5000条遥测记录。OneNet暂时断线时，Qt启动后会优先显示
SQLite中的最后一条历史数据，并在重新收到OneNet数据后自动刷新为实时状态。

“设置”页可以修改产品ID、设备名、Access Key、Bridge路径、轮询周期、
超时时间、演示模式和开机自动启动Bridge。保存后会写入运行目录下的
`config.ini`，并立即应用当前配置。

## 相关文档

- OneNet通信架构：[`../docs/ONENET_ARCHITECTURE.md`](../docs/ONENET_ARCHITECTURE.md)
- 答辩演示流程：[`../docs/DEMO_SCRIPT.md`](../docs/DEMO_SCRIPT.md)
- 最终验收清单：[`../docs/ACCEPTANCE_CHECKLIST.md`](../docs/ACCEPTANCE_CHECKLIST.md)
