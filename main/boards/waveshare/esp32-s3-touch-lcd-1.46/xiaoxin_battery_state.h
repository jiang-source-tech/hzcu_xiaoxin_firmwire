#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  XIAOXIN_BATTERY_STATE_UNKNOWN = 0,
  XIAOXIN_BATTERY_STATE_NORMAL,
  XIAOXIN_BATTERY_STATE_LOW,
  XIAOXIN_BATTERY_STATE_CRITICAL,
} xiaoxin_battery_state_t;

typedef enum {
  XIAOXIN_BATTERY_POWER_BATTERY = 0,
  XIAOXIN_BATTERY_POWER_EXTERNAL,
  XIAOXIN_BATTERY_POWER_UNKNOWN,
} xiaoxin_battery_power_source_t;

typedef enum {
  XIAOXIN_BATTERY_LOAD_IDLE = 0,
  XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE,
} xiaoxin_battery_load_t;

typedef struct {
  xiaoxin_battery_state_t state;
  xiaoxin_battery_power_source_t power_source;
  int estimated_percent;
  int display_percent;
  uint8_t display_level;
  bool percent_reliable;
  int smoothed_voltage_mv;
  bool low_edge;
  bool critical_edge;
  bool recovered_edge;
} xiaoxin_battery_snapshot_t;

typedef struct {
  xiaoxin_battery_state_t state;
  xiaoxin_battery_power_source_t power_source;
  int estimated_percent;
  int display_percent;
  uint8_t display_level;
  bool percent_reliable;
  int smoothed_voltage_mv;
  bool has_sample;
  uint32_t candidate_since_ms;
  xiaoxin_battery_state_t candidate_state;
  xiaoxin_battery_snapshot_t last_snapshot;
} xiaoxin_battery_context_t;

void xiaoxin_battery_state_init(
  xiaoxin_battery_context_t* ctx,
  uint32_t now_ms
);

xiaoxin_battery_snapshot_t xiaoxin_battery_state_update(
  xiaoxin_battery_context_t* ctx,
  int voltage_mv,
  bool sample_valid,
  xiaoxin_battery_load_t load,
  uint32_t now_ms
);

xiaoxin_battery_snapshot_t xiaoxin_battery_state_snapshot(
  const xiaoxin_battery_context_t* ctx
);

#ifdef __cplusplus
}
#endif
