# 小芯卡片分页丝滑度优化实施方案

> **给 agentic workers：** 必须使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 按任务逐步执行本计划。所有步骤使用 checkbox（`- [ ]`）便于跟踪。

**目标：** 降低 Waveshare 1.46 寸小芯卡片分页的可见卡顿，重点优化通知卡片跟手滚动、页面吸附、回弹阶段的顿感。

**架构思路：** 把“丝滑度”当成可测的渲染预算问题，而不是凭感觉乱调视觉效果。先在疑似热点路径加临时耗时探针，再把“手指按住拖动时的连续视觉更新”和“松手后/静止时的完整精致视觉”分离，最后确认宠物 GIF 暂停和 LVGL 动画清理发生在正确时机。

**技术栈：** ESP-IDF、C++17、LVGL 9.x、Waveshare ESP32-S3 touch LCD 1.46、现有 C 分页状态机测试。

## 全局约束

- 除非测量证明状态机参与了卡顿，否则不修改 `xiaoxin_card_pager.c` 的分页语义。
- 保持现有页面行为：Home 下滑进入 Notifications，Home 上滑进入 Overview，反向滑动回到 Home。
- 保留当前视觉识别：深色玻璃卡片、通知圆点、总览行、系统浮层。
- 不在 ESP32-S3 上做真正的 blur/backdrop blur。
- 性能日志必须由编译期开关控制，优化完成后默认关闭。
- 推荐小步提交：测量、拖动快路径、宠物动画协调、最终清理。

---

## 文件地图

| 文件 | 职责 | 计划改动 |
| --- | --- | --- |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc` | LVGL UI、触摸轮询、卡片渲染、宠物 GIF 集成 | 主要优化文件 |
| `tests/xiaoxin_card_pager_test.c` | 纯分页状态机回归测试 | 不修改，只运行 |
| `docs/xiaoxin-vertical-card-pager-plan.zh-CN.md` | 面向人的设计/历史记录 | 实施后追加一段优化说明 |

---

### 任务 1：加入临时 UI 耗时探针

**文件：**
- 修改：`main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**接口：**
- 产出：`AddUiPerfSample(uint32_t& calls, uint32_t& total_us, uint32_t& max_us, uint32_t elapsed_us)`、`LogUiPerfSummary(uint32_t now_ms)`，以及拖动视觉、宠物帧拷贝、触摸循环三类计数器。
- 消费：现有 `NowMs()`、`esp_timer_get_time()`、`ESP_LOGI`。

- [ ] **步骤 1：增加耗时开关和累计字段**

在当前 UI 常量区附近增加：

```cpp
static constexpr bool k_ui_perf_trace_enabled = false;
static constexpr uint32_t k_ui_perf_log_interval_ms = 1000;
```

在 `PaopaoPetDisplay` 的 private 成员区，靠近触摸/卡片字段的位置增加：

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

- [ ] **步骤 2：增加辅助函数**

在 `PaopaoPetDisplay` 的 private 方法区增加：

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

- [ ] **步骤 3：给 `ApplyNotificationScrollVisual` 加探针**

在 `ApplyNotificationScrollVisual` 函数顶部增加：

```cpp
const int64_t perf_start_us = k_ui_perf_trace_enabled ? esp_timer_get_time() : 0;
```

在函数返回前增加：

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

- [ ] **步骤 4：给 `CopyPetFrameToScreen` 加探针**

在 `CopyPetFrameToScreen` 顶部、空指针检查之后增加：

```cpp
const int64_t perf_start_us = k_ui_perf_trace_enabled ? esp_timer_get_time() : 0;
```

在函数末尾、嵌套像素拷贝循环之后增加：

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

- [ ] **步骤 5：给渲染循环主体加探针**

在 `RunRenderLoop()` 中，用下面结构包住加锁区域：

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

- [ ] **步骤 6：构建**

运行：

```powershell
idf.py build
```

预期：`k_ui_perf_trace_enabled = false` 时构建通过。

- [ ] **步骤 7：临时打开测量并实机记录基线**

临时改成：

```cpp
static constexpr bool k_ui_perf_trace_enabled = true;
```

运行：

```powershell
idf.py flash monitor
```

手动测试：

1. 从 Home 下滑进入 Notifications。
2. 慢速拖动通知卡片 5 秒。
3. 快速拨动通知卡片 5 秒。
4. 从 Notifications 返回 Home。

预期日志形态：

```text
[UI-PERF] drag avg=3200us max=9800us calls=42 pet_copy avg=0us max=0us calls=0 touch_loop avg=4100us max=11800us calls=94
```

在改视觉之前，先把基线数字记录到实施备注里。

