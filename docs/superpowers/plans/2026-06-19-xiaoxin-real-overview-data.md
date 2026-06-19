# Xiaoxin Real Overview Data Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the current Overview page UI, add a real Overview data model, show time/date above the Overview cards, and replace hardcoded example card content with real or honest empty-state data.

**Architecture:** Add a small C model that turns an Overview state struct into the existing `xiaoxin_card_item_t` card shape. The LVGL page renders only the model snapshot for Overview; Notifications page cards, gestures, clearing, empty state, and pagination indicators are out of scope and must not be modified. Weather, course, and todo services are not implemented in this slice; their missing data is represented by explicit offline/unconfigured/empty text.

**Amendment:** Device status must not display an exact battery percentage in Overview. The board reads battery through ADC, so the model should use the internal estimated value only for coarse labels: `电量充足`, `电量正常`, `电量偏低`, `请尽快充电`, or `电量未知`.

**Tech Stack:** ESP-IDF C/C++, LVGL, local GCC C tests, pytest source-path tests.

---

## Scope Guard

Only touch Overview data and Overview rendering.

Do not modify these notification-specific areas:

- `RenderNotificationCards(...)`
- `GlassCard`
- notification swipe/dismiss/clear behavior
- notification empty panel
- notification indicator dots
- notification event insertion/removal logic

The only allowed `xiaoxin_card_pager.c` change is removing the static Overview example items and making `xiaoxin_card_pager_items(XIAOXIN_CARD_PAGE_OVERVIEW, ...)` return no items. Notification behavior must remain byte-for-byte equivalent unless formatting tools change whitespace.

## File Structure

| File | Responsibility | Planned Change |
| --- | --- | --- |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h` | Overview input/output contract | Create. Defines state, snapshot, constants, and build API. |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c` | Pure Overview text/model logic | Create. Builds time/date plus four Overview cards. |
| `tests/xiaoxin_overview_model_test.c` | Model regression tests | Create. Covers offline, connected-unconfigured, injected rich data, and unknown battery. |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c` | Card page state machine and notification model | Remove static Overview item array only; leave Notifications untouched. |
| `tests/xiaoxin_card_pager_test.c` | Pager behavior tests | Replace static Overview content assertions with “Overview data belongs to overview model” assertion. |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc` | LVGL rendering for Waveshare round screen | Render Overview snapshot and top time/date labels. Do not alter notification card functions. |
| `main/CMakeLists.txt` | Firmware source list | Add `xiaoxin_overview_model.c` for the Waveshare 1.46 board. |
| `tests/xiaoxin_notification_visual_path_test.py` | Source-path UI guard tests | Add Overview snapshot/time-label checks and keep notification page guard tests passing. |

---

### Task 1: Specify the Overview Model Contract

**Files:**
- Create: `tests/xiaoxin_overview_model_test.c`
- Test: `tests/xiaoxin_overview_model_test.c`

- [ ] **Step 1: Write the failing model test**

Create `tests/xiaoxin_overview_model_test.c` with this full content:

```c
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h"

static void offline_defaults_show_unsynced_weather_and_empty_sources(void) {
    xiaoxin_overview_state_t state = {
        .time_valid = false,
        .network_connected = false,
        .battery_known = true,
        .battery_percent = 78,
        .weather_configured = false,
        .weather_available = false,
        .course_configured = false,
        .course_available_today = false,
        .todo_configured = false,
        .todo_count = 0,
    };
    xiaoxin_overview_snapshot_t snapshot;

    xiaoxin_overview_model_build(&state, &snapshot);

    assert(strcmp(snapshot.time_text, "--:--") == 0);
    assert(strcmp(snapshot.date_text, "时间未同步") == 0);
    assert(snapshot.item_count == XIAOXIN_OVERVIEW_ITEM_COUNT);

    assert(strcmp(snapshot.items[0].title, "天气") == 0);
    assert(strcmp(snapshot.items[0].body, "天气未同步") == 0);
    assert(strcmp(snapshot.items[0].detail, "连接网络后更新") == 0);
    assert(strcmp(snapshot.items[0].tag, "天气") == 0);

    assert(strcmp(snapshot.items[1].title, "下一节课") == 0);
    assert(strcmp(snapshot.items[1].body, "暂无课程") == 0);
    assert(strcmp(snapshot.items[1].detail, "在配置中添加课表") == 0);
    assert(strcmp(snapshot.items[1].tag, "课程") == 0);

    assert(strcmp(snapshot.items[2].title, "今日待办") == 0);
    assert(strcmp(snapshot.items[2].body, "暂无待办") == 0);
    assert(strcmp(snapshot.items[2].detail, "添加提醒后显示") == 0);
    assert(strcmp(snapshot.items[2].tag, "待办") == 0);

    assert(strcmp(snapshot.items[3].title, "设备状态") == 0);
    assert(strcmp(snapshot.items[3].body, "离线模式") == 0);
    assert(strcmp(snapshot.items[3].detail, "电量 78%") == 0);
    assert(strcmp(snapshot.items[3].tag, "设备") == 0);
}

static void connected_without_weather_location_prompts_configuration(void) {
    xiaoxin_overview_state_t state = {
        .time_valid = false,
        .network_connected = true,
        .battery_known = true,
        .battery_percent = 55,
        .weather_configured = false,
        .weather_available = false,
        .course_configured = false,
        .todo_configured = false,
    };
    xiaoxin_overview_snapshot_t snapshot;

    xiaoxin_overview_model_build(&state, &snapshot);

    assert(strcmp(snapshot.items[0].title, "天气") == 0);
    assert(strcmp(snapshot.items[0].body, "未配置位置") == 0);
    assert(strcmp(snapshot.items[0].detail, "设置位置后显示") == 0);
    assert(strcmp(snapshot.items[3].body, "WiFi 已连接") == 0);
    assert(strcmp(snapshot.items[3].detail, "电量 55%") == 0);
}

static void rich_injected_sources_render_current_example_shape(void) {
    xiaoxin_overview_state_t state = {
        .time_valid = true,
        .hour = 14,
        .minute = 32,
        .month = 6,
        .day = 19,
        .weekday = 5,
        .network_connected = true,
        .battery_known = true,
        .battery_percent = 78,
        .weather_available = true,
        .weather_configured = true,
        .weather_summary = "多云 26C",
        .weather_detail = "湿度72% · 东风2级",
        .course_configured = true,
        .course_available_today = true,
        .course_title = "高数 10:10",
        .course_detail = "教2-301 · 还有24分",
        .todo_configured = true,
        .todo_count = 2,
        .todo_detail = "实验报告 · 晚自习",
    };
    xiaoxin_overview_snapshot_t snapshot;

    xiaoxin_overview_model_build(&state, &snapshot);

    assert(strcmp(snapshot.time_text, "14:32") == 0);
    assert(strcmp(snapshot.date_text, "6月19日 周五") == 0);

    assert(strcmp(snapshot.items[0].title, "天气") == 0);
    assert(strcmp(snapshot.items[0].body, "多云 26C") == 0);
    assert(strcmp(snapshot.items[0].detail, "湿度72% · 东风2级") == 0);

    assert(strcmp(snapshot.items[1].title, "下一节课") == 0);
    assert(strcmp(snapshot.items[1].body, "高数 10:10") == 0);
    assert(strcmp(snapshot.items[1].detail, "教2-301 · 还有24分") == 0);

    assert(strcmp(snapshot.items[2].title, "今日待办") == 0);
    assert(strcmp(snapshot.items[2].body, "2 项待办") == 0);
    assert(strcmp(snapshot.items[2].detail, "实验报告 · 晚自习") == 0);

    assert(strcmp(snapshot.items[3].title, "设备状态") == 0);
    assert(strcmp(snapshot.items[3].body, "WiFi 已连接") == 0);
    assert(strcmp(snapshot.items[3].detail, "电量 78%") == 0);
}

static void unknown_battery_outputs_unknown_detail(void) {
    xiaoxin_overview_state_t state = {
        .time_valid = false,
        .network_connected = false,
        .battery_known = false,
        .weather_configured = false,
        .course_configured = false,
        .todo_configured = false,
    };
    xiaoxin_overview_snapshot_t snapshot;

    xiaoxin_overview_model_build(&state, &snapshot);

    assert(strcmp(snapshot.items[3].title, "设备状态") == 0);
    assert(strcmp(snapshot.items[3].body, "离线模式") == 0);
    assert(strcmp(snapshot.items[3].detail, "电量未知") == 0);
}

int main(void) {
    offline_defaults_show_unsynced_weather_and_empty_sources();
    connected_without_weather_location_prompts_configuration();
    rich_injected_sources_render_current_example_shape();
    unknown_battery_outputs_unknown_detail();
    puts("xiaoxin_overview_model tests passed");
    return 0;
}
```

