# Xiaoxin BOOT Settings Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a BOOT-long-press settings overlay for the Waveshare ESP32-S3 Touch LCD 1.46 target, with brightness, Wi-Fi reconfiguration, about-device information, and a gated power-save setting.

**Architecture:** Add a small pure C settings model beside the target-board helpers, then wire it into the existing `PaopaoPetDisplay` overlay layer in `esp32-s3-touch-lcd-1.46.cc`. Button handling remains in `CustomBoard`, with BOOT single-click first checking whether the settings overlay is open before falling through to Wi-Fi config or chat toggling.

**Tech Stack:** ESP-IDF C/C++, LVGL, target-board C helper modules, existing local GCC C tests, existing pytest source-path tests.

## Implementation Update

**Status:** Implemented and reviewed on branch `codex/xiaoxin-boot-settings-page`; hardware follow-up added for BOOT GPIO event visibility.

**Updated:** 2026-06-23

**Implementation commits:**
- `59acf04` - `feat: add xiaoxin settings model`
- `1b8ac9f` - `test: tighten xiaoxin settings model coverage`
- `941e25b` - `feat: add boot settings overlay skeleton`
- `dedd23e` - `feat: add brightness and wifi settings actions`
- `258c025` - `fix: harden settings action path guards`
- `97abe0d` - `feat: gate settings power save with scheduler`
- `4518f56` - `fix: harden power save settings gate`
- `3474bfb` - `fix: avoid settings wifi lock reentry`
- `f802d01` - `fix: defer touch power-save wake`
- `b06f2ad` - `fix: add boot settings long press diagnostics`

**What shipped:**
- Added the pure C Xiaoxin settings model and local C coverage.
- Added `tests/xiaoxin_settings_path_test.py` source-path guards for BOOT routing, overlay behavior, brightness/Wi-Fi actions, about metadata, target capability gating, power-save scheduler truthfulness, and lock-safe deferred actions.
- Implemented the LVGL settings overlay for the Waveshare ESP32-S3 Touch LCD 1.46 board.
- Wired BOOT long press as the settings entry point, gated to `kDeviceStateIdle`.
- Preserved BOOT short-click startup Wi-Fi config and chat-toggle behavior when settings is closed; when settings is open, BOOT short-click closes the overlay first.
- Added brightness application through `Backlight::SetBrightness(clamped, true)`.
- Added Wi-Fi reconfiguration through the existing `EnterWifiConfigMode()` path.
- Added `PowerSaveTimer` integration and made power-save visibility depend on a real scheduler.
- Fixed final-review lock re-entry risks by deferring Wi-Fi reconfiguration and touch-triggered power-save wake until after the display lock is released.
- Added BOOT press/long-press diagnostics after hardware testing reported no visible long-press feedback.
- Added a GPIO0 polling fallback for hardware cases where `iot_button` does not emit BOOT press events:
  - BOOT/PWR inputs now explicitly enable `GPIO_PULLUP_ONLY`.
  - A 50ms `esp_timer` polls `BOOT_BUTTON_GPIO`.
  - GPIO0 held low for 2 seconds calls the same `HandleBootLongPress()` path.
  - The fallback shares `boot_long_press_handled_` with `iot_button` to avoid double-opening the settings overlay.
  - New logs identify the fallback path: `BOOT polling fallback started`, `BOOT poll press down`, `BOOT poll long press fallback`, and `BOOT poll press up`.

**Verification completed:**
- `xiaoxin_settings_model tests passed`
- `python -m pytest tests/xiaoxin_settings_path_test.py -q` -> `15 passed`
- `python -m pytest tests/xiaoxin_notification_visual_path_test.py tests/xiaoxin_pet_mood_integration_path_test.py -q` -> `27 passed`
- `xiaoxin_card_pager tests passed`
- `xiaoxin_system_overlay tests passed`
- Final code review after fixes: ready to merge, no Critical or Important findings.

**Verification limitations:**
- `idf.py build` could not run in this environment because `idf.py` is not available in PATH.
- `tests/wifi_config_status_path_test.py` was not present in this worktree. It existed only as an unrelated untracked file in the main checkout and was intentionally not copied or edited.

## Global Constraints

- BOOT key long press is the settings entry point.
- BOOT key short press keeps existing chat/startup Wi-Fi config semantics when settings is closed.
- Settings only opens from `kDeviceStateIdle`; it does not open from Connecting / Listening / Speaking / WifiConfiguring / AudioTesting / Activating / Upgrading / FatalError.
- Settings is an overlay; do not add a fourth page to `xiaoxin_card_page_t`.
- Waveshare ESP32-S3 Touch LCD 1.46 must not show volume, mute, prompt-sound, or vibration settings by default.
- Brightness must use `Backlight::SetBrightness(value, true)` and existing `Settings("display")` key `brightness`.
- Wi-Fi reconfiguration must reuse existing `EnterWifiConfigMode()`.
- Power-save setting must not be shown until the target board has `PowerSaveTimer`, `SleepTimer`, or an equivalent scheduler wired.
- Preserve unrelated working-tree changes in `main/application.cc`, `main/boards/common/wifi_board.cc`, and `tests/wifi_config_status_path_test.py`.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.h` | Pure C enums, capabilities, visible item generation, state gating, percent clamp helpers. |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c` | Implementation of the settings model. No LVGL, no `Settings`, no board singletons. |
| `tests/xiaoxin_settings_model_test.c` | Pure C tests for item gating, open-state gating, and brightness clamp. |
| `tests/xiaoxin_settings_path_test.py` | Source-path guard tests for BOOT routing, overlay touch suppression, Backlight API usage, Wi-Fi handoff, no audio settings, and power-save gating. |
| `main/CMakeLists.txt` | Add `xiaoxin_settings_model.c` to target sources for the Waveshare 1.46 board. |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc` | LVGL settings overlay, touch routing, BOOT long/short behavior, brightness application, Wi-Fi handoff, about page, optional power-save timer integration. |

---

### Task 1: Add Pure Settings Model

**Files:**
- Create: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.h`
- Create: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c`
- Create: `tests/xiaoxin_settings_model_test.c`

**Interfaces:**
- Consumes:
  - `xiaoxin_settings_caps_t caps`
  - `xiaoxin_settings_runtime_state_t runtime_state`
  - integer brightness percentages
- Produces:
  - `uint8_t xiaoxin_settings_visible_items(xiaoxin_settings_caps_t caps, xiaoxin_settings_item_t* out, uint8_t max_count)`
  - `bool xiaoxin_settings_can_open(xiaoxin_settings_runtime_state_t runtime_state)`
  - `uint8_t xiaoxin_settings_clamp_percent(int value)`
  - `const char* xiaoxin_settings_item_title(xiaoxin_settings_item_t item)`

- [x] **Step 1: Write the failing model test**

Create `tests/xiaoxin_settings_model_test.c`:

```c
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.h"

