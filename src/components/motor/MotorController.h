#pragma once

#include <FreeRTOS.h>
#include <timers.h>
#include <cstdint>

namespace Pinetime {
  namespace Controllers {

    class MotorController {
    public:
      MotorController() = default;

      void Init();
      void RunForDuration(uint8_t motorDuration);
      void StartRinging();
      void StopRinging();
      bool IsRinging();

      // CyberWatch: play a vibration pattern. Durations in ms, alternating
      // motor on / motor off, starting with on. The array must stay valid
      // while it plays (use static data).
      void PlayPattern(const uint16_t* durations, uint8_t count);
      // SOS in Morse code: . . . - - - . . .
      void PlaySos();

    private:
      static void PatternStep(TimerHandle_t xTimer);
      TimerHandle_t patternTimer;
      const uint16_t* pattern = nullptr;
      uint8_t patternCount = 0;
      uint8_t patternIndex = 0;
      static void Ring(TimerHandle_t xTimer);
      static void StopMotor(TimerHandle_t xTimer);
      TimerHandle_t shortVib;
      TimerHandle_t longVib;
    };
  }
}
