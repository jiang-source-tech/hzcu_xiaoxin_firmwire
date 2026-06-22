# Xiaoxin USB Battery Display Stability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make USB-powered battery display stable by separating ADC estimation from UI display state and adding conservative external-power inference.

**Architecture:** Keep `xiaoxin_battery_state` as the deep module: board code feeds raw ADC voltage and receives a stable snapshot. The module owns sample quality, external/unknown inference, display-level hysteresis, and low-battery edge gating. UI, overview, notifications, and pet mood consume the snapshot instead of interpreting ADC values.

**Tech Stack:** C11 battery/overview/overlay modules, C++ board integration in `esp32-s3-touch-lcd-1.46.cc`, local `gcc` C tests, existing Python path tests.

## Implementation Progress

**Status:** implemented and locally verified on branch `codex/xiaoxin-pet-mood-system`.

**Completed in this version:**

- Added stable battery snapshot fields: `power_source`, `display_percent`, `display_level`, and `percent_reliable`.
- Added sample-quality classification so invalid, extreme-low, and high-untrusted samples do not update EMA or directly drive the battery state machine.
- Added conservative `EXTERNAL` inference for high-voltage, rapid-rise, steady-high/low-ripple, and alternating-sample patterns, with confirmation counts.
- Added `UNKNOWN` handling, `EXTERNAL` minimum hold, confirmed `EXTERNAL/UNKNOWN -> BATTERY` exits, and low/critical/recovered edge suppression until battery-state reconfirmation.
- Wired overlay style and fill width to `power_source` and stable `display_level`.
- Gated low-battery notifications and pet mood battery events to confirmed `BATTERY` power source.
- Added overview power-source detail text for external and unknown battery states.
- Closed final review gaps:
  - Confirmed `EXTERNAL` is held through the 30s minimum hold even when invalid samples accumulate.
  - Normal discharge detection now uses a bounded 60s discharge window instead of stale min/max spread.
  - Confirmed `BATTERY + LOW` always snapshots `display_level == 1`; confirmed `BATTERY + CRITICAL` always snapshots `display_level == 0`, including steady-state updates.

**Verification on 2026-06-22:**

- `xiaoxin_battery_level_test`: passed.
- `xiaoxin_battery_state_test`: passed.
- `xiaoxin_system_overlay_test`: passed.
- `xiaoxin_overview_model_test`: passed.
- Python path tests `xiaoxin_notification_visual_path_test.py`, `xiaoxin_pet_mood_integration_path_test.py`, and `xiaoxin_card_pager_threshold_test.py`: `28 passed`.
- `rg` audit confirmed UI paths use `display_level`/`power_source` and do not use `estimated_percent <= 20` or `level <= 20` as the low-battery source.

**Remaining environment limitation:**

- `idf.py` is not available in the current shell, so firmware build verification was not run locally.

## Global Constraints

- Valid battery sample range is `3300mV <= voltage_mv <= 4400mV`.
- Extreme low sample range is `0mV < voltage_mv < 3300mV`; it must not enter EMA or the battery state machine.
- High untrusted sample range is `voltage_mv > 4400mV`; it must not be displayed as full battery.
- USB steady charging around `4100-4300mV` falls inside the valid range, so external inference must not rely only on `> 4400mV`.
- Entering `EXTERNAL` requires one external condition plus confirmation count `>= 3`.
- Rapid-rise external condition: within `60s`, smoothed voltage rises by more than `300mV` and final smoothed voltage is `> 4050mV`.
- Steady-high external condition: smoothed voltage stays `> 4080mV` for more than `120s` and peak-to-valley ripple is `< 50mV`.
- Normal discharge feature: peak-to-valley ripple is `>= 50mV` over a `60s` window.
- `EXTERNAL` minimum hold time is `30s`.
- Exit `EXTERNAL -> BATTERY` requires `20s` of valid samples, `smoothed_voltage_mv < 4080mV`, and normal discharge feature.
- Exit `UNKNOWN -> BATTERY` requires `10s` of valid samples and normal discharge feature.
- `UNKNOWN` keeps the previous `display_level` for the first `10s`; after that it displays muted level `0`.
- Returning from `EXTERNAL` or `UNKNOWN` may update `display_level` immediately, but `low_edge`, `critical_edge`, and `recovered_edge` must satisfy battery state confirmation again.
- First version does not require new VBUS, CHG, or PMIC hardware input.

---

## File Structure

- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h`
  - Add `xiaoxin_battery_power_source_t`.
  - Extend `xiaoxin_battery_snapshot_t`.
  - Extend `xiaoxin_battery_context_t` with internal inference state.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c`
  - Own sample quality classification, power-source inference, display-level hysteresis, edge gating, and snapshot generation.
- Modify `tests/xiaoxin_battery_state_test.c`
  - Add unit tests for the full spec behavior.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.h`
  - Accept `power_source` in the style interface.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c`
  - Color `EXTERNAL` as active and `UNKNOWN` as muted.
- Modify `tests/xiaoxin_system_overlay_test.c`
  - Cover external and unknown source colors.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h`
  - Add `battery_power_source` to overview state.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c`
  - Add `外接供电中` and `电量状态未知` detail behavior.
- Modify `tests/xiaoxin_overview_model_test.c`
  - Cover external and unknown text behavior.
- Modify `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
  - Consume new snapshot fields in battery overlay, notification gating, pet mood gating, and overview state.
- Modify path tests:
  - `tests/xiaoxin_pet_mood_integration_path_test.py`
  - `tests/xiaoxin_notification_visual_path_test.py`

---

### Task 1: Extend Battery Snapshot Interface And Display-Level Hysteresis

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c`
- Test: `tests/xiaoxin_battery_state_test.c`

