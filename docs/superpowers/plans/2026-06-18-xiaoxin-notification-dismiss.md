# Xiaoxin Notification Dismiss Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add phone-like notification management to the Xiaoxin Notifications page: left-swipe the touched card to dismiss it, and tap a prominent `全部清理` button to clear all notifications.

**Architecture:** Keep page navigation in `xiaoxin_card_pager` unchanged, but add a small stateful notification-dismiss model on top of the current static notification array. The LVGL board UI will map visible notification indices to card slots, arbitrate horizontal dismiss gestures against vertical scrolling, animate one card flying left, and render an empty state when all notifications are cleared.

**Tech Stack:** ESP-IDF, C/C++17, LVGL 9.x, existing C pager test harness.

## Global Constraints

- Hand hit rule: the notification card pressed by the finger is the card dismissed by a left swipe.
- Preserve vertical notification scrolling and outer page return gestures.
- Left swipe dismisses; right swipe has no action in this version.
- `全部清理` clears all local notifications and shows `暂无通知`.
- Keep `xiaoxin_card_pager.c` responsible for notification state and tests; keep LVGL animation/touch handling in `esp32-s3-touch-lcd-1.46.cc`.
- Do not use real blur or expensive drag-time shadow/layout changes.
- Do not claim `idf.py build` or device validation passed unless those commands actually ran.

---

## File Map

| File | Responsibility | Planned change |
| --- | --- | --- |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h` | Public pager and notification-state API | Add dismiss/clear APIs and dismiss mask field |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c` | Notification state and visible-index mapping | Implement visible count, item lookup, dismiss, clear-all, empty |
| `tests/xiaoxin_card_pager_test.c` | Pure C regression tests | Add tests for dismiss, clear all, index clamping, invalid dismiss |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc` | LVGL UI, touch arbitration, animations | Add clear-all button, empty state, hit testing, left-swipe fly-out |
| `docs/xiaoxin-vertical-card-pager-plan.zh-CN.md` | Human-facing project notes | Record the notification dismiss interaction after implementation |

---

### Task 1: Notification dismiss state model

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`
- Test: `tests/xiaoxin_card_pager_test.c`

**Interfaces:**
- Produces:
  - `const xiaoxin_card_item_t* xiaoxin_card_pager_notification_at(const xiaoxin_card_pager_t* pager, uint8_t visible_index);`
  - `bool xiaoxin_card_pager_notification_dismiss(xiaoxin_card_pager_t* pager, uint8_t visible_index);`
  - `void xiaoxin_card_pager_notification_clear_all(xiaoxin_card_pager_t* pager);`
  - `bool xiaoxin_card_pager_notification_empty(const xiaoxin_card_pager_t* pager);`
- Changes:
  - `xiaoxin_card_pager_notification_count()` returns visible, not raw, notification count.
  - `notification_index` remains a visible index and is clamped after dismissal.

- [ ] **Step 1: Write failing tests**

Add these tests to `tests/xiaoxin_card_pager_test.c`:

