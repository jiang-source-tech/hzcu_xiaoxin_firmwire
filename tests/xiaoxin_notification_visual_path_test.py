from pathlib import Path


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


def test_overview_title_uses_black_and_notifications_page_has_no_title():
    source = read_source()
    body = function_body(source, "void RenderCardPage(xiaoxin_card_page_t page, bool prepare_entry_animation = false)")

    assert "static constexpr uint32_t k_page_title_color = 0x111111;" in source
    assert "lv_obj_set_style_text_color(card_title_label_, lv_color_hex(k_page_title_color), 0);" in source
    assert 'lv_label_set_text(card_title_label_, "\\xE9\\x80\\x9A\\xE7\\x9F\\xA5");' not in body
    assert 'lv_label_set_text(card_title_label_, "\\xE6\\x80\\xBB\\xE8\\xA7\\x88");' in body


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


def test_low_battery_notification_uses_status_copy_without_percent():
    body = function_body(read_source(), "void SyncLowBatteryNotificationLocked(int level)")

    assert "剩余 %d%%" not in body
    assert "电量偏低，请尽快充电" in body
