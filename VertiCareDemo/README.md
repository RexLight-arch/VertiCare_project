# VertiCare ESP32 Demo

## 硬件接线

| 模块 | 模块引脚 | ESP32-C6 |
| --- | --- | --- |
| DHT11 | DATA | GPIO4 |
| 光敏模块 | AO | GPIO0 |
| 雨滴模块 | AO | GPIO1 |
| 雨滴模块 | DO，可选 | GPIO5 |
| SW-420 | DO | GPIO6 |
| SG90 | 信号线 | GPIO10 |

传感器使用3.3V供电。SG90建议使用独立5V电源，电源GND必须与
ESP32-C6共地。

## Arduino依赖

- DHT sensor library by Adafruit
- Adafruit Unified Sensor
- PubSubClient by Nick O'Leary
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
- 振动：GPIO6使用下降沿中断捕获SW-420短脉冲，30ms消抖，每2秒至少
  2个脉冲才报告有效振动。
- 健康检测：DHT11连续读取失败或雨滴ADC长期处于极限值时，会上报
  传感器异常。

## 控制逻辑

- 自动模式：空气湿度低于开启阈值且没有下雨时开启灌溉；湿度恢复或
  检测到雨水时停止。
- 自动模式下关键传感器异常时强制停止灌溉；手动模式仍允许人工控制。
- 手动模式：由OneNet或Qt上位机直接控制灌溉。
- SG90关闭角度为0度，打开角度为90度。

## v1.1物模型

升级固件前，先在OneNet重新导入：

```text
onenet/verticare_thing_model_control.json
```

新增只读属性：`dhtHealthy`、`rainSensorHealthy`、`sensorHealthy`。

## 编译验证

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32c6 --libraries "你的Arduino库目录" VertiCareDemo
```
