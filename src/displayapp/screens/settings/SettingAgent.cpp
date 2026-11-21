#include "displayapp/screens/settings/SettingAgent.h"
#include <algorithm>
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications::Screens;

namespace {
  void ButtonEventHandler(lv_obj_t* obj, lv_event_t event) {
    static_cast<SettingAgent*>(obj->user_data)->OnButtonEvent(obj, event);
  }

  constexpr lv_coord_t buttonWidth = 70;
  constexpr lv_coord_t buttonHeight = 56;
}

SettingAgent::SettingAgent(Controllers::Settings& settingsController) : settingsController {settingsController} {
  lv_obj_t* title = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(title, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::green);
  lv_label_set_text_static(title, "AGENT HUMBY");
  lv_obj_align(title, nullptr, LV_ALIGN_IN_TOP_MID, 0, 8);

  caution = CreateRow("CAUTION", Colors::orange, 40);
  danger = CreateRow("DANGER", LV_COLOR_RED, 140);
  UpdateValues();
}

SettingAgent::~SettingAgent() {
  lv_obj_clean(lv_scr_act());
  settingsController.SaveSettings();
}

SettingAgent::Row SettingAgent::CreateRow(const char* name, lv_color_t color, lv_coord_t y) {
  Row row;

  lv_obj_t* label = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
  lv_label_set_text_static(label, name);
  lv_obj_align(label, nullptr, LV_ALIGN_IN_TOP_MID, 0, y);

  auto makeButton = [this](const char* text, lv_align_t align, lv_coord_t buttonY) {
    lv_obj_t* button = lv_btn_create(lv_scr_act(), nullptr);
    button->user_data = this;
    lv_obj_set_size(button, buttonWidth, buttonHeight);
    lv_obj_align(button, nullptr, align, 0, buttonY);
    lv_obj_set_style_local_bg_color(button, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, Colors::bgAlt);
    lv_obj_set_event_cb(button, ButtonEventHandler);
    lv_obj_t* buttonLabel = lv_label_create(button, nullptr);
    lv_obj_set_style_local_text_font(buttonLabel, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_42);
    lv_label_set_text_static(buttonLabel, text);
    return button;
  };

  const lv_coord_t buttonY = y + 26;
  row.minus = makeButton("-", LV_ALIGN_IN_TOP_LEFT, buttonY);
  row.plus = makeButton("+", LV_ALIGN_IN_TOP_RIGHT, buttonY);

  row.value = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(row.value, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_42);
  lv_obj_set_style_local_text_color(row.value, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
  lv_obj_set_y(row.value, buttonY + 4);
  return row;
}

void SettingAgent::UpdateValues() {
  lv_label_set_text_fmt(caution.value, "%d", settingsController.GetAgentCautionBpm());
  lv_obj_align(caution.value, nullptr, LV_ALIGN_IN_TOP_MID, 0, lv_obj_get_y(caution.value));
  lv_label_set_text_fmt(danger.value, "%d", settingsController.GetAgentDangerBpm());
  lv_obj_align(danger.value, nullptr, LV_ALIGN_IN_TOP_MID, 0, lv_obj_get_y(danger.value));
}

void SettingAgent::OnButtonEvent(lv_obj_t* object, lv_event_t event) {
  int step = 0;
  if (event == LV_EVENT_SHORT_CLICKED) {
    step = 1;
  } else if (event == LV_EVENT_LONG_PRESSED || event == LV_EVENT_LONG_PRESSED_REPEAT) {
    step = 5;
  } else {
    return;
  }

  int cautionBpm = settingsController.GetAgentCautionBpm();
  int dangerBpm = settingsController.GetAgentDangerBpm();

  if (object == caution.minus) {
    cautionBpm -= step;
  } else if (object == caution.plus) {
    cautionBpm += step;
  } else if (object == danger.minus) {
    dangerBpm -= step;
  } else if (object == danger.plus) {
    dangerBpm += step;
  } else {
    return;
  }

  cautionBpm = std::clamp(cautionBpm, static_cast<int>(minimumBpm), static_cast<int>(maximumBpm - minimumGap));
  dangerBpm = std::clamp(dangerBpm, cautionBpm + minimumGap, static_cast<int>(maximumBpm));

  settingsController.SetAgentCautionBpm(static_cast<uint8_t>(cautionBpm));
  settingsController.SetAgentDangerBpm(static_cast<uint8_t>(dangerBpm));
  UpdateValues();
}
