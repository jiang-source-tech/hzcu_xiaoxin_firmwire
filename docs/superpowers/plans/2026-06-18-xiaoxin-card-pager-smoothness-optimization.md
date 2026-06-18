# Xiaoxin Card Pager Smoothness Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce visible stutter in the Waveshare 1.46" Xiaoxin card pager, especially during finger-following notification card scroll and page snap/rebound.

**Architecture:** Treat smoothness as a measured rendering-budget problem, not a blind visual tweak. First add temporary timing probes around the suspected hot paths, then make drag-time rendering cheaper by separating "continuous drag visual updates" from "settled rich visual styling", then verify that pet GIF pausing and LVGL animation cleanup happen at the right moments.

**Tech Stack:** ESP-IDF, C++17, LVGL 9.x, Waveshare ESP32-S3 touch LCD 1.46", existing C pager tests.

## Global Constraints

- Do not change `xiaoxin_card_pager.c` page-state semantics unless a measurement proves the state machine is involved.
- Preserve current page behavior: Home down-swipe opens Notifications, Home up-swipe opens Overview, reverse swipe returns Home.
- Keep the visual identity: dark glass cards, notification dots, overview rows, system overlay.
- Avoid real blur/backdrop effects on ESP32-S3.
- Debug logs must be behind a compile-time switch and off by default after optimization.
- Prefer small commits: measurement, drag fast path, pet animation verification, final cleanup.

---

## File Map

| File | Responsibility | Planned change |
| --- | --- | --- |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc` | LVGL UI, touch polling, card rendering, pet GIF integration | Main optimization work |
| `tests/xiaoxin_card_pager_test.c` | Pure pager state-machine regression test | Re-run unchanged |
| `docs/xiaoxin-vertical-card-pager-plan.zh-CN.md` | Human-facing design/history note | Add a short optimization note after implementation |

---

### Task 1: Add a temporary UI timing probe

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**Interfaces:**
- Produces: `AddUiPerfSample(uint32_t& calls, uint32_t& total_us, uint32_t& max_us, uint32_t elapsed_us)`, `LogUiPerfSummary(uint32_t now_ms)`, and named counters for drag visual, pet frame copy, and touch loop.
- Consumes: existing `NowMs()`, `esp_timer_get_time()`, `ESP_LOGI`.

- [ ] **Step 1: Add timing constants and accumulator fields**

Add near the current UI constants:

```cpp
static constexpr bool k_ui_perf_trace_enabled = false;
static constexpr uint32_t k_ui_perf_log_interval_ms = 1000;
```

Add to `PaopaoPetDisplay` private members near touch/card fields:

```cpp
uint32_t ui_perf_last_log_ms_ = 0;
uint32_t ui_perf_drag_calls_ = 0;
uint32_t ui_perf_drag_total_us_ = 0;
uint32_t ui_perf_drag_max_us_ = 0;
uint32_t ui_perf_pet_copy_calls_ = 0;
uint32_t ui_perf_pet_copy_total_us_ = 0;
uint32_t ui_perf_pet_copy_max_us_ = 0;
uint32_t ui_perf_touch_loop_calls_ = 0;
uint32_t ui_perf_touch_loop_total_us_ = 0;
uint32_t ui_perf_touch_loop_max_us_ = 0;
```

- [ ] **Step 2: Add helper functions**

Add inside `PaopaoPetDisplay` private methods:

```cpp
void AddUiPerfSample(uint32_t& calls, uint32_t& total_us, uint32_t& max_us, uint32_t elapsed_us) {
    if (!k_ui_perf_trace_enabled) {
        return;
    }
    calls++;
    total_us += elapsed_us;
    if (elapsed_us > max_us) {
        max_us = elapsed_us;
    }
}