static bool contains_item(
  const xiaoxin_settings_item_t* items,
  uint8_t count,
  xiaoxin_settings_item_t expected
) {
  for (uint8_t i = 0; i < count; ++i) {
    if (items[i] == expected) {
      return true;
    }
  }
  return false;
}

static void default_target_items_exclude_audio_and_power_save(void) {
  const xiaoxin_settings_caps_t caps = {
    .has_audio_output = false,
    .has_vibration = false,
    .has_power_save_scheduler = false,
  };
  xiaoxin_settings_item_t items[8] = {};
  const uint8_t count = xiaoxin_settings_visible_items(caps, items, 8);

  assert(count == 3);
  assert(items[0] == XIAOXIN_SETTINGS_ITEM_BRIGHTNESS);
  assert(items[1] == XIAOXIN_SETTINGS_ITEM_WIFI);
  assert(items[2] == XIAOXIN_SETTINGS_ITEM_ABOUT);
  assert(!contains_item(items, count, XIAOXIN_SETTINGS_ITEM_VOLUME));
  assert(!contains_item(items, count, XIAOXIN_SETTINGS_ITEM_MUTE));
  assert(!contains_item(items, count, XIAOXIN_SETTINGS_ITEM_POWER_SAVE));
}

static void power_save_item_requires_scheduler_capability(void) {
  const xiaoxin_settings_caps_t caps = {
    .has_audio_output = false,
    .has_vibration = false,
    .has_power_save_scheduler = true,
  };
  xiaoxin_settings_item_t items[8] = {};
  const uint8_t count = xiaoxin_settings_visible_items(caps, items, 8);

  assert(count == 4);
  assert(contains_item(items, count, XIAOXIN_SETTINGS_ITEM_POWER_SAVE));
}

static void audio_items_require_audio_capability(void) {
  const xiaoxin_settings_caps_t caps = {
    .has_audio_output = true,
    .has_vibration = false,
    .has_power_save_scheduler = false,
  };
  xiaoxin_settings_item_t items[8] = {};
  const uint8_t count = xiaoxin_settings_visible_items(caps, items, 8);

  assert(contains_item(items, count, XIAOXIN_SETTINGS_ITEM_VOLUME));
  assert(contains_item(items, count, XIAOXIN_SETTINGS_ITEM_MUTE));
}

static void item_count_is_clamped_to_output_capacity(void) {
  const xiaoxin_settings_caps_t caps = {
    .has_audio_output = true,
    .has_vibration = true,
    .has_power_save_scheduler = true,
  };
  xiaoxin_settings_item_t items[2] = {};
  const uint8_t count = xiaoxin_settings_visible_items(caps, items, 2);

  assert(count == 2);
  assert(items[0] == XIAOXIN_SETTINGS_ITEM_BRIGHTNESS);
  assert(items[1] == XIAOXIN_SETTINGS_ITEM_WIFI);
}

static void only_idle_runtime_state_can_open_settings(void) {
  assert(xiaoxin_settings_can_open(XIAOXIN_SETTINGS_RUNTIME_IDLE));
  assert(!xiaoxin_settings_can_open(XIAOXIN_SETTINGS_RUNTIME_STARTING));
  assert(!xiaoxin_settings_can_open(XIAOXIN_SETTINGS_RUNTIME_CONNECTING));
  assert(!xiaoxin_settings_can_open(XIAOXIN_SETTINGS_RUNTIME_LISTENING));
  assert(!xiaoxin_settings_can_open(XIAOXIN_SETTINGS_RUNTIME_SPEAKING));
  assert(!xiaoxin_settings_can_open(XIAOXIN_SETTINGS_RUNTIME_WIFI_CONFIGURING));
  assert(!xiaoxin_settings_can_open(XIAOXIN_SETTINGS_RUNTIME_AUDIO_TESTING));
  assert(!xiaoxin_settings_can_open(XIAOXIN_SETTINGS_RUNTIME_ACTIVATING));
  assert(!xiaoxin_settings_can_open(XIAOXIN_SETTINGS_RUNTIME_UPGRADING));
  assert(!xiaoxin_settings_can_open(XIAOXIN_SETTINGS_RUNTIME_FATAL_ERROR));
}

static void brightness_percent_is_clamped(void) {
  assert(xiaoxin_settings_clamp_percent(-20) == 0);
  assert(xiaoxin_settings_clamp_percent(0) == 0);
  assert(xiaoxin_settings_clamp_percent(67) == 67);
  assert(xiaoxin_settings_clamp_percent(100) == 100);
  assert(xiaoxin_settings_clamp_percent(125) == 100);
}

static void titles_are_short_for_round_screen(void) {
  assert(strcmp(xiaoxin_settings_item_title(XIAOXIN_SETTINGS_ITEM_BRIGHTNESS), "亮度") == 0);
  assert(strcmp(xiaoxin_settings_item_title(XIAOXIN_SETTINGS_ITEM_WIFI), "Wi-Fi") == 0);
  assert(strcmp(xiaoxin_settings_item_title(XIAOXIN_SETTINGS_ITEM_POWER_SAVE), "省电") == 0);
  assert(strcmp(xiaoxin_settings_item_title(XIAOXIN_SETTINGS_ITEM_ABOUT), "关于") == 0);
}

