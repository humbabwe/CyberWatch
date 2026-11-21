#include "displayapp/screens/CyberAlert.h"
#include <algorithm>
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications::Screens;

namespace {
  constexpr lv_coord_t screenSize = 240;
  constexpr lv_coord_t bodyTop = 34; // below the sender line
}

CyberAlert::CyberAlert(Controllers::CyberAlertStore& store, System::SystemTask& systemTask) : store {store}, wakeLock(systemTask) {
  // Keep the screen on until the text has played
  wakeLock.Lock();

  lv_obj_set_style_local_bg_color(lv_scr_act(), LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);

  const lv_font_t* font = &jetbrains_mono_bold_20;
  const lv_coord_t glyphWidth = std::max<lv_coord_t>(1, lv_font_get_glyph_width(font, 'W', 0));
  const lv_coord_t lineHeight = std::max<lv_coord_t>(1, lv_font_get_line_height(font));
  columns = static_cast<uint8_t>(std::max<lv_coord_t>(1, screenSize / glyphWidth));
  rows = static_cast<uint8_t>(std::max<lv_coord_t>(1, (screenSize - bodyTop) / lineHeight));
  // Grid text needs one '\n' per row and a terminator; keep it within the buffer
  while (columns * rows + rows + 1 > static_cast<int>(bodyText.size()) && rows > 1) {
    rows--;
  }
  capacity = columns * rows;

  labelSender = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_long_mode(labelSender, LV_LABEL_LONG_CROP);
  lv_obj_set_width(labelSender, screenSize);
  lv_obj_set_style_local_text_color(labelSender, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::green);
  lv_obj_set_style_local_text_font(labelSender, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
  lv_obj_set_pos(labelSender, 0, 4);

  labelBody = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_long_mode(labelBody, LV_LABEL_LONG_CROP);
  lv_obj_set_size(labelBody, screenSize, screenSize - bodyTop);
  lv_obj_set_style_local_text_color(labelBody, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::green);
  lv_obj_set_style_local_text_font(labelBody, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
  lv_obj_set_style_local_text_line_space(labelBody, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_pos(labelBody, 0, bodyTop);

  Start(store.Front());

  taskRefresh = lv_task_create(RefreshTaskCallback, tickMs, LV_TASK_PRIO_MID, this);
}

CyberAlert::~CyberAlert() {
  // Leaving early still counts as played: the text is gone from the watch
  if (alert != nullptr) {
    store.Pop();
  }
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void CyberAlert::Start(const Controllers::CyberAlertStore::Alert* next) {
  alert = next;
  phase = Phase::Sender;
  ticks = 0;
  shown = 0;
  if (alert == nullptr) {
    running = false;
    return;
  }
  lv_label_set_text_fmt(labelSender, "> %s", alert->senderLength > 0 ? alert->sender.data() : "UNKNOWN");
  RenderBody(true);
}

void CyberAlert::Finish() {
  store.Pop();
  alert = nullptr;
  if (store.HasPending()) {
    Start(store.Front()); // another text came in while this one played
  } else {
    running = false;
  }
}

void CyberAlert::Refresh() {
  if (alert == nullptr) {
    return;
  }
  ticks++;

  switch (phase) {
    case Phase::Sender:
      RenderBody((ticks / cursorBlinkTicks) % 2 == 0);
      if (ticks >= senderTicks) {
        phase = Phase::Typing;
        ticks = 0;
      }
      break;

    case Phase::Typing:
      if (shown < alert->textLength) {
        shown++;
        RenderBody(true);
      }
      if (shown >= alert->textLength) {
        phase = Phase::Linger;
        ticks = 0;
      }
      break;

    case Phase::Linger:
      RenderBody((ticks / cursorBlinkTicks) % 2 == 0);
      if (ticks >= lingerTicks) {
        Finish();
      }
      break;
  }
}

void CyberAlert::RenderBody(bool showCursor) {
  // The last (capacity - 1) typed characters plus the cursor. Once the grid is
  // full, each new letter pushes the oldest one off the top-left corner.
  const uint16_t visible = capacity - 1;
  const uint16_t first = shown > visible ? shown - visible : 0;

  size_t out = 0;
  uint16_t column = 0;
  auto put = [&](char c) {
    if (column == columns) {
      bodyText[out++] = '\n';
      column = 0;
    }
    bodyText[out++] = c;
    column++;
  };

  for (uint16_t i = first; i < shown; i++) {
    put(alert->text[i]);
  }
  put(showCursor ? '_' : ' ');
  bodyText[out] = '\0';

  lv_label_set_text(labelBody, bodyText.data());
}

bool CyberAlert::OnTouchEvent(TouchEvents /*event*/) {
  // Ignore swipes so the text isn't left by accident; the button leaves
  return true;
}