void LogUiPerfSummary(uint32_t now_ms) {
    if (!k_ui_perf_trace_enabled) {
        return;
    }
    if (ui_perf_last_log_ms_ != 0 && now_ms - ui_perf_last_log_ms_ < k_ui_perf_log_interval_ms) {
        return;
    }
    ui_perf_last_log_ms_ = now_ms;

    const uint32_t drag_avg = ui_perf_drag_calls_ == 0 ? 0 : ui_perf_drag_total_us_ / ui_perf_drag_calls_;
    const uint32_t pet_avg = ui_perf_pet_copy_calls_ == 0 ? 0 : ui_perf_pet_copy_total_us_ / ui_perf_pet_copy_calls_;
    const uint32_t touch_avg = ui_perf_touch_loop_calls_ == 0 ? 0 : ui_perf_touch_loop_total_us_ / ui_perf_touch_loop_calls_;

    ESP_LOGI(
        TAG,
        "[UI-PERF] drag avg=%uus max=%uus calls=%u pet_copy avg=%uus max=%uus calls=%u touch_loop avg=%uus max=%uus calls=%u",
        (unsigned)drag_avg,
        (unsigned)ui_perf_drag_max_us_,
        (unsigned)ui_perf_drag_calls_,
        (unsigned)pet_avg,
        (unsigned)ui_perf_pet_copy_max_us_,
        (unsigned)ui_perf_pet_copy_calls_,
        (unsigned)touch_avg,
        (unsigned)ui_perf_touch_loop_max_us_,
        (unsigned)ui_perf_touch_loop_calls_
    );

    ui_perf_drag_calls_ = 0;
    ui_perf_drag_total_us_ = 0;
    ui_perf_drag_max_us_ = 0;
    ui_perf_pet_copy_calls_ = 0;
    ui_perf_pet_copy_total_us_ = 0;
    ui_perf_pet_copy_max_us_ = 0;
    ui_perf_touch_loop_calls_ = 0;
    ui_perf_touch_loop_total_us_ = 0;
    ui_perf_touch_loop_max_us_ = 0;
}
```

- [ ] **Step 3: Instrument `ApplyNotificationScrollVisual`**

At the top of `ApplyNotificationScrollVisual`, add:

```cpp
const int64_t perf_start_us = k_ui_perf_trace_enabled ? esp_timer_get_time() : 0;
```

Immediately before returning from the function, add:

```cpp
if (k_ui_perf_trace_enabled) {
    AddUiPerfSample(
        ui_perf_drag_calls_,
        ui_perf_drag_total_us_,
        ui_perf_drag_max_us_,
        (uint32_t)(esp_timer_get_time() - perf_start_us)
    );
}
```

- [ ] **Step 4: Instrument `CopyPetFrameToScreen`**

At the top of `CopyPetFrameToScreen`, after the null checks, add:

```cpp
const int64_t perf_start_us = k_ui_perf_trace_enabled ? esp_timer_get_time() : 0;
```

At the end of the function, after the nested copy loops, add:

```cpp
if (k_ui_perf_trace_enabled) {
    AddUiPerfSample(
        ui_perf_pet_copy_calls_,
        ui_perf_pet_copy_total_us_,
        ui_perf_pet_copy_max_us_,
        (uint32_t)(esp_timer_get_time() - perf_start_us)
    );
}
```

- [ ] **Step 5: Instrument the render loop body**

In `RunRenderLoop()`, wrap the locked section:

```cpp
const int64_t perf_start_us = k_ui_perf_trace_enabled ? esp_timer_get_time() : 0;
{
    DisplayLockGuard lock(this);
    const uint32_t now_ms = NowMs();
    PollTouch(now_ms);
    paopao_pet_trigger_tick(&trigger_, now_ms);
    ApplyPetStateIfChanged();
    LogUiPerfSummary(now_ms);
}
if (k_ui_perf_trace_enabled) {
    AddUiPerfSample(
        ui_perf_touch_loop_calls_,
        ui_perf_touch_loop_total_us_,
        ui_perf_touch_loop_max_us_,
        (uint32_t)(esp_timer_get_time() - perf_start_us)
    );
}
vTaskDelay(pdMS_TO_TICKS(k_touch_poll_ms));
```

- [ ] **Step 6: Build**

Run:

```powershell
idf.py build
```

Expected: build succeeds with `k_ui_perf_trace_enabled = false`.

- [ ] **Step 7: Temporary measurement run**

Temporarily change:

```cpp
static constexpr bool k_ui_perf_trace_enabled = true;
```

Run:

```powershell
idf.py flash monitor
```

Manual test:

1. Swipe Home -> Notifications.
2. Drag notification cards slowly for 5 seconds.
3. Flick notification cards quickly for 5 seconds.
4. Return Notifications -> Home.

Expected log shape:

```text
[UI-PERF] drag avg=3200us max=9800us calls=42 pet_copy avg=0us max=0us calls=0 touch_loop avg=4100us max=11800us calls=94
```

Record baseline numbers in the implementation notes before changing visuals.

- [ ] **Step 8: Re-disable logs and commit**

Set:

```cpp
static constexpr bool k_ui_perf_trace_enabled = false;
```

Commit:

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "perf: add optional UI timing probes for xiaoxin card pager"
```