int main(void) {
  default_target_items_exclude_audio_and_power_save();
  power_save_item_requires_scheduler_capability();
  audio_items_require_audio_capability();
  item_count_is_clamped_to_output_capacity();
  only_idle_runtime_state_can_open_settings();
  brightness_percent_is_clamped();
  titles_are_short_for_round_screen();
  puts("xiaoxin_settings_model tests passed");
  return 0;
}
```

- [x] **Step 2: Run the failing model test**

Run:

```powershell
if (!(Test-Path build)) { New-Item -ItemType Directory build | Out-Null }
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_settings_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c -o build/xiaoxin_settings_model_test.exe
```

Expected: compile fails because `xiaoxin_settings_model.h` / `.c` do not exist.

- [x] **Step 3: Create the settings model header**

Create `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.h`:

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  XIAOXIN_SETTINGS_ITEM_BRIGHTNESS = 0,
  XIAOXIN_SETTINGS_ITEM_WIFI,
  XIAOXIN_SETTINGS_ITEM_POWER_SAVE,
  XIAOXIN_SETTINGS_ITEM_ABOUT,
  XIAOXIN_SETTINGS_ITEM_VOLUME,
  XIAOXIN_SETTINGS_ITEM_MUTE,
  XIAOXIN_SETTINGS_ITEM_PROMPT_SOUND,
  XIAOXIN_SETTINGS_ITEM_VIBRATION,
} xiaoxin_settings_item_t;

typedef enum {
  XIAOXIN_SETTINGS_RUNTIME_UNKNOWN = 0,
  XIAOXIN_SETTINGS_RUNTIME_STARTING,
  XIAOXIN_SETTINGS_RUNTIME_WIFI_CONFIGURING,
  XIAOXIN_SETTINGS_RUNTIME_IDLE,
  XIAOXIN_SETTINGS_RUNTIME_CONNECTING,
  XIAOXIN_SETTINGS_RUNTIME_LISTENING,
  XIAOXIN_SETTINGS_RUNTIME_SPEAKING,
  XIAOXIN_SETTINGS_RUNTIME_UPGRADING,
  XIAOXIN_SETTINGS_RUNTIME_ACTIVATING,
  XIAOXIN_SETTINGS_RUNTIME_AUDIO_TESTING,
  XIAOXIN_SETTINGS_RUNTIME_FATAL_ERROR,
} xiaoxin_settings_runtime_state_t;

typedef struct {
  bool has_audio_output;
  bool has_vibration;
  bool has_power_save_scheduler;
} xiaoxin_settings_caps_t;

uint8_t xiaoxin_settings_visible_items(
  xiaoxin_settings_caps_t caps,
  xiaoxin_settings_item_t* out,
  uint8_t max_count
);

bool xiaoxin_settings_can_open(xiaoxin_settings_runtime_state_t runtime_state);
uint8_t xiaoxin_settings_clamp_percent(int value);
const char* xiaoxin_settings_item_title(xiaoxin_settings_item_t item);

#ifdef __cplusplus
}
#endif
```

- [x] **Step 4: Create the settings model implementation**

Create `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c`:

```c
#include "xiaoxin_settings_model.h"

static void append_item(
  xiaoxin_settings_item_t item,
  xiaoxin_settings_item_t* out,
  uint8_t max_count,
  uint8_t* count
) {
  if (out != 0 && *count < max_count) {
    out[*count] = item;
  }
  if (*count < max_count) {
    *count = (uint8_t)(*count + 1);
  }
}

uint8_t xiaoxin_settings_visible_items(
  xiaoxin_settings_caps_t caps,
  xiaoxin_settings_item_t* out,
  uint8_t max_count
) {
  uint8_t count = 0;
  append_item(XIAOXIN_SETTINGS_ITEM_BRIGHTNESS, out, max_count, &count);
  append_item(XIAOXIN_SETTINGS_ITEM_WIFI, out, max_count, &count);
  if (caps.has_power_save_scheduler) {
    append_item(XIAOXIN_SETTINGS_ITEM_POWER_SAVE, out, max_count, &count);
  }
  append_item(XIAOXIN_SETTINGS_ITEM_ABOUT, out, max_count, &count);
  if (caps.has_audio_output) {
    append_item(XIAOXIN_SETTINGS_ITEM_VOLUME, out, max_count, &count);
    append_item(XIAOXIN_SETTINGS_ITEM_MUTE, out, max_count, &count);
    append_item(XIAOXIN_SETTINGS_ITEM_PROMPT_SOUND, out, max_count, &count);
  }
  if (caps.has_vibration) {
    append_item(XIAOXIN_SETTINGS_ITEM_VIBRATION, out, max_count, &count);
  }
  return count;
}

bool xiaoxin_settings_can_open(xiaoxin_settings_runtime_state_t runtime_state) {
  return runtime_state == XIAOXIN_SETTINGS_RUNTIME_IDLE;
}

uint8_t xiaoxin_settings_clamp_percent(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return (uint8_t)value;
}

const char* xiaoxin_settings_item_title(xiaoxin_settings_item_t item) {
  switch (item) {
    case XIAOXIN_SETTINGS_ITEM_BRIGHTNESS:
      return "亮度";
    case XIAOXIN_SETTINGS_ITEM_WIFI:
      return "Wi-Fi";
    case XIAOXIN_SETTINGS_ITEM_POWER_SAVE:
      return "省电";
    case XIAOXIN_SETTINGS_ITEM_ABOUT:
      return "关于";
    case XIAOXIN_SETTINGS_ITEM_VOLUME:
      return "音量";
    case XIAOXIN_SETTINGS_ITEM_MUTE:
      return "静音";
    case XIAOXIN_SETTINGS_ITEM_PROMPT_SOUND:
      return "提示音";
    case XIAOXIN_SETTINGS_ITEM_VIBRATION:
      return "震动";
    default:
      return "";
  }
}
```

- [x] **Step 5: Run the model test**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_settings_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c -o build/xiaoxin_settings_model_test.exe
.\build\xiaoxin_settings_model_test.exe
```

Expected:

```text
xiaoxin_settings_model tests passed
```

- [x] **Step 6: Add the model to firmware sources**

Modify `main/CMakeLists.txt` in the `if(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_46)` block so the explicit helper list includes:

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c
```

Place it next to `xiaoxin_power_control.c`, `xiaoxin_system_overlay.c`, and `xiaoxin_overview_model.c` in that same block.

- [x] **Step 7: Commit Task 1**

