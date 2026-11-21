#pragma once

// CyberWatch: finds individual heartbeats in the raw optical sensor signal so
// the heart line can spike in time with real beats. (Ppg works out the BPM
// number from a few seconds of data; it can't say when each beat happened.)
//
// Each beat pulls blood under the sensor and the reflected light changes
// sharply, then drifts back slowly. The detector removes the slow drift,
// smooths the signal a little, and reports a beat when the signal's slope
// jumps past a threshold that adapts to how strong the pulse currently is.

#include <cstdint>

namespace Pinetime {
  namespace Controllers {
    class BeatDetector {
    public:
      // Feed one raw sample, taken dtMs after the previous one. Returns true if a beat starts here.
      bool Add(uint16_t hrs, uint16_t dtMs);

      // Current BPM estimate (0 = unknown); used to ignore false double-beats
      void SetExpectedBpm(uint8_t bpm) {
        expectedBpm = bpm;
      }

      void Reset();

    private:
      static constexpr float baselineTimeMs = 1500.0f; // slower than this is drift, not a heartbeat
      static constexpr float smoothTimeMs = 90.0f;     // light smoothing of sensor noise
      static constexpr float envelopeDecayMs = 2500.0f;
      static constexpr float triggerLevel = 0.55f; // fraction of the recent strongest beat
      static constexpr float rearmLevel = 0.35f;
      static constexpr float minimumSlope = 0.5f;        // below this it's noise (sensor units per 50 ms)
      static constexpr float noiseTimeMs = 4000.0f;      // average slope over a few beats
      static constexpr float noiseFactor = 1.5f;         // a beat edge must stand out this much from the average
      static constexpr uint16_t minimumIntervalMs = 300; // 200 bpm

      bool initialized = false;
      bool armed = false;
      float baseline = 0.0f;
      float smooth = 0.0f;
      float envelope = 0.0f;
      float averageSlope = 0.0f;
      uint32_t sinceBeatMs = 0;
      uint8_t expectedBpm = 0;
    };
  }
}
