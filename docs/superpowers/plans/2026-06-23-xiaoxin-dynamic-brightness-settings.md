# Xiaoxin Dynamic Brightness Settings Implementation Plan

> Implementation status: completed on 2026-06-23.

## Goal

Replace the Waveshare ESP32-S3 Touch LCD 1.46 settings page brightness presets with a touch-draggable 10–100 brightness slider that previews while dragging and persists on release.

## Scope

- Keep the existing settings overlay and `Backlight` persistence path.
- Add one pure C coordinate-to-brightness mapper to `xiaoxin_settings_model`.
- Render the slider in the board display layer.
- Keep drag preview and final persistence separate:
  - preview: `Backlight::SetBrightness(value, false)`
  - release: `Backlight::SetBrightness(value, true)`
- Do not add a new NVS key or write `Settings("display")` directly from the settings page.

## Tasks

- [x] Add `xiaoxin_settings_brightness_from_x(int x, int left, int width)`.
- [x] Cover the coordinate mapper in `tests/xiaoxin_settings_model_test.c`.
- [x] Replace the `30 / 70 / 100` brightness preset page with a slider UI.
- [x] Add value label, track, fill, thumb, `低/高` labels, and a brightness-page `返回` button.
- [x] Preview brightness while dragging and persist the final value on release.
- [x] Deduplicate repeated brightness writes during drag.
- [x] Add source-path guards for slider rendering, preview/persist separation, back button behavior, and manual drag capture.
- [x] Verify with C model and Python source-path tests.

## Verification

- `gcc -std=c11 -Wall -Wextra -Werror tests/xiaoxin_settings_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c -o build/xiaoxin_settings_model_test.exe`: passed.
- `.\build\xiaoxin_settings_model_test.exe`: passed, output `xiaoxin_settings_model tests passed`.
- `python -m pytest tests/xiaoxin_settings_path_test.py tests/wifi_config_status_path_test.py -q`: passed, 33 tests.
