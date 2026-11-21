#pragma once

// CyberWatch main screen: "CYBERWATCH" at the top, a jagged hospital-monitor
// heart line scrolling right to left with a spike on each detected heartbeat,
// and "AGENT HUMBY: FINE / CAUTION / DANGER" at the bottom. The whole screen
// is green, orange or red to match, or purple for POISON: heart rate up while
// sitting still (no steps for a while), which is the watch's best guess at
// drinking. It can't tell that apart from coffee or stress. No time and no BPM number on purpose:
// swipe left for the Terminal readout.

#ifndef CYBERWATCH_STILL_MS
  #define CYBERWATCH_STILL_MS (10u * 60u * 1000u)
#endif

#include <array>
#include <cstdint>
#include <lvgl/lvgl.h>
#include "displayapp/apps/Apps.h"
#include "displayapp/Controllers.h"
#include "displayapp/screens/Screen.h"

namespace Pinetime {
  namespace Controllers {
    class HeartRateController;
    class MotionController;
    class Settings;
  }

  namespace Applications {
    namespace Screens {

      class WatchFaceCyberWatch : public Screen {
      public:
        WatchFaceCyberWatch(Controllers::HeartRateController& heartRateController,
                            Controllers::MotionController& motionController,
                            Controllers::Settings& settingsController);
        ~WatchFaceCyberWatch() override;

        void Refresh() override;

      private:
        enum class Level : uint8_t { Offline, Fine, Caution, Danger, Poison };

        static constexpr uint16_t tickMs = 100; // target screen update rate (10 per second)
        static constexpr uint16_t pointMs = 33; // one point per 33 ms, fine enough for a sharp spike
        static constexpr lv_coord_t stepX = 2;  // pixels per point: scrolls 60 px a second
        static constexpr uint8_t pointCount = 240 / stepX + 1;
        static constexpr lv_coord_t baselineY = 132;
        static constexpr uint16_t minimumSpikeGapMs = 300;
        static constexpr uint16_t offlineAfterMs = 5000; // no reading this long: OFFLINE
        static constexpr uint8_t hysteresisBpm = 3;
        static constexpr uint32_t stillMs =
          CYBERWATCH_STILL_MS; // no steps for this long counts as sitting still      // stops flicker right at a cutoff

        void UpdateLevel(uint8_t bpm, bool reading, uint32_t elapsedMs);
        bool IsSittingStill();
        void ApplyLevel();
        int8_t NextOffset(bool reading);

        Controllers::HeartRateController& heartRateController;
        Controllers::MotionController& motionController;
        Controllers::Settings& settingsController;

        std::array<lv_point_t, pointCount> points {};
        int8_t spikeIndex = -1; // position in the spike shape, -1 when not drawing one
        uint32_t lastBeatCount = 0;
        uint32_t sinceSpikeMs = 0;
        uint32_t sinceReadingMs = offlineAfterMs;
        float smoothedBpm = 0.0f;
        Level level = Level::Offline;
        bool levelShown = false;
        uint32_t noiseSeed = 12345;
        uint32_t lastTick = 0;
        uint32_t pendingMs = 0; // time not yet turned into points

        lv_obj_t* labelTitle;
        lv_obj_t* line;
        lv_obj_t* labelStatus;
        lv_task_t* taskRefresh;
      };
    }

    template <>
    struct WatchFaceTraits<WatchFace::CyberWatch> {
      static constexpr WatchFace watchFace = WatchFace::CyberWatch;
      static constexpr const char* name = "CyberWatch";

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::WatchFaceCyberWatch(controllers.heartRateController,
                                                controllers.motionController,
                                                controllers.settingsController);
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      }
    };
  }
}
