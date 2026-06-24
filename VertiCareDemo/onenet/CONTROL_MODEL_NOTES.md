# OneNet Control Model Notes

Import this file for the second iteration:

```text
VertiCareDemo/onenet/verticare_thing_model_control.json
```

It keeps the first working telemetry model and adds four writable properties:

| Identifier | Type | Access | Meaning |
| --- | --- | --- | --- |
| `controlMode` | int32 | rw | `0` auto, `1` manual |
| `manualIrrigation` | bool | rw | Manual irrigation switch |
| `openThreshold` | float | rw | Auto mode open threshold |
| `closeThreshold` | float | rw | Auto mode close threshold |

The original telemetry properties are unchanged:

```text
temperature
airHumidity
lightValue
lightStatus
rainDetected
vibrationDetected
maintenanceEvent
irrigationState
servoAngle
```

After OneNet imports this model successfully, the Arduino demo will subscribe to property-set commands and update these four properties locally.

## Expected Cloud Control Behavior

Set writable properties from OneNet:

```json
{
  "controlMode": 1,
  "manualIrrigation": true
}
```

Expected device behavior:

- `controlMode = 0`: auto mode, humidity and rain decide irrigation.
- `controlMode = 1`: manual mode, `manualIrrigation` directly controls the servo.
- `manualIrrigation = true`: servo goes to open angle.
- `manualIrrigation = false`: servo goes to close angle.
- `openThreshold` and `closeThreshold` replace the default auto mode humidity thresholds.

The Arduino demo subscribes to:

```text
$sys/{productId}/{deviceName}/thing/property/set
```

and replies to:

```text
$sys/{productId}/{deviceName}/thing/property/set_reply
```
