# Xiaoxin ADC Battery State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a stable ADC-backed battery state system so the UI no longer turns red from one transient voltage dip.

**Architecture:** Add a pure C `xiaoxin_battery_state` module beside the existing Waveshare 1.46 board helpers. The board reads ADC voltage, feeds the module with time and load state, then all UI consumers use a shared battery snapshot instead of direct `level <= 20` checks.

**Tech Stack:** C11 pure modules, C++ board integration in `esp32-s3-touch-lcd-1.46.cc`, ESP-IDF/LVGL, existing `gcc` unit-test harness, existing `pytest` source-path tests.

## Global Constraints

- Hardware source is ADC only; do not require a fuel gauge, PMIC SOC output, or new hardware.
- Do not display exact ADC-derived battery percentages in user-facing copy.
- Do not show low-battery warnings in the bottom subtitle area.
- Do not let UI modules decide low battery with direct `level <= 20` logic.
- `LOW` is a gentle low-battery state; `CRITICAL` is the only red battery state.
- Voice-active load must use longer low-battery confirmation windows than idle load.
- Preserve unrelated working-tree changes, including existing subtitle/legacy low-battery popup fixes if present.

---

## File Structure

- Create `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h`
  - Defines battery state enums, load enum, context, snapshot, and update APIs.
- Create `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c`
  - Implements ADC percent estimation, EMA smoothing, hysteresis, time confirmation, and edge flags.
- Create `tests/xiaoxin_battery_state_test.c`
  - Pure C regression tests for state transitions and voice-active suppression.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.h`
  - Accepts `xiaoxin_battery_state_t` instead of raw percent.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c`
  - Maps `NORMAL/LOW/CRITICAL/UNKNOWN` to UI colors.
- Modify `tests/xiaoxin_system_overlay_test.c`
  - Verifies LOW is orange/yellow and CRITICAL is red.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h`
  - Replaces `battery_percent` / `battery_known` with qualitative battery state.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c`
  - Renders qualitative battery status from the new enum.
- Modify `tests/xiaoxin_overview_model_test.c`
  - Verifies qualitative battery labels.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
  - Owns the battery-state context and snapshot; routes overlay, notification, overview, and mood through the snapshot.
- Modify `tests/xiaoxin_notification_visual_path_test.py`
  - Ensures board UI no longer uses direct raw-percent low-battery checks.
- Modify `tests/xiaoxin_pet_mood_integration_path_test.py`
  - Ensures pet mood uses battery-state edges.

---

### Task 1: Add Pure Battery State Module

**Files:**
- Create: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h`
- Create: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c`
- Create: `tests/xiaoxin_battery_state_test.c`

**Interfaces:**
- Consumes: `uint32_t now_ms`, `int voltage_mv`, `bool sample_valid`, `xiaoxin_battery_load_t load`
- Produces:
  - `void xiaoxin_battery_state_init(xiaoxin_battery_context_t* ctx, uint32_t now_ms)`
  - `xiaoxin_battery_snapshot_t xiaoxin_battery_state_update(xiaoxin_battery_context_t* ctx, int voltage_mv, bool sample_valid, xiaoxin_battery_load_t load, uint32_t now_ms)`
  - `xiaoxin_battery_snapshot_t xiaoxin_battery_state_snapshot(const xiaoxin_battery_context_t* ctx)`

- [ ] **Step 1: Write the failing test**

Create `tests/xiaoxin_battery_state_test.c`:

```c
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h"

static xiaoxin_battery_snapshot_t feed(
  xiaoxin_battery_context_t* ctx,
  int voltage_mv,
  xiaoxin_battery_load_t load,
  uint32_t now_ms
) {
  return xiaoxin_battery_state_update(ctx, voltage_mv, true, load, now_ms);
}

static void init_starts_unknown(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 1000);
  xiaoxin_battery_snapshot_t snapshot = xiaoxin_battery_state_snapshot(&ctx);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_UNKNOWN);
  assert(!snapshot.low_edge);
  assert(!snapshot.critical_edge);
  assert(!snapshot.recovered_edge);
}

static void valid_samples_become_normal_after_confirmation(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_NORMAL);
}

static void one_low_sample_does_not_turn_low(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3400, XIAOXIN_BATTERY_LOAD_IDLE, 7000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_NORMAL);
  assert(!snapshot.low_edge);
}

static void sustained_low_enters_low_once(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  feed(&ctx, 3300, XIAOXIN_BATTERY_LOAD_IDLE, 7000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3300, XIAOXIN_BATTERY_LOAD_IDLE, 28000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_LOW);
  assert(snapshot.low_edge);
  snapshot = feed(&ctx, 3300, XIAOXIN_BATTERY_LOAD_IDLE, 29000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_LOW);
  assert(!snapshot.low_edge);
}

static void low_requires_hysteresis_to_recover(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  feed(&ctx, 3300, XIAOXIN_BATTERY_LOAD_IDLE, 7000);
  feed(&ctx, 3300, XIAOXIN_BATTERY_LOAD_IDLE, 28000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3500, XIAOXIN_BATTERY_LOAD_IDLE, 29000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_LOW);
  assert(!snapshot.recovered_edge);
  snapshot = feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 40000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_NORMAL);
  assert(snapshot.recovered_edge);
}

static void sustained_critical_enters_critical(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  feed(&ctx, 3300, XIAOXIN_BATTERY_LOAD_IDLE, 28000);
  feed(&ctx, 3200, XIAOXIN_BATTERY_LOAD_IDLE, 29000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3200, XIAOXIN_BATTERY_LOAD_IDLE, 40000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_CRITICAL);
  assert(snapshot.critical_edge);
}

static void voice_active_extends_low_confirmation(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  feed(&ctx, 3300, XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE, 7000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3300, XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE, 28000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_NORMAL);
  snapshot = feed(&ctx, 3300, XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE, 53000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_LOW);
  assert(snapshot.low_edge);
}

static void invalid_samples_become_unknown_without_low_edge(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  xiaoxin_battery_snapshot_t snapshot = xiaoxin_battery_state_update(
    &ctx,
    0,
    false,
    XIAOXIN_BATTERY_LOAD_IDLE,
    12000
  );
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_UNKNOWN);
  assert(!snapshot.low_edge);
  assert(!snapshot.critical_edge);
}

int main(void) {
  init_starts_unknown();
  valid_samples_become_normal_after_confirmation();
  one_low_sample_does_not_turn_low();
  sustained_low_enters_low_once();
  low_requires_hysteresis_to_recover();
  sustained_critical_enters_critical();
  voice_active_extends_low_confirmation();
  invalid_samples_become_unknown_without_low_edge();
  puts("xiaoxin_battery_state tests passed");
  return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_battery_state_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_state.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_level.c -o build\xiaoxin_battery_state_test.exe
```