---

### Task 2: Add a lightweight drag visual path for notification scrolling

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**Interfaces:**
- Changes: `ApplyNotificationScrollVisual(int16_t scroll_y, bool prepare_entry_animation = false, bool lightweight_drag = false)`.
- Consumers updated: `PollTouch()`, `NotificationScrollSetY()`, `RenderNotificationCards()`, `AnimateNotificationScroll()`.

- [ ] **Step 1: Extend the function signature**

Change:

```cpp
void ApplyNotificationScrollVisual(int16_t scroll_y, bool prepare_entry_animation = false) {
```

to:

```cpp
void ApplyNotificationScrollVisual(
    int16_t scroll_y,
    bool prepare_entry_animation = false,
    bool lightweight_drag = false
) {
```

- [ ] **Step 2: Skip expensive per-frame animation deletion during drag**

Inside the card loop, change:

```cpp
lv_anim_delete(card.container, nullptr);
```

to:

```cpp
if (!lightweight_drag) {
    lv_anim_delete(card.container, nullptr);
}
```

- [ ] **Step 3: Make drag path update only position and opacity**

Replace the style-update block inside the loop with:

```cpp
lv_obj_align(card.container, LV_ALIGN_TOP_MID, 0, k_glass_y_start + y_offset);
lv_obj_set_style_opa(card.container, target_opa, 0);

if (!lightweight_drag) {
    lv_obj_set_style_bg_opa(card.container, near_center ? static_cast<lv_opa_t>(174) : static_cast<lv_opa_t>(82), 0);
    lv_obj_set_style_border_opa(card.container, near_center ? static_cast<lv_opa_t>(44) : static_cast<lv_opa_t>(18), 0);
    lv_obj_set_style_shadow_width(card.container, near_center ? 30 : 8, 0);
    lv_obj_set_style_shadow_opa(card.container, near_center ? LV_OPA_70 : LV_OPA_20, 0);
    lv_obj_set_style_shadow_offset_y(card.container, near_center ? 10 : 4, 0);
}
```

This keeps finger-following responsive while preserving the rich card styling when scrolling settles.

- [ ] **Step 4: Skip foreground reordering during drag**

Wrap the two `lv_obj_move_foreground` loops at the bottom of `ApplyNotificationScrollVisual`:

```cpp
if (!lightweight_drag) {
    for (uint8_t slot = 0; slot < k_card_glass_count; ++slot) {
        if (slot != active_index && glass_cards_[slot].container != nullptr) {
            lv_obj_move_foreground(glass_cards_[slot].container);
        }
    }
    if (active_index < k_card_glass_count &&
        glass_cards_[active_index].container != nullptr &&
        !lv_obj_has_flag(glass_cards_[active_index].container, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(glass_cards_[active_index].container);
    }
    for (uint8_t i = 0; i < k_notification_indicator_dot_count; ++i) {
        lv_obj_t* dot = notification_indicator_dots_[i];
        if (dot != nullptr && !lv_obj_has_flag(dot, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_move_foreground(dot);
        }
    }
}
```