```c
static void notification_dismiss_removes_visible_item(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    assert(xiaoxin_card_pager_notification_count(&pager) == 4);
    const xiaoxin_card_item_t* before = xiaoxin_card_pager_notification_at(&pager, 2);
    assert(before != NULL);

    assert(xiaoxin_card_pager_notification_dismiss(&pager, 1));
    assert(xiaoxin_card_pager_notification_count(&pager) == 3);
    const xiaoxin_card_item_t* after = xiaoxin_card_pager_notification_at(&pager, 1);
    assert(after == before);
}

static void notification_dismiss_current_item_clamps_index(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    assert(xiaoxin_card_pager_notification_next(&pager));
    assert(xiaoxin_card_pager_notification_next(&pager));
    assert(xiaoxin_card_pager_notification_next(&pager));
    assert(xiaoxin_card_pager_notification_index(&pager) == 3);

    assert(xiaoxin_card_pager_notification_dismiss(&pager, 3));
    assert(xiaoxin_card_pager_notification_count(&pager) == 3);
    assert(xiaoxin_card_pager_notification_index(&pager) == 2);
    assert(xiaoxin_card_pager_current_notification(&pager) != NULL);
}

static void notification_clear_all_empties_notifications(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    xiaoxin_card_pager_notification_clear_all(&pager);

    assert(xiaoxin_card_pager_notification_empty(&pager));
    assert(xiaoxin_card_pager_notification_count(&pager) == 0);
    assert(xiaoxin_card_pager_notification_index(&pager) == 0);
    assert(xiaoxin_card_pager_current_notification(&pager) == NULL);
    assert(!xiaoxin_card_pager_notification_next(&pager));
    assert(!xiaoxin_card_pager_notification_prev(&pager));
}

static void notification_dismiss_invalid_index_returns_false(void) {
    xiaoxin_card_pager_t pager;
    xiaoxin_card_pager_init(&pager, 412);

    assert(!xiaoxin_card_pager_notification_dismiss(&pager, 4));
    assert(xiaoxin_card_pager_notification_count(&pager) == 4);
}
```

Call them from `main()` after `notification_pagination_tracks_current_item()`.

- [ ] **Step 2: Run tests and confirm failure**

Run:

```powershell
gcc -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o tests/xiaoxin_card_pager_test.exe
.\tests\xiaoxin_card_pager_test.exe
```

Expected: compile failure for missing function declarations or link failure for missing implementations.

- [ ] **Step 3: Add API declarations and state field**

In `xiaoxin_card_pager.h`, add to `xiaoxin_card_pager_t`:

```c
uint8_t notification_dismissed_mask;
```

Add declarations:

```c
const xiaoxin_card_item_t* xiaoxin_card_pager_notification_at(
  const xiaoxin_card_pager_t* pager,
  uint8_t visible_index
);
bool xiaoxin_card_pager_notification_dismiss(
  xiaoxin_card_pager_t* pager,
  uint8_t visible_index
);
void xiaoxin_card_pager_notification_clear_all(xiaoxin_card_pager_t* pager);
bool xiaoxin_card_pager_notification_empty(const xiaoxin_card_pager_t* pager);
```

- [ ] **Step 4: Implement visible-index mapping**

In `xiaoxin_card_pager.c`, initialize:

```c
pager->notification_dismissed_mask = 0;
```

Add helpers:

```c
static bool notification_raw_dismissed(const xiaoxin_card_pager_t* pager, uint8_t raw_index) {
  return pager != NULL && (pager->notification_dismissed_mask & (uint8_t)(1u << raw_index)) != 0;
}

static uint8_t notification_visible_count_for(const xiaoxin_card_pager_t* pager) {
  uint8_t count = 0;
  const uint8_t total = notification_item_count();
  for (uint8_t raw = 0; raw < total; ++raw) {
    if (!notification_raw_dismissed(pager, raw)) {
      count++;
    }
  }
  return count;
}

static int8_t notification_raw_index_for_visible(const xiaoxin_card_pager_t* pager, uint8_t visible_index) {
  uint8_t visible = 0;
  const uint8_t total = notification_item_count();
  for (uint8_t raw = 0; raw < total; ++raw) {
    if (notification_raw_dismissed(pager, raw)) {
      continue;
    }
    if (visible == visible_index) {
      return (int8_t)raw;
    }
    visible++;
  }
  return -1;
}

static void clamp_notification_index(xiaoxin_card_pager_t* pager) {
  if (pager == NULL) {
    return;
  }
  const uint8_t visible = notification_visible_count_for(pager);
  if (visible == 0) {
    pager->notification_index = 0;
  } else if (pager->notification_index >= visible) {
    pager->notification_index = (uint8_t)(visible - 1);
  }
}
```

- [ ] **Step 5: Update notification APIs**

Implement:

```c
uint8_t xiaoxin_card_pager_notification_count(const xiaoxin_card_pager_t* pager) {
  return pager != NULL ? notification_visible_count_for(pager) : notification_item_count();
}

const xiaoxin_card_item_t* xiaoxin_card_pager_notification_at(
  const xiaoxin_card_pager_t* pager,
  uint8_t visible_index
) {
  const int8_t raw = notification_raw_index_for_visible(pager, visible_index);
  if (raw < 0 || raw >= (int8_t)notification_item_count()) {
    return NULL;
  }
  return &k_notification_items[raw];
}

bool xiaoxin_card_pager_notification_dismiss(xiaoxin_card_pager_t* pager, uint8_t visible_index) {
  if (pager == NULL) {
    return false;
  }
  const int8_t raw = notification_raw_index_for_visible(pager, visible_index);
  if (raw < 0) {
    return false;
  }
  pager->notification_dismissed_mask |= (uint8_t)(1u << (uint8_t)raw);
  clamp_notification_index(pager);
  return true;
}

void xiaoxin_card_pager_notification_clear_all(xiaoxin_card_pager_t* pager) {
  if (pager == NULL) {
    return;
  }
  const uint8_t total = notification_item_count();
  pager->notification_dismissed_mask = (uint8_t)((1u << total) - 1u);
  pager->notification_index = 0;
}

bool xiaoxin_card_pager_notification_empty(const xiaoxin_card_pager_t* pager) {
  return xiaoxin_card_pager_notification_count(pager) == 0;
}
```

Update `next`, `prev`, and `current_notification` to use visible count and `notification_at`.

- [ ] **Step 6: Run tests and commit**

Run:

```powershell
gcc -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o tests/xiaoxin_card_pager_test.exe
.\tests\xiaoxin_card_pager_test.exe
```

Expected:

```text
xiaoxin_card_pager tests passed
```

Commit:

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c tests/xiaoxin_card_pager_test.c
git commit -m "feat: add dismissible notification state"
```

---

### Task 2: Render visible notifications, clear-all button, and empty state

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**Interfaces:**
- Consumes Task 1 APIs:
  - `xiaoxin_card_pager_notification_at(const xiaoxin_card_pager_t* pager, uint8_t visible_index)`
  - `xiaoxin_card_pager_notification_count(const xiaoxin_card_pager_t* pager)`
  - `xiaoxin_card_pager_notification_empty(const xiaoxin_card_pager_t* pager)`

- [ ] **Step 1: Add constants and object fields**

Add constants near notification constants:

```cpp
static constexpr int16_t k_notification_clear_button_w = 104;
static constexpr int16_t k_notification_clear_button_h = 32;
static constexpr int16_t k_notification_clear_button_y = 46;
static constexpr int16_t k_notification_empty_y = 188;
```

Add fields:

```cpp
lv_obj_t* notification_clear_button_ = nullptr;
lv_obj_t* notification_clear_label_ = nullptr;
lv_obj_t* notification_empty_label_ = nullptr;
```

Add to `GlassCard`:

```cpp
uint8_t visible_index = 0xff;
```

- [ ] **Step 2: Create clear-all button and empty label**

In `InitializeCardPagerLayer()`, create:

```cpp
notification_clear_button_ = lv_obj_create(card_layer_);
lv_obj_remove_style_all(notification_clear_button_);
lv_obj_set_size(notification_clear_button_, k_notification_clear_button_w, k_notification_clear_button_h);
lv_obj_set_style_radius(notification_clear_button_, 16, 0);
lv_obj_set_style_bg_color(notification_clear_button_, lv_color_hex(k_tag_bg), 0);
lv_obj_set_style_bg_opa(notification_clear_button_, LV_OPA_COVER, 0);
lv_obj_set_style_border_color(notification_clear_button_, lv_color_hex(k_title_accent), 0);
lv_obj_set_style_border_opa(notification_clear_button_, LV_OPA_70, 0);
lv_obj_set_style_border_width(notification_clear_button_, 1, 0);
lv_obj_align(notification_clear_button_, LV_ALIGN_TOP_MID, 0, k_notification_clear_button_y);
lv_obj_add_flag(notification_clear_button_, LV_OBJ_FLAG_HIDDEN);

