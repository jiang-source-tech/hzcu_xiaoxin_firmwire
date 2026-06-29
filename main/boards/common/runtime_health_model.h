#ifndef RUNTIME_HEALTH_MODEL_H
#define RUNTIME_HEALTH_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  XIAOXIN_RUNTIME_RESET_UNKNOWN = 0,
  XIAOXIN_RUNTIME_RESET_POWERON,
  XIAOXIN_RUNTIME_RESET_BROWNOUT,
  XIAOXIN_RUNTIME_RESET_SOFTWARE,
  XIAOXIN_RUNTIME_RESET_PANIC,
  XIAOXIN_RUNTIME_RESET_WATCHDOG,
  XIAOXIN_RUNTIME_RESET_DEEPSLEEP,
} xiaoxin_runtime_reset_kind_t;

typedef struct {
  uint32_t boot_count;
  uint32_t brownout_count;
  uint32_t poweron_count;
  uint32_t software_reset_count;
  uint32_t watchdog_count;
  uint32_t panic_count;
  uint32_t short_run_streak;
  uint32_t last_runtime_sec;
  uint32_t max_runtime_sec;
  uint32_t current_checkpoint_sec;
  bool current_on_battery;
  bool previous_on_battery;
  xiaoxin_runtime_reset_kind_t last_reset_kind;
} xiaoxin_runtime_health_record_t;

typedef struct {
  uint32_t boot_count;
  uint32_t brownout_count;
  uint32_t poweron_count;
  uint32_t software_reset_count;
  uint32_t watchdog_count;
  uint32_t panic_count;
  uint32_t short_run_streak;
  uint32_t current_runtime_sec;
  uint32_t last_runtime_sec;
  uint32_t max_runtime_sec;
  bool current_on_battery;
  bool previous_on_battery;
  xiaoxin_runtime_reset_kind_t last_reset_kind;
} xiaoxin_runtime_health_snapshot_t;

void xiaoxin_runtime_health_apply_boot(
  xiaoxin_runtime_health_record_t* record,
  xiaoxin_runtime_reset_kind_t reset_kind,
  bool current_on_battery,
  uint32_t short_runtime_threshold_sec
);

bool xiaoxin_runtime_health_should_checkpoint(
  uint32_t last_saved_sec,
  uint32_t now_sec
);

void xiaoxin_runtime_health_format_duration(
  char* buffer,
  size_t buffer_size,
  uint32_t seconds
);

const char* xiaoxin_runtime_health_reset_label(
  xiaoxin_runtime_reset_kind_t reset_kind
);

void xiaoxin_runtime_health_snapshot_from_record(
  const xiaoxin_runtime_health_record_t* record,
  xiaoxin_runtime_health_snapshot_t* snapshot
);

#ifdef __cplusplus
}
#endif

#endif
