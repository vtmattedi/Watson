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

## Hardware

| INMP441 | ESP32 DevKit V1 |
| ------- | --------------- |
| VDD     | 3V3             |
| GND     | GND             |
| L/R     | GND (left)      |
| SCK     | GPIO26          |
| WS      | GPIO25          |
| SD      | GPIO33          |

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