```powershell
git add -- main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c tests/xiaoxin_settings_model_test.c main/CMakeLists.txt
git commit -m "feat: add xiaoxin settings model"
```

---

### Task 2: Add Source-Path Guards for BOOT Routing and Overlay Contract

**Files:**
- Create: `tests/xiaoxin_settings_path_test.py`

**Interfaces:**
- Consumes:
  - `esp32-s3-touch-lcd-1.46.cc` source text
  - `main/CMakeLists.txt` source text
- Produces: pytest guards that later tasks must satisfy.

- [x] **Step 1: Create failing path tests**

Create `tests/xiaoxin_settings_path_test.py`:

```python
from pathlib import Path


BOARD_SOURCE = Path("main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc")
CMAKE_SOURCE = Path("main/CMakeLists.txt")


def read_source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"function body not found: {signature}")


def section_between(source: str, start_marker: str, end_marker: str) -> str:
    start = source.index(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


def test_settings_model_is_included_and_compiled():
    board = read_source(BOARD_SOURCE)
    cmake = read_source(CMAKE_SOURCE)

    assert '#include "xiaoxin_settings_model.h"' in board
    assert "xiaoxin_settings_model.c" in cmake


def test_boot_single_click_closes_settings_before_chat_or_wifi_config():
    source = read_source(BOARD_SOURCE)
    boot_section = section_between(source, "// Boot Button", "// Power Button")

    settings_check = boot_section.index("IsSettingsOpen()")
    close_call = boot_section.index("CloseSettingsOverlay()")
    start_check = boot_section.index("kDeviceStateStarting")
    toggle_call = boot_section.index("ToggleChatState()")

    assert settings_check < close_call < start_check < toggle_call
    assert "return;" in boot_section[close_call:start_check]


def test_boot_long_press_only_opens_settings_from_idle():
    source = read_source(BOARD_SOURCE)
    boot_section = section_between(source, "// Boot Button", "// Power Button")

    assert "OpenSettingsOverlayFromBootButton()" in boot_section
    helper = function_body(source, "void OpenSettingsOverlayFromBootButton()")
    assert "Application::GetInstance()" in helper
    assert "GetDeviceState()" in helper
    assert "XIAOXIN_SETTINGS_RUNTIME_IDLE" in helper
    assert "xiaoxin_settings_can_open" in helper
    assert "OpenSettingsOverlay()" in helper
    assert "ShowNotification(\"请先结束对话\"" in helper


def test_settings_overlay_state_is_public_to_board_button_layer():
    source = read_source(BOARD_SOURCE)

    assert "bool IsSettingsOpen()" in source
    assert "void OpenSettingsOverlay()" in source
    assert "void CloseSettingsOverlay()" in source


def test_settings_touch_suppresses_card_pager_gestures():
    body = function_body(read_source(BOARD_SOURCE), "void PollTouch(uint32_t now_ms)")

    assert "if (settings_open_)" in body
    assert "HandleSettingsTouch" in body
    assert body.index("if (settings_open_)") < body.index("xiaoxin_card_pager_press")


def test_brightness_setting_uses_backlight_api_not_direct_settings_write():
    source = read_source(BOARD_SOURCE)
    body = function_body(source, "void ApplySettingsBrightness(uint8_t brightness)")

    assert "xiaoxin_settings_clamp_percent" in body
    assert "Board::GetInstance().GetBacklight()" in body
    assert "SetBrightness(clamped, true)" in body
    assert 'Settings("display"' not in body


def test_wifi_reconfiguration_reuses_existing_entrypoint():
    source = read_source(BOARD_SOURCE)
    body = function_body(source, "void RequestSettingsWifiConfig()")

    assert "CloseSettingsOverlay()" in body
    assert "EnterWifiConfigMode()" in body


def test_target_settings_caps_do_not_enable_audio_or_power_save_initially():
    body = function_body(read_source(BOARD_SOURCE), "xiaoxin_settings_caps_t SettingsCaps() const")

    assert ".has_audio_output = false" in body
    assert ".has_vibration = false" in body
    assert ".has_power_save_scheduler = false" in body


def test_about_page_uses_esp_app_description():
    source = read_source(BOARD_SOURCE)
    body = function_body(source, "void RenderSettingsAboutPage()")

    assert "#include <esp_app_desc.h>" in source
    assert "esp_app_get_description()" in body
    assert "Waveshare ESP32-S3 Touch LCD 1.46" in body
```

- [x] **Step 2: Run path tests and verify they fail**

Run:

```powershell
python -m pytest tests/xiaoxin_settings_path_test.py -q
```

Expected: failures for missing include, missing overlay methods, and missing BOOT routing changes.

- [x] **Step 3: Keep failing guards unstaged until Task 3 passes**

Do not commit failing tests alone. Carry `tests/xiaoxin_settings_path_test.py` into Task 3 and commit it once the overlay skeleton makes the new guards pass.

---

### Task 3: Build the Settings Overlay Skeleton and BOOT Routing

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Modify: `tests/xiaoxin_settings_path_test.py`

**Interfaces:**
- Consumes:
  - `xiaoxin_settings_visible_items(...)`
  - `xiaoxin_settings_can_open(...)`
  - `xiaoxin_settings_item_title(...)`
- Produces:
  - `bool PaopaoPetDisplay::IsSettingsOpen()`
  - `void PaopaoPetDisplay::OpenSettingsOverlay()`
  - `void PaopaoPetDisplay::CloseSettingsOverlay()`
  - `void CustomBoard::OpenSettingsOverlayFromBootButton()`

- [x] **Step 1: Add required includes**

In `esp32-s3-touch-lcd-1.46.cc`, add:

```cpp
#include <esp_app_desc.h>
```

Inside the existing `extern "C"` block, add:

```cpp
#include "xiaoxin_settings_model.h"
```

- [x] **Step 2: Add settings constants near the other UI constants**

Add:

