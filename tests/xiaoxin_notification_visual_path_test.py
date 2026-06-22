from pathlib import Path
import re


SOURCE = Path(
    "main/boards/waveshare/esp32-s3-touch-lcd-1.46/"
    "esp32-s3-touch-lcd-1.46.cc"
)


def read_source() -> str:
    return SOURCE.read_text(encoding="utf-8")


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
                return source[brace + 1 : index]
    raise AssertionError(f"function body not found: {signature}")


def normalize_whitespace(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def test_notification_scroll_animation_uses_lightweight_visual_path():
    body = function_body(
        read_source(),
        "static void NotificationScrollSetY(void* obj, int32_t scroll_y)",
    )

    assert "ApplyNotificationScrollVisual((int16_t)scroll_y, false, true);" in body


def test_touch_point_logging_only_happens_on_press_transition():
    body = function_body(read_source(), "void PollTouch(uint32_t now_ms)")

    assert "now_ms - touch_last_point_log_ms_ >= 1000" not in body


def test_moving_notification_card_containers_do_not_render_shadows():
    source = read_source()

    assert "lv_obj_set_style_shadow_color(card.container" not in source
    assert "lv_obj_set_style_shadow_width(card.container" not in source
    assert "lv_obj_set_style_shadow_opa(card.container" not in source
    assert "lv_obj_set_style_shadow_offset_y(card.container" not in source


def test_card_pager_layer_has_subtle_empty_state_background():
    source = read_source()
    body = function_body(source, "void InitializeCardPagerLayer()")

    assert "static constexpr uint32_t k_card_layer_bg_color = 0xe9edf3;" in source
    assert "static constexpr lv_opa_t k_card_layer_bg_opa = static_cast<lv_opa_t>(18);" in source
    assert "lv_obj_set_style_bg_color(card_layer_, lv_color_hex(k_card_layer_bg_color), 0);" in body
    assert "lv_obj_set_style_bg_opa(card_layer_, k_card_layer_bg_opa, 0);" in body
    assert "lv_obj_set_style_bg_opa(card_layer_, LV_OPA_TRANSP, 0);" not in body


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


def test_overview_page_consumes_overview_model_snapshot():
    source = read_source()
    body = function_body(source, "void RenderCardPage(xiaoxin_card_page_t page, bool prepare_entry_animation = false)")

    assert '#include "xiaoxin_overview_model.h"' in source
    assert "xiaoxin_overview_state_t overview_state = BuildOverviewState();" in body
    assert "xiaoxin_overview_model_build(&overview_state, &overview_snapshot_);" in body
    assert "const xiaoxin_card_item_t* items = overview_snapshot_.items;" in body
    assert "const uint8_t count = overview_snapshot_.item_count;" in body
    assert "xiaoxin_card_pager_items(page, &items, &count);" not in body


def test_overview_time_labels_are_hidden_before_page_specific_rendering():
    body = function_body(
        read_source(),
        "void RenderCardPage(xiaoxin_card_page_t page, bool prepare_entry_animation = false)",
    )

    assert "AddFlagIfCreated(overview_time_label_, LV_OBJ_FLAG_HIDDEN);" in body
    assert "AddFlagIfCreated(overview_date_label_, LV_OBJ_FLAG_HIDDEN);" in body


def test_status_bar_refreshes_visible_overview_snapshot():
    source = read_source()
    status_body = function_body(source, "virtual void UpdateStatusBar(bool update_all = false) override")
    refresh_body = function_body(source, "void RefreshOverviewPageIfVisible()")

    assert "RefreshOverviewPageIfVisible();" in status_body
    assert "rendered_card_page_ != XIAOXIN_CARD_PAGE_OVERVIEW" in refresh_body
    assert "lv_obj_has_flag(card_layer_, LV_OBJ_FLAG_HIDDEN)" in refresh_body
    assert "RenderCardPage(XIAOXIN_CARD_PAGE_OVERVIEW, false);" in refresh_body


def test_empty_notifications_use_prominent_panel():
    source = read_source()
    init_body = function_body(source, "void InitializeCardPagerLayer()")
    render_body = function_body(
        source,
        "void RenderNotificationCards(const xiaoxin_card_item_t* /*items*/, uint8_t count, bool prepare_entry_animation)",
    )

    assert "lv_obj_t* notification_empty_panel_ = nullptr;" in source
    assert "lv_obj_set_style_bg_color(notification_empty_panel_, lv_color_hex(k_notification_empty_panel_bg), 0);" in init_body
    assert "lv_obj_set_style_bg_opa(notification_empty_panel_, k_notification_empty_panel_opa, 0);" in init_body
    assert "lv_obj_set_style_text_color(notification_empty_label_, lv_color_hex(k_page_title_color), 0);" in init_body
    assert "RemoveFlagIfCreated(notification_empty_panel_, LV_OBJ_FLAG_HIDDEN);" in render_body
    assert "AddFlagIfCreated(notification_empty_panel_, LV_OBJ_FLAG_HIDDEN);" in render_body


def test_notification_clear_button_stays_centered_on_notifications_page():
    source = read_source()
    init_body = function_body(source, "void InitializeCardPagerLayer()")
    render_body = function_body(
        source,
        "void RenderCardPage(xiaoxin_card_page_t page, bool prepare_entry_animation = false)",
    )

    assert "lv_obj_align(notification_clear_button_, LV_ALIGN_TOP_MID, 0, k_notification_clear_button_y);" in init_body
    assert "lv_obj_align(notification_clear_button_, LV_ALIGN_TOP_RIGHT" not in render_body


def test_notification_clear_button_is_foregrounded_when_visible():
    render_body = function_body(
        read_source(),
        "void RenderNotificationCards(const xiaoxin_card_item_t* /*items*/, uint8_t count, bool prepare_entry_animation)",
    )

    assert "lv_obj_align(notification_clear_button_, LV_ALIGN_TOP_MID, 0, k_notification_clear_button_y);" in render_body
    assert "lv_obj_move_foreground(notification_clear_button_);" in render_body


def test_touch_poll_interval_does_not_exceed_display_refresh_budget():
    source = read_source()

    assert "static constexpr uint32_t k_touch_poll_ms = 16;" in source


def test_assistant_chat_message_does_not_create_notification_card():
    body = function_body(read_source(), "virtual void SetChatMessage")

    assert "AddChatReplyNotificationLocked" not in body
    assert "XIAOXIN_NOTIFICATION_EVENT_CHAT_REPLY" not in body


def test_legacy_low_battery_popup_is_suppressed_to_protect_subtitle():
    source = read_source()
    helper_body = function_body(source, "void HideLegacyLowBatteryPopupLocked()")
    status_body = function_body(source, "virtual void UpdateStatusBar(bool update_all = false) override")
    raise_body = function_body(source, "void RaiseOverlayObjects()")

    assert "lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);" in helper_body
    assert "HideLegacyLowBatteryPopupLocked();" in status_body
    assert "HideLegacyLowBatteryPopupLocked();" in raise_body
    assert "lv_obj_move_foreground(low_battery_popup_);" not in raise_body


def test_low_battery_notification_uses_status_copy_without_percent():
    source = read_source()
    status_body = function_body(source, "virtual void UpdateStatusBar(bool update_all = false) override")
    overlay_body = function_body(source, "void ApplyBatteryOverlayLevel()")
    notification_body = function_body(source, "void SyncLowBatteryNotificationLocked()")

    assert '#include "xiaoxin_battery_state.h"' in source
    assert "xiaoxin_battery_context_t battery_context_ = {};" in source
    assert "xiaoxin_battery_snapshot_t battery_snapshot_ = {};" in source
    assert "RefreshBatterySnapshotLocked();" in status_body
    assert "xiaoxin_system_overlay_style(" in overlay_body
    assert "SystemOverlayNetworkState()," in overlay_body
    assert "battery_snapshot_.state," in overlay_body
    assert "battery_snapshot_.power_source" in overlay_body
    assert "battery_snapshot_.state == XIAOXIN_BATTERY_STATE_LOW" in notification_body
    assert "battery_snapshot_.state == XIAOXIN_BATTERY_STATE_CRITICAL" in notification_body
    assert "level <= 20" not in notification_body
    assert "剩余 %d%%" not in notification_body
    assert "电量偏低，请尽快充电" in notification_body


def test_low_battery_notification_reacts_to_state_changes_at_same_percent():
    source = read_source()
    notification_body = function_body(source, "void SyncLowBatteryNotificationLocked()")

    assert "last_low_battery_notification_state_" in source
    assert "last_low_battery_notification_state_ = battery_snapshot_.state" in notification_body
    assert "last_low_battery_notification_state_ == battery_snapshot_.state" in notification_body
    assert "last_low_battery_notification_level_ == battery_snapshot_.estimated_percent" in notification_body


def test_battery_overlay_uses_stable_display_level():
    source = read_source()
    start = source.index("void ApplyBatteryOverlayLevel()")
    end = source.index("static uint32_t OverviewIconBgColorForTag", start)
    body = normalize_whitespace(source[start:end])

    assert "battery_snapshot_.display_level" in body
    assert "battery_snapshot_.estimated_percent" not in body
    assert "battery_snapshot_.power_source" in body
    assert "const int level = std::max(0, std::min(4, (int)battery_snapshot_.display_level));" in body
    assert "const int inner_w = k_system_battery_w - 4;" in body
    assert "battery_snapshot_.power_source == XIAOXIN_BATTERY_POWER_UNKNOWN && battery_snapshot_.display_level == 0 ? 3 : std::max(3, (inner_w * level) / 4);" in body
