#include <initializer_list>
#include <lvgl/lvgl.h>
#include "displayapp/screens/WatchFaceTerminal.h"
#include "components/battery/BatteryController.h"
#include "components/ble/BleController.h"
#include "components/ble/NotificationManager.h"
#include "components/heartrate/HeartRateController.h"
#include "components/motion/MotionController.h"
#include "components/settings/Settings.h"
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications::Screens;

WatchFaceTerminal::WatchFaceTerminal(Controllers::DateTime& dateTimeController,
                                     const Controllers::Battery& batteryController,
                                     const Controllers::Ble& bleController,
                                     Controllers::NotificationManager& notificationManager,
                                     Controllers::Settings& settingsController,
                                     Controllers::HeartRateController& heartRateController,
                                     Controllers::MotionController& motionController)
  : currentDateTime {{}},
    dateTimeController {dateTimeController},
    batteryController {batteryController},
    bleController {bleController},
    notificationManager {notificationManager},
    settingsController {settingsController},
    heartRateController {heartRateController},
    motionController {motionController} {
  // CyberWatch: green on black throughout
  lv_obj_set_style_local_bg_color(lv_scr_act(), LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);

  container = lv_cont_create(lv_scr_act(), nullptr);
  lv_cont_set_layout(container, LV_LAYOUT_COLUMN_LEFT);
  lv_cont_set_fit(container, LV_FIT_TIGHT);
  lv_obj_set_style_local_pad_inner(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, -3);
  lv_obj_set_style_local_bg_opa(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);

  notificationIcon = lv_label_create(container, nullptr);

  labelPrompt1 = lv_label_create(container, nullptr);
  lv_label_set_text_static(labelPrompt1, "user@watch:~ $ now");

  labelTime = lv_label_create(container, nullptr);

  labelDate = lv_label_create(container, nullptr);

  batteryValue = lv_label_create(container, nullptr);

  stepValue = lv_label_create(container, nullptr);

  heartbeatValue = lv_label_create(container, nullptr);

  connectState = lv_label_create(container, nullptr);

  labelPrompt2 = lv_label_create(container, nullptr);
  lv_label_set_text_static(labelPrompt2, "user@watch:~ $");

  for (lv_obj_t* label :
       {notificationIcon, labelPrompt1, labelTime, labelDate, batteryValue, stepValue, heartbeatValue, connectState, labelPrompt2}) {
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::green);
  }

  lv_obj_align(container, nullptr, LV_ALIGN_IN_TOP_LEFT, 0, 7);

  lv_obj_t* signature = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(signature, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::green);
  lv_label_set_text_static(signature, "By:Hum");
  lv_obj_align(signature, nullptr, LV_ALIGN_IN_BOTTOM_RIGHT, -4, -4);

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

WatchFaceTerminal::~WatchFaceTerminal() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFaceTerminal::Refresh() {
  notificationState = notificationManager.AreNewNotificationsAvailable();
  if (notificationState.IsUpdated()) {
    if (notificationState.Get()) {
      lv_label_set_text_static(notificationIcon, "[1]+ Notify");
    } else {
      lv_label_set_text_static(notificationIcon, "");
    }
  }

  currentDateTime = std::chrono::time_point_cast<std::chrono::seconds>(dateTimeController.CurrentDateTime());
  if (currentDateTime.IsUpdated()) {
    uint8_t hour = dateTimeController.Hours();
    uint8_t minute = dateTimeController.Minutes();
    uint8_t second = dateTimeController.Seconds();

    // CyberWatch: always 24-hour time
    lv_label_set_text_fmt(labelTime, "[TIME] %02d:%02d:%02d", hour, minute, second);

    currentDate = std::chrono::time_point_cast<std::chrono::days>(currentDateTime.Get());
    if (currentDate.IsUpdated()) {
      uint16_t year = dateTimeController.Year();
      Controllers::DateTime::Months month = dateTimeController.Month();
      uint8_t day = dateTimeController.Day();
      lv_label_set_text_fmt(labelDate, "[DATE] %04d-%02d-%02d", year, month, day);
    }
  }

  powerPresent = batteryController.IsPowerPresent();
  batteryPercentRemaining = batteryController.PercentRemaining();
  if (batteryPercentRemaining.IsUpdated() || powerPresent.IsUpdated()) {
    lv_label_set_text_fmt(batteryValue, "[BATT] %d%%", batteryPercentRemaining.Get());
    if (batteryController.IsCharging()) {
      lv_label_ins_text(batteryValue, LV_LABEL_POS_LAST, " Charging");
    }
  }

  stepCount = motionController.NbSteps();
  if (stepCount.IsUpdated()) {
    lv_label_set_text_fmt(stepValue, "[STEP] %lu steps", stepCount.Get());
  }

  heartbeat = heartRateController.HeartRate();
  heartbeatRunning = heartRateController.State() != Controllers::HeartRateController::States::Stopped;
  if (heartbeat.IsUpdated() || heartbeatRunning.IsUpdated()) {
    if (heartbeatRunning.Get()) {
      lv_label_set_text_fmt(heartbeatValue, "[L_HR] %d bpm", heartbeat.Get());
    } else {
      lv_label_set_text_static(heartbeatValue, "[L_HR] ---");
    }
  }

  bleState = bleController.IsConnected();
  bleRadioEnabled = bleController.IsRadioEnabled();
  if (bleState.IsUpdated() || bleRadioEnabled.IsUpdated()) {
    if (!bleRadioEnabled.Get()) {
      lv_label_set_text_static(connectState, "[STAT] Disabled");
    } else {
      if (bleState.Get()) {
        lv_label_set_text_static(connectState, "[STAT] Connected");
      } else {
        lv_label_set_text_static(connectState, "[STAT] Disconnected");
      }
    }
  }
}
