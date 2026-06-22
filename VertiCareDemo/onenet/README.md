# OneNet 物模型导入说明

优先导入：

```text
VertiCareDemo/onenet/verticare_thing_model.json
```

这份文件和 Arduino Demo 里的上报字段保持一致。

## 第一阶段必须用到的属性

当前代码只上报 `properties` 里的字段：

| 标识符 | 类型 | 代码是否上报 |
| --- | --- | --- |
| `temperature` | float | 是 |
| `airHumidity` | float | 是 |
| `lightValue` | int | 是 |
| `lightStatus` | string | 是 |
| `rainDetected` | bool | 是 |
| `vibrationDetected` | bool | 是 |
| `maintenanceEvent` | bool | 是 |
| `irrigationState` | bool | 是 |
| `servoAngle` | int | 是 |

`services` 和 `events` 是给下一轮双向通信预留的。第一版如果 OneNet 导入时嫌服务或事件格式不匹配，可以先只导入/创建属性。

## OneNet 控制台注意点

不同 OneNet 入口的导入 JSON 外层字段可能不完全一样。如果导入失败，通常不是属性设计错了，而是控制台要求的文件格式外壳不同。

遇到这种情况时，保留这些关键字段不变：

- `identifier`
- `name`
- `dataType`
- `specs`

然后按照 OneNet 页面提示调整外层结构。

## 和代码的对应关系

Arduino 上报 payload 在：

```text
VertiCareDemo/VertiCareDemo.ino
```

函数：

```cpp
buildOneNetPropertyPayload()
```

如果你在 OneNet 里修改了属性标识符，也必须同步修改这个函数里的字段名。