- [ ] **步骤 8：关闭日志并提交**

恢复：

```cpp
static constexpr bool k_ui_perf_trace_enabled = false;
```

提交：

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "perf: add optional UI timing probes for xiaoxin card pager"
```

---

### 任务 2：为通知滚动增加轻量拖动视觉路径

**文件：**
- 修改：`main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**接口：**
- 修改：`ApplyNotificationScrollVisual(int16_t scroll_y, bool prepare_entry_animation = false, bool lightweight_drag = false)`。
- 更新调用方：`PollTouch()`、`NotificationScrollSetY()`、`RenderNotificationCards()`、`AnimateNotificationScroll()`。

- [ ] **步骤 1：扩展函数签名**

把：

```cpp
void ApplyNotificationScrollVisual(int16_t scroll_y, bool prepare_entry_animation = false) {
```

改成：

```cpp
void ApplyNotificationScrollVisual(
    int16_t scroll_y,
    bool prepare_entry_animation = false,
    bool lightweight_drag = false
) {
```

- [ ] **步骤 2：拖动时跳过昂贵的逐帧动画删除**

在卡片循环里，把：

```cpp
lv_anim_delete(card.container, nullptr);
```

改成：

```cpp
if (!lightweight_drag) {
    lv_anim_delete(card.container, nullptr);
}
```

- [ ] **步骤 3：拖动路径只更新位置和透明度**

把循环里的样式更新块改成：

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

这样手指跟随阶段更轻；松手、吸附、静止阶段仍保留精致卡片视觉。

- [ ] **步骤 4：拖动时跳过前景层级重排**

把 `ApplyNotificationScrollVisual` 底部两个 `lv_obj_move_foreground` 循环包起来：

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

- [ ] **步骤 5：只在手指按住拖动时启用轻量模式**

在 `PollTouch()` 里，把通知内部滚动调用：

```cpp
ApplyNotificationScrollVisual(NotificationScrollDisplayY(raw_scroll, total));
```

改成：

```cpp
ApplyNotificationScrollVisual(NotificationScrollDisplayY(raw_scroll, total), false, true);
```

不要改 `NotificationScrollSetY()` 或 `AnimateNotificationScroll()`：松手后的动画应该走完整视觉路径。

- [ ] **步骤 6：确认纯分页测试仍通过**

运行：

```powershell
gcc -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o tests/xiaoxin_card_pager_test.exe
.\tests\xiaoxin_card_pager_test.exe
```

预期：

```text
xiaoxin_card_pager tests passed
```

- [ ] **步骤 7：构建**

运行：

```powershell
idf.py build
```

预期：构建通过。

- [ ] **步骤 8：提交**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "perf: add lightweight drag visual path for notification cards"
```

---

### 任务 3：收紧卡片层和宠物动画的协作

**文件：**
- 修改：`main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**接口：**
- 消费：现有 `ApplyPetAnimationForCardPager()`、`IsCardLayerVisible()`、`pet_gif_controller_->Pause()`、`pet_gif_controller_->Resume()`。
- 产出：卡片层可见状态变化时，宠物 GIF 暂停/恢复行为一致。

- [ ] **步骤 1：保留现有暂停辅助函数作为唯一策略点**

保留现有函数：

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

- [ ] **步骤 2：卡片层刚被拖出来时立刻调用暂停策略**

在 `ApplyCardPagerVisual` 里，`lv_obj_set_style_opa` 调用之后增加：

```cpp
ApplyPetAnimationForCardPager();
```

这样宠物 GIF 会在卡片第一帧可见时就暂停，而不是等吸附完成。

- [ ] **步骤 3：`SetCardPagerPage()` 改完页面后也调用策略**

在 `SetCardPagerPage` 末尾，`ApplyCardPagerVisual();` 之后增加：

```cpp
ApplyPetAnimationForCardPager();
```

- [ ] **步骤 4：保留已有完成回调里的调用**

确认这些路径仍然会调用 `ApplyPetAnimationForCardPager()`：

```cpp
CardLayerAnimationCompleted(lv_anim_t* anim)
CardLayerDragAnimationCompleted(lv_anim_t* anim)
EnsureCardPageRendered(xiaoxin_card_page_t page, bool prepare_entry_animation)
```

如果其中某条路径没有调用，就在卡片层可见状态更新之后补一次调用。

- [ ] **步骤 5：构建**

运行：

```powershell
idf.py build
```

预期：构建通过。

- [ ] **步骤 6：实机行为检查**

运行：

```powershell
idf.py flash monitor
```

手动检查：

