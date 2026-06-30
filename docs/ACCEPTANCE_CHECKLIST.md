# VertiCare 最终验收清单

用于每次提交或答辩前快速确认系统状态。

## 硬件

- [ ] ESP32-C6 能正常启动，不进入 `waiting for download`。
- [ ] 所有 GND 共地。
- [ ] MQ-135 AO 已通过 10k/20k 分压后接入 GPIO2。
- [ ] 雨滴 AO 接 GPIO1，DO 接 GPIO5。
- [ ] SW-420 旋钮调到静止熄灭、敲击微闪。
- [ ] RC522 使用 3.3V，IRQ 不接。
- [ ] SG90 使用独立 5V 供电或确认 USB 供电足够稳定。

## ESP32 固件

- [ ] 串口启动后出现 `[BOOT] VertiCare hardware summary`。
- [ ] 串口启动后显示 `Firmware version: v1.5-polish`。
- [ ] WiFi 连接成功，打印本机 IP。
- [ ] OneNet MQTT 连接成功。
- [ ] 串口周期性打印温湿度、光照、雨滴、MQ135、维护、安全、RFID 状态。
- [ ] 周期性打印 `MQTT publish ok`。
- [ ] 触摸长按能进入录卡模式，刷卡能显示授权。

## OneNet

- [ ] 已导入 `VertiCareDemo/onenet/verticare_thing_model_control.json`。
- [ ] 产品 ID 和设备名与 `config.h` 一致。
- [ ] 设备 Token 未过期。
- [ ] 服务端订阅绑定当前产品。
- [ ] 消费组缓存会随设备上报增长。

## Bridge

- [ ] `bridge.properties` 已填写消费组 ID、Key、订阅名、产品 ID、设备名。
- [ ] `verticare-bridge.jar` 已复制到 Qt 运行目录的 `bridge/` 文件夹。
- [ ] Qt 启动输出中出现 `VertiCare bridge connected`。
- [ ] 没有同时运行多个 Qt 上位机实例抢同一个订阅。

## Qt 上位机

- [ ] `config.ini` 位于 exe 同级目录。
- [ ] `mockMode=false`。
- [ ] 总览页显示实时数据。
- [ ] Qt 标题或副标题显示 `v1.5-polish`。
- [ ] 事件页显示系统概况、最近事件、维护人员、灌溉策略。
- [ ] 人员页能显示已录入人数和授权剩余时间。
- [ ] 控制页能切换自动/手动，并能设置阈值。

## 演示动作

- [ ] 自动模式下，低湿度或提高开启阈值能触发灌溉。
- [ ] 雨滴触发后自动灌溉停止。
- [ ] 授权卡刷卡后，维护活动显示为授权维护。
- [ ] 未授权状态下触发维护活动，事件页出现未授权维护提醒。
- [ ] 火焰或倾斜触发后，Qt 顶部状态变为安全告警。
- [ ] 对 MQ-135 吹气后，通风指数明显升高。

## 提交前

- [ ] 不提交 `VertiCareDemo/config.h`。
- [ ] 不提交 `VertiCareQt/config.ini`。
- [ ] 不提交 `VertiCareBridge/bridge.properties`。
- [ ] 不提交包含 `Access_key`、`Token`、设备密钥的临时文件。
- [ ] Qt 能编译通过。
- [ ] Arduino IDE 能编译并烧录当前固件。
