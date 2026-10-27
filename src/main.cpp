#include <Arduino.h>
#include <NightMare.h>
#include <esp_heap_caps.h>

#include "audio/SelfTest.h"
#include "audio/WatsonAcoustic.h"

// ============================================================
// Watson: an acoustic observation device.
//
// ESP32 DevKit V1 + INMP441
//
//   INMP441 VDD -> 3V3        INMP441 SCK -> GPIO26
//   INMP441 GND -> GND        INMP441 WS  -> GPIO25
//   INMP441 L/R -> GND        INMP441 SD  -> GPIO33
//
// It listens and reports three independent observations over NightMare:
//
//   acoustic:fan_detected         state
//   acoustic:compressor_detected  state
//   acoustic:beep                 event
//
// It does not infer whether the air conditioner is on, or whether a command
// worked: the controller combines these with what it sent.
//
// Layout (src/audio, src/tcp):
//   AudioCapture   I2S DMA -> blocks              high-priority task, lossless
//   WatsonCore     blocks -> windows -> detectors analysis task, bounded
//   RawAudioTcp    blocks -> PC debug stream      own task, drops under pressure
//   WatsonAcoustic glue; the only code that talks to NightMare, from loop()
//
// Serial is the NightMare console (CONFIG LIST, > acoustic:fan_detected, ...)
// plus WATSON STATS. It never carries audio.
// ============================================================

// The Arduino default is 8 KB. loop() only runs NightMare's cooperative tick and
// the publication pump; the internal RAM is worth more elsewhere.
SET_LOOP_TASK_STACK_SIZE(7 * 1024);

namespace
{
    // WATSON STATS: acquisition, analysis, detector and TCP counters.
    NightMareResults onWatsonCommand(const NightMareMessage &message)
    {
        NightMareResults result{false, "", NightmareContext()};

        if (message.command != "WATSON")
            return result;

        result.result = true;
        if (message.subcommand == "STATS" || message.subcommand.length() == 0)
            result.response = watson::gWatson.report();
        else if (message.subcommand == "SELFTEST")
        {
            // Golden windows through this device's own FFT, against the
            // float64 reference.
            char text[768];
            watson::runSelfTest(text, sizeof(text));
            result.response = text;
        }
        else if (message.subcommand == "SIMFLAT")
        {
            // Test hook: make the acquisition see a dead microphone, to
            // exercise the flatline -> hardware-disconnected path without
            // unplugging anything.  WATSON SIMFLAT ON | OFF
            const bool on = NightMare::Text::equalsIgnoreCase(message.args[0], "ON");
            if (!on && !NightMare::Text::equalsIgnoreCase(message.args[0], "OFF"))
                result.response = "usage: WATSON SIMFLAT ON | OFF";
            else
            {
                watson::gWatson.simulateDeadMic(on);
                result.response = on ? "simulating a dead microphone"
                                     : "microphone input restored";
            }
        }
        else
            result.response = "usage: WATSON STATS | SELFTEST | SIMFLAT ON|OFF";
        return result;
    }
} // namespace

// NightMareNetwork's OTA. Flash writes stall the cache, so the audio lost while
// an update runs is accounted for separately (see AudioCapture::setMaintenance).
void onOta(OTA_INFO info, int data)
{
    static int lastPercent = -10;
    switch (info)
    {
    case OTA_START:
        lastPercent = -10;
        watson::gWatson.setMaintenance(true);
        Serial.println("# ota: update started");
        break;
    case OTA_PROGRESS:
        if (data >= lastPercent + 10)
        {
            lastPercent = data;
            Serial.printf("# ota: %d%%\n", data);
        }
        break;
    case OTA_END:
        Serial.println("# ota: update finished, restarting");
        break;
    case OTA_ERROR:
        watson::gWatson.setMaintenance(false);
        Serial.printf("# ota: error %d\n", data);
        break;
    }
}

#if NM_ENABLE_WIFI
static void onWifiConnected(bool firstConnection)
{
    const NightMare::WiFiInfo info = WiFi_info();
    Serial.printf("# wifi: IP %s: %s (SSID %s, BSSID %s, channel %u, RSSI %d dBm)\n",
                  firstConnection ? "acquired" : "reacquired",
                  info.ip.c_str(), info.ssid.c_str(), info.bssid.c_str(),
                  static_cast<unsigned>(info.channel), static_cast<int>(info.rssi));
}
#endif