- [ ] **Step 5: Use lightweight mode only while the finger is down**

In `PollTouch()`, change the notification inner-scroll call:

```cpp
ApplyNotificationScrollVisual(NotificationScrollDisplayY(raw_scroll, total));
```

to:

```cpp
ApplyNotificationScrollVisual(NotificationScrollDisplayY(raw_scroll, total), false, true);
```

Do not change `NotificationScrollSetY()` or `AnimateNotificationScroll()`: release animations should use the rich settled path.

- [ ] **Step 6: Verify pure pager tests still pass**

Run:

```powershell
gcc -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o tests/xiaoxin_card_pager_test.exe
.\tests\xiaoxin_card_pager_test.exe
```

Expected:

```text
xiaoxin_card_pager tests passed
```

- [ ] **Step 7: Build**

Run:

```powershell
idf.py build
```

Expected: build succeeds.

- [ ] **Step 8: Commit**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "perf: add lightweight drag visual path for notification cards"
```

---

### Task 3: Tighten card-layer/pet-animation coordination

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**Interfaces:**
- Consumes: existing `ApplyPetAnimationForCardPager()`, `IsCardLayerVisible()`, `pet_gif_controller_->Pause()`, `pet_gif_controller_->Resume()`.
- Produces: consistent calls whenever card visibility changes.

- [ ] **Step 1: Confirm current pause helper stays as the single policy point**

Keep this existing helper as the policy boundary:

```cpp
void ApplyPetAnimationForCardPager() {
    if (pet_gif_controller_ == nullptr) {
        pet_animation_paused_for_card_ = false;
        return;
    }

    if (IsCardLayerVisible()) {
        if (pet_gif_controller_->IsPlaying()) {
            pet_gif_controller_->Pause();
        }
        pet_animation_paused_for_card_ = true;
        return;
    }

    if (pet_animation_paused_for_card_) {
        pet_gif_controller_->Resume();
        pet_animation_paused_for_card_ = false;
    }
}
```

- [ ] **Step 2: Call the helper immediately after drag makes the card layer visible**

In `ApplyCardPagerVisual`, after the `lv_obj_set_style_opa` call, add:

```cpp
ApplyPetAnimationForCardPager();
```

This ensures the pet GIF pauses during the first visible drag frame, not only after snap completion.

- [ ] **Step 3: Call the helper after `SetCardPagerPage()` applies visibility**

At the end of `SetCardPagerPage`, after `ApplyCardPagerVisual();`, add:

```cpp
ApplyPetAnimationForCardPager();
```

- [ ] **Step 4: Keep existing completion calls**

Verify these existing completion paths still call `ApplyPetAnimationForCardPager()`:

```cpp
CardLayerAnimationCompleted(lv_anim_t* anim)
CardLayerDragAnimationCompleted(lv_anim_t* anim)
EnsureCardPageRendered(xiaoxin_card_page_t page, bool prepare_entry_animation)
```

If any one of those paths does not call it, add a call after card-layer visibility has been updated.

- [ ] **Step 5: Build**

Run:

```powershell
idf.py build
```

Expected: build succeeds.

- [ ] **Step 6: Manual behavior check**

Run:

```powershell
idf.py flash monitor
```

Manual checks:

1. On Home, pet GIF animates normally.
2. Start dragging into Notifications; pet animation should stop or visibly freeze while card layer is visible.
3. Return to Home; pet GIF resumes.
4. Trigger a pet state change while Notifications is open; state should apply after returning Home, not while card layer is open.

- [ ] **Step 7: Commit**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "perf: pause pet animation as soon as card pager becomes visible"
```

---

### Task 4: Tune notification release animation to avoid a perceptual hitch

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**Interfaces:**
- Consumes: `AnimateNotificationScroll(int16_t start_scroll_y, int16_t target_scroll_y)`, `ApplyNotificationScrollVisual(int16_t scroll_y, bool prepare_entry_animation, bool lightweight_drag)`.
- Produces: release animation uses the rich visual path once, then animates only scroll position.