Expected: FAIL because `xiaoxin_battery_state.c` and `xiaoxin_battery_state.h` do not exist.

- [ ] **Step 3: Add the public header**

Create `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h`:

```c
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
  XIAOXIN_BATTERY_LOAD_IDLE = 0,
  XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE,
} xiaoxin_battery_load_t;

typedef struct {
  xiaoxin_battery_state_t state;
  int estimated_percent;
  int smoothed_voltage_mv;
  bool low_edge;
  bool critical_edge;
  bool recovered_edge;
} xiaoxin_battery_snapshot_t;

typedef struct {
  xiaoxin_battery_state_t state;
  int estimated_percent;
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
```

- [ ] **Step 4: Add the implementation**

Create `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c`:

```c
#include "xiaoxin_battery_state.h"

#include "xiaoxin_battery_level.h"

#include <stddef.h>

enum {
  k_unknown_to_normal_percent = 25,
  k_normal_to_low_percent = 20,
  k_low_to_normal_percent = 30,
  k_low_to_critical_percent = 10,
  k_critical_to_low_percent = 18,
  k_unknown_to_normal_ms = 5000,
  k_normal_to_low_idle_ms = 20000,
  k_normal_to_low_voice_ms = 45000,
  k_low_to_normal_ms = 10000,
  k_low_to_critical_idle_ms = 10000,
  k_low_to_critical_voice_ms = 30000,
  k_critical_to_low_ms = 15000,
  k_min_plausible_mv = 3000,
  k_max_plausible_mv = 4400,
};

static xiaoxin_battery_snapshot_t make_snapshot(
  const xiaoxin_battery_context_t* ctx,
  bool low_edge,
  bool critical_edge,
  bool recovered_edge
) {
  xiaoxin_battery_snapshot_t snapshot = {
    .state = ctx->state,
    .estimated_percent = ctx->estimated_percent,
    .smoothed_voltage_mv = ctx->smoothed_voltage_mv,
    .low_edge = low_edge,
    .critical_edge = critical_edge,
    .recovered_edge = recovered_edge,
  };
  return snapshot;
}

static bool elapsed(uint32_t since_ms, uint32_t now_ms, uint32_t required_ms) {
  return (uint32_t)(now_ms - since_ms) >= required_ms;
}

static void reset_candidate(
  xiaoxin_battery_context_t* ctx,
  xiaoxin_battery_state_t candidate,
  uint32_t now_ms
) {
  if (ctx->candidate_state != candidate) {
    ctx->candidate_state = candidate;
    ctx->candidate_since_ms = now_ms;
  }
}

static bool is_plausible_sample(int voltage_mv, bool sample_valid) {
  return sample_valid &&
         voltage_mv >= k_min_plausible_mv &&
         voltage_mv <= k_max_plausible_mv;
}

static void update_smoothed_sample(xiaoxin_battery_context_t* ctx, int voltage_mv) {
  if (!ctx->has_sample) {
    ctx->has_sample = true;
    ctx->smoothed_voltage_mv = voltage_mv;
  } else {
    ctx->smoothed_voltage_mv = (ctx->smoothed_voltage_mv * 85 + voltage_mv * 15 + 50) / 100;
  }
  ctx->estimated_percent = xiaoxin_battery_percent_from_mv(ctx->smoothed_voltage_mv);
}

static uint32_t required_ms_for(
  xiaoxin_battery_state_t from,
  xiaoxin_battery_state_t to,
  xiaoxin_battery_load_t load
) {
  if (from == XIAOXIN_BATTERY_STATE_UNKNOWN && to == XIAOXIN_BATTERY_STATE_NORMAL) {
    return k_unknown_to_normal_ms;
  }
  if (from == XIAOXIN_BATTERY_STATE_NORMAL && to == XIAOXIN_BATTERY_STATE_LOW) {
    return load == XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE
      ? k_normal_to_low_voice_ms
      : k_normal_to_low_idle_ms;
  }
  if (from == XIAOXIN_BATTERY_STATE_LOW && to == XIAOXIN_BATTERY_STATE_NORMAL) {
    return k_low_to_normal_ms;
  }
  if (from == XIAOXIN_BATTERY_STATE_LOW && to == XIAOXIN_BATTERY_STATE_CRITICAL) {
    return load == XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE
      ? k_low_to_critical_voice_ms
      : k_low_to_critical_idle_ms;
  }
  if (from == XIAOXIN_BATTERY_STATE_CRITICAL && to == XIAOXIN_BATTERY_STATE_LOW) {
    return k_critical_to_low_ms;
  }
  return 0;
}

static xiaoxin_battery_state_t desired_state_for(
  const xiaoxin_battery_context_t* ctx
) {
  const int percent = ctx->estimated_percent;
  switch (ctx->state) {
    case XIAOXIN_BATTERY_STATE_UNKNOWN:
      return percent >= k_unknown_to_normal_percent
        ? XIAOXIN_BATTERY_STATE_NORMAL
        : XIAOXIN_BATTERY_STATE_UNKNOWN;
    case XIAOXIN_BATTERY_STATE_NORMAL:
      return percent <= k_normal_to_low_percent
        ? XIAOXIN_BATTERY_STATE_LOW
        : XIAOXIN_BATTERY_STATE_NORMAL;
    case XIAOXIN_BATTERY_STATE_LOW:
      if (percent <= k_low_to_critical_percent) {
        return XIAOXIN_BATTERY_STATE_CRITICAL;
      }
      if (percent >= k_low_to_normal_percent) {
        return XIAOXIN_BATTERY_STATE_NORMAL;
      }
      return XIAOXIN_BATTERY_STATE_LOW;
    case XIAOXIN_BATTERY_STATE_CRITICAL:
      return percent >= k_critical_to_low_percent
        ? XIAOXIN_BATTERY_STATE_LOW
        : XIAOXIN_BATTERY_STATE_CRITICAL;
  }
  return XIAOXIN_BATTERY_STATE_UNKNOWN;
}

void xiaoxin_battery_state_init(
  xiaoxin_battery_context_t* ctx,
  uint32_t now_ms
) {
  if (ctx == NULL) {
    return;
  }
  ctx->state = XIAOXIN_BATTERY_STATE_UNKNOWN;
  ctx->estimated_percent = 0;
  ctx->smoothed_voltage_mv = 0;
  ctx->has_sample = false;
  ctx->candidate_since_ms = now_ms;
  ctx->candidate_state = XIAOXIN_BATTERY_STATE_UNKNOWN;
  ctx->last_snapshot = make_snapshot(ctx, false, false, false);
}

xiaoxin_battery_snapshot_t xiaoxin_battery_state_snapshot(
  const xiaoxin_battery_context_t* ctx
) {
  if (ctx == NULL) {
    xiaoxin_battery_context_t empty;
    xiaoxin_battery_state_init(&empty, 0);
    return empty.last_snapshot;
  }
  return ctx->last_snapshot;
}

xiaoxin_battery_snapshot_t xiaoxin_battery_state_update(
  xiaoxin_battery_context_t* ctx,
  int voltage_mv,
  bool sample_valid,
  xiaoxin_battery_load_t load,
  uint32_t now_ms
) {
  if (ctx == NULL) {
    xiaoxin_battery_context_t empty;
    xiaoxin_battery_state_init(&empty, now_ms);
    return empty.last_snapshot;
  }

  bool low_edge = false;
  bool critical_edge = false;
  bool recovered_edge = false;

  if (!is_plausible_sample(voltage_mv, sample_valid)) {
    const bool changed = ctx->state != XIAOXIN_BATTERY_STATE_UNKNOWN;
    ctx->state = XIAOXIN_BATTERY_STATE_UNKNOWN;
    ctx->has_sample = false;
    ctx->candidate_state = XIAOXIN_BATTERY_STATE_UNKNOWN;
    ctx->candidate_since_ms = now_ms;
    ctx->last_snapshot = make_snapshot(ctx, false, false, changed);
    return ctx->last_snapshot;
  }

  update_smoothed_sample(ctx, voltage_mv);

  const xiaoxin_battery_state_t desired = desired_state_for(ctx);
  if (desired == ctx->state) {
    ctx->candidate_state = ctx->state;
    ctx->candidate_since_ms = now_ms;
    ctx->last_snapshot = make_snapshot(ctx, false, false, false);
    return ctx->last_snapshot;
  }

  reset_candidate(ctx, desired, now_ms);
  const uint32_t required_ms = required_ms_for(ctx->state, desired, load);
  if (required_ms == 0 || elapsed(ctx->candidate_since_ms, now_ms, required_ms)) {
    const xiaoxin_battery_state_t previous = ctx->state;
    ctx->state = desired;
    ctx->candidate_state = desired;
    ctx->candidate_since_ms = now_ms;
    low_edge = previous == XIAOXIN_BATTERY_STATE_NORMAL &&
               desired == XIAOXIN_BATTERY_STATE_LOW;
    critical_edge = desired == XIAOXIN_BATTERY_STATE_CRITICAL &&
                    previous != XIAOXIN_BATTERY_STATE_CRITICAL;
    recovered_edge =
      (previous == XIAOXIN_BATTERY_STATE_LOW ||
       previous == XIAOXIN_BATTERY_STATE_CRITICAL) &&
      desired == XIAOXIN_BATTERY_STATE_NORMAL;
  }

  ctx->last_snapshot = make_snapshot(ctx, low_edge, critical_edge, recovered_edge);
  return ctx->last_snapshot;
}
```

