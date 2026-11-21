#include "displayapp/screens/WatchFaceCyberWatch.h"
#include <algorithm>
#include "components/heartrate/HeartRateController.h"
#include "components/settings/Settings.h"
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications::Screens;

namespace {
  // One heartbeat on the line, a point every 33 ms (2 px), in pixels from the
  // baseline (negative is up): small dip, tall narrow spike, deep dip, a short
  // flat stretch, then a low rounded bump.
  constexpr int8_t spikeShape[] = {4, 6, -28, -62, -20, 18, 26, 8, 0, 0, 0, 0, -3, -6, -8, -8, -6, -3, 0};
  constexpr int8_t spikeLength = sizeof(spikeShape) / sizeof(spikeShape[0]);

  constexpr lv_color_t red = LV_COLOR_MAKE(0xff, 0x10, 0x10);
}

WatchFaceCyberWatch::WatchFaceCyberWatch(Controllers::HeartRateController& heartRateController, Controllers::Settings& settingsController)
  : heartRateController {heartRateController}, settingsController {settingsController} {
  lv_obj_set_style_local_bg_color(lv_scr_act(), LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);

  labelTitle = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_letter_space(labelTitle, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 4);
  lv_label_set_text_static(labelTitle, "CYBERWATCH");
  lv_obj_align(labelTitle, nullptr, LV_ALIGN_IN_TOP_MID, 0, 8);

  for (uint8_t i = 0; i < pointCount; i++) {
    points[i] = {static_cast<lv_coord_t>(i * stepX), baselineY};
  }
  line = lv_line_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_line_width(line, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, 3);
  lv_obj_set_style_local_line_rounded(line, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, true);
  lv_line_set_points(line, points.data(), pointCount);
  lv_obj_set_pos(line, 0, 0);

  labelStatus = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_long_mode(labelStatus, LV_LABEL_LONG_CROP);
  lv_obj_set_width(labelStatus, 240);
  lv_label_set_align(labelStatus, LV_LABEL_ALIGN_CENTER);
  lv_obj_set_pos(labelStatus, 0, 212);

  ApplyLevel();

  // Keep the sensor on, and fast, while this screen shows
  lastBeatCount = heartRateController.BeatCount();
  if (heartRateController.State() == Controllers::HeartRateController::States::Stopped) {
    heartRateController.Enable();
  }
  heartRateController.SetFastSampling(true);

  lastTick = lv_tick_get();
  taskRefresh = lv_task_create(RefreshTaskCallback, tickMs, LV_TASK_PRIO_MID, this);
}

