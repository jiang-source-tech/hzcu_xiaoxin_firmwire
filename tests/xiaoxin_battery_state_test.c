#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
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
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 7000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_NORMAL);
  assert(!snapshot.low_edge);
}

static void sustained_low_enters_low_once(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 7000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 12000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 17000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 22000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 42000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_LOW);
  assert(snapshot.low_edge);
  snapshot = feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 43000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_LOW);
  assert(!snapshot.low_edge);
}

static void low_requires_hysteresis_to_recover(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 7000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 12000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 17000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 22000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 42000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 43000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_LOW);
  assert(!snapshot.recovered_edge);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 47000);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 52000);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 62000);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 72000);
  snapshot = feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 82000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_NORMAL);
  assert(snapshot.recovered_edge);
}

static void sustained_critical_enters_critical(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 7000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 12000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 17000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 22000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 42000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 47000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_IDLE, 57000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_CRITICAL);
  assert(snapshot.critical_edge);
}

static void voice_active_extends_low_confirmation(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 6000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE, 7000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE, 12000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE, 17000);
  feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE, 22000);
  xiaoxin_battery_snapshot_t snapshot =
    feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE, 62000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_NORMAL);
  snapshot = feed(&ctx, 3000, XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE, 67000);
  assert(snapshot.state == XIAOXIN_BATTERY_STATE_LOW);
  assert(snapshot.low_edge);
}

static void invalid_samples_become_unknown_without_low_edge(void) {
  xiaoxin_battery_context_t ctx;
  xiaoxin_battery_state_init(&ctx, 0);
  feed(&ctx, 3900, XIAOXIN_BATTERY_LOAD_IDLE, 1000);
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
