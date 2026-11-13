# Watson

Watson is the acoustic observation device of the NightMare network: a Waveshare
ESP32-C3-Zero with one INMP441 I2S microphone that listens to an air conditioner
and reports what it hears over [NightMareNetwork](https://github.com/vtmattedi/NightMareNetwork).
The original ESP32 DevKit V1 target remains available.

It does not decide whether the AC is on, or whether a command worked. It
publishes three independent observations and leaves the conclusion to the
controller:

| Resource                       | Type                   | Meaning                            |
| ------------------------------ | ---------------------- | ---------------------------------- |
| `acoustic:fan_detected`        | `ManagedSensor<bool>`  | indoor fan noise present, retained |
| `acoustic:compressor_detected` | `ManagedSensor<bool>`  | compressor tone present, retained  |
| `acoustic:beep`                | `ManagedEvent<string>` | one event per accepted beep        |

## Hardware

**Waveshare ESP32-C3-Zero — one INMP441 (default)**

| INMP441 | ESP32-C3-Zero |
| ------- | ------------- |
| VDD     | 3V3           |
| GND     | GND           |
| L/R     | GND (left channel) |
| WS / LRCLK | **GPIO6** |
| SCK / BCLK | **GPIO7** |
| SD / DOUT | **GPIO4** |

All connections use 3.3 V logic. These three GPIOs avoid the board's
boot-strapping pins (GPIO8/9). GPIO9 is BOOT and GPIO10 drives the onboard
WS2812 LED. No IR transmitter is required.

The older **ESP32 DevKit V1** build remains supported with SCK=GPIO26,
WS=GPIO25, SD=GPIO33, plus VDD=3V3, GND=GND and L/R=GND.

The C3 is single-core and has less compute/memory headroom than the classic
ESP32. The audio DSP, ESP-NOW/MQTT failover, and OTA need on-device validation;
raw PCM TCP is disabled on the C3 to reduce memory pressure.

## Build

```text
cp include/creds.example.h include/creds.h     # then fill it in; creds.h is gitignored
pio run                                        # defaults to esp32c3-zero
pio run -e esp32c3-zero -t upload --upload-port <port>
pio device monitor -p <port> -b 921600
pio run -e esp32dev                            # legacy ESP32, raw PCM TCP
pio run -e esp32dev-notcp                      # legacy ESP32, no raw PCM TCP
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