WatchFaceCyberWatch::~WatchFaceCyberWatch() {
  heartRateController.SetFastSampling(false);
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFaceCyberWatch::Refresh() {
  // Go by the clock, not by frames, so the line keeps a steady speed even if
  // a redraw runs slow
  const uint32_t elapsedMs = std::min<uint32_t>(lv_tick_elaps(lastTick), 1000);
  lastTick = lv_tick_get();

  const uint8_t bpm = heartRateController.HeartRate();
  const bool reading = heartRateController.State() == Controllers::HeartRateController::States::Running && bpm > 0;

  if (reading) {
    sinceReadingMs = 0;
  } else if (sinceReadingMs < offlineAfterMs) {
    sinceReadingMs += elapsedMs;
  }
  sinceSpikeMs += elapsedMs;

  // A beat was detected since the last update
  const uint32_t beatCount = heartRateController.BeatCount();
  bool beat = beatCount != lastBeatCount;
  lastBeatCount = beatCount;

  // If the detector misses beats (loose strap, moving arm) but the BPM is
  // known, keep the line beating at that rate instead of going flat
  if (!beat && reading && sinceSpikeMs > (60000u * 3u) / (2u * bpm)) {
    beat = true;
  }

  if (beat && sinceReadingMs < offlineAfterMs && sinceSpikeMs >= minimumSpikeGapMs) {
    spikeIndex = 0;
    sinceSpikeMs = 0;
  }

  // Scroll left by however many points that much time is worth, and add them on the right
  pendingMs += elapsedMs;
  const uint8_t newPoints = static_cast<uint8_t>(std::min<uint32_t>(pendingMs / pointMs, pointCount));
  pendingMs -= newPoints * pointMs;
  if (newPoints > 0) {
    for (uint8_t i = 0; i + newPoints < pointCount; i++) {
      points[i].y = points[i + newPoints].y;
    }
    for (uint8_t i = pointCount - newPoints; i < pointCount; i++) {
      points[i].y = static_cast<lv_coord_t>(baselineY + NextOffset(sinceReadingMs < offlineAfterMs));
    }
    lv_line_set_points(line, points.data(), pointCount);
  }

  UpdateLevel(bpm, reading, elapsedMs);
}

int8_t WatchFaceCyberWatch::NextOffset(bool alive) {
  if (spikeIndex >= 0) {
    const int8_t offset = spikeShape[spikeIndex++];
    if (spikeIndex >= spikeLength) {
      spikeIndex = -1;
    }
    return offset;
  }
  if (!alive) {
    return 0; // flatline
  }
  // A little jitter so the line looks live between beats
  noiseSeed = noiseSeed * 1103515245u + 12345u;
  return static_cast<int8_t>(static_cast<int>((noiseSeed >> 16) % 3) - 1);
}

void WatchFaceCyberWatch::UpdateLevel(uint8_t bpm, bool reading, uint32_t elapsedMs) {
  Level newLevel = level;

  if (sinceReadingMs >= offlineAfterMs) {
    newLevel = Level::Offline;
    smoothedBpm = 0.0f;
  } else if (reading) {
    // Average over a few seconds so one odd reading doesn't change the colour
    if (smoothedBpm == 0.0f) {
      smoothedBpm = bpm;
    } else {
      // About 3 seconds to catch up
      smoothedBpm += (static_cast<float>(bpm) - smoothedBpm) * std::min(1.0f, static_cast<float>(elapsedMs) / 3000.0f);
    }

    const float caution = settingsController.GetAgentCautionBpm();
    const float danger = settingsController.GetAgentDangerBpm();
    // Going up needs the cutoff; coming back down needs to drop a little below it
    const float down = hysteresisBpm;
    switch (level) {
      case Level::Offline:
        newLevel = smoothedBpm >= danger ? Level::Danger : smoothedBpm >= caution ? Level::Caution : Level::Fine;
        break;
      case Level::Fine:
        newLevel = smoothedBpm >= danger ? Level::Danger : smoothedBpm >= caution ? Level::Caution : Level::Fine;
        break;
      case Level::Caution:
        newLevel = smoothedBpm >= danger ? Level::Danger : smoothedBpm < caution - down ? Level::Fine : Level::Caution;
        break;
      case Level::Danger:
        newLevel = smoothedBpm >= danger - down ? Level::Danger : smoothedBpm >= caution ? Level::Caution : Level::Fine;
        break;
    }
  }

  if (newLevel != level || !levelShown) {
    level = newLevel;
    ApplyLevel();
  }
}

void WatchFaceCyberWatch::ApplyLevel() {
  lv_color_t color = Colors::green;
  const char* text = "AGENT HUMBY: FINE";
  switch (level) {
    case Level::Offline:
      color = Colors::gray;
      text = "AGENT HUMBY: OFFLINE";
      break;
    case Level::Fine:
      break;
    case Level::Caution:
      color = Colors::orange;
      text = "AGENT HUMBY: CAUTION";
      break;
    case Level::Danger:
      color = red;
      text = "AGENT HUMBY: DANGER";
      break;
  }
  lv_obj_set_style_local_text_color(labelTitle, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
  lv_obj_set_style_local_line_color(line, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, color);
  lv_obj_set_style_local_text_color(labelStatus, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
  lv_label_set_text_static(labelStatus, text);
  levelShown = true;
}