- [ ] **Step 1: Ensure settled styles are restored before release animation starts**

At the start of `AnimateNotificationScroll`, keep:

```cpp
notification_scroll_y_ = target_scroll_y;
lv_anim_delete(this, NotificationScrollSetY);
ApplyNotificationScrollVisual(start_scroll_y);
```

Do not pass `lightweight_drag = true` here. The release animation should restore shadows/layering once, then animate position.

- [ ] **Step 2: Shorten bounce-only release if it still feels sluggish**

If baseline logs show `ApplyNotificationScrollVisual` is below 4ms but the release still feels delayed, change:

```cpp
static constexpr uint32_t k_notification_switch_anim_ms = 160;
```

to:

```cpp
static constexpr uint32_t k_notification_switch_anim_ms = 130;
```

Do this only if the actual symptom is a late-feeling release, not drag-time frame drops.

- [ ] **Step 3: Keep page snap at 210ms**

Do not change:

```cpp
static constexpr uint16_t k_card_snap_anim_ms = 210;
```

The outer page transition needs enough time to read as intentional; the suspected hitch is the inner notification scroll/render path.

- [ ] **Step 4: Build and flash**

Run:

```powershell
idf.py build
idf.py flash monitor
```

Expected: no compile errors; release animation finishes faster only if Step 2 was applied.

- [ ] **Step 5: Commit**

If Step 2 was applied:

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "perf: tune notification scroll release timing"
```

If Step 2 was not applied, do not create a commit for this task.

---

### Task 5: Re-measure and compare before/after

**Files:**
- Modify temporarily: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**Interfaces:**
- Consumes: `k_ui_perf_trace_enabled`.
- Produces: measured before/after numbers for the final note.

- [ ] **Step 1: Enable trace temporarily**

Change:

```cpp
static constexpr bool k_ui_perf_trace_enabled = false;
```

to:

```cpp
static constexpr bool k_ui_perf_trace_enabled = true;
```

- [ ] **Step 2: Flash and repeat the same manual scenario as Task 1**

Run:

```powershell
idf.py flash monitor
```

Manual test:

1. Swipe Home -> Notifications.
2. Drag notification cards slowly for 5 seconds.
3. Flick notification cards quickly for 5 seconds.
4. Return Notifications -> Home.

- [ ] **Step 3: Compare against baseline**

Target interpretation:

```text
Good: drag max clearly lower than baseline, and touch_loop max avoids frequent spikes above 16000us.
Acceptable: drag avg lower, occasional spikes remain but no visible hitch.
Not acceptable: drag max unchanged and visible hitch remains.
```

If not acceptable, continue to Task 6. If acceptable, skip Task 6.

- [ ] **Step 4: Disable trace**

Restore:

```cpp
static constexpr bool k_ui_perf_trace_enabled = false;
```

- [ ] **Step 5: Build**

Run:

```powershell
idf.py build
```

Expected: build succeeds with trace off.

---

### Task 6: Optional fallback - remove dynamic shadows during drag entirely

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**Interfaces:**
- Consumes: `lightweight_drag` parameter from Task 2.
- Produces: stronger drag-time simplification if Task 2 is not enough.

- [ ] **Step 1: Freeze shadow styles once at card creation**

In `InitializeCardPagerLayer()`, keep initial shadows for visual depth:

```cpp
lv_obj_set_style_shadow_width(card.container, i == 0 ? 30 : 8, 0);
lv_obj_set_style_shadow_opa(card.container, i == 0 ? LV_OPA_70 : LV_OPA_20, 0);
lv_obj_set_style_shadow_offset_y(card.container, i == 0 ? 10 : 4, 0);
```

- [ ] **Step 2: Remove shadow changes from `ApplyNotificationScrollVisual()`**

Delete these lines from the non-lightweight block:

```cpp
lv_obj_set_style_shadow_width(card.container, near_center ? 30 : 8, 0);
lv_obj_set_style_shadow_opa(card.container, near_center ? LV_OPA_70 : LV_OPA_20, 0);
lv_obj_set_style_shadow_offset_y(card.container, near_center ? 10 : 4, 0);
```

Keep only background/border opacity changes:

```cpp
if (!lightweight_drag) {
    lv_obj_set_style_bg_opa(card.container, near_center ? static_cast<lv_opa_t>(174) : static_cast<lv_opa_t>(82), 0);
    lv_obj_set_style_border_opa(card.container, near_center ? static_cast<lv_opa_t>(44) : static_cast<lv_opa_t>(18), 0);
}
```

- [ ] **Step 3: Re-measure**

Repeat Task 5 with trace enabled temporarily.

Expected: lower drag max if dynamic shadow changes were the dominant cost.

- [ ] **Step 4: Commit if it improves visible smoothness**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "perf: freeze notification card shadows during scroll"
```

