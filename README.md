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

## Hardware

| INMP441 | ESP32 DevKit V1 |
| ------- | --------------- |
| VDD     | 3V3             |
| GND     | GND             |
| L/R     | GND (left)      |
| SCK     | GPIO26          |
| WS      | GPIO25          |
| SD      | GPIO33          |

Experimental **ESP32-C3 SuperMini** wiring:

| INMP441 | ESP32-C3 SuperMini |
| --- | --- |
| VDD / GND | 3V3 / GND |
| L/R | GND (mono, left) |
| SCK / WS / SD | GPIO5 / GPIO6 / GPIO7 |

GPIO8 is left unused: on common SuperMini boards it drives the onboard
LED and is a boot strapping pin. The C3 target uses core 0 for all tasks;
raw PCM TCP is compiled out. Its
single-core CPU load and smaller RAM have **not** been hardware validated.

## Build

```text
cp include/creds.example.h include/creds.h     # then fill it in; creds.h is gitignored
pio run -e esp32dev                            # with the raw PCM debug server
pio run -e esp32dev-notcp                      # without it
pio run -e esp32dev -t upload --upload-port COM18
pio run -e esp32c3-supermini                   # experimental C3 build
pio run -e esp32c3-supermini -t upload --upload-port <port>
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