#if NM_ENABLE_MQTT && NM_NETWORK_ESPNOW
// NightMareNetwork deliberately separates routing preference from service
// lifecycle. Watson owns the latter: ESP-NOW is tried first and MQTT is not
// enabled until ESP-NOW has been unavailable for this long. Zero disables
// automatic MQTT failover, matching the former NMNW Config semantics.
static Config<int> mqttFailoverSeconds("nightmare:connection:failover_secs", 60);
static uint32_t espNowUnavailableSinceMs = 0;
static bool espNowUnavailableTiming = false;

// Reassert the routing preference at boot so a stale persisted MQTT preference
// cannot reverse the intended order.
static void preferEspNow()
{
    if (NightMare::GetPreferredConnection() == NightMare::ConnectionType::ESP_NOW)
        return;

    if (NightMare::SelectConnection(NightMare::ConnectionType::ESP_NOW))
        Serial.println("# watson: preferred connection set to ESP-NOW (failover: MQTT)");
    else
        Serial.println("ERROR: unable to set ESP-NOW routing preference");
}

static void manageMqttFailover()
{
    const bool espNowActive =
        NightMare::EspNow_state() == NightMare::ConnectivityState::CONNECTED &&
        NightMare::GetActiveConnection() == NightMare::ConnectionType::ESP_NOW;

    if (espNowActive)
    {
        espNowUnavailableTiming = false;
        if (NightMare::Mqtt_enabled())
        {
            NightMare::Mqtt_disable();
            Serial.println("# watson: ESP-NOW active; MQTT failover stopped");
        }
        return;
    }

    if (!espNowUnavailableTiming)
    {
        espNowUnavailableSinceMs = millis();
        espNowUnavailableTiming = true;
    }

    if (NightMare::Mqtt_enabled())
        return;

    const int delaySeconds = mqttFailoverSeconds.value();
    if (delaySeconds <= 0)
        return;

    const bool failed = NightMare::EspNow_state() == NightMare::ConnectivityState::ERROR;
    const uint32_t delayMs = static_cast<uint32_t>(delaySeconds) * 1000UL;
    if (!failed && millis() - espNowUnavailableSinceMs < delayMs)
        return;

    if (NightMare::Mqtt_enable(NightMare::MqttProfile::REMOTE))
        Serial.println("# watson: ESP-NOW unavailable; MQTT failover started");
    else
        Serial.println("ERROR: unable to start MQTT failover");
}
#endif

void setup()
{
    Serial.begin(921600);
    delay(300);

    Serial.println();
    Serial.println("Watson acoustic observer");
    Serial.printf("# heap: %lu free at boot\n", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT));

    if (!watson::gWatson.begin())
    {
        Serial.println("ERROR: audio start failed; halting");
        while (true)
            delay(1000);
    }

    Serial.printf("# heap: %lu free after audio buffers (largest block %lu)\n",
                  (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    setCommandResolver(onWatsonCommand);
    onOTAEvent(onOta);
#if NM_ENABLE_WIFI
    WiFi_onConnected(onWifiConnected);
#endif

    // NightMare restores Configs and starts the radio and ESP-NOW. NM_WIFI_AUTO
    // is off so it cannot also start MQTT before Watson declares a failover.
    startNightMareESP();
#if NM_ENABLE_MQTT && NM_NETWORK_ESPNOW
    preferEspNow();

    // Keep the IP service available for SNTP, OTA and debug TCP without
    // enabling its MQTT client. MQTT is owned by manageMqttFailover().
    if (NightMare::WiFiBegin())
        Serial.println("# watson: default connectivity Wi-Fi/IP + ESP-NOW; MQTT armed for failover");
    else
        Serial.println("ERROR: Wi-Fi/IP start failed; MQTT failover unavailable");
#endif

    watson::gWatson.startAnalysis();

    Serial.printf("# heap: %lu free after NightMare start (largest block %lu)\n",
                  (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    Serial.println("Ready. WATSON STATS for counters; CONFIG LIST for settings.");
}

void loop()
{
    tickNightMareESP();
#if NM_ENABLE_MQTT && NM_NETWORK_ESPNOW
    manageMqttFailover();
#endif
    watson::gWatson.pump();

    // Short enough that a beep or a state change is published within a few
    // ms, long enough to leave the core to the analysis task.
    delay(5);
}