notification_clear_label_ = lv_label_create(notification_clear_button_);
lv_obj_set_style_text_color(notification_clear_label_, lv_color_hex(k_text_primary), 0);
lv_label_set_text(notification_clear_label_, "全部清理");
lv_obj_center(notification_clear_label_);

notification_empty_label_ = lv_label_create(card_layer_);
lv_obj_set_width(notification_empty_label_, 220);
lv_obj_set_style_text_align(notification_empty_label_, LV_TEXT_ALIGN_CENTER, 0);
lv_obj_set_style_text_color(notification_empty_label_, lv_color_hex(k_text_dimmed), 0);
lv_label_set_text(notification_empty_label_, "暂无通知");
lv_obj_align(notification_empty_label_, LV_ALIGN_TOP_MID, 0, k_notification_empty_y);
lv_obj_add_flag(notification_empty_label_, LV_OBJ_FLAG_HIDDEN);
```

- [ ] **Step 3: Render visible notifications**

Change `RenderCardPage(XIAOXIN_CARD_PAGE_NOTIFICATIONS)` so it passes the visible notification count:

```cpp
RenderNotificationCards(
    nullptr,
    xiaoxin_card_pager_notification_count(&card_pager_),
    prepare_entry_animation
);
```

Then change `RenderNotificationCards(items, count, prepare_entry_animation)` so each slot uses the visible-index API instead of the raw `items` pointer:

```cpp
const xiaoxin_card_item_t* item = xiaoxin_card_pager_notification_at(&card_pager_, slot);
if (item == nullptr) {
    card.visible_index = 0xff;
    lv_obj_add_flag(card.container, LV_OBJ_FLAG_HIDDEN);
    continue;
}
card.visible_index = slot;
PopulateNotificationCard(card, item);
```

Show/hide clear button and empty label:

```cpp
const bool empty = xiaoxin_card_pager_notification_empty(&card_pager_);
if (empty) {
    AddFlagIfCreated(notification_clear_button_, LV_OBJ_FLAG_HIDDEN);
    RemoveFlagIfCreated(notification_empty_label_, LV_OBJ_FLAG_HIDDEN);
} else {
    RemoveFlagIfCreated(notification_clear_button_, LV_OBJ_FLAG_HIDDEN);
    AddFlagIfCreated(notification_empty_label_, LV_OBJ_FLAG_HIDDEN);
}
```

- [ ] **Step 4: Ensure hide-all path hides new objects**

In `HideCardPageObjects()`, hide:

```cpp
AddFlagIfCreated(notification_clear_button_, LV_OBJ_FLAG_HIDDEN);
AddFlagIfCreated(notification_empty_label_, LV_OBJ_FLAG_HIDDEN);
```

- [ ] **Step 5: Build locally if possible**

Run pager test:

```powershell
gcc -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o tests/xiaoxin_card_pager_test.exe
.\tests\xiaoxin_card_pager_test.exe
```

If `idf.py` exists, run:

```powershell
idf.py build
```

Commit:

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "feat: render notification clear all and empty state"
```

---

### Task 3: Left-swipe hit testing and dismiss animations

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**Interfaces:**
- Consumes:
  - `xiaoxin_card_pager_notification_dismiss(xiaoxin_card_pager_t* pager, uint8_t visible_index)`
  - `xiaoxin_card_pager_notification_clear_all(xiaoxin_card_pager_t* pager)`
  - `GlassCard.visible_index`

- [ ] **Step 1: Add gesture constants and state**

Add constants:

```cpp
static constexpr int16_t k_notification_dismiss_intent_px = 18;
static constexpr int16_t k_notification_dismiss_threshold_px = 72;
static constexpr uint32_t k_notification_dismiss_fly_ms = 160;
static constexpr uint32_t k_notification_dismiss_rebound_ms = 120;
```

Add enum:

```cpp
enum class NotificationGestureMode {
    None,
    VerticalScroll,
    DismissCard,
    ClearAllPress,
};
```

Add fields:

```cpp
NotificationGestureMode notification_gesture_mode_ = NotificationGestureMode::None;
int8_t notification_pressed_slot_ = -1;
uint8_t notification_pressed_visible_index_ = 0xff;
int16_t notification_card_drag_x_ = 0;
bool notification_dismiss_animating_ = false;
```

- [ ] **Step 2: Add hit testing helpers**

Implement:

```cpp
static bool PointInObj(lv_obj_t* obj, uint16_t x, uint16_t y) {
    if (obj == nullptr || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    return x >= coords.x1 && x <= coords.x2 && y >= coords.y1 && y <= coords.y2;
}

int8_t NotificationCardSlotAtPoint(uint16_t x, uint16_t y) const {
    for (uint8_t slot = 0; slot < k_card_glass_count; ++slot) {
        if (PointInObj(glass_cards_[slot].container, x, y)) {
            return (int8_t)slot;
        }
    }
    return -1;
}

bool NotificationClearButtonContains(uint16_t x, uint16_t y) const {
    return PointInObj(notification_clear_button_, x, y);
}
```

- [ ] **Step 3: Add animation helpers**

Implement:

```cpp
static void NotificationDismissSetX(void* obj, int32_t x) {
    lv_obj_set_x(static_cast<lv_obj_t*>(obj), (lv_coord_t)x);
}

static void NotificationDismissSetOpacity(void* obj, int32_t opa) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)opa, 0);
}
```

Add `AnimateNotificationDismiss(slot, visible_index)` and `AnimateNotificationDismissRebound(slot)` using `lv_anim_t`. The dismiss completion callback must:

```cpp
self->notification_dismiss_animating_ = false;
xiaoxin_card_pager_notification_dismiss(&self->card_pager_, visible_index);
self->notification_scroll_y_ = ClampNotificationScrollY(
    self->notification_scroll_y_,
    xiaoxin_card_pager_notification_count(&self->card_pager_)
);
self->EnsureCardPageRendered(XIAOXIN_CARD_PAGE_NOTIFICATIONS, false);
self->ApplyNotificationScrollVisual(self->notification_scroll_y_);
self->RaiseOverlayObjects();
```

- [ ] **Step 4: Update touch press and move handling**

On new press in Notifications:

```cpp
notification_gesture_mode_ = NotificationGestureMode::None;
notification_pressed_slot_ = NotificationCardSlotAtPoint(x, y);
notification_pressed_visible_index_ =
    notification_pressed_slot_ >= 0 ? glass_cards_[notification_pressed_slot_].visible_index : 0xff;
if (NotificationClearButtonContains(x, y)) {
    notification_gesture_mode_ = NotificationGestureMode::ClearAllPress;
}
```

During move in Notifications before vertical scroll handling:

```cpp
if (!notification_dismiss_animating_ &&
    notification_pressed_slot_ >= 0 &&
    notification_pressed_visible_index_ != 0xff &&
    dx < 0 &&
    abs_dx >= k_notification_dismiss_intent_px &&
    ((int32_t)abs_dx * 4) > ((int32_t)abs_dy * 5)) {
    notification_gesture_mode_ = NotificationGestureMode::DismissCard;
}

if (notification_gesture_mode_ == NotificationGestureMode::DismissCard) {
    notification_card_drag_x_ = std::max<int16_t>((int16_t)-DISPLAY_WIDTH, dx);
    lv_obj_set_x(glass_cards_[notification_pressed_slot_].container, notification_card_drag_x_);
    const int32_t opa = LV_OPA_COVER + ((int32_t)notification_card_drag_x_ * LV_OPA_COVER) / DISPLAY_WIDTH;
    lv_obj_set_style_opa(
        glass_cards_[notification_pressed_slot_].container,
        (lv_opa_t)std::max<int32_t>(LV_OPA_30, opa),
        0
    );
    return;
}
```

