# 小芯通知卡片左滑清理设计规格

> 状态：待评审  
> 日期：2026-06-18  
> 设备：Waveshare ESP32-S3 Touch LCD 1.46  
> 所属分支：`codex/xiaoxin-card-pager-smoothness`

## 1. 目标

把通知页做得更接近手机通知中心：

- 在通知页内，手指按在哪张通知卡片上，向左滑就清理哪张。
- 被清理的通知卡片向左飞出屏幕并淡出。
- 通知页顶部增加醒目的 `全部清理` 按钮。
- 所有通知清空后显示空状态：`暂无通知`。
- 原有竖向滚动浏览通知、从通知页返回 Home 的交互继续保留。

## 2. 非目标

- 不接入真实后端通知源。本轮仍基于当前静态通知数据做本地 dismiss/clear 状态。
- 不实现右滑操作。
- 不实现撤销。
- 不把左滑清理写进外层页面状态机；外层分页状态机继续只负责 Home / Notifications / Overview。
- 不使用真实 blur 或复杂遮罩动画。

## 3. 用户交互

### 3.1 单条左滑清理

命中规则已经确定：

> 手指按在哪张通知卡片上，左滑就清理哪张。

交互细节：

1. 用户在 Notifications 页面按下某张可见通知卡片。
2. 如果横向位移明显大于纵向位移，并且方向为向左，则进入“清理拖动”模式。
3. 拖动过程中，只有被命中的卡片跟随手指向左移动，并逐渐降低透明度。
4. 松手时：
   - 左滑超过阈值：卡片继续向左飞出屏幕，动画结束后从通知列表移除。
   - 未超过阈值：卡片回弹到原位。
5. 移除后，剩余通知卡片重新补位，当前滚动位置自动夹紧，避免空洞或越界。

建议阈值：

- 进入横向清理意图：`abs(dx) > 18px && abs(dx) > abs(dy) * 1.25`
- 触发清理：`dx <= -72px`
- 最大跟手左移：`-DISPLAY_WIDTH`
- 回弹动画：`120ms ease_out`
- 飞出动画：`160ms ease_out`

### 3.2 竖向滚动

竖向滚动保持现有语义：

- 通知未滚到底时，上下滑动浏览通知。
- 通知滚到底后继续上滑，外层通知页才收回到 Home。
- 竖向手势一旦成立，不触发左滑清理。

### 3.3 全部清理按钮

通知页顶部增加醒目的 `全部清理` 胶囊按钮：

- 建议位置：通知页顶部标题区域右侧，避开右上角系统 WiFi / 电量浮层。
- 建议视觉：蓝色描边或深蓝底色，文字 `全部清理`，圆角胶囊。
- 点击行为：清空所有通知，所有卡片淡出或直接隐藏，然后显示 `暂无通知`。
- 如果按下后发生明显滑动，则不视为点击。

### 3.4 空状态

当所有通知都被清理后：

- 隐藏所有通知卡片。
- 隐藏底部圆点分页指示器。
- 保留顶部 `全部清理` 按钮但置灰/隐藏均可；本轮推荐隐藏，避免“空列表还可清理”的困惑。
- 显示居中的空状态文案：`暂无通知`。
- 通知页仍可通过反向/边界手势返回 Home。

## 4. 数据模型

当前通知数据是静态数组 `k_notification_items[]`。本轮不改数据来源，只在 `xiaoxin_card_pager_t` 中新增本地 dismiss 状态。

建议新增字段：

```c
uint8_t notification_dismissed_mask;
```

含义：

- bit `0` 对应原始通知 0。
- bit `1` 对应原始通知 1。
- 以此类推。
- bit 为 `1` 表示该原始通知已被清理。

建议新增逻辑 API：

```c
uint8_t xiaoxin_card_pager_notification_count(const xiaoxin_card_pager_t* pager);
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

兼容策略：

- `xiaoxin_card_pager_notification_count()` 改为返回“未清理通知数量”。
- `xiaoxin_card_pager_current_notification()` 按可见通知索引返回当前通知。
- `notification_index` 始终表示“可见通知索引”，不是原始数组索引。
- `xiaoxin_card_pager_items(XIAOXIN_CARD_PAGE_NOTIFICATIONS, items, count)` 可以继续返回原始静态数组，供旧测试或静态展示使用；状态化 UI 必须改用 `xiaoxin_card_pager_notification_at()`。

## 5. UI 架构

### 5.1 新增 UI 对象

在 `PaopaoPetDisplay` 内新增：

```cpp
lv_obj_t* notification_clear_button_ = nullptr;
lv_obj_t* notification_clear_label_ = nullptr;
lv_obj_t* notification_empty_label_ = nullptr;
```

`GlassCard` 增加当前渲染的可见通知索引：

```cpp
uint8_t visible_index = 0xff;
```

### 5.2 新增触摸状态

建议新增：

```cpp
enum class NotificationGestureMode {
    None,
    VerticalScroll,
    DismissCard,
    ClearAllPress,
};