```cpp
static constexpr uint8_t k_settings_item_max_count = 6;
static constexpr int16_t k_settings_panel_w = 264;
static constexpr int16_t k_settings_panel_h = 250;
static constexpr int16_t k_settings_panel_radius = 28;
static constexpr int16_t k_settings_title_y = 22;
static constexpr int16_t k_settings_row_x = 22;
static constexpr int16_t k_settings_row_y = 64;
static constexpr int16_t k_settings_row_w = 220;
static constexpr int16_t k_settings_row_h = 38;
static constexpr int16_t k_settings_row_pitch = 42;
static constexpr uint32_t k_settings_panel_bg = 0x111827;
static constexpr uint32_t k_settings_panel_border = 0x4a9eff;
static constexpr uint32_t k_settings_text_primary = 0xe8eaed;
static constexpr uint32_t k_settings_text_secondary = 0x7d9cc6;
```

- [x] **Step 3: Add settings view enum and row struct**

Near `NotificationGestureMode`, add:

```cpp
enum class SettingsView {
    List,
    Brightness,
    Wifi,
    About,
};

struct SettingsRow {
    lv_obj_t* container = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* value = nullptr;
    xiaoxin_settings_item_t item = XIAOXIN_SETTINGS_ITEM_ABOUT;
};
```

- [x] **Step 4: Add settings fields to `PaopaoPetDisplay`**

In the private fields block, add:

```cpp
lv_obj_t* settings_layer_ = nullptr;
lv_obj_t* settings_panel_ = nullptr;
lv_obj_t* settings_title_label_ = nullptr;
lv_obj_t* settings_hint_label_ = nullptr;
SettingsRow settings_rows_[k_settings_item_max_count];
xiaoxin_settings_item_t settings_items_[k_settings_item_max_count] = {};
uint8_t settings_item_count_ = 0;
SettingsView settings_view_ = SettingsView::List;
bool settings_open_ = false;
```

- [x] **Step 5: Add public overlay methods to `PaopaoPetDisplay`**

In the public section after `AttachTouch(...)`, add:

```cpp
bool IsSettingsOpen() {
    DisplayLockGuard lock(this);
    return settings_open_;
}

void OpenSettingsOverlay() {
    DisplayLockGuard lock(this);
    if (settings_open_) {
        return;
    }
    EnsureSettingsOverlayLocked();
    settings_view_ = SettingsView::List;
    settings_open_ = true;
    RenderSettingsListLocked();
    lv_obj_remove_flag(settings_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settings_layer_);
    RaiseOverlayObjects();
}

void CloseSettingsOverlay() {
    DisplayLockGuard lock(this);
    if (!settings_open_) {
        return;
    }
    settings_open_ = false;
    settings_view_ = SettingsView::List;
    AddFlagIfCreated(settings_layer_, LV_OBJ_FLAG_HIDDEN);
    RaiseOverlayObjects();
}
```

- [x] **Step 6: Add target settings capabilities**

In the private section, add:

```cpp
xiaoxin_settings_caps_t SettingsCaps() const {
    const xiaoxin_settings_caps_t caps = {
        .has_audio_output = false,
        .has_vibration = false,
        .has_power_save_scheduler = false,
    };
    return caps;
}
```

- [x] **Step 7: Add overlay creation and list rendering helpers**

Add these methods to `PaopaoPetDisplay` private section:

```cpp
void EnsureSettingsOverlayLocked() {
    if (settings_layer_ != nullptr) {
        return;
    }
    lv_obj_t* screen = lv_screen_active();
    settings_layer_ = lv_obj_create(screen);
    lv_obj_remove_style_all(settings_layer_);
    lv_obj_set_size(settings_layer_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(settings_layer_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(settings_layer_, static_cast<lv_opa_t>(118), 0);
    lv_obj_clear_flag(settings_layer_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(settings_layer_, LV_OBJ_FLAG_HIDDEN);

    settings_panel_ = lv_obj_create(settings_layer_);
    lv_obj_remove_style_all(settings_panel_);
    lv_obj_set_size(settings_panel_, k_settings_panel_w, k_settings_panel_h);
    lv_obj_set_style_radius(settings_panel_, k_settings_panel_radius, 0);
    lv_obj_set_style_bg_color(settings_panel_, lv_color_hex(k_settings_panel_bg), 0);
    lv_obj_set_style_bg_opa(settings_panel_, static_cast<lv_opa_t>(220), 0);
    lv_obj_set_style_border_width(settings_panel_, 1, 0);
    lv_obj_set_style_border_color(settings_panel_, lv_color_hex(k_settings_panel_border), 0);
    lv_obj_set_style_border_opa(settings_panel_, LV_OPA_70, 0);
    lv_obj_clear_flag(settings_panel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(settings_panel_, LV_ALIGN_CENTER, 0, 0);

    settings_title_label_ = lv_label_create(settings_panel_);
    lv_obj_set_style_text_color(settings_title_label_, lv_color_hex(k_settings_text_primary), 0);
    lv_obj_align(settings_title_label_, LV_ALIGN_TOP_MID, 0, k_settings_title_y);

    settings_hint_label_ = lv_label_create(settings_panel_);
    lv_obj_set_style_text_color(settings_hint_label_, lv_color_hex(k_settings_text_secondary), 0);
    lv_obj_align(settings_hint_label_, LV_ALIGN_BOTTOM_MID, 0, -18);

    for (uint8_t i = 0; i < k_settings_item_max_count; ++i) {
        SettingsRow& row = settings_rows_[i];
        row.container = lv_obj_create(settings_panel_);
        lv_obj_remove_style_all(row.container);
        lv_obj_set_size(row.container, k_settings_row_w, k_settings_row_h);
        lv_obj_set_style_radius(row.container, 14, 0);
        lv_obj_set_style_bg_color(row.container, lv_color_hex(0x1d3654), 0);
        lv_obj_set_style_bg_opa(row.container, static_cast<lv_opa_t>(122), 0);
        lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(row.container, LV_ALIGN_TOP_LEFT, k_settings_row_x, k_settings_row_y + i * k_settings_row_pitch);

        row.title = lv_label_create(row.container);
        lv_obj_set_style_text_color(row.title, lv_color_hex(k_settings_text_primary), 0);
        lv_obj_align(row.title, LV_ALIGN_LEFT_MID, 14, 0);

        row.value = lv_label_create(row.container);
        lv_obj_set_style_text_color(row.value, lv_color_hex(k_settings_text_secondary), 0);
        lv_obj_align(row.value, LV_ALIGN_RIGHT_MID, -14, 0);
    }
}

void HideSettingsRowsLocked() {
    for (uint8_t i = 0; i < k_settings_item_max_count; ++i) {
        AddFlagIfCreated(settings_rows_[i].container, LV_OBJ_FLAG_HIDDEN);
    }
}

void RenderSettingsListLocked() {
    EnsureSettingsOverlayLocked();
    settings_view_ = SettingsView::List;
    lv_label_set_text(settings_title_label_, "设置");
    lv_label_set_text(settings_hint_label_, "BOOT 返回");
    settings_item_count_ = xiaoxin_settings_visible_items(SettingsCaps(), settings_items_, k_settings_item_max_count);
    HideSettingsRowsLocked();
    for (uint8_t i = 0; i < settings_item_count_; ++i) {
        SettingsRow& row = settings_rows_[i];
        row.item = settings_items_[i];
        lv_label_set_text(row.title, xiaoxin_settings_item_title(row.item));
        lv_label_set_text(row.value, "›");
        lv_obj_remove_flag(row.container, LV_OBJ_FLAG_HIDDEN);
    }
}
```