- [ ] **Step 5: Run the test and adjust only if the asserted behavior differs**

Run:

```powershell
if (!(Test-Path build)) { New-Item -ItemType Directory build | Out-Null }
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_battery_state_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_state.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_level.c -o build\xiaoxin_battery_state_test.exe
.\build\xiaoxin_battery_state_test.exe
```

Expected:

```text
xiaoxin_battery_state tests passed
```

- [ ] **Step 6: Commit**

```powershell
git add main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_state.h main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_state.c tests\xiaoxin_battery_state_test.c
git commit -m "feat: add xiaoxin adc battery state"
```

---

### Task 2: Make System Overlay Consume Battery State

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.h`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c`
- Modify: `tests/xiaoxin_system_overlay_test.c`

**Interfaces:**
- Consumes: `xiaoxin_battery_state_t`
- Produces: `xiaoxin_system_overlay_style(network_state, battery_state)` with LOW orange and CRITICAL red.

- [ ] **Step 1: Write the failing overlay tests**

Modify `tests/xiaoxin_system_overlay_test.c` so its battery assertions use state names:

```c
#include <assert.h>
#include <stdio.h>

#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.h"

static void connected_network_uses_active_color(void) {
  const xiaoxin_system_overlay_style_t style = xiaoxin_system_overlay_style(
    XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED,
    XIAOXIN_BATTERY_STATE_NORMAL
  );
  assert(style.network_color == XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR);
  assert(style.network_opa == XIAOXIN_SYSTEM_OVERLAY_ACTIVE_OPA);
  assert(!style.network_disconnected);
  assert(style.battery_color == XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR);
}

static void disconnected_network_is_muted(void) {
  const xiaoxin_system_overlay_style_t style = xiaoxin_system_overlay_style(
    XIAOXIN_SYSTEM_OVERLAY_NETWORK_DISCONNECTED,
    XIAOXIN_BATTERY_STATE_NORMAL
  );
  assert(style.network_color == XIAOXIN_SYSTEM_OVERLAY_MUTED_COLOR);
  assert(style.network_opa == XIAOXIN_SYSTEM_OVERLAY_MUTED_OPA);
  assert(style.network_disconnected);
}

static void low_battery_uses_warning_color(void) {
  const xiaoxin_system_overlay_style_t style = xiaoxin_system_overlay_style(
    XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED,
    XIAOXIN_BATTERY_STATE_LOW
  );
  assert(style.battery_color == XIAOXIN_SYSTEM_OVERLAY_LOW_BATTERY_COLOR);
}

static void critical_battery_uses_critical_color(void) {
  const xiaoxin_system_overlay_style_t style = xiaoxin_system_overlay_style(
    XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED,
    XIAOXIN_BATTERY_STATE_CRITICAL
  );
  assert(style.battery_color == XIAOXIN_SYSTEM_OVERLAY_CRITICAL_BATTERY_COLOR);
}

static void unknown_battery_is_muted(void) {
  const xiaoxin_system_overlay_style_t style = xiaoxin_system_overlay_style(
    XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED,
    XIAOXIN_BATTERY_STATE_UNKNOWN
  );
  assert(style.battery_color == XIAOXIN_SYSTEM_OVERLAY_MUTED_COLOR);
}

int main(void) {
  connected_network_uses_active_color();
  disconnected_network_is_muted();
  low_battery_uses_warning_color();
  critical_battery_uses_critical_color();
  unknown_battery_is_muted();
  puts("xiaoxin_system_overlay tests passed");
  return 0;
}
```

