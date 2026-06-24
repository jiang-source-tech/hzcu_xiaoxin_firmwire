#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_low_power_clock_model.h"

static void valid_time_formats_as_hh_mm(void) {
  xiaoxin_low_power_clock_state_t state = {
    .time_valid = true,
    .hour = 9,
    .minute = 5,
  };
  xiaoxin_low_power_clock_snapshot_t snapshot = {};

  xiaoxin_low_power_clock_model_build(&state, &snapshot);

  assert(strcmp(snapshot.time_text, "09:05") == 0);
  assert(snapshot.brightness_percent == 12);
  assert(snapshot.brightness_percent == XIAOXIN_LOW_POWER_CLOCK_DEFAULT_BRIGHTNESS);
}

static void invalid_time_uses_placeholder(void) {
  xiaoxin_low_power_clock_state_t state = {
    .time_valid = false,
    .hour = 14,
    .minute = 32,
  };
  xiaoxin_low_power_clock_snapshot_t snapshot = {};

  xiaoxin_low_power_clock_model_build(&state, &snapshot);

  assert(strcmp(snapshot.time_text, "--:--") == 0);
}

static void time_fields_are_clamped_to_displayable_range(void) {
  xiaoxin_low_power_clock_state_t state = {
    .time_valid = true,
    .hour = 128,
    .minute = -3,
  };
  xiaoxin_low_power_clock_snapshot_t snapshot = {};

  xiaoxin_low_power_clock_model_build(&state, &snapshot);

  assert(strcmp(snapshot.time_text, "23:00") == 0);
}

static void refresh_only_when_minute_changes(void) {
  assert(!xiaoxin_low_power_clock_should_refresh(32, 32));
  assert(xiaoxin_low_power_clock_should_refresh(32, 33));
  assert(xiaoxin_low_power_clock_should_refresh(59, 0));
}

static void animation_phase_wraps_to_circle_degrees(void) {
  assert(xiaoxin_low_power_clock_animation_phase(0) == 0);
  assert(xiaoxin_low_power_clock_animation_phase(1) == 12);
  assert(xiaoxin_low_power_clock_animation_phase(29) == 348);
  assert(xiaoxin_low_power_clock_animation_phase(30) == 0);
  assert(xiaoxin_low_power_clock_animation_phase(31) == 12);
  assert(XIAOXIN_LOW_POWER_CLOCK_ARC_SPAN_DEGREES == 76);
}

int main(void) {
  valid_time_formats_as_hh_mm();
  invalid_time_uses_placeholder();
  time_fields_are_clamped_to_displayable_range();
  refresh_only_when_minute_changes();
  animation_phase_wraps_to_circle_degrees();
  return 0;
}
