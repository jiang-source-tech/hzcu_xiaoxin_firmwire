# Xiaoxin About Page Product Info Implementation Plan

> Implementation status: completed on 2026-06-23.

## Goal

Redesign the Waveshare ESP32-S3 Touch LCD 1.46 settings “About” page so it presents user-facing Xiaoxin product information instead of exposing the development board model.

## Scope

- Keep the change localized to the Waveshare 1.46-inch board UI implementation.
- Keep `esp_app_get_description()` as the source for firmware version metadata.
- Format the visible card around product identity:
  - `小芯 D151`
  - `桌面助手`
  - firmware version
- Keep the About page compact enough for the 1.46-inch round display.
- Do not change settings navigation semantics outside the About page layout adjustment.

## Tasks

- [x] Add a source-path guard that rejects `Waveshare ESP32-S3 Touch LCD 1.46` in `RenderSettingsAboutPage()`.
- [x] Update the About page visible copy to prioritize Xiaoxin product identity.
- [x] Move the About page body upward with `k_settings_about_body_y` so the text avoids the round-screen bottom.
- [x] Verify with the settings source-path test suite.

## Verification

- `python -m pytest tests/xiaoxin_settings_path_test.py tests/wifi_config_status_path_test.py -q`: passed, 33 tests.