1. Home 页面宠物 GIF 正常动。
2. 开始拖入 Notifications 时，只要卡片层可见，宠物动画应停止或看起来冻结。
3. 返回 Home 后，宠物 GIF 恢复。
4. Notifications 打开时触发宠物状态变化，状态应在回到 Home 后再应用，不应在卡片层打开时抢刷新。

- [ ] **步骤 7：提交**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "perf: pause pet animation as soon as card pager becomes visible"
```

---

### 任务 4：调整通知释放动画，避免主观上的“顿一下”

**文件：**
- 修改：`main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**接口：**
- 消费：`AnimateNotificationScroll(int16_t start_scroll_y, int16_t target_scroll_y)`、`ApplyNotificationScrollVisual(int16_t scroll_y, bool prepare_entry_animation, bool lightweight_drag)`。
- 产出：松手动画先恢复一次完整视觉，然后只动画滚动位置。

- [ ] **步骤 1：确保释放动画开始前恢复完整样式**

在 `AnimateNotificationScroll` 开头保留：

```cpp
notification_scroll_y_ = target_scroll_y;
lv_anim_delete(this, NotificationScrollSetY);
ApplyNotificationScrollVisual(start_scroll_y);
```

这里不要传 `lightweight_drag = true`。松手后的动画应该先恢复阴影/层级，再动画位置。

- [ ] **步骤 2：如果回弹仍显得拖泥带水，再缩短内部滚动动画**

如果基线日志显示 `ApplyNotificationScrollVisual` 小于 4ms，但松手释放仍然主观延迟，把：

```cpp
static constexpr uint32_t k_notification_switch_anim_ms = 160;
```

改成：

```cpp
static constexpr uint32_t k_notification_switch_anim_ms = 130;
```

只有当症状是“松手释放慢”时才改；如果问题是拖动中掉帧，不要靠改时长掩盖。

- [ ] **步骤 3：外层页面吸附保持 210ms**

不要修改：

```cpp
static constexpr uint16_t k_card_snap_anim_ms = 210;
```

外层页面切换需要足够时间读起来像有意图的手表 UI；当前疑点主要是内部通知滚动和渲染路径。

- [ ] **步骤 4：构建并烧录**

运行：

```powershell
idf.py build
idf.py flash monitor
```

预期：无编译错误；只有在步骤 2 被应用时，释放动画会更短。

- [ ] **步骤 5：按条件提交**

如果步骤 2 被应用：

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "perf: tune notification scroll release timing"
```

如果步骤 2 没有应用，本任务不需要提交。

---

### 任务 5：重新测量并对比优化前后

**文件：**
- 临时修改：`main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**接口：**
- 消费：`k_ui_perf_trace_enabled`。
- 产出：最终说明中使用的优化前/优化后数据。

- [ ] **步骤 1：临时开启 trace**

把：

```cpp
static constexpr bool k_ui_perf_trace_enabled = false;
```

改成：

```cpp
static constexpr bool k_ui_perf_trace_enabled = true;
```

- [ ] **步骤 2：烧录并重复任务 1 的同一组手动场景**

运行：

```powershell
idf.py flash monitor
```

手动测试：

1. 从 Home 下滑进入 Notifications。
2. 慢速拖动通知卡片 5 秒。
3. 快速拨动通知卡片 5 秒。
4. 从 Notifications 返回 Home。

- [ ] **步骤 3：和基线对比**

判断标准：

```text
好：drag max 明显低于基线，touch_loop max 不再频繁超过 16000us。
可接受：drag avg 降低，偶发峰值仍存在，但肉眼没有明显顿感。
不可接受：drag max 基本不变，肉眼顿感仍在。
```

如果不可接受，继续任务 6。
如果可接受，跳过任务 6。

- [ ] **步骤 4：关闭 trace**

恢复：

```cpp
static constexpr bool k_ui_perf_trace_enabled = false;
```

- [ ] **步骤 5：构建**

运行：

```powershell
idf.py build
```

预期：trace 关闭时构建通过。

---

### 任务 6：可选兜底——拖动期间完全移除动态阴影变化

**文件：**
- 修改：`main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`

**接口：**
- 消费：任务 2 增加的 `lightweight_drag` 参数。
- 产出：如果任务 2 不够，进一步降低拖动期渲染成本。

- [ ] **步骤 1：卡片创建时固定一次阴影**

在 `InitializeCardPagerLayer()` 中保留初始阴影，用作静态视觉深度：

```cpp
lv_obj_set_style_shadow_width(card.container, i == 0 ? 30 : 8, 0);
lv_obj_set_style_shadow_opa(card.container, i == 0 ? LV_OPA_70 : LV_OPA_20, 0);
lv_obj_set_style_shadow_offset_y(card.container, i == 0 ? 10 : 4, 0);
```