- [ ] **Step 2: Run the overlay test to verify it fails**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_system_overlay_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_system_overlay.c -o build\xiaoxin_system_overlay_test.exe
```

Expected: FAIL because the overlay signature still expects `int battery_level_percent`.

- [ ] **Step 3: Update overlay header**

Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.h`:

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "xiaoxin_battery_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR 0x168a73u
#define XIAOXIN_SYSTEM_OVERLAY_MUTED_COLOR 0x7f9890u
#define XIAOXIN_SYSTEM_OVERLAY_LOW_BATTERY_COLOR 0xf2a43au
#define XIAOXIN_SYSTEM_OVERLAY_CRITICAL_BATTERY_COLOR 0xff5e5bu
#define XIAOXIN_SYSTEM_OVERLAY_ACTIVE_OPA 255u
#define XIAOXIN_SYSTEM_OVERLAY_MUTED_OPA 118u

typedef enum {
  XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED = 0,
  XIAOXIN_SYSTEM_OVERLAY_NETWORK_DISCONNECTED,
  XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONFIGURING,
} xiaoxin_system_overlay_network_state_t;

typedef struct {
  uint32_t network_color;
  uint8_t network_opa;
  bool network_disconnected;
  uint32_t battery_color;
} xiaoxin_system_overlay_style_t;

xiaoxin_system_overlay_style_t xiaoxin_system_overlay_style(
  xiaoxin_system_overlay_network_state_t network_state,
  xiaoxin_battery_state_t battery_state
);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 4: Update overlay implementation**

Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c`:

```c
#include "xiaoxin_system_overlay.h"

