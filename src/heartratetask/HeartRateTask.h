#pragma once
#include <FreeRTOS.h>
#include <cstdint>
#include <optional>
#include <task.h>
#include <queue.h>
#include <components/heartrate/Ppg.h>
#include <components/heartrate/BeatDetector.h>
#include "components/settings/Settings.h"

namespace Pinetime {
  namespace Drivers {
    class Hrs3300;
  }

  namespace Controllers {
    class HeartRateController;
  }

  namespace Applications {
    class HeartRateTask {
    public:
      enum class Messages : uint8_t { GoToSleep, WakeUp, Enable, Disable };

      explicit HeartRateTask(Drivers::Hrs3300& heartRateSensor,
                             Controllers::HeartRateController& controller,
                             Controllers::Settings& settings);
      void Start();
      void Work();
      void PushMessage(Messages msg);

    private:
      enum class States : uint8_t { Disabled, Waiting, BackgroundMeasuring, ForegroundMeasuring };
      static void Process(void* instance);
      void HandleSensorData();
      void ProcessPpgSample(uint16_t hrs, uint16_t als);
      void UpdateSamplingSpeed();

      uint8_t SamplesPerPpgSample() const {
        return fastSampling ? 2 : 1;
      }
      void StartMeasurement();
      void StopMeasurement();

      [[nodiscard]] bool BackgroundMeasurementNeeded() const;
      [[nodiscard]] std::optional<TickType_t> BackgroundMeasurementInterval() const;
      TickType_t CurrentTaskDelay();

      TaskHandle_t taskHandle;
      QueueHandle_t messageQueue;
      bool valueCurrentlyShown;
      bool measurementSucceeded;
      States state = States::Disabled;
      uint16_t count;
      Drivers::Hrs3300& heartRateSensor;
      Controllers::HeartRateController& controller;
      Controllers::Settings& settings;
      Controllers::Ppg ppg;
      Controllers::BeatDetector beatDetector;
      // CyberWatch fast mode: two sensor readings per Ppg sample, averaged
      bool fastSampling = false;
      uint8_t subSampleCount = 0;
      uint32_t hrsSum = 0;
      uint32_t alsSum = 0;
      TickType_t lastMeasurementTime;
      TickType_t measurementStartTime;
    };

  }
}