**Interfaces:**
- Consumes: existing `xiaoxin_battery_state_update(ctx, voltage_mv, sample_valid, load, now_ms)`.
- Produces:
  - `xiaoxin_battery_power_source_t`
  - `snapshot.power_source`
  - `snapshot.display_percent`
  - `snapshot.display_level`
  - `snapshot.percent_reliable`

- [ ] **Step 1: Write failing interface and display-level tests**

Add these helper assertions and tests to `tests/xiaoxin_battery_state_test.c`:

```c
static void init_snapshot_has_display_fields(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 1000);
  xiaoxin_battery_snapshot_t snapshot = xiaoxin_battery_state_snapshot(&ctx);
  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_UNKNOWN);
  assert(snapshot.display_percent == 0);
  assert(snapshot.display_level == 0);
  assert(!snapshot.percent_reliable);
}

static void display_level_has_hysteresis(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);

  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 4050, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  snapshot = feed(&ctx, 4050, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_BATTERY);
  assert(snapshot.display_level == 4);

  snapshot = feed(&ctx, 3950, XIAOXIN_BATTERY_LOAD_IDLE, 11000);
  assert(snapshot.display_level == 4);

  for (uint32_t now_ms = 16000; now_ms <= 70000; now_ms += 5000) {
    snapshot = feed(&ctx, 3850, XIAOXIN_BATTERY_LOAD_IDLE, now_ms);
  }
  assert(snapshot.display_level == 3);
}
```

Call both tests from `main()`.

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_battery_state_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.c -o build/xiaoxin_battery_state_test.exe
```

Expected: compile fails because `XIAOXIN_BATTERY_POWER_UNKNOWN`, `power_source`, `display_percent`, `display_level`, and `percent_reliable` do not exist.

- [ ] **Step 3: Extend the public structs**

Add to `xiaoxin_battery_state.h`:

```c
typedef enum {
  XIAOXIN_BATTERY_POWER_BATTERY = 0,
  XIAOXIN_BATTERY_POWER_EXTERNAL,
  XIAOXIN_BATTERY_POWER_UNKNOWN,
} xiaoxin_battery_power_source_t;
```

Extend `xiaoxin_battery_snapshot_t`:

```c
  xiaoxin_battery_power_source_t power_source;
  int display_percent;
  uint8_t display_level;
  bool percent_reliable;
```

Extend `xiaoxin_battery_context_t`:

```c
  xiaoxin_battery_power_source_t power_source;
  int display_percent;
  uint8_t display_level;
  bool percent_reliable;
```

- [ ] **Step 4: Initialize and snapshot new fields**

In `xiaoxin_battery_state_init()` set:

```c
  ctx->power_source = XIAOXIN_BATTERY_POWER_UNKNOWN;
  ctx->display_percent = 0;
  ctx->display_level = 0;
  ctx->percent_reliable = false;
```

In `make_snapshot()` include:

```c
    .power_source = ctx->power_source,
    .display_percent = ctx->display_percent,
    .display_level = ctx->display_level,
    .percent_reliable = ctx->percent_reliable,
```

- [ ] **Step 5: Add display-level hysteresis helper**

Add this helper to `xiaoxin_battery_state.c`:

```c
static uint8_t display_level_for_percent(uint8_t current_level, int percent) {
  if (current_level >= 4) {
    return percent < 65 ? 3 : 4;
  }
  if (current_level == 3) {
    if (percent >= 70) {
      return 4;
    }
    return percent < 35 ? 2 : 3;
  }
  if (current_level == 2) {
    if (percent >= 40) {
      return 3;
    }
    return percent < 10 ? 1 : 2;
  }
  if (current_level == 1) {
    return percent >= 20 ? 2 : 1;
  }
  if (percent >= 70) {
    return 4;
  }
  if (percent >= 40) {
    return 3;
  }
  if (percent >= 15) {
    return 2;
  }
  return 1;
}
```

Update the valid-sample path after `estimated_percent` is calculated:

```c
  ctx->power_source = XIAOXIN_BATTERY_POWER_BATTERY;
  ctx->percent_reliable = true;
  ctx->display_percent = ctx->estimated_percent;
  ctx->display_level = display_level_for_percent(
    ctx->display_level,
    ctx->display_percent
  );
```

- [ ] **Step 6: Run test to verify it passes**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_battery_state_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.c -o build/xiaoxin_battery_state_test.exe; .\build\xiaoxin_battery_state_test.exe
```

Expected: `xiaoxin_battery_state tests passed`.

- [ ] **Step 7: Commit**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c tests/xiaoxin_battery_state_test.c
git commit -m "feat: add stable battery display snapshot"
```

---

### Task 2: Add Sample Quality And External-Power Entry Inference

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c`
- Test: `tests/xiaoxin_battery_state_test.c`

**Interfaces:**
- Consumes: snapshot fields from Task 1.
- Produces: `EXTERNAL` and `UNKNOWN` inference inside `xiaoxin_battery_state_update()`.

- [ ] **Step 1: Write failing sample-quality tests**

Add these tests:

