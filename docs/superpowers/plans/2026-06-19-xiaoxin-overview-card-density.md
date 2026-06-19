# Xiaoxin Overview Card Density Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the current four-card Overview page layout while making each card show fuller information: category title, main value, and one compact detail line.

**Architecture:** Extend the existing `xiaoxin_card_item_t` model with an optional `detail` field, populate richer static Overview items, and render the detail in the existing `OverviewRow` UI objects. This keeps the card pager state machine and page layout intact; only the card content contract and row rendering become denser.

**Tech Stack:** ESP-IDF C/C++, LVGL, local GCC C regression tests.

---

## Scope

This plan only increases information density inside the existing Overview cards. It does not add dynamic weather, real course syncing, new navigation behavior, settings pages, backend APIs, or a new page layout.

## File Structure

| File | Responsibility | Planned Change |
| --- | --- | --- |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h` | Shared card pager data contract | Add `const char* detail` to `xiaoxin_card_item_t` after `body`. |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c` | Pure C card pager state and static Overview item data | Populate Overview `body` as the main value and `detail` as the extra compact line; keep notification items with `detail = NULL`. |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc` | LVGL rendering for the Waveshare round screen | Add `OverviewRow::detail`, create the LVGL label, and render `items[i].detail` under the main value. |
| `tests/xiaoxin_card_pager_test.c` | Local regression tests for pager behavior and data contracts | Add a test that locks the richer Overview content contract. |
| `docs/update.md` | Human-readable project update log | Add a short implementation note after code passes verification. |

## Design Notes

- The page still shows four rows.
- Each row still has the same icon, title, and right arrow.
- Title remains short: `下一节课`, `校园导航`, `天气`, `今日待办`.
- `body` becomes the main value: `高数 10:10`, `常用地点`, `多云 26C`, `2 项待办`.
- `detail` becomes the extra information line:
  - `教2-301 · 还有24分`
  - `教学楼 / 食堂 / 图书馆`
  - `湿度72% · 东风2级`
  - `实验报告 · 晚自习`
- Notification cards ignore `detail`; they keep rendering title and body exactly as before.

---

### Task 1: Lock the Richer Overview Data Contract

**Files:**
- Modify: `tests/xiaoxin_card_pager_test.c`
- Test: `tests/xiaoxin_card_pager_test.c`

- [ ] **Step 1: Add a failing Overview detail test**

Add this function after `overview_items_are_priority_sorted()`:

```c
static void overview_items_include_main_and_detail_text(void) {
    const xiaoxin_card_item_t* items = NULL;
    uint8_t count = 0;

    xiaoxin_card_pager_items(XIAOXIN_CARD_PAGE_OVERVIEW, &items, &count);

    assert(count == 4);
    assert(items != NULL);

    assert(strcmp(items[0].title, "下一节课") == 0);
    assert(strcmp(items[0].body, "高数 10:10") == 0);
    assert(strcmp(items[0].detail, "教2-301 · 还有24分") == 0);

    assert(strcmp(items[1].title, "校园导航") == 0);
    assert(strcmp(items[1].body, "常用地点") == 0);
    assert(strcmp(items[1].detail, "教学楼 / 食堂 / 图书馆") == 0);

    assert(strcmp(items[2].title, "天气") == 0);
    assert(strcmp(items[2].body, "多云 26C") == 0);
    assert(strcmp(items[2].detail, "湿度72% · 东风2级") == 0);

    assert(strcmp(items[3].title, "今日待办") == 0);
    assert(strcmp(items[3].body, "2 项待办") == 0);
    assert(strcmp(items[3].detail, "实验报告 · 晚自习") == 0);
}
```

Add it to `main()` immediately after `overview_items_are_priority_sorted();`:

```c
    overview_items_include_main_and_detail_text();
```

- [ ] **Step 2: Run the test and confirm it fails**

Run from repo root:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_card_pager_test.exe
```

Expected result:

```text
tests/xiaoxin_card_pager_test.c: error: 'xiaoxin_card_item_t' has no member named 'detail'
```

If the compiler line number differs, the failure is still correct when it says `detail` is not a member.

- [ ] **Step 3: Commit the failing test**

```powershell
git add tests/xiaoxin_card_pager_test.c
git commit -m "test: specify richer xiaoxin overview card content"
```

---

### Task 2: Add Overview Detail Data

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h`
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`
- Test: `tests/xiaoxin_card_pager_test.c`

- [ ] **Step 1: Extend `xiaoxin_card_item_t`**

In `xiaoxin_card_pager.h`, change the struct to:

```c
typedef struct {
  const char* title;
  const char* body;
  const char* detail;
  const char* tag;
  uint32_t priority;
  uint32_t ttl_ms;
} xiaoxin_card_item_t;
```

