#pragma once

#include <stddef.h>
#include <stdint.h>

// Structural DSP parameters. These fix buffer sizes and timing, so they are
// compile-time constants and deliberately not NightMare Configs.

namespace watson
{
    constexpr uint32_t kSampleRate = 16000;

    // Analysis: 2048-point Hamming FFT every 512 samples.
    //   bin width        7.8125 Hz   (compressor fundamental ~58.6 Hz)
    //   decision cadence 32 ms
    //   overlap          75 %        (analysis span 128 ms)
    constexpr int kFftSize = 2048;
    constexpr int kHop = 512;
    constexpr int kBins = kFftSize / 2 + 1;
    constexpr uint32_t kHopMs = 1000UL * kHop / kSampleRate; // 32

    // One acquisition block is exactly one hop and one TCP frame, so each
    // consumer sees the same unit and "dropped" always counts whole frames.
    constexpr int kBlockSamples = kHop;

    // INMP441: 24 significant bits. Samples are delivered right-aligned in an
    // int32 (the I2S word >> 8), the same as the PC tools expect, so full
    // scale is 2^23.
    constexpr float kFullScale = 8388608.0f;

    // Beep detector timing model. The PC detector describes a tone by one
    // "window" length, used for the duration correction and the refractory,
    // and it was calibrated for the PC's 64 ms window. A 128 ms Hamming
    // window flags only about as much extra length as a 64 ms one (its outer
    // quarters carry almost no weight), so the firmware keeps the PC's 64 ms
    // footprint rather than the true 128 ms. With the true length the
    // reviewed beeps read as too_short and bursts of beeps 250 ms apart merge;
    // with 64 ms every beep the PC finds is found (validation/calibrate_beep.py).
    constexpr uint32_t kBeepFootprintMs = 64;

    // I2S DMA: 8 descriptors x 256 samples = 128 ms of slack in front of the
    // acquisition task (8 KB of internal RAM).
    constexpr int kDmaDescriptors = 8;
    constexpr int kDmaFrames = 256;

    // Blocks (32 ms each) the analysis and TCP queues can hold before they
    // start dropping. Analysis needs enough to ride out a full FFT burst plus
    // the time startNightMareESP() takes at boot.
    //
    // Internal RAM is the scarce resource: the Wi-Fi driver and the TLS
    // session both need contiguous internal heap, so these stay small. A
    // window costs ~3.5 ms of CPU, so analysis normally holds one block.
    // The analysis queue is also the analysis history: a 2048-sample window is
    // read in place across the four oldest blocks, so it holds those three
    // blocks of history plus whatever backlog has built up.
    // The TCP queue only exists while a client is connected.
    constexpr int kAnalysisQueueBlocks = 8; // 3 history + 5 backlog (160 ms), 16 KB
    constexpr int kTcpQueueBlocks = 6;      // 192 ms, 12 KB

#if defined(CONFIG_IDF_TARGET_ESP32C3)
    // Waveshare ESP32-C3-Zero + single INMP441, L/R tied to GND (left slot).
    // Final physical wiring: WS=GPIO6, SCK/BCLK=GPIO7, SD/DIN=GPIO4.
    // All tasks share the C3's only CPU core.
    constexpr int kI2sBclk = 7;
    constexpr int kI2sLrclk = 6;
    constexpr int kI2sDin = 4;
    constexpr int kAcquisitionCore = 0;
    constexpr int kAnalysisCore = 0;
    constexpr int kTcpCore = 0;
#else
    // Classic dual-core ESP32 DevKit V1 wiring remains supported.
    constexpr int kI2sBclk = 26;
    constexpr int kI2sLrclk = 25;
    constexpr int kI2sDin = 33;
    constexpr int kAcquisitionCore = 0;
    constexpr int kAnalysisCore = 1;
    constexpr int kTcpCore = 1;
#endif

    // Acquisition must outrank analysis; TCP is lowest priority.
    constexpr int kAcquisitionPriority = 12;
    constexpr int kAnalysisPriority = 4;
    constexpr int kTcpPriority = 3;
} // namespace watson