- [x] **Step 8: Suppress card pager touch handling while settings is open**

At the top of `PollTouch(uint32_t now_ms)`, after touch reading has updated `pressed`, `x`, and `y`, but before notification/card pager gesture branches call `xiaoxin_card_pager_press`, add:

```cpp
if (settings_open_) {
    HandleSettingsTouch(x, y, pressed);
    touch_pressed_ = pressed;
    return;
}
```

Then add a minimal handler:

```cpp
void HandleSettingsTouch(uint16_t x, uint16_t y, bool pressed) {
    if (!pressed || touch_pressed_) {
        return;
    }
    if (settings_view_ != SettingsView::List) {
        RenderSettingsListLocked();
        return;
    }
    for (uint8_t i = 0; i < settings_item_count_; ++i) {
        if (PointInObj(settings_rows_[i].container, x, y)) {
            OpenSettingsItemLocked(settings_rows_[i].item);
            return;
        }
    }
}

void OpenSettingsItemLocked(xiaoxin_settings_item_t item) {
    switch (item) {
        case XIAOXIN_SETTINGS_ITEM_BRIGHTNESS:
            settings_view_ = SettingsView::Brightness;
            lv_label_set_text(settings_title_label_, "亮度");
            lv_label_set_text(settings_hint_label_, "30  70  100");
            HideSettingsRowsLocked();
            break;
        case XIAOXIN_SETTINGS_ITEM_WIFI:
            settings_view_ = SettingsView::Wifi;
            lv_label_set_text(settings_title_label_, "Wi-Fi");
            lv_label_set_text(settings_hint_label_, "点击重新配网");
            HideSettingsRowsLocked();
            break;
        case XIAOXIN_SETTINGS_ITEM_ABOUT:
            RenderSettingsAboutPage();
            break;
        default:
            RenderSettingsListLocked();
            break;
    }
}
```

- [x] **Step 9: Add about page renderer**

Add:

```cpp
void RenderSettingsAboutPage() {
    settings_view_ = SettingsView::About;
    EnsureSettingsOverlayLocked();
    HideSettingsRowsLocked();
    const esp_app_desc_t* app_desc = esp_app_get_description();
    char text[160] = {};
    std::snprintf(
        text,
        sizeof(text),
        "固件 %s\n%s\nWaveshare ESP32-S3 Touch LCD 1.46\n%s %s",
        app_desc != nullptr ? app_desc->version : "-",
        app_desc != nullptr ? app_desc->project_name : "ai_pet",
        app_desc != nullptr ? app_desc->date : "-",
        app_desc != nullptr ? app_desc->time : "-"
    );
    lv_label_set_text(settings_title_label_, "关于");
    lv_label_set_text(settings_hint_label_, text);
}
```

- [x] **Step 10: Wire BOOT single click and long press**

In `InitializeButtons()`, update the BOOT single-click callback to:

```cpp
iot_button_register_cb(boot_btn, BUTTON_SINGLE_CLICK, nullptr, [](void* button_handle, void* usr_data) {
    auto self = static_cast<CustomBoard*>(usr_data);
    auto* display = static_cast<PaopaoPetDisplay*>(self->display_);
    if (display != nullptr && display->IsSettingsOpen()) {
        display->CloseSettingsOverlay();
        return;
    }
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() == kDeviceStateStarting) {
        self->EnterWifiConfigMode();
        return;
    }
    app.ToggleChatState();
}, this);
```

Replace the long-press placeholder with:

```cpp
iot_button_register_cb(boot_btn, BUTTON_LONG_PRESS_START, nullptr, [](void* button_handle, void* usr_data) {
    auto self = static_cast<CustomBoard*>(usr_data);
    self->OpenSettingsOverlayFromBootButton();
}, this);
```

Add `CustomBoard::OpenSettingsOverlayFromBootButton()`:

```cpp
void OpenSettingsOverlayFromBootButton() {
    auto& app = Application::GetInstance();
    const xiaoxin_settings_runtime_state_t runtime = SettingsRuntimeState(app.GetDeviceState());
    if (!xiaoxin_settings_can_open(runtime)) {
        if (app.GetDeviceState() == kDeviceStateConnecting ||
            app.GetDeviceState() == kDeviceStateListening ||
            app.GetDeviceState() == kDeviceStateSpeaking) {
            GetDisplay()->ShowNotification("请先结束对话", 1600);
        }
        return;
    }
    auto* display = static_cast<PaopaoPetDisplay*>(display_);
    if (display != nullptr) {
        display->OpenSettingsOverlay();
    }
}

static xiaoxin_settings_runtime_state_t SettingsRuntimeState(DeviceState state) {
    switch (state) {
        case kDeviceStateStarting:
            return XIAOXIN_SETTINGS_RUNTIME_STARTING;
        case kDeviceStateWifiConfiguring:
            return XIAOXIN_SETTINGS_RUNTIME_WIFI_CONFIGURING;
        case kDeviceStateIdle:
            return XIAOXIN_SETTINGS_RUNTIME_IDLE;
        case kDeviceStateConnecting:
            return XIAOXIN_SETTINGS_RUNTIME_CONNECTING;
        case kDeviceStateListening:
            return XIAOXIN_SETTINGS_RUNTIME_LISTENING;
        case kDeviceStateSpeaking:
            return XIAOXIN_SETTINGS_RUNTIME_SPEAKING;
        case kDeviceStateUpgrading:
            return XIAOXIN_SETTINGS_RUNTIME_UPGRADING;
        case kDeviceStateActivating:
            return XIAOXIN_SETTINGS_RUNTIME_ACTIVATING;
        case kDeviceStateAudioTesting:
            return XIAOXIN_SETTINGS_RUNTIME_AUDIO_TESTING;
        case kDeviceStateFatalError:
            return XIAOXIN_SETTINGS_RUNTIME_FATAL_ERROR;
        default:
            return XIAOXIN_SETTINGS_RUNTIME_UNKNOWN;
    }
}
```

