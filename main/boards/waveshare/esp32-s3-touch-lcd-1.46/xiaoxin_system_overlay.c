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
