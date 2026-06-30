# VertiCare 常见问题排查

## Qt 没有数据显示

- 确认 ESP32 串口打印 `MQTT publish ok`。
- 确认 OneNet 消费组缓存已增长，说明服务端订阅收到消息。
- 确认 `bridge.properties` 中消费组 ID、Key、订阅名称正确。
- 确认 Qt 运行目录下有 `bridge/verticare-bridge.jar` 和 `bridge/bridge.properties`。
- 不要同时打开多个 Qt 上位机实例，同一个 Failover 订阅只能稳定服务一个实例。

## Qt 能显示但不能控制

- 确认 `config.ini` 中 `mockMode=false`。
- 确认 `productAccessKey` 是产品 Access Key，不是设备 Token。
- 确认 OneNet 物模型中控制属性为 `rw`。
- 控制后看 ESP32 串口是否收到 `$sys/.../thing/property/set` 消息。

## 雨滴一直显示有雨

- 确认 AO 接 GPIO1，DO 接 GPIO5，不能接反。
- 开机校准时保持雨滴板干燥。
- 如果此前湿润状态被保存为基线，可以清除 NVS 或临时调大雨滴阈值后重新烧录。

## 维护状态一直待机

- 串口观察 `Vibration pulses in window` 和 `MaintenanceScore`。
- 维护判断需要连续活动窗口确认，不是敲一下立刻保持。
- SW-420 旋钮建议调到静止熄灭、敲击微闪。
- Qt 总览显示依赖 `vibrationDetected` 或 `maintenanceEvent`，若串口已上报 `maintenanceEvent=true` 但 Qt 不变，重启 Qt 和 Bridge。

## RFID 录卡超时

- 确认 RC522 使用 3.3V，不要接 5V。
- 确认 SDA/SS、SCK、MOSI、MISO、RST 接线与最终接线表一致。
- 长按 TTP223 约 3 秒进入录卡模式后，再把卡贴近天线。
- 新卡没有预置业务数据也可以录入，本 demo 使用 UID 作为身份标识。

## ESP32 开机进入 waiting for download

- 通常是外设占用了启动敏感引脚。
- 当前 RC522 已改用 GPIO18/19/20/21/11，TTP223 使用 GPIO12。
- 如果仍出现该问题，先断开新接模块，只保留 USB 启动，再逐个接回排查。

## MQ-135 空气质量长期为 0

- 静置环境下 0 或很低是正常的，表示空气状态清新。
- 对模块吹气后原始值上升，通风指数也应上升。
- MQ-135 需要预热，刚上电数值会漂移；本 demo 展示趋势，不做专业 ppm 标定。