```c
static void high_voltage_does_not_update_ema_or_show_full(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  xiaoxin_battery_snapshot_t snapshot = feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  snapshot = feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  const int before_mv = snapshot.smoothed_voltage_mv;
  const uint8_t before_level = snapshot.display_level;

  snapshot = feed(&ctx, 4500, XIAOXIN_BATTERY_LOAD_IDLE, 7000);
  assert(snapshot.smoothed_voltage_mv == before_mv);
  assert(snapshot.display_level == before_level);
  assert(snapshot.estimated_percent < 100);
}

static void extreme_low_sample_does_not_update_ema_or_state(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  xiaoxin_battery_snapshot_t snapshot = feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  snapshot = feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  const int before_mv = snapshot.smoothed_voltage_mv;
  const xiaoxin_battery_state_t before_state = snapshot.state;

  snapshot = feed(&ctx, 2000, XIAOXIN_BATTERY_LOAD_IDLE, 7000);
  assert(snapshot.smoothed_voltage_mv == before_mv);
  assert(snapshot.state == before_state);
}

static void single_high_voltage_spike_does_not_enter_external(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 4500, XIAOXIN_BATTERY_LOAD_IDLE, 2000);
  assert(snapshot.power_source != XIAOXIN_BATTERY_POWER_EXTERNAL);
}

static void three_high_voltage_samples_enter_external(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 4500, XIAOXIN_BATTERY_LOAD_IDLE, 2000);
  feed(&ctx, 4510, XIAOXIN_BATTERY_LOAD_IDLE, 3000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 4520, XIAOXIN_BATTERY_LOAD_IDLE, 4000);
  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_EXTERNAL);
  assert(snapshot.display_level == 4);
  assert(!snapshot.percent_reliable);
}
```

Call them from `main()`.

- [ ] **Step 2: Write failing trend tests**

Add:

```c
static void rapid_rise_enters_external_after_confirmation(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3700, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 4200, XIAOXIN_BATTERY_LOAD_IDLE, 10000);
  feed(&ctx, 4200, XIAOXIN_BATTERY_LOAD_IDLE, 17000);
  feed(&ctx, 4200, XIAOXIN_BATTERY_LOAD_IDLE, 24000);
  feed(&ctx, 4200, XIAOXIN_BATTERY_LOAD_IDLE, 31000);
  feed(&ctx, 4200, XIAOXIN_BATTERY_LOAD_IDLE, 38000);
  feed(&ctx, 4200, XIAOXIN_BATTERY_LOAD_IDLE, 45000);
  feed(&ctx, 4200, XIAOXIN_BATTERY_LOAD_IDLE, 52000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 4200, XIAOXIN_BATTERY_LOAD_IDLE, 59000);
  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_EXTERNAL);
}

static void steady_high_voltage_enters_external(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  xiaoxin_battery_snapshot_t snapshot = xiaoxin_battery_state_snapshot(&ctx);
  for (uint32_t now_ms = 1000; now_ms <= 130000; now_ms += 10000) {
    snapshot = feed(&ctx, 4200, XIAOXIN_BATTERY_LOAD_IDLE, now_ms);
  }
  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_EXTERNAL);
}

static void alternating_sample_types_enter_external(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 4500, XIAOXIN_BATTERY_LOAD_IDLE, 2000);
  feed(&ctx, 3910, XIAOXIN_BATTERY_LOAD_IDLE, 3000);
  feed(&ctx, 4510, XIAOXIN_BATTERY_LOAD_IDLE, 4000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3920, XIAOXIN_BATTERY_LOAD_IDLE, 5000);
  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_EXTERNAL);
}
```

- [ ] **Step 3: Run test to verify it fails**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_battery_state_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.c -o build/xiaoxin_battery_state_test.exe; .\build\xiaoxin_battery_state_test.exe
```

Expected: assertions fail because high/extreme-low samples still affect the current logic and external inference does not exist.

- [ ] **Step 4: Add internal sample classification**

Add to `xiaoxin_battery_state.c`:

```c
typedef enum {
  XIAOXIN_BATTERY_SAMPLE_INVALID = 0,
  XIAOXIN_BATTERY_SAMPLE_EXTREME_LOW,
  XIAOXIN_BATTERY_SAMPLE_VALID,
  XIAOXIN_BATTERY_SAMPLE_HIGH,
} xiaoxin_battery_sample_quality_t;

static xiaoxin_battery_sample_quality_t classify_sample(
  int voltage_mv,
  bool sample_valid
) {
  if (!sample_valid || voltage_mv <= 0) {
    return XIAOXIN_BATTERY_SAMPLE_INVALID;
  }
  if (voltage_mv < 3300) {
    return XIAOXIN_BATTERY_SAMPLE_EXTREME_LOW;
  }
  if (voltage_mv > 4400) {
    return XIAOXIN_BATTERY_SAMPLE_HIGH;
  }
  return XIAOXIN_BATTERY_SAMPLE_VALID;
}
```

Replace `is_plausible_sample()` usage so only `XIAOXIN_BATTERY_SAMPLE_VALID` updates EMA and battery state.

- [ ] **Step 5: Extend context with inference counters**

Add to `xiaoxin_battery_context_t`:

```c
  xiaoxin_battery_power_source_t candidate_power_source;
  uint8_t candidate_power_count;
  uint32_t power_source_since_ms;
  uint8_t invalid_sample_count;
  uint8_t high_sample_count;
  int trend_window_min_mv;
  int trend_window_max_mv;
  uint32_t trend_window_start_ms;
  int rise_window_start_mv;
  uint32_t rise_window_start_ms;
  uint32_t steady_high_since_ms;
  uint32_t alternation_window_start_ms;
  uint8_t alternating_sample_count;
  uint8_t last_sample_quality;