static uint32_t battery_color_for_state(xiaoxin_battery_state_t battery_state) {
  switch (battery_state) {
    case XIAOXIN_BATTERY_STATE_NORMAL:
      return XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR;
    case XIAOXIN_BATTERY_STATE_LOW:
      return XIAOXIN_SYSTEM_OVERLAY_LOW_BATTERY_COLOR;
    case XIAOXIN_BATTERY_STATE_CRITICAL:
      return XIAOXIN_SYSTEM_OVERLAY_CRITICAL_BATTERY_COLOR;
    case XIAOXIN_BATTERY_STATE_UNKNOWN:
    default:
      return XIAOXIN_SYSTEM_OVERLAY_MUTED_COLOR;
  }
}

xiaoxin_system_overlay_style_t xiaoxin_system_overlay_style(
  xiaoxin_system_overlay_network_state_t network_state,
  xiaoxin_battery_state_t battery_state
) {
  const bool disconnected =
    network_state == XIAOXIN_SYSTEM_OVERLAY_NETWORK_DISCONNECTED ||
    network_state == XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONFIGURING;

  xiaoxin_system_overlay_style_t style = {
    .network_color = disconnected
      ? XIAOXIN_SYSTEM_OVERLAY_MUTED_COLOR
      : XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR,
    .network_opa = disconnected
      ? XIAOXIN_SYSTEM_OVERLAY_MUTED_OPA
      : XIAOXIN_SYSTEM_OVERLAY_ACTIVE_OPA,
    .network_disconnected = disconnected,
    .battery_color = battery_color_for_state(battery_state),
  };

  return style;
}
```

- [ ] **Step 5: Run tests**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_system_overlay_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_system_overlay.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_state.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_level.c -o build\xiaoxin_system_overlay_test.exe
.\build\xiaoxin_system_overlay_test.exe
```

Expected:

```text
xiaoxin_system_overlay tests passed
```

- [ ] **Step 6: Commit**

```powershell
git add main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_system_overlay.h main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_system_overlay.c tests\xiaoxin_system_overlay_test.c
git commit -m "refactor: style battery overlay from state"
```

---

### Task 3: Make Overview Model Consume Qualitative Battery State

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c`
- Modify: `tests/xiaoxin_overview_model_test.c`

**Interfaces:**
- Consumes: `xiaoxin_battery_state_t battery_state`
- Produces: overview detail text from `UNKNOWN/NORMAL/LOW/CRITICAL`

- [ ] **Step 1: Update tests first**

In `tests/xiaoxin_overview_model_test.c`, replace percent-based cases with state-based cases. Add this helper near existing assertions:

```c
static xiaoxin_overview_snapshot_t build_device_snapshot(
  xiaoxin_battery_state_t battery_state
) {
  xiaoxin_overview_state_t state = {
    .network_connected = true,
    .battery_state = battery_state,
  };
  xiaoxin_overview_snapshot_t snapshot;
  xiaoxin_overview_model_build(&state, &snapshot);
  return snapshot;
}
```

Replace the old percent-band test with:

```c
static void battery_state_uses_qualitative_status(void) {
  xiaoxin_overview_snapshot_t snapshot =
    build_device_snapshot(XIAOXIN_BATTERY_STATE_UNKNOWN);
  assert_card(&snapshot, DEVICE_INDEX, "设备状态", "设备", 4, "WiFi 已连接", "电量未知");

  snapshot = build_device_snapshot(XIAOXIN_BATTERY_STATE_NORMAL);
  assert_card(&snapshot, DEVICE_INDEX, "设备状态", "设备", 4, "WiFi 已连接", "电量正常");

  snapshot = build_device_snapshot(XIAOXIN_BATTERY_STATE_LOW);
  assert_card(&snapshot, DEVICE_INDEX, "设备状态", "设备", 4, "WiFi 已连接", "电量偏低");

  snapshot = build_device_snapshot(XIAOXIN_BATTERY_STATE_CRITICAL);
  assert_card(&snapshot, DEVICE_INDEX, "设备状态", "设备", 4, "WiFi 已连接", "请尽快充电");
}
```

Update `main()` to call `battery_state_uses_qualitative_status();` and remove calls to percent-specific test helpers.

- [ ] **Step 2: Run overview test to verify it fails**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_overview_model_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_overview_model.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_card_pager.c -o build\xiaoxin_overview_model_test.exe
```

Expected: FAIL because `xiaoxin_overview_state_t` has no `battery_state` field.