- [ ] **步骤 2：从 `ApplyNotificationScrollVisual()` 删除动态阴影改动**

从非轻量块里删除：

```cpp
lv_obj_set_style_shadow_width(card.container, near_center ? 30 : 8, 0);
lv_obj_set_style_shadow_opa(card.container, near_center ? LV_OPA_70 : LV_OPA_20, 0);
lv_obj_set_style_shadow_offset_y(card.container, near_center ? 10 : 4, 0);
```

只保留背景和边框透明度变化：

```cpp
if (!lightweight_drag) {
    lv_obj_set_style_bg_opa(card.container, near_center ? static_cast<lv_opa_t>(174) : static_cast<lv_opa_t>(82), 0);
    lv_obj_set_style_border_opa(card.container, near_center ? static_cast<lv_opa_t>(44) : static_cast<lv_opa_t>(18), 0);
}
```

- [ ] **步骤 3：重新测量**

临时打开 trace，重复任务 5。

预期：如果动态阴影是主要成本，`drag max` 会进一步降低。

- [ ] **步骤 4：如果肉眼丝滑度确实改善，再提交**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "perf: freeze notification card shadows during scroll"
```

---

### 任务 7：最终验证和文档记录

**文件：**
- 修改：`docs/xiaoxin-vertical-card-pager-plan.zh-CN.md`
- 验证：`tests/xiaoxin_card_pager_test.c`

**接口：**
- 消费：任务 1 和任务 5 得到的基线/最终测量数字。
- 产出：记录本次到底是哪些改动提升了丝滑度。

- [ ] **步骤 1：运行纯状态机回归测试**

运行：

```powershell
gcc -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o tests/xiaoxin_card_pager_test.exe
.\tests\xiaoxin_card_pager_test.exe
```

预期：

```text
xiaoxin_card_pager tests passed
```

- [ ] **步骤 2：构建固件**

运行：

```powershell
idf.py build
```

预期：构建通过。

- [ ] **步骤 3：实机验收**

运行：

```powershell
idf.py flash monitor
```

验收清单：

```text
[ ] Home 页宠物仍然正常动画。
[ ] 下滑进入 Notifications 时，卡片跟手，没有明显顿一下。
[ ] Notifications 内部通知滚动连续跟手。
[ ] 通知释放/回弹像有意图的吸附，而不是延迟卡住。
[ ] 从 Notifications 底部继续上滑返回 Home 仍然可用。
[ ] 上滑进入 Overview 仍然可用。
[ ] Overview 反向滑回 Home 仍然可用。
[ ] 系统浮层仍然可见，层级正确。
[ ] 返回 Home 后宠物动画恢复。
```

- [ ] **步骤 4：追加实施说明**

在 `docs/xiaoxin-vertical-card-pager-plan.zh-CN.md` 末尾追加：

```markdown
## 2026-06-18 丝滑度优化记录

本次卡片分页丝滑度优化把可见卡顿视为渲染预算问题处理。基线测量显示，昂贵路径主要集中在通知页拖动视觉更新，尤其是手指按住时反复修改样式、层级和阴影。实现上新增轻量拖动路径：拖动期间只连续更新位置和透明度，松手/吸附/静止阶段再恢复更完整的卡片样式。宠物 GIF 也会在卡片层刚变为可见时暂停，避免全屏宠物帧拷贝和卡片拖动抢同一段刷新预算。

验证：
- `xiaoxin_card_pager_test`：通过
- `idf.py build`：通过
- 实机检查：Home / Notifications / Overview 页面切换通过
```

- [ ] **步骤 5：提交文档和最终清理**

确认该常量为 false：

```cpp
static constexpr bool k_ui_perf_trace_enabled = false;
```

运行：

```powershell
git status --short
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc docs/xiaoxin-vertical-card-pager-plan.zh-CN.md
git commit -m "docs: record xiaoxin card pager smoothness optimization"
```

---

## 自查

1. **需求覆盖：** 本计划覆盖测量、拖动期渲染降本、宠物动画协作、释放动画调参、可选阴影兜底、测试、构建、实机验收和文档记录。
2. **占位扫描：** 没有未解决的占位标记，也没有“之后再处理”这种不可执行步骤。任务 6 虽然是可选项，但有明确进入条件和具体代码改法。
3. **类型一致性：** `ApplyNotificationScrollVisual` 新增一个默认参数 `bool lightweight_drag = false`，现有调用点仍然兼容；只有手指按住拖动路径传 `true`。
4. **风险控制：** 性能探针由编译期开关控制，最终恢复为 `false`；状态机文件不改；视觉简化默认只作用于连续拖动阶段，除非任务 6 被明确启用。