- [ ] **Step 2: Run the model test and confirm it fails**

Run from repo root:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_overview_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c -o build/xiaoxin_overview_model_test.exe
```

Expected result:

```text
fatal error: ../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h: No such file or directory
```

- [ ] **Step 3: Commit the failing model test**

```powershell
git add -- tests/xiaoxin_overview_model_test.c
git commit -m "test: specify xiaoxin overview data model"
```

---

### Task 2: Implement the Pure Overview Model

**Files:**
- Create: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h`
- Create: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c`
- Test: `tests/xiaoxin_overview_model_test.c`

- [ ] **Step 1: Add the Overview model header**

Create `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h` with this full content:

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "xiaoxin_card_pager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XIAOXIN_OVERVIEW_ITEM_COUNT 4
#define XIAOXIN_OVERVIEW_BODY_MAX 40
#define XIAOXIN_OVERVIEW_DETAIL_MAX 64

typedef struct {
  bool time_valid;
  int hour;
  int minute;
  int month;
  int day;
  uint8_t weekday;

  bool network_connected;
  int battery_percent;
  bool battery_known;

  bool weather_available;
  bool weather_configured;
  const char* weather_summary;
  const char* weather_detail;

  bool course_configured;
  bool course_available_today;
  const char* course_title;
  const char* course_detail;

  bool todo_configured;
  uint8_t todo_count;
  const char* todo_detail;
} xiaoxin_overview_state_t;

typedef struct {
  char time_text[8];
  char date_text[24];
  xiaoxin_card_item_t items[XIAOXIN_OVERVIEW_ITEM_COUNT];
  char body_storage[XIAOXIN_OVERVIEW_ITEM_COUNT][XIAOXIN_OVERVIEW_BODY_MAX];
  char detail_storage[XIAOXIN_OVERVIEW_ITEM_COUNT][XIAOXIN_OVERVIEW_DETAIL_MAX];
  uint8_t item_count;
} xiaoxin_overview_snapshot_t;

void xiaoxin_overview_model_build(
  const xiaoxin_overview_state_t* state,
  xiaoxin_overview_snapshot_t* snapshot
);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: Add the Overview model implementation**

Create `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c` with this full content:

```c
#include "xiaoxin_overview_model.h"

#include <stdio.h>
#include <string.h>

static const char* k_weekday_names[] = {
  "周日",
  "周一",
  "周二",
  "周三",
  "周四",
  "周五",
  "周六",
};

static bool has_text(const char* text) {
  return text != NULL && text[0] != '\0';
}

static int clamp_percent(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return value;
}

static void copy_text(char* dest, size_t dest_size, const char* text) {
  if (dest == NULL || dest_size == 0) {
    return;
  }
  snprintf(dest, dest_size, "%s", text != NULL ? text : "");
}

static void set_item(
  xiaoxin_overview_snapshot_t* snapshot,
  uint8_t index,
  const char* title,
  const char* body,
  const char* detail,
  const char* tag,
  uint32_t priority
) {
  if (snapshot == NULL || index >= XIAOXIN_OVERVIEW_ITEM_COUNT) {
    return;
  }

  copy_text(snapshot->body_storage[index], XIAOXIN_OVERVIEW_BODY_MAX, body);
  copy_text(snapshot->detail_storage[index], XIAOXIN_OVERVIEW_DETAIL_MAX, detail);

  snapshot->items[index].title = title;
  snapshot->items[index].body = snapshot->body_storage[index];
  snapshot->items[index].detail = snapshot->detail_storage[index];
  snapshot->items[index].tag = tag;
  snapshot->items[index].priority = priority;
  snapshot->items[index].ttl_ms = 0;
}

