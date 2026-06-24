# VertiCare 垂直绿化管家

基于 ESP32-C6、OneNet 和 Qt 的垂直绿化监测与灌溉控制课程设计。

## Demo 功能

- DHT11采集环境温度和空气湿度
- 光敏电阻采集环境亮度
- MH-RD雨滴模块检测雨水，并进行自动干燥基线校准
- SW-420通过GPIO中断捕获振动，过滤短时毛刺
- SG90模拟滴灌阀门
- ESP32-C6通过MQTT向OneNet上报物模型属性
- OneNet服务端订阅通过Pulsar向Java Bridge推送数据
- Qt上位机显示实时状态，并通过OneNet HTTP API控制设备
- 支持自动灌溉、手动灌溉和远程阈值设置

## 项目结构

```text
VertiCareDemo/     ESP32-C6 Arduino固件和OneNet物模型
VertiCareBridge/   OneNet Pulsar消息接收、验签和解密服务
VertiCareQt/       Qt 5.12上位机
```

## 通信架构

```text
ESP32 -> OneNet MQTT -> 服务端订阅 -> Pulsar -> Java Bridge -> Qt
Qt -> OneNet HTTP API -> OneNet MQTT -> ESP32
```

## 快速开始

1. 复制 `VertiCareDemo/config.h.example` 为 `config.h`，填写WiFi和设备Token。
2. 使用Arduino IDE选择ESP32-C6开发板并烧录固件。
3. 将 `VertiCareBridge/bridge.properties.example` 复制为
   `bridge.properties`，填写消费组信息。
4. 构建Bridge：

```powershell
mvn -f VertiCareBridge/pom.xml -DskipTests package
```

5. 使用Qt Creator打开 `VertiCareQt/VertiCareQt.pro`。
6. 将 `VertiCareQt/config.ini.example` 复制到程序运行目录并命名为
   `config.ini`，填写产品Access Key，将 `mockMode` 设置为 `false`。

真实配置文件均已加入 `.gitignore`，不要将密钥提交到GitHub。

## 已验证环境

```text
ESP32 Arduino Core 3.2.0
Qt 5.12.9 MinGW 64-bit
JDK 8
Maven 3.9.16
OneNet OneJSON
```
