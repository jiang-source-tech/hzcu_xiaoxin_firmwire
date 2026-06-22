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
