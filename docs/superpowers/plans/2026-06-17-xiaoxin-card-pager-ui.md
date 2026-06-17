# 小芯卡片分页 UI 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将卡片分页 UI 从简陋的白色文字叠加层改造为深色极简科技风毛玻璃卡片界面，通知页使用层叠玻璃卡片+彩色状态点，总览页使用图标行+细线分隔。

**Architecture:** 所有改动在 `esp32-s3-touch-lcd-1.46.cc` 的 `PaopaoPetDisplay` 类内完成。核心是将现有的 `card_item_labels_[4]` 纯文字标签替换为两组结构化元素：3 个毛玻璃卡片（通知页）和 4 个图标行（总览页）。页面切换逻辑（状态机、手势、吸附动画）完全不动，只改渲染层。

**Tech Stack:** C++ (ESP-IDF), LVGL 9.x, GCC

---

## File Map

| 文件 | 作用 | 改动 |
|------|------|------|
| `esp32-s3-touch-lcd-1.46.cc` | 唯一修改文件，包含所有 UI 代码 | 常量区 + 成员变量 + 5 个函数 |
| `xiaoxin_card_pager.h` | 卡片状态机 API | 不改 |
| `xiaoxin_card_pager.c` | 卡片状态机实现 | 不改 |
| `xiaoxin_card_pager_test.c` | 纯逻辑测试 | 不改（渲染层改动不影响状态机测试） |

---

### Task 1: 添加设计常量、数据结构和新成员变量

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc:78-78`（常量区）
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc:502-505`（成员变量区）

- [ ] **Step 1: 替换常量 `k_card_item_label_count` 为新的设计常量**

在 `k_card_snap_anim_ms = 210;` 行之后，将：
```cpp
static constexpr uint8_t k_card_item_label_count = 4;
```
替换为：
```cpp
static constexpr uint8_t k_card_glass_count = 3;
static constexpr uint8_t k_overview_row_count = 4;
static constexpr uint8_t k_overview_sep_count = 3;

// Card glass colors (notifications page)
static constexpr uint32_t k_card_bg_color = 0x4a9eff;
// Glass card background opacities: highest → lowest priority
static constexpr lv_opa_t k_glass_bg_opa[3] = {LV_OPA_8, LV_OPA_5, LV_OPA_3};
static constexpr lv_opa_t k_glass_border_opa[3] = {LV_OPA_14, LV_OPA_8, LV_OPA_5};
static constexpr int16_t k_glass_radius = 12;
static constexpr int16_t k_glass_width = 195;
static constexpr int16_t k_glass_pad_v = 10;
static constexpr int16_t k_glass_pad_h = 14;
static constexpr int16_t k_glass_gap = 8;

// Status dot
static constexpr int16_t k_dot_size = 8;
static constexpr uint32_t k_dot_color_urgent = 0xff5e5b;
static constexpr uint32_t k_dot_color_warning = 0xffb84d;
static constexpr uint32_t k_dot_color_info = 0x4fc3f7;

// Overview page icons
static constexpr int16_t k_ov_icon_size = 28;
static constexpr int16_t k_ov_icon_radius = 8;
static constexpr uint32_t k_ov_icon_bg_course = 0x4fc3f7;
static constexpr uint32_t k_ov_icon_bg_nav = 0xa0d468;
static constexpr uint32_t k_ov_icon_bg_weather = 0xffb84d;
static constexpr uint32_t k_ov_icon_bg_todo = 0xac92ec;

// Shared colors
static constexpr uint32_t k_dark_bg = 0x0c0f16;
static constexpr uint32_t k_title_accent = 0x4a9eff;
static constexpr uint32_t k_text_primary = 0xe8eaed;
static constexpr uint32_t k_text_secondary = 0x7d9cc6;
static constexpr uint32_t k_text_dimmed = 0xdde4ed;
static constexpr uint32_t k_indicator_top = 0x2a4a6b;
static constexpr uint32_t k_indicator_home = 0x1e3350;
static constexpr uint32_t k_separator_color = 0x4a9eff;
static constexpr lv_opa_t k_separator_opa = LV_OPA_8;

// Entry animation
static constexpr uint32_t k_entry_fade_ms = 120;
static constexpr uint32_t k_entry_stagger_ms = 50;
```

