#include "xiaoxin_low_power_clock_model.h"

#include <stdio.h>
#include <string.h>

static int clamp_int(int value, int min, int max) {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

void xiaoxin_low_power_clock_model_build(
  const xiaoxin_low_power_clock_state_t* state,
  xiaoxin_low_power_clock_snapshot_t* snapshot
) {
  if (snapshot == 0) {
    return;
  }

  memset(snapshot, 0, sizeof(*snapshot));
  snprintf(snapshot->icon_text, sizeof(snapshot->icon_text), "%s", XIAOXIN_LOW_POWER_CLOCK_ICON_TEXT);
  snprintf(snapshot->hint_text, sizeof(snapshot->hint_text), "%s", "POWER \xE5\x94\xA4\xE9\x86\x92");
  snapshot->brightness_percent = XIAOXIN_LOW_POWER_CLOCK_DEFAULT_BRIGHTNESS;

  if (state == 0 || !state->time_valid) {
    snprintf(snapshot->time_text, sizeof(snapshot->time_text), "%s", "--:--");
    return;
  }

  const int hour = clamp_int(state->hour, 0, 23);
  const int minute = clamp_int(state->minute, 0, 59);
  snprintf(snapshot->time_text, sizeof(snapshot->time_text), "%02d:%02d", hour, minute);
}

bool xiaoxin_low_power_clock_should_refresh(
  uint8_t previous_minute,
  uint8_t current_minute
) {
  return previous_minute != current_minute;
}

uint16_t xiaoxin_low_power_clock_animation_phase(uint32_t tick) {
  return (uint16_t)((tick % 30U) * 12U);
}