- [ ] **Step 3: Update overview header**

Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h`:

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "xiaoxin_battery_state.h"
#include "xiaoxin_card_pager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XIAOXIN_OVERVIEW_ITEM_COUNT 4
#define XIAOXIN_OVERVIEW_BODY_MAX 40
#define XIAOXIN_OVERVIEW_DETAIL_MAX 64

typedef struct {
  bool time_valid;
  int hour;
  int minute;
  int month;
  int day;
  uint8_t weekday;
  bool network_connected;
  xiaoxin_battery_state_t battery_state;
  bool weather_available;
  bool weather_configured;
  const char* weather_summary;
  const char* weather_detail;
  bool course_configured;
  bool course_available_today;
  const char* course_title;
  const char* course_detail;
  bool todo_configured;
  uint8_t todo_count;
  const char* todo_detail;
} xiaoxin_overview_state_t;

typedef struct {
  char time_text[8];
  char date_text[24];
  xiaoxin_card_item_t items[XIAOXIN_OVERVIEW_ITEM_COUNT];
  char body_storage[XIAOXIN_OVERVIEW_ITEM_COUNT][XIAOXIN_OVERVIEW_BODY_MAX];
  char detail_storage[XIAOXIN_OVERVIEW_ITEM_COUNT][XIAOXIN_OVERVIEW_DETAIL_MAX];
  uint8_t item_count;
} xiaoxin_overview_snapshot_t;

void xiaoxin_overview_model_build(
  const xiaoxin_overview_state_t* state,
  xiaoxin_overview_snapshot_t* snapshot
);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 4: Update overview model battery text**

In `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c`, replace the percent helper with:

```c
static const char* battery_status_text(xiaoxin_battery_state_t state) {
  switch (state) {
    case XIAOXIN_BATTERY_STATE_NORMAL:
      return "电量正常";
    case XIAOXIN_BATTERY_STATE_LOW:
      return "电量偏低";
    case XIAOXIN_BATTERY_STATE_CRITICAL:
      return "请尽快充电";
    case XIAOXIN_BATTERY_STATE_UNKNOWN:
    default:
      return "电量未知";
  }
}
```

Replace the device detail assignment with:

```c
copy_detail(snapshot, XIAOXIN_OVERVIEW_DEVICE_INDEX, battery_status_text(state->battery_state));
```

Keep the existing fallback for `state == NULL` as `"电量未知"`.

- [ ] **Step 5: Run overview test**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_overview_model_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_overview_model.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_card_pager.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_state.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_level.c -o build\xiaoxin_overview_model_test.exe
.\build\xiaoxin_overview_model_test.exe
```

Expected:

```text
xiaoxin_overview_model tests passed
```

- [ ] **Step 6: Commit**

```powershell
git add main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_overview_model.h main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_overview_model.c tests\xiaoxin_overview_model_test.c
git commit -m "refactor: use qualitative battery state in overview"
```

---

### Task 4: Wire Board Display to the Battery Snapshot

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Modify: `tests/xiaoxin_notification_visual_path_test.py`
- Modify: `tests/xiaoxin_pet_mood_integration_path_test.py`

**Interfaces:**
- Consumes:
  - `xiaoxin_battery_state_update(...)`
  - `xiaoxin_battery_state_snapshot(...)`
  - `xiaoxin_system_overlay_style(network_state, battery_state)`
- Produces:
  - Board-level `battery_context_`
  - Board-level `battery_snapshot_`
  - `RefreshBatterySnapshotLocked()`
  - `CurrentBatteryLoad()`

- [ ] **Step 1: Add source-path tests for board integration**

In `tests/xiaoxin_notification_visual_path_test.py`, add:

```python
def test_board_routes_battery_ui_through_state_snapshot():
    source = read_source()
    status_body = function_body(source, "virtual void UpdateStatusBar(bool update_all = false) override")
    overlay_body = function_body(source, "void ApplyBatteryOverlayLevel()")
    notification_body = function_body(source, "void SyncLowBatteryNotificationLocked()")

    assert '#include "xiaoxin_battery_state.h"' in source
    assert "xiaoxin_battery_context_t battery_context_ = {};" in source
    assert "xiaoxin_battery_snapshot_t battery_snapshot_ = {};" in source
    assert "RefreshBatterySnapshotLocked();" in status_body
    assert "xiaoxin_system_overlay_style(SystemOverlayNetworkState(), battery_snapshot_.state)" in overlay_body
    assert "battery_snapshot_.state == XIAOXIN_BATTERY_STATE_LOW" in notification_body
    assert "battery_snapshot_.state == XIAOXIN_BATTERY_STATE_CRITICAL" in notification_body
    assert "level <= 20" not in notification_body
```

In `tests/xiaoxin_pet_mood_integration_path_test.py`, replace the battery-level assertion with:

```python
def test_device_status_refresh_syncs_mood_edges_from_battery_snapshot():
    body = function_body(source=read_source(), signature="virtual void UpdateStatusBar(bool update_all = false) override")

    assert "RefreshBatterySnapshotLocked();" in body
    assert "SyncPetMoodDeviceStateLocked();" in body
```

Add:

```python
def test_pet_mood_uses_battery_state_edges_not_percent_thresholds():
    body = function_body(source=read_source(), signature="void SyncPetMoodDeviceStateLocked()")

    assert "battery_snapshot_.low_edge" in body
    assert "battery_snapshot_.critical_edge" in body
    assert "battery_snapshot_.recovered_edge" in body
    assert "battery_level <=" not in body
```

- [ ] **Step 2: Run source-path tests to verify they fail**

Run:

```powershell
python -m pytest tests\xiaoxin_notification_visual_path_test.py tests\xiaoxin_pet_mood_integration_path_test.py -q
```

Expected: FAIL because board code still uses percent-based functions.

- [ ] **Step 3: Include the state header and add members**

In the `extern "C"` include block of `esp32-s3-touch-lcd-1.46.cc`, add:

```cpp
#include "xiaoxin_battery_state.h"
```

Near existing `mood_` member declarations, add:

```cpp
    xiaoxin_battery_context_t battery_context_ = {};
    xiaoxin_battery_snapshot_t battery_snapshot_ = {};
```

After `paopao_pet_mood_init(&mood_, now_ms);` in setup, add:

```cpp
        xiaoxin_battery_state_init(&battery_context_, now_ms);
        battery_snapshot_ = xiaoxin_battery_state_snapshot(&battery_context_);
```

- [ ] **Step 4: Add board helpers**

Replace `NotificationBatteryLevelPercent()` with:

