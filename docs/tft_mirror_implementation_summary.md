# TFT Mirror Implementation Summary

This change adds a pixel-exact TFT mirror for the Waveshare
ESP32-S3-Touch-LCD-1.46 board.

## What Was Added

- A binary TFT mirror protocol with fixed packet headers, RGB565 metadata,
  rectangle bounds validation, and CRC32 helpers.
- Unit tests for protocol header layout, payload struct sizes, rectangle bounds,
  and CRC32 behavior.
- Project Kconfig options under `Display Mirror`:
  - `DISPLAY_MIRROR_ENABLE`
  - `DISPLAY_MIRROR_TRANSPORT_USB_CDC`
  - `DISPLAY_MIRROR_USB_CDC_BAUD_HINT`
  - `DISPLAY_MIRROR_FULL_FRAME_INTERVAL`
- CMake wiring so mirror firmware sources are compiled only when mirror support
  is enabled.
- An LCD panel proxy that wraps the real `esp_lcd_panel_handle_t`, mirrors final
  `draw_bitmap` RGB565 rectangles into a framebuffer, sends mirror packets, and
  forwards the original draw call to the real panel.
- A TinyUSB CDC transport for binary mirror packets, using a dedicated CDC data
  path rather than the ESP-IDF log console stream.
- A Python PC viewer that reads the USB CDC stream, resynchronizes on packet
  loss, validates CRC32, reconstructs the RGB565 framebuffer, and displays it
  in a Tk window.
- User documentation for enabling the feature, flashing firmware, finding the
  CDC port, and running the viewer.

## Firmware Flow

When `CONFIG_DISPLAY_MIRROR_ENABLE=y`, `LcdDisplay` wraps the LCD panel before
calling `lvgl_port_add_disp()`. This places the mirror at the LCD panel boundary,
so the mirror receives the final RGB565 rectangles submitted by LVGL through
`esp_lvgl_port`.

For each valid rectangle, the wrapper:

1. Copies the RGB565 bytes into a full mirror framebuffer.
2. Emits a `RECT` packet over USB CDC when a viewer is connected.
3. Emits `FRAME_END` metadata with the framebuffer CRC.
4. Sends periodic or reconnect-triggered `FULL_FRAME` checkpoints.
5. Forwards the same `draw_bitmap` call to the real panel.

## Host Flow

Run:

```powershell
python scripts\tft_mirror_viewer.py --serial COM7 --baud 2000000
```

The viewer applies `RECT` packets to its own framebuffer and updates the window
after `FRAME_END`. It validates payload CRCs and compares the reconstructed
framebuffer CRC with the firmware-provided CRC.

## Validation Performed

- Protocol unit test passed with GCC.
- Python viewer syntax check passed.
- Normal `idf.py build` passed.
- A temporary mirror-enabled SDK config was built successfully in `build_mirror`,
  compiling `display_mirror_protocol.c`, `display_mirror_panel.cc`, and
  `display_mirror_transport_usb.cc`.

## Notes

- The feature guarantees the same RGB565 pixel values submitted to the LCD
  panel, not identical physical panel brightness, gamma, backlight, color
  temperature, or viewing angle.
- The mirror is currently scoped to
  `BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_46`.
- Full-frame packets are checkpoints. Normal operation depends on rectangle
  packets because a full 412x412 RGB565 frame is 339488 bytes.
