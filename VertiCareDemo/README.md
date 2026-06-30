# VertiCare ESP32 Demo

## 硬件接线

完整答辩版接线表见：[`../docs/FINAL_WIRING.md`](../docs/FINAL_WIRING.md)。

| 模块 | 模块引脚 | ESP32-C6 |
| --- | --- | --- |
| DHT11 | DATA | GPIO4 |
| 光敏模块 | AO | GPIO0 |
| 雨滴模块 | AO | GPIO1 |
| MQ-135 | AO，经10k/20k分压 | GPIO2 |
| 火焰模块 | DO | GPIO3 |
| 雨滴模块 | DO，可选 | GPIO5 |
| SW-420 | DO | GPIO6 |
| SW-520D | DO | GPIO7 |
| SG90 | 信号线 | GPIO10 |
| RC522 RFID | RST | GPIO11 |
| TTP223 | OUT | GPIO12 |
| RC522 RFID | SDA/SS | GPIO18 |
| RC522 RFID | SCK | GPIO19 |
| RC522 RFID | MOSI | GPIO20 |
| RC522 RFID | MISO | GPIO21 |

RC522的`IRQ`不接。除MQ-135和SG90外，传感器使用3.3V供电。MQ-135建议5V供电，
AO必须分压后再接入ESP32-C6 ADC。SG90建议使用独立5V电源，
电源GND必须与ESP32-C6共地。

## Arduino依赖

- DHT sensor library by Adafruit
- Adafruit Unified Sensor
- PubSubClient by Nick O'Leary
- MFRC522 by GithubCommunity/Miguel Balboa
- ESP32开发板支持包

## 配置

复制：

```text
config.h.example
```

为：

```text
config.h
```

填写WiFi、OneNet设备身份、Token和Topic。`config.h`包含密钥，已被Git忽略。

## 传感器处理

- 光照：ADC越大通常表示环境越暗，Qt端换算为0到100%的环境亮度。
- 雨滴：开机保持检测板干燥，程序前约10秒建立干燥基线；只有相对基线
  明显下降并连续确认后才报告有雨。首次校准结果会保存到ESP32，
  后续重启直接复用。
- 振动：GPIO6使用下降沿中断捕获SW-420短脉冲，30ms消抖；连续多个
  活动窗口达到脉冲阈值后，才判断为维护活动。
- RFID与触摸：长按TTP223约3秒进入/退出录卡模式，录卡模式下刷卡会
  保存授权卡并自动退出；15秒未刷卡自动退出；长按约8秒清空授权卡。
- MQ-135：读取分压后的ADC原始值，并换算为0到100的通风指数，
  数值越高表示越需要通风；Qt端显示为清新、建议通风、立即通风。
- 火焰与倾斜：数字输入连续确认后才触发安全告警。
- 健康检测：DHT11连续读取失败或雨滴ADC长期处于极限值时，会上报
  传感器异常。

## 控制逻辑

- 自动模式：空气湿度低于开启阈值且没有下雨时开启灌溉；湿度恢复或
  检测到雨水时停止。
- 自动模式下关键传感器异常时强制停止灌溉；手动模式仍允许人工控制。
- 手动模式：由OneNet或Qt上位机直接控制灌溉。
- SG90关闭角度为0度，打开角度为90度。

## v1.4物模型

升级固件前，先在OneNet重新导入：

```text
onenet/verticare_thing_model_control.json
```

新增只读属性：`mq135Value`、`airQualityPercent`、`airQualityStatus`、
`flameDetected`、`tiltDetected`、`safetyAlert`、`mq135Healthy`、
`dhtHealthy`、`rainSensorHealthy`、`sensorHealthy`。

v1.3新增事件中心字段：`lastEventType`、`lastEventLevel`、
`lastEventMessage`、`lastEventTime`、`eventSequence`。ESP32只保存最近
一次事件，Qt根据`eventSequence`去重并保留最近12条展示记录。

v1.4新增RFID字段：`rfidEnrollMode`、`rfidAuthorized`、
`authorizedCardCount`、`currentOperatorId`、`lastAccessEvent`、
`lastCardUid`、`authRemainingSec`。

## 编译验证

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32c6 --libraries "你的Arduino库目录" VertiCareDemo
```

## 现场演示

答辩演示流程见：[`../docs/DEMO_SCRIPT.md`](../docs/DEMO_SCRIPT.md)。
如果出现数据不上报、Qt不刷新、雨滴或维护状态异常，先参考：
[`../docs/TROUBLESHOOTING.md`](../docs/TROUBLESHOOTING.md)。
