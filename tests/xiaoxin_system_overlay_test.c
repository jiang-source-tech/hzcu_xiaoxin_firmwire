#include <assert.h>
#include <stdio.h>

#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.h"

static void connected_wifi_uses_calm_active_style(void) {
    xiaoxin_system_overlay_style_t style =
        xiaoxin_system_overlay_style(XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED, 75);

    assert(style.network_color == XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR);
    assert(style.network_opa == XIAOXIN_SYSTEM_OVERLAY_ACTIVE_OPA);
    assert(!style.network_disconnected);
    assert(style.battery_color == XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR);
}

static void disconnected_wifi_is_dimmed_and_marked(void) {
    xiaoxin_system_overlay_style_t style =
        xiaoxin_system_overlay_style(XIAOXIN_SYSTEM_OVERLAY_NETWORK_DISCONNECTED, 75);

    assert(style.network_color == XIAOXIN_SYSTEM_OVERLAY_MUTED_COLOR);
    assert(style.network_opa == XIAOXIN_SYSTEM_OVERLAY_MUTED_OPA);
    assert(style.network_disconnected);
}

static void config_mode_is_treated_as_disconnected(void) {
    xiaoxin_system_overlay_style_t style =
        xiaoxin_system_overlay_style(XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONFIGURING, 75);

    assert(style.network_color == XIAOXIN_SYSTEM_OVERLAY_MUTED_COLOR);
    assert(style.network_opa == XIAOXIN_SYSTEM_OVERLAY_MUTED_OPA);
    assert(style.network_disconnected);
}

static void low_battery_keeps_warning_color(void) {
    xiaoxin_system_overlay_style_t style =
        xiaoxin_system_overlay_style(XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED, 20);

    assert(style.battery_color == XIAOXIN_SYSTEM_OVERLAY_LOW_BATTERY_COLOR);
}

int main(void) {
    connected_wifi_uses_calm_active_style();
    disconnected_wifi_is_dimmed_and_marked();
    config_mode_is_treated_as_disconnected();
    low_battery_keeps_warning_color();
    puts("xiaoxin_system_overlay tests passed");
    return 0;
}