```

Initialize them in `xiaoxin_battery_state_init()`:

```c
  ctx->candidate_power_source = XIAOXIN_BATTERY_POWER_UNKNOWN;
  ctx->candidate_power_count = 0;
  ctx->power_source_since_ms = now_ms;
  ctx->invalid_sample_count = 0;
  ctx->high_sample_count = 0;
  ctx->trend_window_min_mv = 0;
  ctx->trend_window_max_mv = 0;
  ctx->trend_window_start_ms = now_ms;
  ctx->rise_window_start_mv = 0;
  ctx->rise_window_start_ms = now_ms;
  ctx->steady_high_since_ms = 0;
  ctx->alternation_window_start_ms = now_ms;
  ctx->alternating_sample_count = 0;
  ctx->last_sample_quality = XIAOXIN_BATTERY_SAMPLE_INVALID;
```

- [ ] **Step 6: Add confirmed power-source switch helper**

Add:

```c
static void propose_power_source(
  xiaoxin_battery_context_t* ctx,
  xiaoxin_battery_power_source_t source,
  uint32_t now_ms
) {
  if (ctx->candidate_power_source != source) {
    ctx->candidate_power_source = source;
    ctx->candidate_power_count = 1;
    return;
  }
  if (ctx->candidate_power_count < 3) {
    ctx->candidate_power_count++;
  }
  if (ctx->candidate_power_count >= 3 && ctx->power_source != source) {
    ctx->power_source = source;
    ctx->power_source_since_ms = now_ms;
  }
}
```

- [ ] **Step 7: Implement external entry conditions**

In `xiaoxin_battery_state_update()` before battery-state transitions:

```c
const xiaoxin_battery_sample_quality_t quality =
  classify_sample(voltage_mv, sample_valid);
```

Update counters:

```c
if (quality == XIAOXIN_BATTERY_SAMPLE_HIGH) {
  ctx->high_sample_count++;
} else {
  ctx->high_sample_count = 0;
}

if (quality == XIAOXIN_BATTERY_SAMPLE_INVALID) {
  ctx->invalid_sample_count++;
} else {
  ctx->invalid_sample_count = 0;
}
```

After valid EMA update, maintain windows:

```c
if (ctx->trend_window_start_ms == 0 ||
    (uint32_t)(now_ms - ctx->trend_window_start_ms) > 60000) {
  ctx->trend_window_start_ms = now_ms;
  ctx->trend_window_min_mv = ctx->smoothed_voltage_mv;
  ctx->trend_window_max_mv = ctx->smoothed_voltage_mv;
} else {
  if (ctx->smoothed_voltage_mv < ctx->trend_window_min_mv) {
    ctx->trend_window_min_mv = ctx->smoothed_voltage_mv;
  }
  if (ctx->smoothed_voltage_mv > ctx->trend_window_max_mv) {
    ctx->trend_window_max_mv = ctx->smoothed_voltage_mv;
  }
}

if (ctx->rise_window_start_mv == 0 ||
    (uint32_t)(now_ms - ctx->rise_window_start_ms) > 60000) {
  ctx->rise_window_start_ms = now_ms;
  ctx->rise_window_start_mv = ctx->smoothed_voltage_mv;
}

if (ctx->smoothed_voltage_mv > 4080) {
  if (ctx->steady_high_since_ms == 0) {
    ctx->steady_high_since_ms = now_ms;
  }
} else {
  ctx->steady_high_since_ms = 0;
}
```

Track valid/high/invalid alternation inside a 10 second window:

```c
const bool alternating_candidate =
  (quality == XIAOXIN_BATTERY_SAMPLE_VALID ||
   quality == XIAOXIN_BATTERY_SAMPLE_HIGH ||
   quality == XIAOXIN_BATTERY_SAMPLE_INVALID);

if (!alternating_candidate ||
    (uint32_t)(now_ms - ctx->alternation_window_start_ms) > 10000) {
  ctx->alternation_window_start_ms = now_ms;
  ctx->alternating_sample_count = 0;
} else if (ctx->last_sample_quality != quality &&
           ctx->last_sample_quality != XIAOXIN_BATTERY_SAMPLE_EXTREME_LOW) {
  ctx->alternating_sample_count++;
}
ctx->last_sample_quality = (uint8_t)quality;
```

External proposals:

```c
const bool condition_a = ctx->high_sample_count >= 3;
const bool condition_b =
  (uint32_t)(now_ms - ctx->rise_window_start_ms) <= 60000 &&
  ctx->smoothed_voltage_mv - ctx->rise_window_start_mv > 300 &&
  ctx->smoothed_voltage_mv > 4050;
const bool condition_c =
  ctx->steady_high_since_ms != 0 &&
  elapsed(ctx->steady_high_since_ms, now_ms, 120000) &&
  ctx->smoothed_voltage_mv > 4080 &&
  ctx->trend_window_max_mv - ctx->trend_window_min_mv < 50;
const bool condition_d = ctx->alternating_sample_count >= 3;

if (condition_a || condition_b || condition_c || condition_d) {
  propose_power_source(ctx, XIAOXIN_BATTERY_POWER_EXTERNAL, now_ms);
}
```

- [ ] **Step 8: Keep external display stable**

When `ctx->power_source == XIAOXIN_BATTERY_POWER_EXTERNAL`, set:

```c
ctx->percent_reliable = false;
ctx->display_level = 4;
```

Do not update `ctx->display_percent` from high, extreme-low, or invalid samples.

- [ ] **Step 9: Run test to verify it passes**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_battery_state_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.c -o build/xiaoxin_battery_state_test.exe; .\build\xiaoxin_battery_state_test.exe
```

Expected: `xiaoxin_battery_state tests passed`.

