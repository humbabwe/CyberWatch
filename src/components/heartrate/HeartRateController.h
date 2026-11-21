#pragma once

#include <atomic>
#include <cstdint>
#include <components/ble/HeartRateService.h>

namespace Pinetime {
  namespace Applications {
    class HeartRateTask;
  }

  namespace System {
    class SystemTask;
  }

  namespace Controllers {
    class HeartRateController {
    public:
      enum class States : uint8_t { Stopped, NotEnoughData, NoTouch, Running };

      HeartRateController() = default;
      void Enable();
      void Disable();
      void Update(States newState, uint8_t heartRate);

      void SetHeartRateTask(Applications::HeartRateTask* task);

      States State() const {
        return state;
      }

      uint8_t HeartRate() const {
        return heartRate;
      }

      void SetService(Pinetime::Controllers::HeartRateService* service);

      // CyberWatch: individual beats for the heart line
      void Beat() {
        beatCount++;
      }

      // Counts up by one per detected beat; compare with the last value seen
      uint32_t BeatCount() const {
        return beatCount;
      }

      // Read the sensor faster while the CyberWatch screen is on
      void SetFastSampling(bool fast) {
        fastSampling = fast;
      }

      bool FastSampling() const {
        return fastSampling;
      }

    private:
      std::atomic<uint32_t> beatCount {0};
      std::atomic<bool> fastSampling {false};
      Applications::HeartRateTask* task = nullptr;
      States state = States::Stopped;
      uint8_t heartRate = 0;
      Pinetime::Controllers::HeartRateService* service = nullptr;
    };
  }
}