```cpp
    xiaoxin_battery_load_t CurrentBatteryLoad() const {
        const auto state = Application::GetInstance().GetDeviceState();
        switch (state) {
            case kDeviceStateListening:
            case kDeviceStateSpeaking:
                return XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE;
            default:
                return XIAOXIN_BATTERY_LOAD_IDLE;
        }
    }

    void RefreshBatterySnapshotLocked() {
        int level = 0;
        bool charging = false;
        bool discharging = true;
        const bool sample_valid = Board::GetInstance().GetBatteryLevel(level, charging, discharging);
        const int voltage_mv = sample_valid ? last_battery_voltage_mv_ : 0;
        battery_snapshot_ = xiaoxin_battery_state_update(
            &battery_context_,
            voltage_mv,
            sample_valid,
            CurrentBatteryLoad(),
            NowMs()
        );
    }
```

Add `int last_battery_voltage_mv_ = 0;` as a `CustomBoard` member near `battery_adc_available_`, then in `GetBatteryLevel()` set it:

```cpp
        last_battery_voltage_mv_ = voltage_mv;
```

Expose it from `CustomBoard` through a method:

```cpp
    int LastBatteryVoltageMv() const {
        return last_battery_voltage_mv_;
    }
```

If accessing `CustomBoard` from `PaopaoPetDisplay` is awkward, use `level` converted back is not acceptable; instead add a `BoardBatteryVoltageMv()` helper in `PaopaoPetDisplay` that dynamic-casts through `CustomBoard::instance_` and returns `0` when unavailable.

- [ ] **Step 5: Update status bar flow**

Change `UpdateStatusBar()` body from percent flow to snapshot flow:

```cpp
            RefreshBatterySnapshotLocked();
            HideLegacyLowBatteryPopupLocked();
            ApplySystemOverlayNetworkStyle();
            ApplyBatteryOverlayLevel();
            SyncLowBatteryNotificationLocked();
            SyncPetMoodDeviceStateLocked();
            RefreshOverviewPageIfVisible();
            RaiseOverlayObjects();
```

- [ ] **Step 6: Update overlay helpers**

Change `ApplySystemOverlayNetworkStyle()` style lookup to:

```cpp
        const auto style = xiaoxin_system_overlay_style(network_state, battery_snapshot_.state);
```

Change `ApplyBatteryOverlayLevel(int level)` to `ApplyBatteryOverlayLevel()`:

```cpp
    void ApplyBatteryOverlayLevel() {
        if (battery_overlay_fill_ == nullptr || battery_overlay_box_ == nullptr) {
            return;
        }

        const int clamped = std::max(0, std::min(100, battery_snapshot_.estimated_percent));
        const auto style = xiaoxin_system_overlay_style(SystemOverlayNetworkState(), battery_snapshot_.state);
        const int inner_w = k_system_battery_w - 4;
        const int fill_w = battery_snapshot_.state == XIAOXIN_BATTERY_STATE_UNKNOWN
            ? 3
            : std::max(3, (inner_w * clamped) / 100);
        lv_obj_set_width(battery_overlay_fill_, fill_w);
        lv_obj_set_style_bg_color(battery_overlay_fill_, lv_color_hex(style.battery_color), 0);
        lv_obj_set_style_border_color(battery_overlay_box_, lv_color_hex(style.battery_color), 0);
        if (battery_overlay_cap_ != nullptr) {
            lv_obj_set_style_bg_color(battery_overlay_cap_, lv_color_hex(style.battery_color), 0);
        }
    }
```

Update the initialization call from:

```cpp
        ApplyBatteryOverlayLevel(NotificationBatteryLevelPercent());
```

to:

```cpp
        ApplyBatteryOverlayLevel();
```

- [ ] **Step 7: Update low-battery notifications**

Change `SyncLowBatteryNotificationLocked(int level)` to:

```cpp
    void SyncLowBatteryNotificationLocked() {
        const bool low =
            battery_snapshot_.state == XIAOXIN_BATTERY_STATE_LOW ||
            battery_snapshot_.state == XIAOXIN_BATTERY_STATE_CRITICAL;
        if (!low) {
            if (low_battery_notification_active_) {
                RemoveNotificationEventLocked(XIAOXIN_NOTIFICATION_EVENT_LOW_BATTERY);
            }
            low_battery_notification_active_ = false;
            last_low_battery_notification_level_ = battery_snapshot_.estimated_percent;
            return;
        }

        const char* body = battery_snapshot_.state == XIAOXIN_BATTERY_STATE_CRITICAL
            ? "电量很低，请尽快充电"
            : "电量偏低，请尽快充电";

        if (low_battery_notification_active_ &&
            last_low_battery_notification_level_ == battery_snapshot_.estimated_percent) {
            return;
        }

        const xiaoxin_notification_event_t event = {
            .type = XIAOXIN_NOTIFICATION_EVENT_LOW_BATTERY,
            .title = nullptr,
            .body = body,
            .tag = nullptr,
            .priority = 0,
            .ttl_ms = 0,
        };
        UpsertNotificationEventLocked(event);
        low_battery_notification_active_ = true;
        last_low_battery_notification_level_ = battery_snapshot_.estimated_percent;
    }
```

- [ ] **Step 8: Update mood sync**

Change `SyncPetMoodDeviceStateLocked(int battery_level)` to:

