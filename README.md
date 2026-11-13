# Watson

Watson is the acoustic observation device of the NightMare network: an ESP32
DevKit V1 with an INMP441 I2S microphone that listens to an air conditioner and
reports what it hears over [NightMareNetwork](https://github.com/vtmattedi/NightMareNetwork).

It does not decide whether the AC is on, or whether a command worked. It
publishes three independent observations and leaves the conclusion to the
controller:

| Resource                       | Type                   | Meaning                            |
| ------------------------------ | ---------------------- | ---------------------------------- |
| `acoustic:fan_detected`        | `ManagedSensor<bool>`  | indoor fan noise present, retained |
| `acoustic:compressor_detected` | `ManagedSensor<bool>`  | compressor tone present, retained  |
| `acoustic:beep`                | `ManagedEvent<string>` | one event per accepted beep        |

Network output controls (all default to `true` after boot):

| Resource | Type | Effect when false |
| --- | --- | --- |
| `acoustic:fan:enabled` | `ManagedState<bool>` | withdraws fan state from network |
| `acoustic:compressor:enabled` | `ManagedState<bool>` | withdraws compressor state from network |
| `acoustic:beep:enabled` | `ManagedState<bool>` | suppresses beep events |

These switches affect **network output only**: detection continues, unchanged.
Sensor output resumes with the current observation when re-enabled; beep
events are never queued for replay. These run-time toggles reset to enabled on
reboot. The three original resources and beep JSON remain unchanged.

An additional `irsend` `ManagedAction` transmits one 32-bit NEC frame at
38 kHz (LSB-first), e.g. `{"code":"0xEB14FF00"}`. No receiver, learning
mode, repeats or other protocols are included.

## Hardware

| INMP441 | ESP32 DevKit V1 |
| ------- | --------------- |
| VDD     | 3V3             |
| GND     | GND             |
| L/R     | GND (left)      |
| SCK     | GPIO26          |
| WS      | GPIO25          |
| SD      | GPIO33          |

Optional IR TX on **GPIO18** for the classic ESP32 DevKit. Use an IR LED
through a transistor with suitable resistors (do not drive the LED directly
from GPIO). The source also reserves GPIO7 on ESP32-C3 builds, but the current
firmware/build target remains the two-core ESP32 DevKit V1: C3 audio performance
and memory have not been validated.

## Build

```text
cp include/creds.example.h include/creds.h     # then fill it in; creds.h is gitignored
pio run -e esp32dev                            # with the raw PCM debug server
pio run -e esp32dev-notcp                      # without it
pio run -e esp32dev -t upload --upload-port COM18
```

NightMareNetwork is fetched from GitHub by `lib_deps`. `-DNM_ENABLE_OTA=1` in
`platformio.ini` has to stay in step with `NM_ENABLE_OTA` in
`include/NightMareConfig.h`.

## More

- [docs/WATSON_FIRMWARE.md](docs/WATSON_FIRMWARE.md): pipeline, detectors,
  Configs, ESP-NOW/MQTT connection, validation and known limits.
- [tools/audio/README.md](tools/audio/README.md): the host-side tooling for
  recording, analysing and live-classifying the raw PCM stream.
- `validation/`: the float64 reference and the golden-vector and replay tests
  that keep the firmware's analysis in step with it.