- [x] **Step 11: Run path and model tests**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_settings_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c -o build/xiaoxin_settings_model_test.exe
.\build\xiaoxin_settings_model_test.exe
python -m pytest tests/xiaoxin_settings_path_test.py -q
```

Expected:

```text
xiaoxin_settings_model tests passed
```

and pytest reports all tests in `xiaoxin_settings_path_test.py` passing.

- [x] **Step 12: Commit Task 2 and Task 3 together**

```powershell
git add -- main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc tests/xiaoxin_settings_path_test.py
git commit -m "feat: add boot settings overlay skeleton"
```

---

### Task 4: Implement Brightness Setting and Wi-Fi Reconfiguration

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Modify: `tests/xiaoxin_settings_path_test.py`

**Interfaces:**
- Consumes:
  - `uint8_t xiaoxin_settings_clamp_percent(int value)`
  - `Backlight::SetBrightness(uint8_t brightness, bool permanent)`
  - `CustomBoard::EnterWifiConfigMode()`
- Produces:
  - `void PaopaoPetDisplay::ApplySettingsBrightness(uint8_t brightness)`
  - `void CustomBoard::RequestSettingsWifiConfig()`

- [x] **Step 1: Extend path tests for brightness presets and Wi-Fi request**

Add to `tests/xiaoxin_settings_path_test.py`:

```python
def test_brightness_page_exposes_three_presets():
    body = function_body(read_source(BOARD_SOURCE), "void RenderSettingsBrightnessPage()")

    assert "30" in body
    assert "70" in body
    assert "100" in body
    assert "ApplySettingsBrightness(30)" in body
    assert "ApplySettingsBrightness(70)" in body
    assert "ApplySettingsBrightness(100)" in body


def test_wifi_page_calls_board_reconfiguration_request():
    body = function_body(read_source(BOARD_SOURCE), "void RenderSettingsWifiPage()")

    assert "重新配网" in body
    assert "CustomBoard::Instance()->RequestSettingsWifiConfig()" in body
```

- [x] **Step 2: Run tests to verify failure**

Run:

```powershell
python -m pytest tests/xiaoxin_settings_path_test.py -q
```

Expected: the two new tests fail because the render helpers are not implemented yet.

- [x] **Step 3: Implement brightness page and application helper**

In `PaopaoPetDisplay`, add:

```cpp
void RenderSettingsBrightnessPage() {
    settings_view_ = SettingsView::Brightness;
    EnsureSettingsOverlayLocked();
    HideSettingsRowsLocked();
    lv_label_set_text(settings_title_label_, "亮度");
    lv_label_set_text(settings_hint_label_, "30  70  100");
    ApplySettingsBrightness(70);
}

void ApplySettingsBrightness(uint8_t brightness) {
    const uint8_t clamped = xiaoxin_settings_clamp_percent(brightness);
    auto backlight = Board::GetInstance().GetBacklight();
    if (backlight != nullptr) {
        backlight->SetBrightness(clamped, true);
    }
}
```

Then in `OpenSettingsItemLocked`, replace the brightness branch with:

```cpp
case XIAOXIN_SETTINGS_ITEM_BRIGHTNESS:
    RenderSettingsBrightnessPage();
    break;
```

For first implementation, tapping the brightness row applies 70 as the safe middle preset. A later UI refinement can add exact hit boxes for 30 / 70 / 100 if needed; the persistence path is already exercised through `Backlight::SetBrightness(..., true)`.

- [x] **Step 4: Implement Wi-Fi page and board request**

Add `CustomBoard::RequestSettingsWifiConfig()`:

```cpp
void RequestSettingsWifiConfig() {
    auto* display = static_cast<PaopaoPetDisplay*>(display_);
    if (display != nullptr) {
        display->CloseSettingsOverlay();
    }
    EnterWifiConfigMode();
}
```

In `PaopaoPetDisplay`, add:

```cpp
void RenderSettingsWifiPage() {
    settings_view_ = SettingsView::Wifi;
    EnsureSettingsOverlayLocked();
    HideSettingsRowsLocked();
    lv_label_set_text(settings_title_label_, "Wi-Fi");
    lv_label_set_text(settings_hint_label_, "重新配网");
    if (CustomBoard::Instance() != nullptr) {
        CustomBoard::Instance()->RequestSettingsWifiConfig();
    }
}
```

Then in `OpenSettingsItemLocked`, replace the Wi-Fi branch with:

```cpp
case XIAOXIN_SETTINGS_ITEM_WIFI:
    RenderSettingsWifiPage();
    break;
```

- [x] **Step 5: Run settings tests**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_settings_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c -o build/xiaoxin_settings_model_test.exe
.\build\xiaoxin_settings_model_test.exe
python -m pytest tests/xiaoxin_settings_path_test.py -q
```

Expected: model test passes and path tests pass.

- [x] **Step 6: Commit Task 4**

```powershell
git add -- main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc tests/xiaoxin_settings_path_test.py
git commit -m "feat: add brightness and wifi settings actions"
```

---

### Task 5: Add Power-Save Scheduler Gate and Optional Setting

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Modify: `tests/xiaoxin_settings_path_test.py`

**Interfaces:**
- Consumes:
  - `PowerSaveTimer`
  - `Settings("wifi").GetBool("sleep_mode", true)`
  - `Display::SetPowerSaveMode(bool)`