- [ ] **Step 10: Commit**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c tests/xiaoxin_battery_state_test.c
git commit -m "feat: infer external battery power from adc trends"
```

---

### Task 3: Add UNKNOWN Handling, Exit Rules, And Edge Gating

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c`
- Test: `tests/xiaoxin_battery_state_test.c`

**Interfaces:**
- Consumes: power-source inference from Task 2.
- Produces: minimum hold, exit confirmation, UNKNOWN display hold, and edge suppression.

- [ ] **Step 1: Write failing UNKNOWN and hold tests**

Add:

```c
static void invalid_samples_enter_unknown_after_ten_reads(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);

  xiaoxin_battery_snapshot_t snapshot = xiaoxin_battery_state_snapshot(&ctx);
  for (uint32_t now_ms = 7000; now_ms <= 16000; now_ms += 1000) {
    snapshot = xiaoxin_battery_state_update(
      &ctx,
      0,
      true,
      XIAOXIN_BATTERY_LOAD_IDLE,
      now_ms
    );
  }

  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_UNKNOWN);
  assert(snapshot.display_level != 0);
}

static void unknown_display_level_drops_after_ten_seconds(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  for (uint32_t now_ms = 7000; now_ms <= 16000; now_ms += 1000) {
    xiaoxin_battery_state_update(&ctx, 0, true, XIAOXIN_BATTERY_LOAD_IDLE, now_ms);
  }

  xiaoxin_battery_snapshot_t snapshot =
    xiaoxin_battery_state_update(&ctx, 0, true, XIAOXIN_BATTERY_LOAD_IDLE, 27000);
  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_UNKNOWN);
  assert(snapshot.display_level == 0);
  assert(!snapshot.percent_reliable);
}
```

- [ ] **Step 2: Write failing external exit and edge tests**

Add:

```c
static void external_minimum_hold_blocks_early_exit(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 4500, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 4510, XIAOXIN_BATTERY_LOAD_IDLE, 2000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 4520, XIAOXIN_BATTERY_LOAD_IDLE, 3000);
  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_EXTERNAL);

  snapshot = feed(&ctx, 3800, XIAOXIN_BATTERY_LOAD_IDLE, 10000);
  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_EXTERNAL);
}

static void external_returns_to_battery_after_stable_discharge_window(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 4500, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 4510, XIAOXIN_BATTERY_LOAD_IDLE, 2000);
  feed(&ctx, 4520, XIAOXIN_BATTERY_LOAD_IDLE, 3000);

  xiaoxin_battery_snapshot_t snapshot = xiaoxin_battery_state_snapshot(&ctx);
  const int voltages[] = {3950, 3890, 3920, 3860, 3900};
  for (int i = 0; i < 5; ++i) {
    snapshot = feed(&ctx, voltages[i], XIAOXIN_BATTERY_LOAD_IDLE, 35000 + (uint32_t)i * 5000);
  }

  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_BATTERY);
  assert(snapshot.percent_reliable);
}

static void external_to_battery_does_not_emit_low_edge_without_confirmation(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 4500, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 4510, XIAOXIN_BATTERY_LOAD_IDLE, 2000);
  feed(&ctx, 4520, XIAOXIN_BATTERY_LOAD_IDLE, 3000);

  xiaoxin_battery_snapshot_t snapshot = xiaoxin_battery_state_snapshot(&ctx);
  const int voltages[] = {3500, 3440, 3490, 3430, 3480};
  for (int i = 0; i < 5; ++i) {
    snapshot = feed(&ctx, voltages[i], XIAOXIN_BATTERY_LOAD_IDLE, 35000 + (uint32_t)i * 5000);
  }

  assert(snapshot.power_source == XIAOXIN_BATTERY_POWER_BATTERY);
  assert(!snapshot.low_edge);
  assert(!snapshot.critical_edge);
}
```

- [ ] **Step 3: Run test to verify it fails**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_battery_state_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.c -o build/xiaoxin_battery_state_test.exe; .\build\xiaoxin_battery_state_test.exe
```

Expected: assertions fail for UNKNOWN and exit behavior.

- [ ] **Step 4: Extend context with hold and exit timers**

Add to `xiaoxin_battery_context_t`:

```c
  uint32_t unknown_since_ms;
  uint32_t battery_candidate_since_ms;
  bool state_edges_suppressed_until_reconfirmed;
```

Initialize:

```c
  ctx->unknown_since_ms = 0;
  ctx->battery_candidate_since_ms = 0;
  ctx->state_edges_suppressed_until_reconfirmed = false;
```

- [ ] **Step 5: Implement UNKNOWN entry and display hold**

In update logic:

```c
if (ctx->invalid_sample_count >= 10) {
  propose_power_source(ctx, XIAOXIN_BATTERY_POWER_UNKNOWN, now_ms);
}

if (ctx->power_source == XIAOXIN_BATTERY_POWER_UNKNOWN) {
  if (ctx->unknown_since_ms == 0) {
    ctx->unknown_since_ms = now_ms;
  }
  ctx->percent_reliable = false;
  if (elapsed(ctx->unknown_since_ms, now_ms, 10000)) {
    ctx->display_level = 0;
  }
}
```

- [ ] **Step 6: Implement external minimum hold and exit rules**

Add helper:

```c
static bool has_normal_discharge_feature(const xiaoxin_battery_context_t* ctx) {
  return ctx->trend_window_max_mv > 0 &&
         ctx->trend_window_min_mv > 0 &&
         ctx->trend_window_max_mv - ctx->trend_window_min_mv >= 50;
}
```

When power source is external:

```c
const bool external_hold_done =
  elapsed(ctx->power_source_since_ms, now_ms, 30000);