static void build_time_text(
  const xiaoxin_overview_state_t* state,
  xiaoxin_overview_snapshot_t* snapshot
) {
  if (state != NULL && state->time_valid) {
    const uint8_t weekday = state->weekday <= 6 ? state->weekday : 0;
    snprintf(snapshot->time_text, sizeof(snapshot->time_text), "%02d:%02d", state->hour, state->minute);
    snprintf(
      snapshot->date_text,
      sizeof(snapshot->date_text),
      "%d月%d日 %s",
      state->month,
      state->day,
      k_weekday_names[weekday]
    );
    return;
  }

  copy_text(snapshot->time_text, sizeof(snapshot->time_text), "--:--");
  copy_text(snapshot->date_text, sizeof(snapshot->date_text), "时间未同步");
}

static void build_weather_item(
  const xiaoxin_overview_state_t* state,
  xiaoxin_overview_snapshot_t* snapshot
) {
  if (state == NULL || !state->network_connected) {
    set_item(snapshot, 0, "天气", "天气未同步", "连接网络后更新", "天气", 1);
    return;
  }

  if (!state->weather_configured) {
    set_item(snapshot, 0, "天气", "未配置位置", "设置位置后显示", "天气", 1);
    return;
  }

  if (state->weather_available && has_text(state->weather_summary)) {
    set_item(
      snapshot,
      0,
      "天气",
      state->weather_summary,
      has_text(state->weather_detail) ? state->weather_detail : "天气详情待同步",
      "天气",
      1
    );
    return;
  }

  set_item(snapshot, 0, "天气", "天气未同步", "连接网络后更新", "天气", 1);
}

static void build_course_item(
  const xiaoxin_overview_state_t* state,
  xiaoxin_overview_snapshot_t* snapshot
) {
  if (state == NULL || !state->course_configured) {
    set_item(snapshot, 1, "下一节课", "暂无课程", "在配置中添加课表", "课程", 2);
    return;
  }

  if (!state->course_available_today) {
    set_item(snapshot, 1, "下一节课", "今日无课", "可以安排自习", "课程", 2);
    return;
  }

  set_item(
    snapshot,
    1,
    "下一节课",
    has_text(state->course_title) ? state->course_title : "课程待确认",
    has_text(state->course_detail) ? state->course_detail : "查看课表详情",
    "课程",
    2
  );
}

static void build_todo_item(
  const xiaoxin_overview_state_t* state,
  xiaoxin_overview_snapshot_t* snapshot
) {
  if (state == NULL || !state->todo_configured || state->todo_count == 0) {
    set_item(snapshot, 2, "今日待办", "暂无待办", "添加提醒后显示", "待办", 3);
    return;
  }

  char body[XIAOXIN_OVERVIEW_BODY_MAX];
  snprintf(body, sizeof(body), "%u 项待办", (unsigned)state->todo_count);
  set_item(
    snapshot,
    2,
    "今日待办",
    body,
    has_text(state->todo_detail) ? state->todo_detail : "打开待办查看",
    "待办",
    3
  );
}

static void build_device_item(
  const xiaoxin_overview_state_t* state,
  xiaoxin_overview_snapshot_t* snapshot
) {
  char detail[XIAOXIN_OVERVIEW_DETAIL_MAX];
  const bool battery_known = state != NULL && state->battery_known;
  if (battery_known) {
    snprintf(detail, sizeof(detail), "电量 %d%%", clamp_percent(state->battery_percent));
  } else {
    copy_text(detail, sizeof(detail), "电量未知");
  }

  set_item(
    snapshot,
    3,
    "设备状态",
    state != NULL && state->network_connected ? "WiFi 已连接" : "离线模式",
    detail,
    "设备",
    4
  );
}

void xiaoxin_overview_model_build(
  const xiaoxin_overview_state_t* state,
  xiaoxin_overview_snapshot_t* snapshot
) {
  if (snapshot == NULL) {
    return;
  }

  memset(snapshot, 0, sizeof(*snapshot));
  build_time_text(state, snapshot);
  build_weather_item(state, snapshot);
  build_course_item(state, snapshot);
  build_todo_item(state, snapshot);
  build_device_item(state, snapshot);
  snapshot->item_count = XIAOXIN_OVERVIEW_ITEM_COUNT;
}
```

- [ ] **Step 3: Run the model test and confirm it passes**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_overview_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c -o build/xiaoxin_overview_model_test.exe
.\build\xiaoxin_overview_model_test.exe
```

Expected output:

```text
xiaoxin_overview_model tests passed
```

- [ ] **Step 4: Commit the model**

```powershell
git add -- main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c tests/xiaoxin_overview_model_test.c
git commit -m "feat: add xiaoxin overview data model"
```

