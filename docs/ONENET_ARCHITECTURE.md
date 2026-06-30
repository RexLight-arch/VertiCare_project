# VertiCare OneNet 通信架构说明

当前 demo 使用 OneNet 作为云端中转，ESP32 和 Qt 上位机不直接通信。

## 总体链路

```text
ESP32-C6
  -> MQTT OneJSON 属性上报
  -> OneNet 物模型
  -> 服务端订阅 / Pulsar
  -> VertiCareBridge
  -> Qt 上位机实时展示

Qt 上位机
  -> OneNet HTTP API 设置设备属性
  -> OneNet 下发属性设置
  -> ESP32-C6 接收控制指令
```

## 为什么需要 Bridge

OneNet 的服务端订阅不是普通 MQTT 客户端直连，而是基于 Pulsar 的消费组订阅。
Qt 5.12 自身没有直接消费 OneNet Pulsar 服务端订阅的现成能力，所以项目中加入
`VertiCareBridge`：

- 负责连接 OneNet 服务端订阅。
- 使用消费组 ID 和消费组 Key 完成鉴权。
- 校验消息签名并解密消息体。
- 只保留当前产品和设备的物模型属性消息。
- 将遥测数据整理成一行 JSON，通过标准输出交给 Qt。

Qt 只需要启动 Bridge 进程并读取输出，不直接处理 Pulsar 协议细节。

## 设备身份

| 端 | 身份 | 用途 |
| --- | --- | --- |
| ESP32 | OneNet 设备 `verticare_01` | MQTT 上报属性、接收属性设置 |
| Qt | 产品 Access Key + 服务端订阅消费组 | 查询/设置属性、接收设备上报 |
| Bridge | 消费组 ID / Key / 订阅名 | 消费 OneNet 服务端订阅消息 |

Qt 不需要单独创建一个 OneNet 设备。它是上位机客户端，不是被监测设备。

## 数据方向

### ESP32 上报

ESP32 使用 OneJSON 格式向如下主题发布属性：

```text
$sys/{productId}/{deviceName}/thing/property/post
```

上报数据包括环境、安全、人员、事件和控制状态。

### Qt 控制

Qt 通过 OneNet HTTP API 设置物模型属性，当前可控属性是：

| 标识符 | 说明 |
| --- | --- |
| `controlMode` | 0 为自动，1 为手动 |
| `manualIrrigation` | 手动模式下灌溉开关 |
| `openThreshold` | 自动灌溉开启湿度阈值 |
| `closeThreshold` | 自动灌溉关闭湿度阈值 |

ESP32 收到 OneNet 属性设置消息后更新本地状态，并发布设置回复。

## 物模型分组

### 环境监测

| 标识符 | 说明 |
| --- | --- |
| `temperature` | 空气温度 |
| `airHumidity` | 空气湿度 |
| `lightValue` | 光照 ADC 原始值 |
| `lightStatus` | 光照状态 |
| `rainDetected` | 是否检测到雨滴 |
| `mq135Value` | MQ-135 ADC 原始值 |
| `airQualityPercent` | 通风指数，0 到 100 |
| `airQualityStatus` | 空气状态 |

### 灌溉控制

| 标识符 | 说明 |
| --- | --- |
| `irrigationState` | 当前是否灌溉 |
| `servoAngle` | SG90 舵机角度 |
| `controlMode` | 自动/手动模式 |
| `manualIrrigation` | 手动灌溉开关 |
| `openThreshold` | 开启阈值 |
| `closeThreshold` | 关闭阈值 |

### 安全与维护

| 标识符 | 说明 |
| --- | --- |
| `vibrationDetected` | 当前窗口是否确认维护活动 |
| `maintenanceEvent` | 维护事件保持状态 |
| `flameDetected` | 火焰告警 |
| `tiltDetected` | 倾斜告警 |
| `safetyAlert` | 综合安全告警 |

### RFID 人员

| 标识符 | 说明 |
| --- | --- |
| `rfidEnrollMode` | 是否处于录卡模式 |
| `rfidAuthorized` | 当前是否有授权人员 |
| `authorizedCardCount` | 已录入卡数量 |
| `currentOperatorId` | 当前维护人员编号 |
| `lastAccessEvent` | 最近刷卡事件 |
| `lastCardUid` | 最近卡 UID |
| `authRemainingSec` | 授权剩余时间 |

### 事件中心

| 标识符 | 说明 |
| --- | --- |
| `lastEventType` | 最近事件类型 |
| `lastEventLevel` | 最近事件等级 |
| `lastEventMessage` | 最近事件说明 |
| `lastEventTime` | 最近事件时间 |
| `eventSequence` | 事件序号，Qt 用它去重 |

### 传感器健康

| 标识符 | 说明 |
| --- | --- |
| `dhtHealthy` | DHT11 状态 |
| `rainSensorHealthy` | 雨滴 ADC 状态 |
| `mq135Healthy` | MQ-135 ADC 状态 |
| `sensorHealthy` | 综合关键传感器状态 |

## 运行注意

- 同一个消费组 Failover 订阅建议只运行一个 Qt 上位机实例。
- `config.h`、`config.ini`、`bridge.properties` 都包含密钥或敏感配置，不提交 GitHub。
- 若 Qt 没有实时数据，优先检查 Bridge 是否连接成功，再检查消费组是否收到 OneNet 消息。