- [ ] **Step 5: Update release handling**

At the top of notification release handling:

```cpp
if (release_current_page == XIAOXIN_CARD_PAGE_NOTIFICATIONS &&
    notification_gesture_mode_ == NotificationGestureMode::DismissCard &&
    notification_pressed_slot_ >= 0) {
    if (dx <= -k_notification_dismiss_threshold_px) {
        AnimateNotificationDismiss(notification_pressed_slot_, notification_pressed_visible_index_);
    } else {
        AnimateNotificationDismissRebound(notification_pressed_slot_);
    }
    notification_gesture_mode_ = NotificationGestureMode::None;
    return;
}
```

For clear all:

```cpp
if (release_current_page == XIAOXIN_CARD_PAGE_NOTIFICATIONS &&
    notification_gesture_mode_ == NotificationGestureMode::ClearAllPress &&
    abs_dx < 8 && abs_dy < 8) {
    xiaoxin_card_pager_notification_clear_all(&card_pager_);
    notification_scroll_y_ = 0;
    EnsureCardPageRendered(XIAOXIN_CARD_PAGE_NOTIFICATIONS, false);
    ApplyNotificationScrollVisual(notification_scroll_y_);
    RaiseOverlayObjects();
    notification_gesture_mode_ = NotificationGestureMode::None;
    return;
}
```

- [ ] **Step 6: Verify and commit**

Run:

```powershell
gcc -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o tests/xiaoxin_card_pager_test.exe
.\tests\xiaoxin_card_pager_test.exe
```

If `idf.py` exists, run:

```powershell
idf.py build
```

Commit:

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "feat: add left swipe notification dismiss"
```

---

### Task 4: Documentation and device acceptance checklist

**Files:**
- Modify: `docs/xiaoxin-vertical-card-pager-plan.zh-CN.md`

- [ ] **Step 1: Append implementation note**

Append:

```markdown
## 2026-06-18 通知左滑清理交互记录

通知页新增手机式管理交互：手指按在哪张通知卡片上，向左滑动就清理哪张；顶部新增 `全部清理` 按钮，用于一次清空所有本地通知。清空后通知页显示 `暂无通知` 空状态。竖向滑动仍然用于浏览通知，横向左滑只作用于被命中的通知卡片，不改变 Home / Notifications / Overview 外层页面状态机。

验证：
- `xiaoxin_card_pager_test`：通过
- `idf.py build`：在 ESP-IDF 环境可用时执行
- 实机检查：左滑单条、未过阈值回弹、全部清理、空状态、返回 Home
```

- [ ] **Step 2: Run final local test**

Run:

```powershell
gcc -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o tests/xiaoxin_card_pager_test.exe
.\tests\xiaoxin_card_pager_test.exe
```

Expected:

```text
xiaoxin_card_pager tests passed
```

- [ ] **Step 3: Commit**

```powershell
git add docs/xiaoxin-vertical-card-pager-plan.zh-CN.md
git commit -m "docs: record notification dismiss interaction"
```

---

## Self-Review

1. **Spec coverage:** The plan covers data model, visible-index mapping, left-swipe dismiss, clear-all button, empty state, tests, docs, and device validation.
2. **Placeholder scan:** No unresolved placeholders. Conditional `idf.py` steps are explicitly tied to environment availability.
3. **Type consistency:** The visible-index APIs introduced in Task 1 are consumed by Tasks 2 and 3. `GlassCard.visible_index` bridges UI hit testing to pager dismiss logic.
4. **Risk control:** Horizontal dismiss is separated by `NotificationGestureMode`, so it does not silently compete with existing vertical scroll or outer page gestures.
