#pragma once

// NightMareNetwork feature selection for Watson.
//
// Watson is an acoustic observation device: it publishes two Sensors and one
// Event, and exposes its detector settings as Configs. It needs the Resource
// layer, a network connection, Wi-Fi (broker + OTA) and OTA. It does not need
// HTTP, WebSocket or LVGL.

#define NM_FIRMWARE_VERSION "watson-1.0.0"

#define NM_ENABLE_SETTINGS 1
#define NM_ENABLE_RESOURCES 1
#define NM_ENABLE_NETWORK 1
#define NM_ENABLE_CONSOLE 1
#define NM_ENABLE_WIFI 1
#define NM_ENABLE_MQTT 1
#define NM_ENABLE_TELEMETRY 1
#define NM_ENABLE_SCHEDULER 1
#define NM_ENABLE_JOBS 1
// The TLS broker connection validates certificates against the wall clock, so
// SNTP is required even though Watson itself only uses uptime.
#define NM_ENABLE_TIME_SYNC 1

// OTA is NightMareNetwork's, not a second project-specific ArduinoOTA.
#define NM_ENABLE_OTA 1
#define NM_ENABLE_HTTP 0
#define NM_ENABLE_WEBSOCKET 0
#define NM_ENABLE_LVGL 0

// Connectivity services. Watson starts ESP-NOW first. Wi-Fi/IP remains up for
// SNTP, OTA and the raw PCM debug server, but MQTT itself stays disabled while
// ESP-NOW is active and is enabled only by Watson's failover controller.
// There is no local broker. ESP-NOW needs NM_ESPNOW_PSK in creds.h.
#define NM_NETWORK_MQTT 1
#define NM_NETWORK_LOCALMQTT 0
#define NM_NETWORK_ESPNOW 1

// Watson owns MQTT failover timing when both transports are built. Keep
// NightMare from enabling MQTT merely because Wi-Fi/IP is enabled; setup()
// starts Wi-Fi/IP explicitly and enables MQTT only after ESP-NOW fails.
#if NM_ENABLE_MQTT && NM_NETWORK_ESPNOW
#define NM_WIFI_AUTO 0
#endif

#define NM_CONSOLE_BUILTINS 1
// Serial carries the NightMare console (CONFIG ..., > resource ..., and the
// WATSON STATS command registered in main.cpp). It never carries audio.
#define NM_CONSOLE_SERIAL 1

// The Scheduler runs from tickNightMareESP() in loop() rather than in a task
// of its own: loop() spins every few ms, and a task stack is internal RAM that
// the TLS session needs.
#define NM_SCHEDULER_OWN_TASK 0

#define NM_LOG_LEVEL 2
