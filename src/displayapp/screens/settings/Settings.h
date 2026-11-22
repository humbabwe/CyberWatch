#pragma once

#include <array>
#include <memory>
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/ScreenList.h"
#include "displayapp/screens/Symbols.h"
#include "displayapp/screens/List.h"

namespace Pinetime {

  namespace Applications {
    namespace Screens {

      class Settings : public Screen {
      public:
        Settings(DisplayApp* app, Pinetime::Controllers::Settings& settingsController);
        ~Settings() override;

        bool OnTouchEvent(Pinetime::Applications::TouchEvents event) override;

        // CyberWatch: called by the list when an entry is opened
        static void RecordOpen(Controllers::Settings& settingsController, Apps app);

      private:
        DisplayApp* app;
        auto CreateScreenList() const;
        std::unique_ptr<Screen> CreateScreen(unsigned int screenNum) const;

        Controllers::Settings& settingsController;

        static constexpr int entriesPerScreen = 4;

        // Increment this when more space is needed
        static constexpr int nScreens = 3;

        static constexpr std::array<List::Applications, entriesPerScreen * nScreens> entries {{
          {Symbols::sun, "Screen", Apps::SettingDisplay},
          {Symbols::eye, "Wake Up", Apps::SettingWakeUp},
          {Symbols::heartBeat, "Agent", Apps::SettingAgent},
          {Symbols::heartBeat, "Heartrate", Apps::SettingHeartRate},

          {Symbols::shoe, "Steps", Apps::SettingSteps},
          {Symbols::batteryHalf, "Battery", Apps::BatteryInfo},
          {Symbols::check, "Firmware", Apps::FirmwareValidation},
          {Symbols::shieldAlt, "Over-the-air", Apps::SettingOTA},

          {Symbols::bluetooth, "Bluetooth", Apps::SettingBluetooth},
          {Symbols::list, "About", Apps::SysInfo},
        }};
        // Entries sorted most-opened first (indexes into entries)
        using Order = std::array<uint8_t, entriesPerScreen * nScreens>;
        static Order SortedOrder(const Controllers::Settings& settingsController);
        Order order;

        ScreenList<nScreens> screens;
      };
    }
  }
}
