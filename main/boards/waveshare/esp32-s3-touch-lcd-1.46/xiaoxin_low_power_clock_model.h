#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XIAOXIN_LOW_POWER_CLOCK_TIME_MAX 8
#define XIAOXIN_LOW_POWER_CLOCK_HINT_MAX 32
#define XIAOXIN_LOW_POWER_CLOCK_DEFAULT_BRIGHTNESS 8
#define XIAOXIN_LOW_POWER_CLOCK_ICON_TEXT "\xEF\x83\xB3"

typedef struct {
  bool time_valid;
  int hour;
  int minute;
} xiaoxin_low_power_clock_state_t;

typedef struct {
  char icon_text[8];
  char time_text[XIAOXIN_LOW_POWER_CLOCK_TIME_MAX];
  char hint_text[XIAOXIN_LOW_POWER_CLOCK_HINT_MAX];
  uint8_t brightness_percent;
} xiaoxin_low_power_clock_snapshot_t;

void xiaoxin_low_power_clock_model_build(
  const xiaoxin_low_power_clock_state_t* state,
  xiaoxin_low_power_clock_snapshot_t* snapshot
);

bool xiaoxin_low_power_clock_should_refresh(
  uint8_t previous_minute,
  uint8_t current_minute
);

#ifdef __cplusplus
}
#endif
