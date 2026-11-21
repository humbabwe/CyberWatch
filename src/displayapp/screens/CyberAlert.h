#pragma once

// CyberWatch: CyberAlert screen. Shows who a text is from, then types the
// text out in green on black, Terminal style, about 10 characters a second.
// Once the screen is full, the oldest letters disappear from the top as new
// ones appear. When it has played (or the screen is left early), the text is
// deleted from the watch: one chance to read it.

#include <array>
#include <cstdint>
#include <lvgl/lvgl.h>
#include "displayapp/screens/Screen.h"
#include "components/ble/CyberAlertStore.h"
#include "systemtask/WakeLock.h"

namespace Pinetime {
  namespace Applications {
    namespace Screens {

      class CyberAlert : public Screen {
      public:
        CyberAlert(Controllers::CyberAlertStore& store, System::SystemTask& systemTask);
        ~CyberAlert() override;

        void Refresh() override;
        bool OnTouchEvent(TouchEvents event) override;

      private:
        enum class Phase : uint8_t { Sender, Typing, Linger };

        static constexpr uint32_t tickMs = 100;         // 10 characters per second
        static constexpr uint16_t senderTicks = 8;      // pause on the sender name before typing
        static constexpr uint16_t lingerTicks = 30;     // keep the end on screen for 3 s
        static constexpr uint16_t cursorBlinkTicks = 5; // cursor blink while lingering

        void Start(const Controllers::CyberAlertStore::Alert* next);
        void Finish();
        void RenderBody(bool showCursor);

        Controllers::CyberAlertStore& store;
        System::WakeLock wakeLock;
        const Controllers::CyberAlertStore::Alert* alert = nullptr;

        Phase phase = Phase::Sender;
        uint16_t ticks = 0;
        uint16_t shown = 0; // characters typed so far

        // Character grid for the message (monospace font)
        uint8_t columns = 1;
        uint8_t rows = 1;
        uint16_t capacity = 1; // characters that fit, including the cursor
        std::array<char, 300> bodyText {};

        lv_obj_t* labelSender;
        lv_obj_t* labelBody;
        lv_task_t* taskRefresh;
      };
    }
  }
}
