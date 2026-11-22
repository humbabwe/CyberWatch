#include "displayapp/screens/settings/Settings.h"
#include <lvgl/lvgl.h>
#include <algorithm>
#include <functional>
#include "displayapp/apps/Apps.h"
#include "displayapp/DisplayApp.h"

using namespace Pinetime::Applications::Screens;

constexpr std::array<List::Applications, Settings::entries.size()> Settings::entries;

auto Settings::CreateScreenList() const {
  std::array<std::function<std::unique_ptr<Screen>()>, nScreens> screens;
  for (size_t i = 0; i < screens.size(); i++) {
    screens[i] = [this, i]() -> std::unique_ptr<Screen> {
      return CreateScreen(i);
    };
  }
  return screens;
}

Settings::Settings(Pinetime::Applications::DisplayApp* app, Pinetime::Controllers::Settings& settingsController)
  : app {app},
    settingsController {settingsController},
    order {SortedOrder(settingsController)},
    screens {app, settingsController.GetSettingsMenu(), CreateScreenList(), Screens::ScreenListModes::UpDown} {
}

Settings::~Settings() {
  lv_obj_clean(lv_scr_act());
  settingsController.SaveSettings(); // keeps the open counts
}

Settings::Order Settings::SortedOrder(const Controllers::Settings& settingsController) {
  Order sorted;
  for (size_t i = 0; i < sorted.size(); i++) {
    sorted[i] = static_cast<uint8_t>(i);
  }
  auto count = [&settingsController](uint8_t index) -> int {
    if (entries[index].application == Apps::None) {
      return -1; // empty slots stay at the end
    }
    return settingsController.GetSettingsMenuOpenCount(index);
  };
  // Most opened first; ties keep the usual order
  std::stable_sort(sorted.begin(), sorted.end(), [&count](uint8_t a, uint8_t b) {
    return count(a) > count(b);
  });
  return sorted;
}

void Settings::RecordOpen(Controllers::Settings& settingsController, Apps app) {
  static_assert(entries.size() <= Controllers::Settings::settingsMenuSlots, "Not enough open counters for the Settings list");
  for (size_t i = 0; i < entries.size(); i++) {
    if (entries[i].application == app && app != Apps::None) {
      settingsController.CountSettingsMenuOpen(i);
      return;
    }
  }
}

bool Settings::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  return screens.OnTouchEvent(event);
}

std::unique_ptr<Screen> Settings::CreateScreen(unsigned int screenNum) const {
  std::array<List::Applications, entriesPerScreen> screens;
  for (int i = 0; i < entriesPerScreen; i++) {
    screens[i] = entries[order[screenNum * entriesPerScreen + i]];
  }

  return std::make_unique<Screens::List>(screenNum, nScreens, app, settingsController, screens);
}
