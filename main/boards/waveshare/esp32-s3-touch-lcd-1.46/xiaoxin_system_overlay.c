#include "xiaoxin_system_overlay.h"

xiaoxin_system_overlay_style_t xiaoxin_system_overlay_style(
  xiaoxin_system_overlay_network_state_t network_state,
  int battery_level_percent
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
    .battery_color = battery_level_percent <= 20
      ? XIAOXIN_SYSTEM_OVERLAY_LOW_BATTERY_COLOR
      : XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR,
  };

  return style;
}
