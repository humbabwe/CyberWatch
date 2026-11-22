#pragma once

// CyberWatch: placeholder for the CyberBox control screen. To be wired up to
// the CyberBox (the Pi in the Unisonic TV) once it's built. Replaces Music.

#include <lvgl/lvgl.h>
#include "displayapp/apps/Apps.h"
#include "displayapp/Controllers.h"
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/Symbols.h"

namespace Pinetime {
  namespace Applications {
    namespace Screens {
      class CyberBox : public Screen {
      public:
        CyberBox();
        ~CyberBox() override;
      };
    }

    template <>
    struct AppTraits<Apps::CyberBox> {
      static constexpr Apps app = Apps::CyberBox;
      static constexpr const char* icon = Screens::Symbols::play;

      static Screens::Screen* Create(AppControllers& /*controllers*/) {
        return new Screens::CyberBox();
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      };
    };
  }
}
