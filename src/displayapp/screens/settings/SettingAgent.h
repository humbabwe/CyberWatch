#pragma once

// CyberWatch: set the heart rates where AGENT HUMBY goes to CAUTION and DANGER.

#include <cstdint>
#include <lvgl/lvgl.h>
#include "components/settings/Settings.h"
#include "displayapp/screens/Screen.h"

namespace Pinetime {
  namespace Applications {
    namespace Screens {

      class SettingAgent : public Screen {
      public:
        explicit SettingAgent(Controllers::Settings& settingsController);
        ~SettingAgent() override;

        void OnButtonEvent(lv_obj_t* object, lv_event_t event);

      private:
        struct Row {
          lv_obj_t* value;
          lv_obj_t* minus;
          lv_obj_t* plus;
        };

        static constexpr uint8_t minimumBpm = 60;
        static constexpr uint8_t maximumBpm = 220;
        static constexpr uint8_t minimumGap = 5; // DANGER stays at least this far above CAUTION

        Row CreateRow(const char* name, lv_color_t color, lv_coord_t y);
        void UpdateValues();

        Controllers::Settings& settingsController;
        Row caution;
        Row danger;
      };
    }
  }
}