---

### Task 3: Remove Static Overview Example Data From the Pager

**Files:**
- Modify: `tests/xiaoxin_card_pager_test.c`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`
- Test: `tests/xiaoxin_card_pager_test.c`

- [ ] **Step 1: Replace static Overview content tests**

In `tests/xiaoxin_card_pager_test.c`, replace the current `overview_items_are_priority_sorted()` and `overview_items_include_main_and_detail_text()` functions with:

```c
static void overview_items_are_owned_by_overview_model(void) {
    const xiaoxin_card_item_t* items = NULL;
    uint8_t count = 0;

    xiaoxin_card_pager_items(XIAOXIN_CARD_PAGE_NOTIFICATIONS, &items, &count);
    assert(count == 0);
    assert(items == NULL);

    xiaoxin_card_pager_items(XIAOXIN_CARD_PAGE_OVERVIEW, &items, &count);
    assert(count == 0);
    assert(items == NULL);
}
```

In `main()`, replace:

```c
    overview_items_are_priority_sorted();
    overview_items_include_main_and_detail_text();
```

with:

```c
    overview_items_are_owned_by_overview_model();
```

- [ ] **Step 2: Run the pager test and confirm it fails**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_card_pager_test.exe
.\build\xiaoxin_card_pager_test.exe
```

Expected result:

```text
Assertion failed: count == 0
```

- [ ] **Step 3: Remove only the static Overview array and Overview branch**

In `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`, delete the full `k_overview_items` definition:

```c
static const xiaoxin_card_item_t k_overview_items[] = {
  {"下一节课", "高数 10:10", "教2-301 · 还有24分", "课程", 1, 0},
  {"校园导航", "常用地点", "教学楼 / 食堂 / 图书馆", "导航", 2, 0},
  {"天气", "多云 26C", "湿度72% · 东风2级", "天气", 3, 0},
  {"今日待办", "2 项待办", "实验报告 · 晚自习", "待办", 4, 0},
};
```

Then replace the body of `xiaoxin_card_pager_items(...)` with:

```c
void xiaoxin_card_pager_items(
  xiaoxin_card_page_t page,
  const xiaoxin_card_item_t** items,
  uint8_t* count
) {
  if (items == NULL || count == NULL) {
    return;
  }

  (void)page;
  *items = NULL;
  *count = 0;
}
```

This intentionally leaves notification insertion, sorting, dismiss, clear, and pagination functions unchanged.

- [ ] **Step 4: Run the pager test and confirm it passes**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_card_pager_test.exe
.\build\xiaoxin_card_pager_test.exe
```

Expected output:

```text
xiaoxin_card_pager tests passed
```

- [ ] **Step 5: Commit the pager cleanup**

```powershell
git add -- tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c
git commit -m "refactor: move xiaoxin overview data out of pager"
```

---

### Task 4: Add Overview UI Source-Path Guards

**Files:**
- Modify: `tests/xiaoxin_notification_visual_path_test.py`
- Test: `tests/xiaoxin_notification_visual_path_test.py`

- [ ] **Step 1: Replace the Overview title test**

In `tests/xiaoxin_notification_visual_path_test.py`, replace `test_overview_title_uses_black_and_notifications_page_has_no_title()` with:

```python
def test_overview_uses_time_labels_and_keeps_notifications_page_titleless():
    source = read_source()
    body = function_body(source, "void RenderCardPage(xiaoxin_card_page_t page, bool prepare_entry_animation = false)")

    assert "static constexpr uint32_t k_page_title_color = 0x111111;" in source
    assert "lv_obj_t* overview_time_label_ = nullptr;" in source
    assert "lv_obj_t* overview_date_label_ = nullptr;" in source
    assert "xiaoxin_overview_snapshot_t overview_snapshot_ = {};" in source
    assert "lv_obj_set_style_text_color(overview_time_label_, lv_color_hex(k_page_title_color), 0);" in source
    assert "lv_label_set_text(overview_time_label_, overview_snapshot_.time_text);" in body
    assert "lv_label_set_text(overview_date_label_, overview_snapshot_.date_text);" in body
    assert 'lv_label_set_text(card_title_label_, "\\xE9\\x80\\x9A\\xE7\\x9F\\xA5");' not in body
    assert 'lv_label_set_text(card_title_label_, "\\xE6\\x80\\xBB\\xE8\\xA7\\x88");' not in body
