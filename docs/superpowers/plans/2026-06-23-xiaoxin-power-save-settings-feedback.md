# Xiaoxin Power Save Settings Feedback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add visible feedback for the settings page power-save toggle and tint the home battery indicator amber when power save is enabled.

**Architecture:** Keep setting semantics in `Settings("wifi").sleep_mode` and reuse the existing `PowerSaveTimer`. Add small pure helpers to `xiaoxin_settings_model` for display text and battery color selection, then let the board UI read/write settings and render the resulting state.

**Tech Stack:** ESP-IDF C++ board code, LVGL UI objects, C model tests, Python path tests.

## Global Constraints

- Do not add a power-save icon.
- Do not show a bottom hint after toggling power save; it overlaps the `退出设置` control on the round screen.
- Low-battery color must override the power-save amber color.
- Do not introduce configurable sleep durations.
- Keep the feature scoped to `main/boards/waveshare/esp32-s3-touch-lcd-1.46`.

---

### Task 1: Model power-save display helpers

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.h`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c`
- Test: `tests/xiaoxin_settings_model_test.c`

**Interfaces:**
- Produces: `xiaoxin_settings_power_save_value_label(bool enabled) -> const char*`
- Produces: `xiaoxin_settings_power_save_battery_color(bool power_save_enabled, bool low_battery, uint32_t normal_color, uint32_t low_color, uint32_t power_save_color) -> uint32_t`

- [x] Write failing C tests for enabled/disabled labels and color priority.
- [x] Run the C model test and confirm it fails because the helpers do not exist.
- [x] Add the two helper declarations and implementations.
- [x] Run the C model test and confirm it passes.

### Task 2: Render and toggle power-save state in settings

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Test: `tests/xiaoxin_settings_path_test.py`

**Interfaces:**
- Consumes: `xiaoxin_settings_power_save_value_label(bool enabled)`
- Consumes: `Settings("wifi").GetBool("sleep_mode", true)`
- Consumes: `Settings("wifi", true).SetBool("sleep_mode", enabled)`
- Consumes: `PowerSaveTimer::SetEnabled(bool enabled)`

- [x] Write failing path tests for visible `已开启/已关闭` value styling and click-to-toggle behavior.
- [x] Run the path tests and confirm they fail.
- [x] Add board helpers to read and toggle `sleep_mode`.
- [x] Update list rendering so the power-save row uses a status capsule and non-power rows keep normal arrow styling.
- [x] Handle `XIAOXIN_SETTINGS_ITEM_POWER_SAVE` in `OpenSettingsItemLocked()` by toggling and re-rendering the list.
- [x] Run the path tests and confirm they pass.

### Task 3: Tint home battery amber while power save is enabled

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Test: `tests/xiaoxin_settings_path_test.py`

**Interfaces:**
- Consumes: `xiaoxin_settings_power_save_battery_color(...)`

- [x] Write failing path tests for amber battery color selection and low-battery priority.
- [x] Run the path tests and confirm they fail.
- [x] Add a board-level amber color constant.
- [x] Route `style.battery_color` through `xiaoxin_settings_power_save_battery_color(...)` in `ApplyBatteryOverlayLevel()`.
- [x] Run the path tests and C model test.
