#ifndef DISPLAY_MIRROR_PANEL_H
#define DISPLAY_MIRROR_PANEL_H

#include <esp_lcd_panel_ops.h>
#include <stdint.h>

esp_lcd_panel_handle_t display_mirror_wrap_panel(
    esp_lcd_panel_handle_t real_panel,
    uint16_t width,
    uint16_t height,
    uint16_t flags
);

esp_lcd_panel_handle_t display_mirror_unwrap_panel(esp_lcd_panel_handle_t panel);

#endif
