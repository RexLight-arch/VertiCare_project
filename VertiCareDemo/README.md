# VertiCareDemo 垂直绿化管家 Demo

这是课设第一版可运行 Demo，目标是先跑通：

```text
传感器采集 -> 本地控制决策 -> SG90 舵机模拟滴灌 -> OneNet MQTT 上报展示
```

当前实现：

- DHT11 采集空气温度、空气湿度
- 光敏电阻采集光照强度
- MH-RD 雨滴模块采集有雨/无雨状态
- SW-420 检测振动/维护事件
- SG90 舵机模拟滴灌阀门开关
- ESP32-C6 通过 WiFi + MQTT 上传到 OneNet

## Arduino IDE 依赖库

在 Arduino IDE 的库管理器安装：

- `DHT sensor library` by Adafruit
- `Adafruit Unified Sensor`
- `PubSubClient` by Nick O'Leary

还需要安装 ESP32 开发板支持包，并选择你的 ESP32-C6 开发板型号。

## 配置方法

复制：

```text
VertiCareDemo/config.h.example
```

为：

```text
VertiCareDemo/config.h
```

然后填写：

```cpp
WIFI_SSID
WIFI_PASSWORD
MQTT_CLIENT_ID
MQTT_USERNAME
MQTT_PASSWORD
ONENET_PROPERTY_POST_TOPIC
```

OneNet 新平台 MQTT 常见对应关系：

- `MQTT_CLIENT_ID`：设备名称
- `MQTT_USERNAME`：产品 ID
- `MQTT_PASSWORD`：设备 token 或 OneNet 生成的 MQTT 密码/token
- `ONENET_PROPERTY_POST_TOPIC`：`$sys/{产品ID}/{设备名称}/thing/property/post`

如果 OneNet 控制台给你的 topic 不一样，以控制台为准。

## OneNet 物模型属性建议

建议创建产品：`VertiCare`

设备名称：`verticare_01`

物模型属性：

| 标识符 | 类型 | 含义 |
| --- | --- | --- |
| `temperature` | float | 空气温度 |
| `airHumidity` | float | 空气湿度 |
| `lightValue` | int | 光照 ADC 原始值 |
| `lightStatus` | string 或 enum | `low`、`normal`、`high` |
| `rainDetected` | bool | 是否检测到雨水 |
| `vibrationDetected` | bool | 当前是否振动 |
| `maintenanceEvent` | bool | 最近是否触发维护/振动事件 |
| `irrigationState` | bool | 滴灌是否开启 |
| `servoAngle` | int | 舵机角度 |

代码上传的物模型格式类似：

```json
{
  "id": "12345",
  "version": "1.0",
  "params": {
    "temperature": { "value": 26.5 },
    "airHumidity": { "value": 45.0 },
    "lightValue": { "value": 1800 },
    "lightStatus": { "value": "normal" },
    "rainDetected": { "value": false },
    "vibrationDetected": { "value": false },
    "maintenanceEvent": { "value": false },
    "irrigationState": { "value": true },
    "servoAngle": { "value": 90 }
  }
}
```

## 接线草案

先按下面的引脚跑 Demo，后面可根据你的 ESP32-C6 开发板实际丝印调整 `config.h`。

| 模块 | 示例引脚 | 说明 |
| --- | --- | --- |
| DHT11 DATA | GPIO4 | 如果模块支持，优先接 3.3V |
| 光敏电阻 AO | GPIO0 | ADC 输入 |
| 雨滴模块 AO | GPIO1 | ADC 输入 |
| 雨滴模块 DO | GPIO5 | 数字输入 |
| SW-420 DO | GPIO6 | 数字输入 |
| SG90 信号线 | GPIO10 | 舵机建议外接 5V 供电 |

注意：

- 所有模块 GND 必须和 ESP32-C6 GND 共地。
- 不建议用 ESP32-C6 的 3.3V 直接带 SG90。
- 如果某个模块输出 5V 电平，需要降到 3.3V，或者确认模块可用 3.3V 供电。

## 自动滴灌逻辑

第一版控制逻辑：

```text
空气湿度 < AIR_HUMIDITY_OPEN_THRESHOLD，且没有下雨 -> 舵机打开
空气湿度 >= AIR_HUMIDITY_CLOSE_THRESHOLD，或检测到下雨 -> 舵机关闭
```

默认阈值：

```cpp
AIR_HUMIDITY_OPEN_THRESHOLD = 45.0
AIR_HUMIDITY_CLOSE_THRESHOLD = 60.0
```

DHT11 测的是空气湿度，所以这版属于微气候湿度闭环演示。最终升级时建议加入土壤湿度传感器，让滴灌控制更符合真实植物养护。

## 调试步骤

1. 打开 `VertiCareDemo.ino`。
2. 复制并填写 `config.h`。
3. 上传到 ESP32-C6。
4. 打开串口监视器，波特率设为 `115200`。
5. 确认串口显示 WiFi connected。
6. 确认串口显示 OneNet MQTT connected。
7. 在 OneNet 页面查看属性数据是否刷新。
8. 做交互测试：
   - 遮挡/照射光敏电阻，观察 `lightStatus`。
   - 给雨滴模块滴水，观察 `rainDetected` 和 `irrigationState`。
   - 轻敲 SW-420，观察 `maintenanceEvent`。
   - 临时调高/调低湿度阈值，强制观察舵机开关。

## 第一轮需要调的参数

串口会打印原始值，调试时重点改这些：

```cpp
AIR_HUMIDITY_OPEN_THRESHOLD
AIR_HUMIDITY_CLOSE_THRESHOLD
LIGHT_LOW_THRESHOLD
LIGHT_HIGH_THRESHOLD
RAIN_WET_ADC_THRESHOLD
RAIN_DIGITAL_ACTIVE_LEVEL
VIBRATION_ACTIVE_LEVEL
```

如果雨滴或振动状态反了，优先改 active level。