- Produces:
  - target-board `PowerSaveTimer* power_save_timer_`
  - `SettingsCaps().has_power_save_scheduler = true` only after timer is initialized.

- [x] **Step 1: Add failing path test for power-save scheduler truthfulness**

Add to `tests/xiaoxin_settings_path_test.py`:

```python
def test_power_save_setting_is_only_enabled_when_timer_is_initialized():
    source = read_source(BOARD_SOURCE)
    caps_body = function_body(source, "xiaoxin_settings_caps_t SettingsCaps() const")

    assert "PowerSaveTimer* power_save_timer_" in source
    assert "InitializePowerSaveTimer()" in source
    assert ".has_power_save_scheduler = power_save_timer_ != nullptr" in caps_body
    assert "new PowerSaveTimer(-1, 60, 300)" in source
    assert "SetPowerSaveMode(true)" in source
    assert "SetPowerSaveMode(false)" in source
```

- [x] **Step 2: Run the test to verify failure**

Run:

```powershell
python -m pytest tests/xiaoxin_settings_path_test.py::test_power_save_setting_is_only_enabled_when_timer_is_initialized -q
```

Expected: failure because target board has no `PowerSaveTimer` yet.

- [x] **Step 3: Include and add timer field**

In `esp32-s3-touch-lcd-1.46.cc`, add:

```cpp
#include "power_save_timer.h"
```

In `CustomBoard` private fields, add:

```cpp
PowerSaveTimer* power_save_timer_ = nullptr;
```

- [x] **Step 4: Initialize timer with display power-save callbacks**

Add:

```cpp
void InitializePowerSaveTimer() {
    power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
    power_save_timer_->OnEnterSleepMode([this]() {
        GetDisplay()->SetPowerSaveMode(true);
    });
    power_save_timer_->OnExitSleepMode([this]() {
        GetDisplay()->SetPowerSaveMode(false);
    });
    power_save_timer_->OnShutdownRequest([this]() {
        RequestPowerOff();
    });
    power_save_timer_->SetEnabled(true);
}
```

Call it from `CustomBoard()` after `InitializeButtons();` and before `GetBacklight()->RestoreBrightness();`:

```cpp
InitializePowerSaveTimer();
```

- [x] **Step 5: Make settings capabilities reflect real scheduler**

Expose a board helper on `CustomBoard`:

```cpp
bool HasPowerSaveScheduler() const {
    return power_save_timer_ != nullptr;
}
```

Then set:

```cpp
.has_power_save_scheduler = CustomBoard::Instance() != nullptr &&
    CustomBoard::Instance()->HasPowerSaveScheduler(),
```

Keep:

```cpp
.has_audio_output = false,
.has_vibration = false,
```

- [x] **Step 6: Run settings path tests**

Run:

```powershell
python -m pytest tests/xiaoxin_settings_path_test.py -q
```

Expected: all settings path tests pass.

- [x] **Step 7: Commit Task 5**

```powershell
git add -- main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc tests/xiaoxin_settings_path_test.py
git commit -m "feat: gate settings power save with scheduler"
```

---

### Task 6: Final Verification and Firmware Build Attempt

**Files:**
- Verify only; no planned source edits.

**Interfaces:**
- Consumes all prior task outputs.
- Produces verification evidence and a final commit if any small fixes are required.

- [x] **Step 1: Run pure C model tests**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_settings_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c -o build/xiaoxin_settings_model_test.exe
.\build\xiaoxin_settings_model_test.exe
```

Expected:

```text
xiaoxin_settings_model tests passed
```

- [x] **Step 2: Run settings path tests**

Run:

```powershell
python -m pytest tests/xiaoxin_settings_path_test.py -q
```

Expected: all tests pass.

- [x] **Step 3: Run nearby regression path tests**

Run:

```powershell
python -m pytest tests/xiaoxin_notification_visual_path_test.py tests/xiaoxin_pet_mood_integration_path_test.py tests/wifi_config_status_path_test.py -q
```

Expected: all selected tests pass. If `tests/wifi_config_status_path_test.py` is still an uncommitted user file, do not edit it unless a failure directly comes from settings-page changes.

- [x] **Step 4: Run core local C regressions**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_card_pager_test.exe
.\build\xiaoxin_card_pager_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_system_overlay_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.c -o build/xiaoxin_system_overlay_test.exe
.\build\xiaoxin_system_overlay_test.exe
```

Expected:

```text
xiaoxin_card_pager tests passed
xiaoxin_system_overlay tests passed
```

- [x] **Step 5: Attempt firmware build**

Run:

```powershell
idf.py build
```

Expected: firmware builds successfully for the currently selected sdkconfig. If `idf.py` is unavailable in the environment, record the exact shell error and rely on the local GCC/pytest checks above.

- [x] **Step 6: Commit any verification-only fixes**

If verification required small fixes:

```powershell
git add -- main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c tests/xiaoxin_settings_model_test.c tests/xiaoxin_settings_path_test.py main/CMakeLists.txt
git commit -m "fix: stabilize boot settings page"
```

If no fixes are needed, do not create an empty commit.

---

## Self-Review Notes

- Spec coverage:
  - BOOT long entry and short-click interception: Tasks 2 and 3.
  - Idle-only entry and active-chat blocking: Tasks 1, 2, and 3.
  - Overlay rather than card-pager page: Task 3.
  - Brightness through Backlight API: Task 4.
  - Wi-Fi through `EnterWifiConfigMode()`: Task 4.
  - About page with app metadata: Task 3.
  - No audio/vibration on target board: Task 1 and Task 2.
  - Power-save gating before display: Task 5.
  - Notification model continues underneath: Task 3 keeps existing notification model untouched; final regression uses existing notification path tests.
- Placeholder scan: no open-ended placeholder markers or unspecified “add tests” steps.
- Type consistency:
  - `xiaoxin_settings_runtime_state_t` is defined in Task 1 and consumed by Task 3.
  - `SettingsCaps()` returns `xiaoxin_settings_caps_t` in Task 3 and is extended in Task 5.
  - `OpenSettingsOverlay()`, `CloseSettingsOverlay()`, and `IsSettingsOpen()` names match path tests and BOOT routing.
