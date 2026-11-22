#include "displayapp/screens/CyberBox.h"
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications::Screens;

CyberBox::CyberBox() {
  lv_obj_set_style_local_bg_color(lv_scr_act(), LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);

  lv_obj_t* title = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(title, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::green);
  lv_obj_set_style_local_text_letter_space(title, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 4);
  lv_label_set_text_static(title, "CYBERBOX");
  lv_obj_align(title, nullptr, LV_ALIGN_IN_TOP_MID, 0, 8);

  lv_obj_t* body = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(body, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::green);
  lv_label_set_align(body, LV_LABEL_ALIGN_CENTER);
  lv_label_set_text_static(body, "CONTROL\n\n[ NO LINK ]\n\nAwaiting hardware_");
  lv_obj_align(body, nullptr, LV_ALIGN_CENTER, 0, 10);
}

CyberBox::~CyberBox() {
  lv_obj_clean(lv_scr_act());
}