- [ ] **Step 2: Keep notification items compatible**

In `xiaoxin_card_pager.c`, update `notification_rebind_slot()` so notification cards explicitly have no detail line:

```c
static void notification_rebind_slot(xiaoxin_card_pager_t* pager, uint8_t slot) {
  if (pager == NULL || slot >= XIAOXIN_CARD_NOTIFICATION_MAX) {
    return;
  }
  pager->notification_items[slot].title = pager->notification_title_storage[slot];
  pager->notification_items[slot].body = pager->notification_body_storage[slot];
  pager->notification_items[slot].detail = NULL;
  pager->notification_items[slot].tag = pager->notification_tag_storage[slot];
}
```

- [ ] **Step 3: Replace static Overview data**

In `xiaoxin_card_pager.c`, replace `k_overview_items` with:

```c
static const xiaoxin_card_item_t k_overview_items[] = {
  {"下一节课", "高数 10:10", "教2-301 · 还有24分", "课程", 1, 0},
  {"校园导航", "常用地点", "教学楼 / 食堂 / 图书馆", "导航", 2, 0},
  {"天气", "多云 26C", "湿度72% · 东风2级", "天气", 3, 0},
  {"今日待办", "2 项待办", "实验报告 · 晚自习", "待办", 4, 0},
};
```

- [ ] **Step 4: Run the pager test**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_card_pager_test.exe
.\build\xiaoxin_card_pager_test.exe
```

Expected output:

```text
xiaoxin_card_pager tests passed
```

- [ ] **Step 5: Commit the model and data change**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c tests/xiaoxin_card_pager_test.c
git commit -m "feat: add richer xiaoxin overview card data"
```

---

### Task 3: Render the Detail Line in Existing Overview Cards

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Test: local compile and firmware build

- [ ] **Step 1: Add a detail label to `OverviewRow`**

Change the struct near the existing `OverviewRow` definition:

```cpp
struct OverviewRow {
    lv_obj_t* container = nullptr;
    lv_obj_t* icon_bg = nullptr;
    lv_obj_t* icon = nullptr;
    lv_obj_t* text_box = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* body = nullptr;
    lv_obj_t* detail = nullptr;
    lv_obj_t* arrow = nullptr;
};
```

- [ ] **Step 2: Slightly widen the text area without changing the card count or row positions**

Change the constants:

```cpp
static constexpr int16_t k_overview_text_w = 160;
static constexpr int16_t k_overview_arrow_x = 228;
```

Keep these constants unchanged:

```cpp
static constexpr uint8_t k_overview_row_count = 4;
static constexpr int16_t k_overview_row_w = 252;
static constexpr int16_t k_overview_row_h = 48;
static constexpr int16_t k_overview_y_start = 64;
static constexpr int16_t k_overview_row_pitch = 51;
```

- [ ] **Step 3: Create the third text line**

In `InitializeCardPagerLayer()`, replace the Overview title/body label block with:

```cpp
            row.text_box = lv_obj_create(row.container);
            lv_obj_remove_style_all(row.text_box);
            lv_obj_set_size(row.text_box, k_overview_text_w, 42);
            lv_obj_set_style_layout(row.text_box, LV_LAYOUT_NONE, 0);
            lv_obj_clear_flag(row.text_box, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(row.text_box, LV_ALIGN_LEFT_MID, k_overview_text_x, 0);

            row.title = lv_label_create(row.text_box);
            lv_obj_set_width(row.title, k_overview_text_w);
            lv_obj_set_style_text_color(row.title, lv_color_hex(k_text_primary), 0);
            lv_label_set_long_mode(row.title, LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_text(row.title, "");
            lv_obj_set_pos(row.title, 0, 0);

            row.body = lv_label_create(row.text_box);
            lv_obj_set_width(row.body, k_overview_text_w);
            lv_obj_set_style_text_color(row.body, lv_color_hex(k_title_accent), 0);
            lv_label_set_long_mode(row.body, LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_text(row.body, "");
            lv_obj_set_pos(row.body, 0, 14);

            row.detail = lv_label_create(row.text_box);
            lv_obj_set_width(row.detail, k_overview_text_w);
            lv_obj_set_style_text_color(row.detail, lv_color_hex(k_text_dimmed), 0);
            lv_label_set_long_mode(row.detail, LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_text(row.detail, "");
            lv_obj_set_pos(row.detail, 0, 28);
```

- [ ] **Step 4: Stop overriding Overview body with hard-coded compact text**

Delete this helper:

```cpp
    static const char* OverviewCompactBodyText(uint8_t row, const char* fallback) {
        static const char* k_compact_body[k_overview_row_count] = {
            "\xE9\xAB\x98\xE6\x95\xB0 10:10",
            "\xE5\xB8\xB8\xE7\x94\xA8\xE5\x9C\xB0\xE7\x82\xB9",
            "\xE5\xA4\x9A\xE4\xBA\x91 26C",
            "2 \xE9\xA1\xB9\xE5\xBE\x85\xE5\x8A\x9E"
        };
        return row < k_overview_row_count ? k_compact_body[row] : fallback;
    }
```

- [ ] **Step 5: Render body and detail from the data source**

In `RenderCardPage()`, replace this code:

```cpp
                if (row.body != nullptr) {
                    lv_label_set_text(row.body, OverviewCompactBodyText(i, items[i].body));
                }
```

with:

```cpp
                if (row.body != nullptr) {
                    lv_label_set_text(row.body, items[i].body != nullptr ? items[i].body : "");
                }
                if (row.detail != nullptr) {
                    lv_label_set_text(row.detail, items[i].detail != nullptr ? items[i].detail : "");
                }
```

- [ ] **Step 6: Build the C regression test**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_card_pager_test.exe
.\build\xiaoxin_card_pager_test.exe
```

Expected output:

```text
xiaoxin_card_pager tests passed
```

- [ ] **Step 7: Build the firmware**

```powershell
idf.py build
```

Expected output includes:

```text
Project build complete.
```

- [ ] **Step 8: Commit the LVGL rendering change**

```powershell
git add main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
git commit -m "feat: render richer xiaoxin overview cards"
```

---

### Task 4: Device Visual QA and Documentation

**Files:**
- Modify: `docs/update.md`

- [ ] **Step 1: Flash or run the current firmware on the device**

Use the project’s normal device workflow:

```powershell
idf.py flash monitor
```

Expected monitor behavior:

```text
I (...) Touch reader attached:
```

The exact touch reader name may differ by board configuration.

- [ ] **Step 2: Check the Overview page on the round LCD**

On the physical device:

1. Start from Home.
2. Swipe up into Overview.
3. Confirm four cards are still present.
4. Confirm each card shows three text levels: title, main value, detail.
5. Confirm the right arrow is not clipped by the round screen edge.
6. Confirm the detail text does not overlap the next card.
7. Confirm the pet remains visible behind the card stack just as before.

Expected visible copy:

```text
下一节课
高数 10:10
教2-301 · 还有24分

校园导航
常用地点
教学楼 / 食堂 / 图书馆

天气
多云 26C
湿度72% · 东风2级

今日待办
2 项待办
实验报告 · 晚自习
```

- [ ] **Step 3: If text clips, make the smallest layout adjustment**

If the detail line clips horizontally, change only these constants:

```cpp
static constexpr int16_t k_overview_text_w = 166;
static constexpr int16_t k_overview_arrow_x = 232;
```

If the third line clips vertically, change only these positions:

```cpp
            lv_obj_set_pos(row.body, 0, 13);
            lv_obj_set_pos(row.detail, 0, 27);
```

After either adjustment, rerun:

```powershell
idf.py build
```

Expected output includes:

```text
Project build complete.
```

- [ ] **Step 4: Update `docs/update.md`**

Append this note near the latest update section:

```markdown
## 2026-06-19 小芯总览卡片信息密度优化

- 总览页保留原有四张卡片布局、图标和右侧箭头。
- 每张卡片从“标题 + 单行正文”扩展为“标题 + 主信息 + 辅助信息”。
- 总览数据模型新增 `detail` 字段；通知卡片不使用该字段，原有通知渲染保持不变。
- 已验证 `xiaoxin_card_pager_test` 通过，并完成固件构建。
```

- [ ] **Step 5: Commit QA documentation**

```powershell
git add docs/update.md
git commit -m "docs: record xiaoxin overview density update"
```

---

## Final Verification

- [ ] Run the C regression test:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/xiaoxin_card_pager_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c -o build/xiaoxin_card_pager_test.exe
.\build\xiaoxin_card_pager_test.exe
```

Expected output:

```text
xiaoxin_card_pager tests passed
```

- [ ] Run the firmware build:

```powershell
idf.py build
```

Expected output includes:

```text
Project build complete.
```

- [ ] Perform physical Overview page check:

```text
Home 上滑进入总览页；四张卡片仍在；每张卡片显示标题、主信息、辅助信息；文本不重叠、不明显裁切。
```

## Self-Review

- Spec coverage: The user asked to keep the current Overview design for now and only make each card show fuller information. Tasks 1-3 implement that exact scope; Task 4 verifies it on-device.
- Placeholder scan: No step relies on unspecified behavior. Every code edit includes concrete snippets and every verification step includes commands and expected output.
- Type consistency: `xiaoxin_card_item_t.detail`, `OverviewRow::detail`, and `items[i].detail` are introduced once and used consistently. Notification cards explicitly set `detail = NULL` and continue to render only title/body.