```

- [ ] **Step 2: Add a guard that Overview no longer reads pager example items**

Append this test near the other Overview source-path tests:

```python
def test_overview_page_consumes_overview_model_snapshot():
    source = read_source()
    body = function_body(source, "void RenderCardPage(xiaoxin_card_page_t page, bool prepare_entry_animation = false)")

    assert '#include "xiaoxin_overview_model.h"' in source
    assert "xiaoxin_overview_state_t overview_state = BuildOverviewState();" in body
    assert "xiaoxin_overview_model_build(&overview_state, &overview_snapshot_);" in body
    assert "const xiaoxin_card_item_t* items = overview_snapshot_.items;" in body
    assert "const uint8_t count = overview_snapshot_.item_count;" in body
    assert "xiaoxin_card_pager_items(page, &items, &count);" not in body
```

- [ ] **Step 3: Add a guard that time labels are hidden outside Overview**

Append:

```python
def test_overview_time_labels_are_hidden_before_page_specific_rendering():
    body = function_body(
        read_source(),
        "void RenderCardPage(xiaoxin_card_page_t page, bool prepare_entry_animation = false)",
    )

    assert "AddFlagIfCreated(overview_time_label_, LV_OBJ_FLAG_HIDDEN);" in body
    assert "AddFlagIfCreated(overview_date_label_, LV_OBJ_FLAG_HIDDEN);" in body
```

- [ ] **Step 4: Run the source-path tests and confirm they fail**

```powershell
python -m pytest tests/xiaoxin_notification_visual_path_test.py -q
```

Expected result:

```text
FAILED tests/xiaoxin_notification_visual_path_test.py::test_overview_uses_time_labels_and_keeps_notifications_page_titleless
FAILED tests/xiaoxin_notification_visual_path_test.py::test_overview_page_consumes_overview_model_snapshot
FAILED tests/xiaoxin_notification_visual_path_test.py::test_overview_time_labels_are_hidden_before_page_specific_rendering
```

- [ ] **Step 5: Commit the failing source-path tests**

```powershell
git add -- tests/xiaoxin_notification_visual_path_test.py
git commit -m "test: specify real overview page rendering path"
```

---

### Task 5: Render the Overview Snapshot and Top Time Area

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Modify: `main/CMakeLists.txt`
- Test: `tests/xiaoxin_notification_visual_path_test.py`
- Test: `tests/xiaoxin_overview_model_test.c`
- Test: `tests/xiaoxin_card_pager_test.c`

- [ ] **Step 1: Register the model source in CMake**

In `main/CMakeLists.txt`, inside the existing block:

```cmake
if(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_46)
    list(APPEND SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_power_control.c
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c
    )
```

change it to:

```cmake
if(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_46)
    list(APPEND SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_power_control.c
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_system_overlay.c
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c
    )
```

- [ ] **Step 2: Add includes to the board file**

In `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`, add:

```cpp
#include <time.h>
```

near the other system includes.

Inside the existing `extern "C"` block, add:

```cpp
#include "xiaoxin_overview_model.h"
```

immediately after:

```cpp
#include "xiaoxin_card_pager.h"
```

- [ ] **Step 3: Add Overview time layout constants**

Near the existing Overview layout constants, replace:

```cpp
static constexpr int16_t k_overview_y_start = 70;
static constexpr int16_t k_overview_row_pitch = 61;
```

with:

```cpp
static constexpr int16_t k_overview_time_y = 26;
static constexpr int16_t k_overview_date_y = 49;
static constexpr int16_t k_overview_time_w = 260;
static constexpr int16_t k_overview_y_start = 78;
static constexpr int16_t k_overview_row_pitch = 58;
```

- [ ] **Step 4: Add Overview label and snapshot fields**

In the private fields near `card_title_label_`, add:

```cpp
    lv_obj_t* overview_time_label_ = nullptr;
    lv_obj_t* overview_date_label_ = nullptr;
```

Near `xiaoxin_card_pager_t card_pager_ = {};`, add:

```cpp
    xiaoxin_overview_snapshot_t overview_snapshot_ = {};