---

### Task 7: Final verification and documentation

**Files:**
- Modify: `docs/xiaoxin-vertical-card-pager-plan.zh-CN.md`
- Verify: `tests/xiaoxin_card_pager_test.c`

**Interfaces:**
- Consumes: measured baseline/final numbers from Tasks 1 and 5.
- Produces: short documentation of what actually improved smoothness.

- [ ] **Step 1: Run the pure state-machine regression test**

Run:

```powershell
gcc -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o tests/xiaoxin_card_pager_test.exe
.\tests\xiaoxin_card_pager_test.exe
```

Expected:

```text
xiaoxin_card_pager tests passed
```

- [ ] **Step 2: Build firmware**

Run:

```powershell
idf.py build
```

Expected: build succeeds.

- [ ] **Step 3: Manual acceptance test on device**

Run:

```powershell
idf.py flash monitor
```

Acceptance checklist:

```text
[ ] Home pet still animates.
[ ] Down-swipe into Notifications follows finger without obvious hitch.
[ ] Notification inner scroll follows finger continuously.
[ ] Notification release/bounce feels intentional, not delayed.
[ ] Swipe from Notifications bottom back to Home still works.
[ ] Up-swipe into Overview still works.
[ ] Reverse swipe from Overview to Home still works.
[ ] System overlay remains visible and correctly layered.
[ ] Pet resumes after returning Home.
```

- [ ] **Step 4: Add implementation note**

Append to `docs/xiaoxin-vertical-card-pager-plan.zh-CN.md`:

```markdown
## 2026-06-18 Smoothness optimization note

The card pager smoothness work treated visible stutter as a rendering-budget issue. Baseline measurements showed the expensive path was the notification drag visual update, especially repeated style/layer/shadow changes while the finger was down. The implementation now uses a lightweight drag path that updates position and opacity continuously, then restores richer card styling during release/settled animation. Pet GIF playback is also paused as soon as the card layer becomes visible, so full-screen pet frame copying does not compete with card dragging.

Verification:
- `xiaoxin_card_pager_test`: passed
- `idf.py build`: passed
- Device check: Home/Notifications/Overview transitions passed
```

- [ ] **Step 5: Commit docs and final cleanup**

Ensure this constant is false:

```cpp
static constexpr bool k_ui_perf_trace_enabled = false;
```

Run:

```powershell
git status --short
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc docs/xiaoxin-vertical-card-pager-plan.zh-CN.md
git commit -m "docs: record xiaoxin card pager smoothness optimization"
```

---

## Self-Review

1. **Spec coverage:** The plan covers measurement, drag-time rendering reduction, pet animation coordination, release-timing tuning, optional shadow fallback, tests, build, device acceptance, and docs.
2. **Placeholder scan:** No unresolved placeholder markers or unspecified "handle later" steps. Optional Task 6 has an explicit entry condition and exact code changes.
3. **Type consistency:** `ApplyNotificationScrollVisual` gains one `bool lightweight_drag` parameter with default `false`; all existing call sites remain valid. Only the active finger-drag path passes `true`.
4. **Risk control:** Timing instrumentation is compile-time gated and restored to `false`. State-machine files are not modified. Visual simplification applies only during continuous drag unless Task 6 is explicitly needed.