- [ ] **Step 2: 添加卡片和概览行的数据结构**

在 `PaopaoPetDisplay` 类的 private 区域，在 `#include` 之后、类定义之前添加：
```cpp
struct GlassCard {
    lv_obj_t* container = nullptr;
    lv_obj_t* dot = nullptr;
    lv_obj_t* label = nullptr;
    lv_obj_t* arrow = nullptr;
};

struct OverviewRow {
    lv_obj_t* container = nullptr;
    lv_obj_t* icon_bg = nullptr;
    lv_obj_t* label = nullptr;
    lv_obj_t* arrow = nullptr;
};
```

- [ ] **Step 3: 替换成员变量**

将：
```cpp
lv_obj_t* card_item_labels_[k_card_item_label_count] = {};
```
替换为：
```cpp
lv_obj_t* pull_indicator_ = nullptr;
lv_obj_t* home_indicator_ = nullptr;
GlassCard glass_cards_[k_card_glass_count];
OverviewRow overview_rows_[k_overview_row_count];
lv_obj_t* overview_separators_[k_overview_sep_count] = {};
```

- [ ] **Step 4: 提交**

```bash
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "refactor: add card UI design constants and data structures for glass-morphism cards"
```

---

### Task 2: 重写 InitializeCardPagerLayer() 创建深色毛玻璃卡片元素

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc:603-632`

- [ ] **Step 1: 完整替换 InitializeCardPagerLayer() 函数体**

将第 603-632 行完整的 `InitializeCardPagerLayer()` 替换为：

```cpp
void InitializeCardPagerLayer() {
    lv_obj_t* screen = lv_screen_active();

    card_layer_ = lv_obj_create(screen);
    lv_obj_remove_style_all(card_layer_);
    lv_obj_set_size(card_layer_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(card_layer_, lv_color_hex(k_dark_bg), 0);
    lv_obj_set_style_bg_opa(card_layer_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card_layer_, 0, 0);
    lv_obj_set_scrollbar_mode(card_layer_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(card_layer_, LV_ALIGN_CENTER, 0, 0);

    // Pull indicator bar (top, for notifications page)
    pull_indicator_ = lv_obj_create(card_layer_);
    lv_obj_remove_style_all(pull_indicator_);
    lv_obj_set_size(pull_indicator_, 34, 3);
    lv_obj_set_style_bg_color(pull_indicator_, lv_color_hex(k_indicator_top), 0);
    lv_obj_set_style_radius(pull_indicator_, 2, 0);
    lv_obj_align(pull_indicator_, LV_ALIGN_TOP_MID, 0, 24);

    // Home indicator bar (bottom)
    home_indicator_ = lv_obj_create(card_layer_);
    lv_obj_remove_style_all(home_indicator_);
    lv_obj_set_size(home_indicator_, 56, 3);
    lv_obj_set_style_bg_color(home_indicator_, lv_color_hex(k_indicator_home), 0);
    lv_obj_set_style_radius(home_indicator_, 2, 0);
    lv_obj_align(home_indicator_, LV_ALIGN_BOTTOM_MID, 0, -8);

    // Page title label
    card_title_label_ = lv_label_create(card_layer_);
    lv_obj_set_width(card_title_label_, 260);
    lv_obj_set_style_text_align(card_title_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(card_title_label_, lv_color_hex(k_title_accent), 0);
    lv_label_set_text(card_title_label_, "");

    // ---- Notification glass cards (3) ----
    for (uint8_t i = 0; i < k_card_glass_count; ++i) {
        GlassCard& gc = glass_cards_[i];

        gc.container = lv_obj_create(card_layer_);
        lv_obj_remove_style_all(gc.container);
        lv_obj_set_size(gc.container, k_glass_width, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(gc.container, k_glass_radius, 0);
        lv_obj_set_style_bg_color(gc.container, lv_color_hex(k_card_bg_color), 0);
        lv_obj_set_style_bg_opa(gc.container, k_glass_bg_opa[i], 0);
        lv_obj_set_style_border_color(gc.container, lv_color_hex(k_card_bg_color), 0);
        lv_obj_set_style_border_opa(gc.container, k_glass_border_opa[i], 0);
        lv_obj_set_style_border_width(gc.container, i == 0 ? 2 : 1, 0);
        lv_obj_set_style_pad_top(gc.container, k_glass_pad_v, 0);
        lv_obj_set_style_pad_bottom(gc.container, k_glass_pad_v, 0);
        lv_obj_set_style_pad_left(gc.container, k_glass_pad_h, 0);
        lv_obj_set_style_pad_right(gc.container, k_glass_pad_h, 0);
        lv_obj_set_flex_flow(gc.container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(gc.container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_align(gc.container, LV_ALIGN_TOP_MID, 0,
            (int32_t)84 + (int32_t)i * (int32_t)(LV_SIZE_CONTENT + k_glass_gap));
        lv_obj_add_flag(gc.container, LV_OBJ_FLAG_HIDDEN);

        // Status dot
        gc.dot = lv_obj_create(gc.container);
        lv_obj_remove_style_all(gc.dot);
        lv_obj_set_size(gc.dot, k_dot_size, k_dot_size);
        lv_obj_set_style_radius(gc.dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(gc.dot, lv_color_hex(k_dot_color_info), 0);
        lv_obj_set_style_bg_opa(gc.dot, LV_OPA_COVER, 0);

        // Arrow indicator
        gc.arrow = lv_label_create(gc.container);
        lv_obj_set_style_text_color(gc.arrow, lv_color_hex(k_title_accent), 0);
        lv_label_set_text(gc.arrow, "›");

        // Text label (title\nbody)
        gc.label = lv_label_create(gc.container);
        lv_obj_set_flex_grow(gc.label, 1);
        lv_obj_set_style_pad_left(gc.label, 8, 0);
        lv_obj_set_style_text_color(gc.label, lv_color_hex(k_text_primary), 0);
        lv_label_set_text(gc.label, "");
    }

    // ---- Overview rows (4) ----
    for (uint8_t i = 0; i < k_overview_row_count; ++i) {
        OverviewRow& row = overview_rows_[i];

        row.container = lv_obj_create(card_layer_);
        lv_obj_remove_style_all(row.container);
        lv_obj_set_size(row.container, k_glass_width, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row.container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(row.container, 8, 0);
        lv_obj_set_flex_flow(row.container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row.container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_align(row.container, LV_ALIGN_TOP_MID, 0, (int32_t)40 + (int32_t)i * 48);
        lv_obj_add_flag(row.container, LV_OBJ_FLAG_HIDDEN);

        // Icon background (28x28 rounded square)
        row.icon_bg = lv_obj_create(row.container);
        lv_obj_remove_style_all(row.icon_bg);
        lv_obj_set_size(row.icon_bg, k_ov_icon_size, k_ov_icon_size);
        lv_obj_set_style_radius(row.icon_bg, k_ov_icon_radius, 0);
        lv_obj_set_style_bg_color(row.icon_bg, lv_color_hex(k_ov_icon_bg_weather), 0);
        lv_obj_set_style_bg_opa(row.icon_bg, LV_OPA_12, 0);

        // Arrow
        row.arrow = lv_label_create(row.container);
        lv_obj_set_style_text_color(row.arrow, lv_color_hex(k_title_accent), 0);
        lv_label_set_text(row.arrow, "›");

        // Text label
        row.label = lv_label_create(row.container);
        lv_obj_set_flex_grow(row.label, 1);
        lv_obj_set_style_pad_left(row.label, 10, 0);
        lv_obj_set_style_text_color(row.label, lv_color_hex(k_text_primary), 0);
        lv_label_set_text(row.label, "");

        // Separator line (not after last row)
        if (i < k_overview_sep_count) {
            overview_separators_[i] = lv_obj_create(card_layer_);
            lv_obj_remove_style_all(overview_separators_[i]);
            lv_obj_set_size(overview_separators_[i], 180, 1);
            lv_obj_set_style_bg_color(overview_separators_[i], lv_color_hex(k_separator_color), 0);
            lv_obj_set_style_bg_opa(overview_separators_[i], k_separator_opa, 0);
            lv_obj_align(overview_separators_[i], LV_ALIGN_TOP_MID, 0, (int32_t)(40 + 48 * (i + 1) - 1));
            lv_obj_add_flag(overview_separators_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_add_flag(card_layer_, LV_OBJ_FLAG_HIDDEN);
}
```

- [ ] **Step 2: 提交**

```bash
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "feat: rewrite InitializeCardPagerLayer with glass cards and overview icon rows"
```

---

### Task 3: 重写 RenderCardPage() 区分通知页和总览页渲染

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc:662-701`

- [ ] **Step 1: 添加状态点颜色辅助函数**

在 `RenderCardPage` 函数之前插入：

```cpp
static uint32_t DotColorForPriority(uint32_t priority) {
    if (priority <= 1) return k_dot_color_urgent;
    if (priority == 2) return k_dot_color_warning;
    return k_dot_color_info;
}

static uint32_t OverviewIconBgColorForTag(const char* tag) {
    if (tag == nullptr) return k_ov_icon_bg_weather;
    if (std::strstr(tag, "课程")) return k_ov_icon_bg_course;
    if (std::strstr(tag, "导航")) return k_ov_icon_bg_nav;
    if (std::strstr(tag, "天气")) return k_ov_icon_bg_weather;
    if (std::strstr(tag, "待办")) return k_ov_icon_bg_todo;
    return k_ov_icon_bg_weather;
}
```

- [ ] **Step 2: 完整替换 RenderCardPage() 函数体**

将第 662-701 行替换为：

```cpp
void RenderCardPage(xiaoxin_card_page_t page) {
    if (card_layer_ == nullptr) {
        return;
    }

    // Hide all card elements first
    for (uint8_t i = 0; i < k_card_glass_count; ++i) {
        lv_obj_add_flag(glass_cards_[i].container, LV_OBJ_FLAG_HIDDEN);
    }
    for (uint8_t i = 0; i < k_overview_row_count; ++i) {
        lv_obj_add_flag(overview_rows_[i].container, LV_OBJ_FLAG_HIDDEN);
    }
    for (uint8_t i = 0; i < k_overview_sep_count; ++i) {
        if (overview_separators_[i] != nullptr) {
            lv_obj_add_flag(overview_separators_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_add_flag(pull_indicator_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(home_indicator_, LV_OBJ_FLAG_HIDDEN);

    if (page == XIAOXIN_CARD_PAGE_HOME) {
        lv_obj_add_flag(card_layer_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const xiaoxin_card_item_t* items = nullptr;
    uint8_t count = 0;
    xiaoxin_card_pager_items(page, &items, &count);

    lv_obj_remove_flag(card_layer_, LV_OBJ_FLAG_HIDDEN);

    if (page == XIAOXIN_CARD_PAGE_NOTIFICATIONS) {
        // ---- NOTIFICATIONS: glass cards ----
        lv_obj_align(card_title_label_, LV_ALIGN_TOP_MID, 0, 48);
        lv_label_set_text(card_title_label_, "通知");

        lv_obj_remove_flag(pull_indicator_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(home_indicator_, LV_OBJ_FLAG_HIDDEN);

        const uint8_t visible = std::min<uint8_t>(count, k_card_glass_count);
        for (uint8_t i = 0; i < visible && items != nullptr; ++i) {
            GlassCard& gc = glass_cards_[i];

            // Status dot color
            lv_obj_set_style_bg_color(gc.dot, lv_color_hex(DotColorForPriority(items[i].priority)), 0);
            // Urgent items get shadow glow
            if (items[i].priority <= 1) {
                lv_obj_set_style_shadow_color(gc.dot, lv_color_hex(k_dot_color_urgent), 0);
                lv_obj_set_style_shadow_width(gc.dot, 8, 0);
                lv_obj_set_style_shadow_opa(gc.dot, LV_OPA_40, 0);
            } else {
                lv_obj_set_style_shadow_width(gc.dot, 0, 0);
                lv_obj_set_style_shadow_opa(gc.dot, LV_OPA_TRANSP, 0);
            }

            // Text: "title\nbody"
            char text[96];
            std::snprintf(text, sizeof(text), "%s\n%s", items[i].title, items[i].body);
            lv_label_set_text(gc.label, text);

            // Arrow color: dimmer for lower cards
            const uint32_t arrow_colors[] = {k_title_accent, 0x3d7ab8, 0x2d5a8a};
            lv_obj_set_style_text_color(gc.arrow, lv_color_hex(arrow_colors[i]), 0);

            lv_obj_remove_flag(gc.container, LV_OBJ_FLAG_HIDDEN);
            // Reset opacity for entry animation
            lv_obj_set_style_opa(gc.container, LV_OPA_0, 0);
        }
    } else {
        // ---- OVERVIEW: icon rows ----
        lv_obj_align(card_title_label_, LV_ALIGN_BOTTOM_MID, 0, -28);
        lv_label_set_text(card_title_label_, "总览");

        lv_obj_remove_flag(home_indicator_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(home_indicator_, LV_ALIGN_TOP_MID, 0, 8);

        const uint8_t visible = std::min<uint8_t>(count, k_overview_row_count);
        for (uint8_t i = 0; i < visible && items != nullptr; ++i) {
            OverviewRow& row = overview_rows_[i];

            // Icon background color by tag
            lv_obj_set_style_bg_color(row.icon_bg,
                lv_color_hex(OverviewIconBgColorForTag(items[i].tag)), 0);

            char text[96];
            std::snprintf(text, sizeof(text), "%s\n%s", items[i].title, items[i].body);
            lv_label_set_text(row.label, text);

            // Dimmer arrow for lower rows
            const uint32_t arrow_colors[] = {k_title_accent, 0x3d7ab8, 0x3d7ab8, 0x2d5a8a};
            lv_obj_set_style_text_color(row.arrow, lv_color_hex(arrow_colors[i]), 0);

            lv_obj_remove_flag(row.container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_opa(row.container, LV_OPA_0, 0);

            if (i < k_overview_sep_count && overview_separators_[i] != nullptr) {
                lv_obj_remove_flag(overview_separators_[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_opa(overview_separators_[i], LV_OPA_0, 0);
            }
        }
    }

    lv_obj_move_foreground(card_layer_);
    RaiseOverlayObjects();
}
```

- [ ] **Step 3: 提交**

```bash
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "feat: rewrite RenderCardPage with separate glass card and overview row rendering"
```

---

### Task 4: 更新 ApplyCardPagerVisual() 与卡片动画回调

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc:703-761`

- [ ] **Step 1: 添加卡片渐入动画函数**

在 `CardLayerAnimationCompleted` 之后添加 `ApplyCardEntryAnimation`：

在 `CardLayerAnimationCompleted` 函数（约第 660 行）之后插入：

```cpp
void ApplyCardEntryAnimation(xiaoxin_card_page_t page) {
    if (page == XIAOXIN_CARD_PAGE_NOTIFICATIONS) {
        for (uint8_t i = 0; i < k_card_glass_count; ++i) {
            GlassCard& gc = glass_cards_[i];
            if (lv_obj_has_flag(gc.container, LV_OBJ_FLAG_HIDDEN)) continue;

            lv_anim_t anim;
            lv_anim_init(&anim);
            lv_anim_set_var(&anim, gc.container);
            lv_anim_set_values(&anim, LV_OPA_0, LV_OPA_COVER);
            lv_anim_set_time(&anim, k_entry_fade_ms);
            lv_anim_set_delay(&anim, (uint32_t)i * k_entry_stagger_ms);
            lv_anim_set_exec_cb(&anim, [](void* obj, int32_t v) {
                lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)v, 0);
            });
            lv_anim_start(&anim);
        }
    } else if (page == XIAOXIN_CARD_PAGE_OVERVIEW) {
        for (uint8_t i = 0; i < k_overview_row_count; ++i) {
            OverviewRow& row = overview_rows_[i];
            if (lv_obj_has_flag(row.container, LV_OBJ_FLAG_HIDDEN)) continue;

            lv_anim_t anim;
            lv_anim_init(&anim);
            lv_anim_set_var(&anim, row.container);
            lv_anim_set_values(&anim, LV_OPA_0, LV_OPA_COVER);
            lv_anim_set_time(&anim, k_entry_fade_ms);
            lv_anim_set_delay(&anim, (uint32_t)i * k_entry_stagger_ms);
            lv_anim_set_exec_cb(&anim, [](void* obj, int32_t v) {
                lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)v, 0);
            });
            lv_anim_start(&anim);

            // Also animate separator
            if (i < k_overview_sep_count && overview_separators_[i] != nullptr &&
                !lv_obj_has_flag(overview_separators_[i], LV_OBJ_FLAG_HIDDEN)) {
                lv_anim_t sep_anim;
                lv_anim_init(&sep_anim);
                lv_anim_set_var(&sep_anim, overview_separators_[i]);
                lv_anim_set_values(&sep_anim, LV_OPA_0, k_separator_opa);
                lv_anim_set_time(&sep_anim, k_entry_fade_ms);
                lv_anim_set_delay(&sep_anim, (uint32_t)i * k_entry_stagger_ms);
                lv_anim_set_exec_cb(&sep_anim, [](void* obj, int32_t v) {
                    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)v, 0);
                });
                lv_anim_start(&sep_anim);
            }
        }
    }
}
```

- [ ] **Step 2: 更新 CardLayerAnimationCompleted 触发渐入动画**

在 `CardLayerAnimationCompleted` 函数（第 650-660 行）中，将：
```cpp
static void CardLayerAnimationCompleted(lv_anim_t* anim) {
    auto* self = static_cast<PaopaoPetDisplay*>(lv_anim_get_user_data(anim));
    if (self == nullptr || self->card_layer_ == nullptr) {
        return;
    }
    lv_obj_set_y(self->card_layer_, 0);
    lv_obj_set_style_opa(self->card_layer_, LV_OPA_COVER, 0);
    if (xiaoxin_card_pager_current_page(&self->card_pager_) == XIAOXIN_CARD_PAGE_HOME) {
        lv_obj_add_flag(self->card_layer_, LV_OBJ_FLAG_HIDDEN);
    }
}
```
替换为：
```cpp
static void CardLayerAnimationCompleted(lv_anim_t* anim) {
    auto* self = static_cast<PaopaoPetDisplay*>(lv_anim_get_user_data(anim));
    if (self == nullptr || self->card_layer_ == nullptr) {
        return;
    }
    lv_obj_set_y(self->card_layer_, 0);
    lv_obj_set_style_opa(self->card_layer_, LV_OPA_COVER, 0);
    if (xiaoxin_card_pager_current_page(&self->card_pager_) == XIAOXIN_CARD_PAGE_HOME) {
        lv_obj_add_flag(self->card_layer_, LV_OBJ_FLAG_HIDDEN);
    } else {
        self->ApplyCardEntryAnimation(
            xiaoxin_card_pager_current_page(&self->card_pager_));
    }
}
```

- [ ] **Step 3: 更新 ApplyCardPagerVisual() 移除旧引用**

在 `ApplyCardPagerVisual()` 中，`card_item_labels_` 已不存在，但该函数只操作 `card_layer_` 本身，不直接引用 labels。确认第 703-728 行代码无需修改（仅操作 `card_layer_` 的 y 偏移和透明度），但移除对 opacity 的条件设置：

将第 722-727 行：
```cpp
lv_obj_set_y(card_layer_, offset / k_card_layer_follow_divisor);
lv_obj_set_style_opa(
    card_layer_,
    xiaoxin_card_pager_is_dragging(&card_pager_) ? LV_OPA_80 : LV_OPA_COVER,
    0
);
```
保持不变（这段已经是正确的）。

- [ ] **Step 4: 提交**

```bash
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "feat: add staggered card entry animation and wire into snap-complete callback"
```

---

### Task 5: 更新 AnimateCardPagerRelease() 和 SetupUI() 暗色背景

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc:730-753`（AnimateCardPagerRelease）
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc:357-393`（SetupUI 中 card_layer_ bg）

- [ ] **Step 1: 确认 AnimateCardPagerRelease 兼容性**

阅读现有 `AnimateCardPagerRelease()`（第 730-753 行），该函数在松手吸附时播放 card_layer_ 的 y 位移动画。不需要修改逻辑，但确认它在 `visual_page != HOME` 时正确调用 `RenderCardPage`。这已经存在，无需改动。

- [ ] **Step 2: 确保 SetupUI() 中 card_layer_ background 已更新**

在 `SetupUI()` 中，`InitializeCardPagerLayer()` 已经设置了 `card_layer_` 的背景为 `k_dark_bg`（在 Task 2 中完成）。但需要确认 `SetupUI()` 中没有覆盖这个设置。检查第 357-393 行 — `InitializeCardPagerLayer()` 在第 380 行被调用，这是该函数中唯一设置 card_layer_ 的地方。无需额外修改。

- [ ] **Step 3: 编译验证**

```bash
idf.py build
```
预期：编译通过，无错误（可能需要解决一些未使用变量警告）。

- [ ] **Step 4: 提交**

```bash
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "feat: finalize dark glass-morphism card pager UI integration"
```

---

### Task 6: 最终验证与清理

**Files:**
- 验证: `tests/xiaoxin_card_pager_test.c`（确认已有测试仍然通过）

- [ ] **Step 1: 运行卡片分页逻辑测试（如果构建环境支持）**

```bash
cd tests && gcc -I.. xiaoxin_card_pager_test.c ../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o pager_test && ./pager_test
```
预期：`xiaoxin_card_pager tests passed`

> 注：此测试只验证状态机逻辑，不涉及 LVGL UI 层。

- [ ] **Step 2: 视觉验收对照设计规格**

逐一检查 spec 中的验收标准：

| 验收项 | 对应代码 | 状态 |
|--------|----------|------|
| 通知页深色毛玻璃卡片 3 条 | `RenderCardPage` → `XIAOXIN_CARD_PAGE_NOTIFICATIONS` | 实机验证 |
| 总览页图标+分隔线 4 条目 | `RenderCardPage` → `XIAOXIN_CARD_PAGE_OVERVIEW` | 实机验证 |
| 状态点颜色正确+紧急性光晕 | `DotColorForPriority()` + shadow on priority≤1 | 实机验证 |
| 卡片颜色/透明度/间距 | `k_glass_bg_opa`, `k_glass_border_opa`, 布局参数 | 实机验证 |
| 页面切换动画流畅 | `AnimateCardPagerRelease` + `ApplyCardPagerVisual` | 实机验证 |
| Home 页白色宠物不变 | `SetupUI` 未改动 screen 背景 | 实机验证 |
| 卡片渐入动画 | `ApplyCardEntryAnimation` | 实机验证 |

- [ ] **Step 3: 烧录实机验证**

```bash
idf.py flash monitor
```

手动测试：
1. 首页确认宠物白色背景正常
2. 从顶部下滑 → 通知页（3 张毛玻璃卡片，状态点颜色正确）
3. 松手 → 吸附动画 + 卡片渐入
4. 从底部上滑 → 总览页（图标行 + 分隔线）
5. 反向滑动回首页
6. 短拖动回弹

- [ ] **Step 4: 提交最终版本**

```bash
git add -A
git commit -m "feat: complete dark glass-morphism card pager UI implementation"
```

---

## 自审

1. **Spec coverage:** 逐项对照设计规格第 2-9 节——色彩系统、布局参数、动画规范、LVGL 结构、页面差异化、集成点、验收标准均在计划中覆盖。色彩值已编码为 `k_*` 常量。所有新增 UI 元素符合 LVGL 对象树结构。`InitializeCardPagerLayer()` 中不包括真正的 blur（规格第 6.3 节已确认用半透明背景替代）。

2. **Placeholder scan:** 无 "TBD"、"TODO"、"implement later"。每步包含完整代码，每条命令有明确预期。

3. **Type consistency:**
   - `GlassCard` / `OverviewRow` 结构定义于 Task 1，使用于 Task 2-4
   - `k_card_glass_count` (3) 定义于 Task 1，所有循环使用
   - `k_overview_row_count` (4) 定义于 Task 1，所有循环使用
   - `k_overview_sep_count` (3) 定义于 Task 1 = `k_overview_row_count - 1`
   - 函数名一致：`ApplyCardEntryAnimation`, `DotColorForPriority`, `OverviewIconBgColorForTag`
   - `card_title_label_` 保留原有变量名，不破坏其他引用（`CardTitleForPage` 辅助函数使用 `card_title_label_`）
   - 已删除的 `card_item_labels_[]` 在所有修改函数中不再引用

   **确认遗留引用：** `card_item_labels_` 仅在已替换的 `InitializeCardPagerLayer` 和 `RenderCardPage` 中被使用。这两个函数都在本计划中完整替换，因此无遗留引用。
