#include "IrTx.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NightMare.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>

#include <stdint.h>
#include <stdio.h>
#include <string>

namespace watson
{
    namespace ir
    {
        namespace
        {
#if CONFIG_IDF_TARGET_ESP32C3
            // C3 SuperMini: GPIO7 is a free output (8/9 are strapping pins).
            constexpr gpio_num_t kTxPin = GPIO_NUM_7;
#else
            // Classic ESP32 DevKit: GPIO7 is connected to SPI flash.
            constexpr gpio_num_t kTxPin = GPIO_NUM_18;
#endif
            constexpr uint32_t kRmtResolutionHz = 1000000; // 1 tick = 1 us
            constexpr uint32_t kCarrierHz = 38000;

            const ActionArgMetadata kArgs[] = {
                {"code", NetValueType::STRING, true},
            };
            ManagedAction irSend("irsend", kArgs);

            rmt_channel_handle_t txChannel = nullptr;
            rmt_encoder_handle_t copyEncoder = nullptr;
            bool ready = false;

            // The wire uses 32 NEC bits, LSB first. Codes are exactly eight
            // hex digits, optionally prefixed with 0x. The same API also
            // accepts a JSON object with a string "code" field.
            bool parseCode(const std::string &payload, uint32_t &out)
            {
                std::string code = payload;
                if (!code.empty() && code[0] == '{')
                {
                    StaticJsonDocument<128> doc;
                    if (deserializeJson(doc, payload.c_str()))
                        return false;
                    const char *field = doc["code"];
                    if (field == nullptr)
                        return false;
                    code = field;
                }

                if (code.size() >= 2 && code[0] == '0' &&
                    (code[1] == 'x' || code[1] == 'X'))
                    code.erase(0, 2);
                if (code.size() != 8)
                    return false;

                uint32_t value = 0;
                for (char ch : code)
                {
                    uint32_t digit = 0;
                    if (ch >= '0' && ch <= '9')
                        digit = static_cast<uint32_t>(ch - '0');
                    else if (ch >= 'a' && ch <= 'f')
                        digit = static_cast<uint32_t>(ch - 'a' + 10);
                    else if (ch >= 'A' && ch <= 'F')
                        digit = static_cast<uint32_t>(ch - 'A' + 10);
                    else
                        return false;
                    value = (value << 4) | digit;
                }
                out = value;
                return true;
            }

            void symbol(rmt_symbol_word_t &s, uint16_t markUs, uint16_t spaceUs)
            {
                s.level0 = 1;
                s.duration0 = markUs;
                s.level1 = 0;
                s.duration1 = spaceUs;
            }

            ActionResult sendNec(ManagedAction &, const std::string &payload)
            {
                if (!ready)
                    return {false, "IR TX not initialized"};

                uint32_t code = 0;
                if (!parseCode(payload, code))
                    return {false, "expected 8 hex digits (NEC), e.g. 0xEB14FF00"};

                // Start + 32 data bits + stop. Stack data is safe because we
                // wait for RMT to finish before the action handler returns.
                rmt_symbol_word_t frame[34] = {};
                symbol(frame[0], 9000, 4500);
                for (int i = 0; i < 32; i++)
                    symbol(frame[i + 1], 560, (code & (uint32_t(1) << i)) ? 1690 : 560);
                symbol(frame[33], 560, 5000);

                rmt_transmit_config_t config = {};
                esp_err_t err = rmt_transmit(
                    txChannel, copyEncoder, frame, sizeof(frame), &config);
                if (err == ESP_OK)
                    err = rmt_tx_wait_all_done(txChannel, pdMS_TO_TICKS(120));
                if (err != ESP_OK)
                    return {false, "IR transmission failed"};

                char response[32];
                snprintf(response, sizeof(response), "NEC 0x%08lX sent",
                         static_cast<unsigned long>(code));
                return {true, response};
            }
        }

        bool begin()
        {
            irSend.onInvoke = sendNec;
            rmt_tx_channel_config_t config = {};
            config.gpio_num = kTxPin;
            config.clk_src = RMT_CLK_SRC_DEFAULT;
            config.resolution_hz = kRmtResolutionHz;
            config.mem_block_symbols = 64;
            config.trans_queue_depth = 1;

            esp_err_t err = rmt_new_tx_channel(&config, &txChannel);
            if (err != ESP_OK)
                return false;

            rmt_copy_encoder_config_t copyConfig = {};
            err = rmt_new_copy_encoder(&copyConfig, &copyEncoder);
            if (err != ESP_OK)
                return false;

            rmt_carrier_config_t carrier = {};
            carrier.frequency_hz = kCarrierHz;
            carrier.duty_cycle = 0.33f;
            err = rmt_apply_carrier(txChannel, &carrier);
            if (err != ESP_OK)
                return false;

            ready = (rmt_enable(txChannel) == ESP_OK);
            if (ready)
                Serial.printf("# ir: NEC TX ready on GPIO%d at %lu Hz\n",
                              static_cast<int>(kTxPin),
                              static_cast<unsigned long>(kCarrierHz));
            return ready;
        }
    }
}