NotificationGestureMode notification_gesture_mode_ = NotificationGestureMode::None;
int8_t notification_pressed_slot_ = -1;
uint8_t notification_pressed_visible_index_ = 0xff;
int16_t notification_card_drag_x_ = 0;
bool notification_dismiss_animating_ = false;
```

作用：

- 明确区分竖向滚动、横向清理、按钮点击。
- 防止同一次触摸同时触发滚动和清理。
- 动画期间避免重复触发 dismiss。

### 5.3 命中检测

新增两个命中函数：

```cpp
int8_t NotificationCardSlotAtPoint(uint16_t x, uint16_t y) const;
bool NotificationClearButtonContains(uint16_t x, uint16_t y) const;
```

实现建议：

- 使用 `lv_obj_get_coords(obj, &area)` 获取卡片/按钮屏幕坐标。
- 只命中非隐藏对象。
- 卡片命中返回 `glass_cards_[slot].visible_index`。

## 6. 手势仲裁

在 `PollTouch()` 的 Notifications 分支中优先处理：

1. 按下瞬间：
   - 记录 `notification_drag_start_scroll_y_`。
   - 如果命中 `全部清理` 按钮，标记 `ClearAllPress` 候选。
   - 否则如果命中某张卡片，记录 slot 和 visible index。

2. 移动中：
   - 如果 `abs_dy > abs_dx`，走现有竖向滚动路径。
   - 如果 `dx < 0 && abs_dx > 18 && abs_dx > abs_dy * 1.25 && 命中卡片`，进入 `DismissCard`。
   - 进入 `DismissCard` 后，本次触摸不再触发竖向滚动。

3. 松手：
   - `DismissCard`：超过阈值飞出并 dismiss；否则回弹。
   - `ClearAllPress`：如果移动距离很小，执行全部清理。
   - `VerticalScroll`：保持现有滚动释放逻辑。

## 7. 动画

### 7.1 单条飞出

建议函数：

```cpp
void AnimateNotificationDismiss(int8_t slot, uint8_t visible_index);
```

动画：

- 起点：当前卡片 `x = notification_card_drag_x_`
- 终点：`x = -DISPLAY_WIDTH`
- 透明度：当前透明度到 `LV_OPA_0`
- 时长：`160ms`
- 完成回调：
  - 调用 `xiaoxin_card_pager_notification_dismiss(&card_pager_, visible_index)`
  - 重置卡片 `x = 0`、透明度
  - `notification_scroll_y_ = ClampNotificationScrollY(notification_scroll_y_, count)`
  - `RenderNotificationCards(items, count, prepare_entry_animation)`

### 7.2 回弹

建议函数：

```cpp
void AnimateNotificationDismissRebound(int8_t slot);
```

动画：

- 起点：当前卡片 `x`
- 终点：`0`
- 透明度回到当前滚动视觉计算值或 `LV_OPA_COVER`
- 时长：`120ms`

### 7.3 全部清理

第一版可以不做复杂逐张飞出，推荐：

1. 点击 `全部清理`。
2. 所有卡片快速淡出。
3. 调用 `xiaoxin_card_pager_notification_clear_all(&card_pager_)`。
4. 刷新为空状态。

如果追求更强动效，后续可加 stagger 飞出。

## 8. 测试策略

### 8.1 逻辑层测试

修改 `tests/xiaoxin_card_pager_test.c`，新增测试：

- `notification_dismiss_removes_visible_item`
  - 初始 count 为 4。
  - dismiss visible index 1。
  - count 变 3。
  - visible index 1 变成原来的第三条通知。

- `notification_dismiss_current_item_clamps_index`
  - 当前 index 到最后一条。
  - dismiss 最后一条。
  - current index 自动夹紧到新的最后一条。

- `notification_clear_all_empties_notifications`
  - clear all 后 count 为 0。
  - `current_notification` 返回 `NULL`。
  - next/prev 返回 false。
  - empty 返回 true。

- `notification_dismiss_invalid_index_returns_false`
  - dismiss 超出可见范围返回 false。
  - count 不变。

### 8.2 UI 层验证

本地环境无法完整编译 LVGL 固件时，至少跑：

```powershell
gcc -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o tests/xiaoxin_card_pager_test.exe
.\tests\xiaoxin_card_pager_test.exe
```

ESP-IDF 环境可用时必须跑：

```powershell
idf.py build
idf.py flash monitor
```

实机验收：

- 左滑第一张通知，第一张飞出，第二张补位。
- 左滑中间通知，只有被按住的那张飞出。
- 左滑未过阈值，卡片回弹。
- 竖向滑动仍能浏览通知。
- 横向滑动不误触返回 Home。
- 点击 `全部清理`，所有通知清空，显示 `暂无通知`。
- 清空后通知页可返回 Home。

## 9. 风险与应对

### 风险 1：横向清理和竖向滚动抢手势

应对：

- 使用明确 gesture mode。
- 只有 `abs_dx > abs_dy * 1.25` 时才进入横向清理。
- 一旦进入横向清理，本次触摸不再滚动。

### 风险 2：动画期间重复 dismiss

应对：

- 使用 `notification_dismiss_animating_` 防重入。
- 动画完成后再修改逻辑数据。

### 风险 3：静态数组无法删除

应对：

- 不修改静态数组。
- 用 dismiss mask 表示本地清理状态。
- UI 渲染使用 visible index 映射到原始 item。

### 风险 4：性能再次变差

应对：

- 拖动中只移动被命中的卡片。
- 不在拖动中重建对象。
- 不在拖动中改阴影/布局复杂属性。
- 清理完成后再刷新列表。

## 10. 验收标准

- [ ] 手指按在哪张通知上，左滑清理哪张。
- [ ] 被清理卡片向左飞出屏幕。
- [ ] 未过阈值回弹。
- [ ] 竖向通知滚动仍然可用。
- [ ] 顶部有醒目的 `全部清理` 按钮。
- [ ] 点击 `全部清理` 后显示 `暂无通知`。
- [ ] 清空通知后不会崩溃，不会越界，不会显示旧通知。
- [ ] `xiaoxin_card_pager_test` 通过。
- [ ] ESP-IDF 环境可用时 `idf.py build` 通过。