```cpp
    void SyncPetMoodDeviceStateLocked() {
        const uint32_t now_ms = NowMs();
        if (battery_snapshot_.low_edge || battery_snapshot_.critical_edge) {
            DispatchPetMoodEventLocked(
                PAOPAO_PET_MOOD_EVENT_BATTERY_LOW,
                PAOPAO_PET_TRIGGER_NONE,
                now_ms
            );
        } else if (battery_snapshot_.recovered_edge) {
            DispatchPetMoodEventLocked(
                PAOPAO_PET_MOOD_EVENT_BATTERY_RECOVERED,
                PAOPAO_PET_TRIGGER_NONE,
                now_ms
            );
        }

        const bool wifi_connected =
            SystemOverlayNetworkState() == XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED;
        if (wifi_connected != mood_.wifi_connected) {
            DispatchPetMoodEventLocked(
                wifi_connected
                    ? PAOPAO_PET_MOOD_EVENT_WIFI_CONNECTED
                    : PAOPAO_PET_MOOD_EVENT_WIFI_DISCONNECTED,
                PAOPAO_PET_TRIGGER_NONE,
                now_ms
            );
        }
    }
```

- [ ] **Step 9: Run source-path tests**

Run:

```powershell
python -m pytest tests\xiaoxin_notification_visual_path_test.py tests\xiaoxin_pet_mood_integration_path_test.py -q
```

Expected:

```text
... passed
```

- [ ] **Step 10: Commit**

```powershell
git add main\boards\waveshare\esp32-s3-touch-lcd-1.46\esp32-s3-touch-lcd-1.46.cc tests\xiaoxin_notification_visual_path_test.py tests\xiaoxin_pet_mood_integration_path_test.py
git commit -m "refactor: route board battery ui through state"
```

---

### Task 5: Final Regression Sweep and Build Wiring Check

**Files:**
- Modify if needed: `main/CMakeLists.txt`

**Interfaces:**
- Consumes: all previous tasks.
- Produces: verified test suite and build-source inclusion.

- [ ] **Step 1: Check CMake source inclusion**

Run:

```powershell
rg -n "file\\(GLOB BOARD_SOURCES|xiaoxin_battery_state" main\CMakeLists.txt main\boards\waveshare\esp32-s3-touch-lcd-1.46 -S
```

Expected: `main/CMakeLists.txt` has `file(GLOB BOARD_SOURCES ... *.c)` for the board source directory, and `xiaoxin_battery_state.c` exists in that directory. If the build configuration has changed and the glob is gone, add this line inside the existing `if(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_46)` source append block:

```cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c
```

- [ ] **Step 2: Run all Python tests**

Run:

```powershell
python -m pytest tests -q
```

Expected:

```text
passed
```

- [ ] **Step 3: Run all related C tests**

Run:

```powershell
if (!(Test-Path build)) { New-Item -ItemType Directory build | Out-Null }
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_battery_level_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_level.c -o build\xiaoxin_battery_level_test.exe
.\build\xiaoxin_battery_level_test.exe
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_battery_state_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_state.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_level.c -o build\xiaoxin_battery_state_test.exe
.\build\xiaoxin_battery_state_test.exe
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_system_overlay_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_system_overlay.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_state.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_level.c -o build\xiaoxin_system_overlay_test.exe
.\build\xiaoxin_system_overlay_test.exe
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_overview_model_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_overview_model.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_card_pager.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_state.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_battery_level.c -o build\xiaoxin_overview_model_test.exe
.\build\xiaoxin_overview_model_test.exe
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_card_pager_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_card_pager.c -o build\xiaoxin_card_pager_test.exe
.\build\xiaoxin_card_pager_test.exe
gcc -std=c11 -Wall -Wextra -I. tests\paopao_pet_mood_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\paopao_pet_mood.c -o build\paopao_pet_mood_test.exe
.\build\paopao_pet_mood_test.exe
```

Expected each executable prints its `tests passed` line.

- [ ] **Step 4: Check full ESP-IDF build availability**

Run:

```powershell
Get-Command idf.py -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
```

If it prints a path, run:

```powershell
idf.py build
```

Expected: build exits with code 0. If the command prints no path, record in the handoff that full firmware build was not run because `idf.py` is unavailable in the current shell.

- [ ] **Step 5: Commit verification-only CMake change if made**

If `main/CMakeLists.txt` was changed in Step 1:

```powershell
git add main\CMakeLists.txt
git commit -m "build: include xiaoxin battery state source"
```

If `main/CMakeLists.txt` was unchanged, do not create a commit for this task.

---

## Self-Review

Spec coverage:

- Stable ADC state module: Task 1.
- LOW vs CRITICAL split: Tasks 1 and 2.
- No exact user-facing percentage: Tasks 3 and 4.
- Unified state consumed by overlay, notifications, overview, and mood: Tasks 2, 3, and 4.
- Voice-active suppression: Task 1 state-machine tests and Task 4 `CurrentBatteryLoad()`.
- Subtitle protection: preserved by global constraint and guarded by existing notification visual tests.
- Build/test verification: Task 5.

Placeholder scan:

- Placeholder-pattern scan passed after drafting.
- Each code-changing step includes the exact code shape or replacement snippet.
- Each test step includes the command and expected result.

Type consistency:

- `xiaoxin_battery_state_t`, `xiaoxin_battery_load_t`, `xiaoxin_battery_context_t`, and `xiaoxin_battery_snapshot_t` are defined in Task 1 and reused by later tasks.
- `xiaoxin_system_overlay_style()` signature changes in Task 2 and is consumed with the new signature in Task 4.
- `xiaoxin_overview_state_t.battery_state` is defined in Task 3 and populated from the board snapshot in Task 4.