const bool can_exit_external =
  external_hold_done &&
  quality == XIAOXIN_BATTERY_SAMPLE_VALID &&
  ctx->smoothed_voltage_mv < 4080 &&
  has_normal_discharge_feature(ctx);
```

Require 20 seconds:

```c
if (can_exit_external) {
  if (ctx->battery_candidate_since_ms == 0) {
    ctx->battery_candidate_since_ms = now_ms;
  }
  if (elapsed(ctx->battery_candidate_since_ms, now_ms, 20000)) {
    ctx->power_source = XIAOXIN_BATTERY_POWER_BATTERY;
    ctx->power_source_since_ms = now_ms;
    ctx->state_edges_suppressed_until_reconfirmed = true;
  }
} else {
  ctx->battery_candidate_since_ms = 0;
}
```

For unknown, use `10000` instead of `20000`.

- [ ] **Step 7: Suppress edge events outside confirmed BATTERY state**

Before returning snapshot:

```c
if (ctx->power_source != XIAOXIN_BATTERY_POWER_BATTERY ||
    ctx->state_edges_suppressed_until_reconfirmed) {
  low_edge = false;
  critical_edge = false;
  recovered_edge = false;
}
```

Clear suppression only after the existing battery-state candidate has completed its required confirmation:

```c
if (ctx->power_source == XIAOXIN_BATTERY_POWER_BATTERY &&
    desired == ctx->state) {
  ctx->state_edges_suppressed_until_reconfirmed = false;
}
```

- [ ] **Step 8: Run test to verify it passes**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_battery_state_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.c -o build/xiaoxin_battery_state_test.exe; .\build\xiaoxin_battery_state_test.exe
```

Expected: `xiaoxin_battery_state tests passed`.

- [ ] **Step 9: Commit**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c tests/xiaoxin_battery_state_test.c
git commit -m "feat: stabilize battery source transitions"
```

---

### Task 4: Integrate Power Source Into Overlay Style And Battery Fill

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.h`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c`
- Modify: `tests/xiaoxin_system_overlay_test.c`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Test: `tests/xiaoxin_notification_visual_path_test.py`

**Interfaces:**
- Consumes: `snapshot.power_source` and `snapshot.display_level`.
- Produces: stable overlay color and fill width.

- [ ] **Step 1: Write failing overlay style tests**

Update existing calls to include a third parameter:

```c
xiaoxin_system_overlay_style(
  XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED,
  XIAOXIN_BATTERY_STATE_NORMAL,
  XIAOXIN_BATTERY_POWER_BATTERY
)
```

Add:

```c
static void external_power_uses_active_battery_color(void) {
  const xiaoxin_system_overlay_style_t style = xiaoxin_system_overlay_style(
    XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED,
    XIAOXIN_BATTERY_STATE_UNKNOWN,
    XIAOXIN_BATTERY_POWER_EXTERNAL
  );
  assert(style.battery_color == XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR);
}