```

- [ ] **Step 5: Create the top time/date labels**

In `InitializeCardPagerLayer()`, immediately after the existing `card_title_label_` setup:

```cpp
        card_title_label_ = lv_label_create(card_layer_);
        lv_obj_set_width(card_title_label_, 260);
        lv_obj_set_style_text_align(card_title_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(card_title_label_, lv_color_hex(k_page_title_color), 0);
        lv_obj_set_style_text_opa(card_title_label_, LV_OPA_COVER, 0);
        lv_label_set_text(card_title_label_, "");
        lv_obj_add_flag(card_title_label_, LV_OBJ_FLAG_HIDDEN);
```

add:

```cpp
        overview_time_label_ = lv_label_create(card_layer_);
        lv_obj_set_width(overview_time_label_, k_overview_time_w);
        lv_obj_set_style_text_align(overview_time_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(overview_time_label_, lv_color_hex(k_page_title_color), 0);
        lv_obj_set_style_text_opa(overview_time_label_, LV_OPA_COVER, 0);
        lv_label_set_text(overview_time_label_, "");
        lv_obj_align(overview_time_label_, LV_ALIGN_TOP_MID, 0, k_overview_time_y);
        lv_obj_add_flag(overview_time_label_, LV_OBJ_FLAG_HIDDEN);

        overview_date_label_ = lv_label_create(card_layer_);
        lv_obj_set_width(overview_date_label_, k_overview_time_w);
        lv_obj_set_style_text_align(overview_date_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(overview_date_label_, lv_color_hex(k_text_dimmed), 0);
        lv_obj_set_style_text_opa(overview_date_label_, LV_OPA_COVER, 0);
        lv_label_set_text(overview_date_label_, "");
        lv_obj_align(overview_date_label_, LV_ALIGN_TOP_MID, 0, k_overview_date_y);
        lv_obj_add_flag(overview_date_label_, LV_OBJ_FLAG_HIDDEN);
```

- [ ] **Step 6: Add Overview state builders**

Inside the `PaopaoPetDisplay` private section, after `NotificationBatteryLevelPercent()`, add:

```cpp
    static void PopulateOverviewTime(xiaoxin_overview_state_t& state) {
        time_t now = 0;
        time(&now);

        struct tm timeinfo = {};
        if (now > 24 * 60 * 60 &&
            localtime_r(&now, &timeinfo) != nullptr &&
            timeinfo.tm_year >= 120) {
            state.time_valid = true;
            state.hour = timeinfo.tm_hour;
            state.minute = timeinfo.tm_min;
            state.month = timeinfo.tm_mon + 1;
            state.day = timeinfo.tm_mday;
            state.weekday = (uint8_t)timeinfo.tm_wday;
        }
    }

    static bool ReadOverviewBattery(int& level) {
        bool charging = false;
        bool discharging = true;
        if (!Board::GetInstance().GetBatteryLevel(level, charging, discharging)) {
            return false;
        }
        level = std::min(100, std::max(0, level));
        return true;
    }

    xiaoxin_overview_state_t BuildOverviewState() {
        xiaoxin_overview_state_t state = {};
        PopulateOverviewTime(state);

        state.network_connected = SystemOverlayNetworkState() == XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED;
        state.battery_known = ReadOverviewBattery(state.battery_percent);

        state.weather_configured = false;
        state.weather_available = false;
        state.weather_summary = nullptr;
        state.weather_detail = nullptr;

        state.course_configured = false;
        state.course_available_today = false;
        state.course_title = nullptr;
        state.course_detail = nullptr;

        state.todo_configured = false;
        state.todo_count = 0;
        state.todo_detail = nullptr;

        return state;
    }
```

This keeps weather, course, and todo as honest empty-state inputs until real providers exist.

- [ ] **Step 7: Hide Overview time labels before page-specific rendering**

In `RenderCardPage(...)`, near the existing object-hide block:

```cpp
        AddFlagIfCreated(card_title_label_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(pull_indicator_, LV_OBJ_FLAG_HIDDEN);
```

change it to:

```cpp
        AddFlagIfCreated(card_title_label_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(overview_time_label_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(overview_date_label_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(pull_indicator_, LV_OBJ_FLAG_HIDDEN);
```

- [ ] **Step 8: Make Overview render from the snapshot**

In `RenderCardPage(...)`, remove this pre-branch block:

```cpp
        const xiaoxin_card_item_t* items = nullptr;
        uint8_t count = 0;
        xiaoxin_card_pager_items(page, &items, &count);
```

Then replace the start of the `XIAOXIN_CARD_PAGE_OVERVIEW` branch:

```cpp
        } else if (page == XIAOXIN_CARD_PAGE_OVERVIEW) {
            if (card_title_label_ != nullptr) {
                lv_obj_set_width(card_title_label_, 260);
                lv_obj_set_style_text_align(card_title_label_, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_align(card_title_label_, LV_ALIGN_TOP_MID, 0, 34);
                lv_label_set_text(card_title_label_, "\xE6\x80\xBB\xE8\xA7\x88");
                lv_obj_remove_flag(card_title_label_, LV_OBJ_FLAG_HIDDEN);
            }
```

with:

```cpp
        } else if (page == XIAOXIN_CARD_PAGE_OVERVIEW) {
            xiaoxin_overview_state_t overview_state = BuildOverviewState();
            xiaoxin_overview_model_build(&overview_state, &overview_snapshot_);

            if (overview_time_label_ != nullptr) {
                lv_label_set_text(overview_time_label_, overview_snapshot_.time_text);
                lv_obj_align(overview_time_label_, LV_ALIGN_TOP_MID, 0, k_overview_time_y);
                lv_obj_remove_flag(overview_time_label_, LV_OBJ_FLAG_HIDDEN);
            }
            if (overview_date_label_ != nullptr) {
                lv_label_set_text(overview_date_label_, overview_snapshot_.date_text);
                lv_obj_align(overview_date_label_, LV_ALIGN_TOP_MID, 0, k_overview_date_y);
                lv_obj_remove_flag(overview_date_label_, LV_OBJ_FLAG_HIDDEN);
            }
```

Immediately before the Overview row loop, add:

```cpp
            const xiaoxin_card_item_t* items = overview_snapshot_.items;
            const uint8_t count = overview_snapshot_.item_count;
```

Keep the existing row rendering loop intact after that.

- [ ] **Step 9: Run the source-path tests**

```powershell
python -m pytest tests/xiaoxin_notification_visual_path_test.py -q
```

Expected output:

```text
13 passed
```

- [ ] **Step 10: Run the local C tests**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_overview_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c -o build/xiaoxin_overview_model_test.exe
.\build\xiaoxin_overview_model_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_card_pager_test.exe
.\build\xiaoxin_card_pager_test.exe
```

Expected output:

```text
xiaoxin_overview_model tests passed
xiaoxin_card_pager tests passed
```

- [ ] **Step 11: Commit the UI wiring**

```powershell
git add -- main/CMakeLists.txt main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc tests/xiaoxin_notification_visual_path_test.py
git commit -m "feat: render real xiaoxin overview snapshot"
```

---

### Task 6: Final Verification

**Files:**
- Verify only

- [ ] **Step 1: Run all local Overview and pager checks**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_overview_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c -o build/xiaoxin_overview_model_test.exe
.\build\xiaoxin_overview_model_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_card_pager_test.exe
.\build\xiaoxin_card_pager_test.exe
python -m pytest tests/xiaoxin_card_pager_threshold_test.py tests/xiaoxin_notification_visual_path_test.py -q
```

Expected output includes:

```text
xiaoxin_overview_model tests passed
xiaoxin_card_pager tests passed
```

and pytest reports all selected tests passing.

- [ ] **Step 2: Run the firmware build if ESP-IDF is available in the shell**

```powershell
idf.py build
```

Expected result:

```text
Project build complete.
```

If `idf.py` is not available, record that limitation in the final handoff and rely on the local GCC and pytest checks above.

- [ ] **Step 3: Inspect the diff for notification page drift**

```powershell
git diff main -- main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c
```

Confirm the diff does not change these symbols:

```text
RenderNotificationCards
NotificationGestureMode
notification_clear_button_
notification_empty_panel_
notification_indicator_dots_
xiaoxin_card_pager_notification_upsert_event
xiaoxin_card_pager_notification_dismiss
xiaoxin_card_pager_notification_clear_all
```

- [ ] **Step 4: Commit verification notes only if docs are updated**

If implementation adds a short note to `docs/update.md`, commit it separately:

```powershell
git add -- docs/update.md
git commit -m "docs: note real xiaoxin overview data path"
```

Do not create this commit if `docs/update.md` is unchanged.
