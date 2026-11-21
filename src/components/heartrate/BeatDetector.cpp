#include "components/heartrate/BeatDetector.h"
#include <algorithm>

using namespace Pinetime::Controllers;

void BeatDetector::Reset() {
  initialized = false;
  armed = false;
  envelope = 0.0f;
  averageSlope = 0.0f;
  sinceBeatMs = 0;
}

bool BeatDetector::Add(uint16_t hrs, uint16_t dtMs) {
  const float x = static_cast<float>(hrs);
  const float dt = static_cast<float>(std::max<uint16_t>(dtMs, 1));

  if (!initialized) {
    baseline = x;
    smooth = 0.0f;
    initialized = true;
    return false;
  }

  sinceBeatMs += dtMs;

  // Remove slow drift (arm movement, skin contact changes)
  baseline += (x - baseline) * std::min(1.0f, dt / baselineTimeMs);
  const float pulse = x - baseline;

  // Light smoothing, then how steeply the signal is changing. Each beat is a
  // sharp edge followed by a slow return; which way the edge goes depends on
  // the sensor, so only its steepness is used.
  const float previous = smooth;
  smooth += (pulse - smooth) * (dt / (dt + smoothTimeMs));
  float slope = (previous - smooth) * (50.0f / dt);
  if (slope < 0.0f) {
    slope = -slope;
  }
  averageSlope += (slope - averageSlope) * std::min(1.0f, dt / noiseTimeMs);

  // Track how strong recent beats are, fading old ones out
  envelope = std::max(envelope * (1.0f - std::min(1.0f, dt / envelopeDecayMs)), slope);

  if (slope < envelope * rearmLevel) {
    armed = true;
  }

  uint32_t minimumInterval = minimumIntervalMs;
  if (expectedBpm > 0) {
    // Ignore anything sooner than 70% of a normal beat gap
    minimumInterval = std::max<uint32_t>(minimumInterval, (60000u * 7u) / (10u * expectedBpm));
  }

  if (armed && slope > minimumSlope && slope > envelope * triggerLevel && slope > averageSlope * noiseFactor &&
      sinceBeatMs >= minimumInterval) {
    armed = false;
    sinceBeatMs = 0;
    return true;
  }
  return false;
}