static void unknown_power_uses_muted_battery_color(void) {
  const xiaoxin_system_overlay_style_t style = xiaoxin_system_overlay_style(
    XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED,
    XIAOXIN_BATTERY_STATE_NORMAL,
    XIAOXIN_BATTERY_POWER_UNKNOWN
  );
  assert(style.battery_color == XIAOXIN_SYSTEM_OVERLAY_MUTED_COLOR);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_system_overlay_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c -o build/xiaoxin_system_overlay_test.exe
```

Expected: compile fails because `xiaoxin_system_overlay_style()` has only two parameters.

- [ ] **Step 3: Extend overlay style interface**

In `xiaoxin_system_overlay.h`:

```c
xiaoxin_system_overlay_style_t xiaoxin_system_overlay_style(
  xiaoxin_system_overlay_network_state_t network_state,
  xiaoxin_battery_state_t battery_state,
  xiaoxin_battery_power_source_t power_source
);
```

In `xiaoxin_system_overlay.c`, update `battery_color_for_state()`:

```c
static uint32_t battery_color_for_state(
  xiaoxin_battery_state_t battery_state,
  xiaoxin_battery_power_source_t power_source
) {
  if (power_source == XIAOXIN_BATTERY_POWER_EXTERNAL) {
    return XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR;
  }
  if (power_source == XIAOXIN_BATTERY_POWER_UNKNOWN) {
    return XIAOXIN_SYSTEM_OVERLAY_MUTED_COLOR;
  }
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
```

- [ ] **Step 4: Update board overlay call sites**

In `esp32-s3-touch-lcd-1.46.cc`, replace:

```cpp
xiaoxin_system_overlay_style(network_state, battery_snapshot_.state)
```

with:

```cpp
xiaoxin_system_overlay_style(
    network_state,
    battery_snapshot_.state,
    battery_snapshot_.power_source
)
```

Replace the fill-width calculation in `ApplyBatteryOverlayLevel()`:

```cpp
const int level = std::max(0, std::min(4, (int)battery_snapshot_.display_level));
const int inner_w = k_system_battery_w - 4;
const int fill_w = battery_snapshot_.power_source == XIAOXIN_BATTERY_POWER_UNKNOWN &&
                   battery_snapshot_.display_level == 0
    ? 3
    : std::max(3, (inner_w * level) / 4);
```

Remove direct use of `battery_snapshot_.estimated_percent` from this function.

- [ ] **Step 5: Add path test for display_level usage**

In `tests/xiaoxin_notification_visual_path_test.py`, add:

```python
def test_battery_overlay_uses_stable_display_level():
    source = Path("main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc").read_text(encoding="utf-8")
    start = source.index("void ApplyBatteryOverlayLevel()")
    end = source.index("static uint32_t OverviewIconBgColorForTag", start)
    body = source[start:end]
    assert "battery_snapshot_.display_level" in body
    assert "battery_snapshot_.estimated_percent" not in body
    assert "battery_snapshot_.power_source" in body
```

- [ ] **Step 6: Run tests**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_system_overlay_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c -o build/xiaoxin_system_overlay_test.exe; .\build\xiaoxin_system_overlay_test.exe
python -m pytest tests/xiaoxin_notification_visual_path_test.py -q
```

Expected:

```text
xiaoxin_system_overlay tests passed
... passed
```

- [ ] **Step 7: Commit**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c tests/xiaoxin_system_overlay_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc tests/xiaoxin_notification_visual_path_test.py
git commit -m "feat: render stable battery overlay levels"
```

---

### Task 5: Gate Notifications And Pet Mood By Battery Power Source

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Modify: `tests/xiaoxin_pet_mood_integration_path_test.py`
- Modify: `tests/xiaoxin_notification_visual_path_test.py`

**Interfaces:**
- Consumes: `battery_snapshot_.power_source`.
- Produces: notifications and pet mood only respond to confirmed battery supply.

- [ ] **Step 1: Add path tests for notification gating**

In `tests/xiaoxin_notification_visual_path_test.py`, add:

```python
def test_low_battery_notification_requires_battery_power_source():
    source = Path("main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc").read_text(encoding="utf-8")
    start = source.index("void SyncLowBatteryNotificationLocked()")
    end = source.index("void ApplySystemOverlayNetworkStyle()", start)
    body = source[start:end]
    assert "battery_snapshot_.power_source == XIAOXIN_BATTERY_POWER_BATTERY" in body
    assert "battery_snapshot_.estimated_percent <= 20" not in body
```

- [ ] **Step 2: Add path tests for pet mood gating**

In `tests/xiaoxin_pet_mood_integration_path_test.py`, add:

```python
def test_pet_mood_battery_edges_require_battery_power_source():
    source = Path("main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc").read_text(encoding="utf-8")
    start = source.index("void SyncPetMoodDeviceStateLocked()")
    end = source.index("void ApplyBatteryOverlayLevel()", start)
    body = source[start:end]
    assert "battery_snapshot_.power_source == XIAOXIN_BATTERY_POWER_BATTERY" in body
    assert "battery_snapshot_.low_edge" in body
    assert "battery_snapshot_.critical_edge" in body
    assert "battery_snapshot_.recovered_edge" in body
```

- [ ] **Step 3: Run tests to verify they fail**

Run:

```powershell
python -m pytest tests/xiaoxin_notification_visual_path_test.py tests/xiaoxin_pet_mood_integration_path_test.py -q
```

Expected: the new assertions fail because board code does not check `power_source`.

- [ ] **Step 4: Gate low-battery notifications**

In `SyncLowBatteryNotificationLocked()` replace:

```cpp
const bool low =
    battery_snapshot_.state == XIAOXIN_BATTERY_STATE_LOW ||
    battery_snapshot_.state == XIAOXIN_BATTERY_STATE_CRITICAL;
```

with:

```cpp
const bool battery_powered =
    battery_snapshot_.power_source == XIAOXIN_BATTERY_POWER_BATTERY;
const bool low =
    battery_powered &&
    (battery_snapshot_.state == XIAOXIN_BATTERY_STATE_LOW ||
     battery_snapshot_.state == XIAOXIN_BATTERY_STATE_CRITICAL);
```

- [ ] **Step 5: Gate pet mood battery edges**

In `SyncPetMoodDeviceStateLocked()` wrap battery edge logic:

```cpp
const bool battery_powered =
    battery_snapshot_.power_source == XIAOXIN_BATTERY_POWER_BATTERY;
if (battery_powered &&
    (battery_snapshot_.low_edge || battery_snapshot_.critical_edge)) {
    DispatchPetMoodEventLocked(
        PAOPAO_PET_MOOD_EVENT_BATTERY_LOW,
        PAOPAO_PET_TRIGGER_NONE,
        now_ms
    );
} else if (battery_powered && battery_snapshot_.recovered_edge) {
    DispatchPetMoodEventLocked(
        PAOPAO_PET_MOOD_EVENT_BATTERY_RECOVERED,
        PAOPAO_PET_TRIGGER_NONE,
        now_ms
    );
}
```

- [ ] **Step 6: Run tests**

Run:

```powershell
python -m pytest tests/xiaoxin_notification_visual_path_test.py tests/xiaoxin_pet_mood_integration_path_test.py -q
```

Expected: all tests pass.

- [ ] **Step 7: Commit**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc tests/xiaoxin_notification_visual_path_test.py tests/xiaoxin_pet_mood_integration_path_test.py
git commit -m "feat: gate low battery effects by power source"
```

---

### Task 6: Add Overview Power-Source Text

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c`
- Modify: `tests/xiaoxin_overview_model_test.c`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**Interfaces:**
- Consumes: `xiaoxin_battery_power_source_t`.
- Produces: overview text `外接供电中` and `电量状态未知`.

- [ ] **Step 1: Write failing overview tests**

In `tests/xiaoxin_overview_model_test.c`, update `xiaoxin_overview_state_t` initializers to include:

```c
.battery_power_source = XIAOXIN_BATTERY_POWER_BATTERY,
```

Add:

```c
static void external_power_source_uses_external_detail(void) {
    const xiaoxin_overview_state_t state = {
        .network_connected = true,
        .battery_state = XIAOXIN_BATTERY_STATE_NORMAL,
        .battery_power_source = XIAOXIN_BATTERY_POWER_EXTERNAL,
    };
    xiaoxin_overview_snapshot_t snapshot;

    xiaoxin_overview_model_build(&state, &snapshot);

    assert_card(&snapshot, DEVICE_INDEX, "设备状态", "设备", 4, "WiFi 已连接", "外接供电中");
}

static void unknown_power_source_uses_unknown_detail(void) {
    const xiaoxin_overview_state_t state = {
        .network_connected = true,
        .battery_state = XIAOXIN_BATTERY_STATE_NORMAL,
        .battery_power_source = XIAOXIN_BATTERY_POWER_UNKNOWN,
    };
    xiaoxin_overview_snapshot_t snapshot;

    xiaoxin_overview_model_build(&state, &snapshot);

    assert_card(&snapshot, DEVICE_INDEX, "设备状态", "设备", 4, "WiFi 已连接", "电量状态未知");
}
```

If the existing file stores Chinese text in escaped or mojibake form, copy the exact existing expected strings from the file for title/body/tag and only add the new detail strings in the same encoding style used by the file.

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_overview_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_overview_model_test.exe
```

Expected: compile fails because `battery_power_source` does not exist.

- [ ] **Step 3: Extend overview state**

In `xiaoxin_overview_model.h` add next to `battery_state`:

```c
  xiaoxin_battery_power_source_t battery_power_source;
```

- [ ] **Step 4: Implement power-source-aware detail text**

Change `battery_status_text()` signature:

```c
static const char* battery_status_text(
  xiaoxin_battery_state_t state,
  xiaoxin_battery_power_source_t power_source
)
```

At the top of the function:

```c
  if (power_source == XIAOXIN_BATTERY_POWER_EXTERNAL) {
    return "外接供电中";
  }
  if (power_source == XIAOXIN_BATTERY_POWER_UNKNOWN) {
    return "电量状态未知";
  }
```

Update `build_device_item()`:

```c
const xiaoxin_battery_power_source_t power_source =
  state != NULL ? state->battery_power_source : XIAOXIN_BATTERY_POWER_UNKNOWN;
const xiaoxin_battery_state_t battery_state =
  state != NULL ? state->battery_state : XIAOXIN_BATTERY_STATE_UNKNOWN;

copy_detail(
  snapshot,
  XIAOXIN_OVERVIEW_DEVICE_INDEX,
  battery_status_text(battery_state, power_source)
);
```

- [ ] **Step 5: Pass power source from board**

In `BuildOverviewState()` in `esp32-s3-touch-lcd-1.46.cc`, add:

```cpp
state.battery_power_source = battery_snapshot_.power_source;
```

- [ ] **Step 6: Run test**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_overview_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_overview_model_test.exe; .\build\xiaoxin_overview_model_test.exe
```

Expected: `xiaoxin_overview_model tests passed`.

- [ ] **Step 7: Commit**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c tests/xiaoxin_overview_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "feat: show external battery source in overview"
```

---

### Task 7: Final Verification And Spec Coverage Audit

**Files:**
- Modify only if verification reveals a gap:
  - `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.*`
  - `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
  - related tests

**Interfaces:**
- Consumes: all prior tasks.
- Produces: verified implementation matching the modified spec.

- [ ] **Step 1: Run C unit tests**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_battery_level_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.c -o build/xiaoxin_battery_level_test.exe; .\build\xiaoxin_battery_level_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_battery_state_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.c -o build/xiaoxin_battery_state_test.exe; .\build\xiaoxin_battery_state_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_system_overlay_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c -o build/xiaoxin_system_overlay_test.exe; .\build\xiaoxin_system_overlay_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_overview_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_overview_model_test.exe; .\build\xiaoxin_overview_model_test.exe
```

Expected:

```text
xiaoxin_battery_level tests passed
xiaoxin_battery_state tests passed
xiaoxin_system_overlay tests passed
xiaoxin_overview_model tests passed
```

- [ ] **Step 2: Run Python path tests**

Run:

```powershell
python -m pytest tests/xiaoxin_notification_visual_path_test.py tests/xiaoxin_pet_mood_integration_path_test.py tests/xiaoxin_card_pager_threshold_test.py -q
```

Expected: all selected tests pass.

- [ ] **Step 3: Search for direct UI use of unstable percent**

Run:

```powershell
rg -n "estimated_percent|battery_level <=|level <= 20|display_level|power_source" main\boards\waveshare\esp32-s3-touch-lcd-1.46 tests
```

Expected:

- `estimated_percent` remains in state internals, overview diagnostic assignment, or notification bookkeeping only.
- `ApplyBatteryOverlayLevel()` uses `display_level`.
- `SyncLowBatteryNotificationLocked()` and `SyncPetMoodDeviceStateLocked()` check `power_source == XIAOXIN_BATTERY_POWER_BATTERY`.
- No Xiaoxin UI path uses `level <= 20` as the low-battery source.

- [ ] **Step 4: Build firmware if ESP-IDF environment is available**

Run:

```powershell
idf.py build
```

Expected: build completes without errors. If `idf.py` is unavailable in the shell, record that local C/Python tests passed and firmware build was not run because the ESP-IDF environment was not loaded.

- [ ] **Step 5: Commit final verification fixes**

If no fixes were needed, skip this commit. If fixes were needed:

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46 tests
git commit -m "test: verify usb battery display stability"
```
