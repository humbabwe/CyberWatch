#include "components/motor/MotorController.h"
#include <hal/nrf_gpio.h>
#include "systemtask/SystemTask.h"
#include "drivers/PinMap.h"

using namespace Pinetime::Controllers;

void MotorController::Init() {
  nrf_gpio_cfg_output(PinMap::Motor);
  nrf_gpio_pin_set(PinMap::Motor);

  shortVib = xTimerCreate("shortVib", 1, pdFALSE, nullptr, StopMotor);
  longVib = xTimerCreate("longVib", pdMS_TO_TICKS(1000), pdTRUE, this, Ring);
  patternTimer = xTimerCreate("patternVib", 1, pdFALSE, this, PatternStep);
}

void MotorController::Ring(TimerHandle_t xTimer) {
  auto* motorController = static_cast<MotorController*>(pvTimerGetTimerID(xTimer));
  motorController->RunForDuration(50);
}

void MotorController::RunForDuration(uint8_t motorDuration) {
  if (motorDuration > 0 && xTimerChangePeriod(shortVib, pdMS_TO_TICKS(motorDuration), 0) == pdPASS && xTimerStart(shortVib, 0) == pdPASS) {
    nrf_gpio_pin_clear(PinMap::Motor);
  }
}

void MotorController::StartRinging() {
  RunForDuration(50);
  xTimerStart(longVib, 0);
}

void MotorController::StopRinging() {
  xTimerStop(longVib, 0);
  nrf_gpio_pin_set(PinMap::Motor);
}

bool MotorController::IsRinging() {
  return (xTimerIsTimerActive(longVib) == pdTRUE);
}

void MotorController::StopMotor(TimerHandle_t /*xTimer*/) {
  nrf_gpio_pin_set(PinMap::Motor);
}

namespace {
  // Morse timing: dot = 1 unit, dash = 3 units, gap inside a letter = 1 unit,
  // gap between letters = 3 units. One unit = 120 ms.
  constexpr uint16_t morseUnit = 120;
  constexpr uint16_t dot = morseUnit;
  constexpr uint16_t dash = 3 * morseUnit;
  constexpr uint16_t partGap = morseUnit;
  constexpr uint16_t letterGap = 3 * morseUnit;

  constexpr uint16_t sosPattern[] = {
    dot,
    partGap,
    dot,
    partGap,
    dot,
    letterGap, // S
    dash,
    partGap,
    dash,
    partGap,
    dash,
    letterGap, // O
    dot,
    partGap,
    dot,
    partGap,
    dot, // S
  };
}

void MotorController::PlaySos() {
  PlayPattern(sosPattern, sizeof(sosPattern) / sizeof(sosPattern[0]));
}

void MotorController::PlayPattern(const uint16_t* durations, uint8_t count) {
  if (durations == nullptr || count == 0) {
    return;
  }
  xTimerStop(patternTimer, 0);
  pattern = durations;
  patternCount = count;
  patternIndex = 0;
  nrf_gpio_pin_clear(PinMap::Motor); // motor on
  xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(pattern[0]), 0);
}

void MotorController::PatternStep(TimerHandle_t xTimer) {
  auto* motorController = static_cast<MotorController*>(pvTimerGetTimerID(xTimer));
  motorController->patternIndex++;
  if (motorController->patternIndex >= motorController->patternCount) {
    nrf_gpio_pin_set(PinMap::Motor); // done, motor off
    return;
  }
  // Even steps are "on", odd steps are "off"
  if (motorController->patternIndex % 2 == 0) {
    nrf_gpio_pin_clear(PinMap::Motor);
  } else {
    nrf_gpio_pin_set(PinMap::Motor);
  }
  xTimerChangePeriod(xTimer, pdMS_TO_TICKS(motorController->pattern[motorController->patternIndex]), 0);
}
