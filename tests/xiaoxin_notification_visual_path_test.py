from pathlib import Path
import re


SOURCE = Path(
    "main/boards/waveshare/esp32-s3-touch-lcd-1.46/"
    "esp32-s3-touch-lcd-1.46.cc"
)
TEXTURE_SOURCE = Path(
    "main/boards/waveshare/esp32-s3-touch-lcd-1.46/"
    "xiaoxin_notification_heads_up_glass_texture.c"
)


def read_source() -> str:
    return SOURCE.read_text(encoding="utf-8")


def read_texture_source() -> str:
    return TEXTURE_SOURCE.read_text(encoding="utf-8")


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


def block_after_marker(source: str, marker: str) -> str:
    start = source.index(marker)
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
    raise AssertionError(f"block not found: {marker}")


def normalize_whitespace(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def texture_map_bytes(source: str) -> list[int]:
    match = re.search(
        r"xiaoxin_heads_up_glass_texture_map\[\]\s*=\s*\{(?P<body>.*?)\};",
        source,
        re.DOTALL,
    )
    assert match, "texture byte map not found"
    return [int(byte, 16) for byte in re.findall(r"0x([0-9a-fA-F]{2})", match.group("body"))]


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


def test_heads_up_rim_border_sides_use_lvgl_enum_casts():
    source = read_source()

    assert "static_cast<lv_border_side_t>(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT)" in source
    assert "static_cast<lv_border_side_t>(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT)" in source


def test_heads_up_refresh_does_not_replay_animation_for_same_snapshot():
    source = read_source()
    refresh_body = function_body(source, "void RefreshNotificationHeadsUpLocked()")

    assert "bool notification_heads_up_rendered_visible_ = false;" in source
    assert "bool NotificationHeadsUpSnapshotChanged(" in source
    assert "RememberNotificationHeadsUpSnapshot(snapshot);" in refresh_body
    assert "if (NotificationHeadsUpSnapshotChanged(snapshot))" in refresh_body
    assert "ShowNotificationHeadsUpLocked();" in refresh_body
    assert "if (notification_heads_up_rendered_visible_) {" in refresh_body
    assert "HideNotificationHeadsUpLocked();" in refresh_body
    assert "ClearNotificationHeadsUpRenderedSnapshot();" in refresh_body


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


def test_home_screen_does_not_create_wifi_status_overlay():
    source = read_source()
    status_body = function_body(source, "virtual void UpdateStatusBar(bool update_all = false) override")

    assert "lv_obj_t* system_overlay_ = nullptr;" not in source
    assert "system_overlay_ = lv_obj_create" not in source
    assert "network_label_ = lv_label_create(system_overlay_)" not in source
    assert "lv_label_set_text(network_label_" not in source
    assert "ApplySystemOverlayNetworkStyle();" not in status_body
    assert "SyncNetworkStatusLocked();" in status_body


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

    assert "battery_context_" not in status_body
    assert "battery_snapshot_" not in status_body
    assert "RefreshBatterySnapshotLocked();" not in status_body
    assert "ApplyBatteryOverlayLevel();" not in status_body
    assert "SyncLowBatteryNotificationLocked();" not in status_body


def test_low_battery_notification_requires_battery_power_source():
    source = read_source()

    assert "void SyncLowBatteryNotificationLocked()" not in source
    assert "XIAOXIN_NOTIFICATION_EVENT_LOW_BATTERY" not in source
    assert "low_battery_notification_active_" not in source
    assert "last_low_battery_notification_" not in source


def test_low_battery_notification_reacts_to_state_changes_at_same_percent():
    source = read_source()

    assert "lv_obj_t* battery_overlay_" not in source
    assert "battery_overlay_ = lv_obj_create" not in source
    assert "battery_overlay_box_" not in source
    assert "battery_overlay_fill_" not in source
    assert "battery_overlay_cap_" not in source


def test_battery_overlay_uses_stable_display_level():
    source = read_source()
    status_body = function_body(source, "virtual void UpdateStatusBar(bool update_all = false) override")

    assert "void ApplyBatteryOverlayLevel()" not in source
    assert "battery_snapshot_.display_level" not in status_body
    assert "k_system_battery_w" not in source


def test_notification_heads_up_uses_frosted_glass_banner_visuals():
    source = read_source()
    texture_source = read_texture_source()
    init_body = function_body(source, "void InitializeNotificationHeadsUpLayerLocked()")
    raise_body = function_body(source, "void RaiseOverlayObjects()")
    foreground_body = function_body(source, "void RaiseNotificationHeadsUpLayerLocked()")
    show_body = function_body(source, "void ShowNotificationHeadsUpLocked()")
    hide_body = function_body(source, "void HideNotificationHeadsUpLocked()")
    texture_bytes = texture_map_bytes(texture_source)

    assert '#include "xiaoxin_notification_heads_up.h"' in source
    assert "xiaoxin_notification_heads_up_t notification_heads_up_model_ = {};" in source

    assert "static constexpr int16_t k_notification_heads_up_hidden_y = -70;" in source
    assert "static constexpr int16_t k_notification_heads_up_visible_y = 18;" in source
    assert "lv_obj_t* notification_heads_up_layer_ = nullptr;" in source
    assert "lv_obj_t* notification_heads_up_title_label_ = nullptr;" in source
    assert "lv_obj_t* notification_heads_up_body_label_ = nullptr;" in source
    assert "lv_obj_t* notification_heads_up_tag_label_ = nullptr;" in source

    assert "lv_obj_t* notification_heads_up_rim_top_left_ = nullptr;" in source
    assert "lv_obj_t* notification_heads_up_rim_bottom_right_ = nullptr;" in source
    assert "lv_obj_t* notification_heads_up_texture_overlay_ = nullptr;" in source
    assert "lv_obj_t* notification_heads_up_tag_capsule_ = nullptr;" in source
    assert '#include "xiaoxin_notification_heads_up_glass_texture.c"' not in source
    assert "extern const lv_image_dsc_t xiaoxin_heads_up_glass_texture;" in source

    assert "lv_obj_set_size(notification_heads_up_layer_, 250, 58);" in init_body
    assert "lv_obj_set_style_radius(notification_heads_up_layer_, 22, 0);" in init_body
    assert "lv_obj_set_style_bg_color(notification_heads_up_layer_, lv_color_hex(0xF4F6F9), 0);" in init_body
    assert "lv_obj_set_style_bg_opa(notification_heads_up_layer_, static_cast<lv_opa_t>(180), 0);" in init_body
    assert "lv_obj_set_style_bg_grad_color(notification_heads_up_layer_, lv_color_hex(0xEDF0F5), 0);" in init_body
    assert "lv_obj_set_style_bg_grad_dir(notification_heads_up_layer_, LV_GRAD_DIR_VER, 0);" in init_body
    assert "lv_obj_align(notification_heads_up_layer_, LV_ALIGN_TOP_MID, 0, k_notification_heads_up_hidden_y);" in init_body

    assert "lv_obj_set_style_border_color(notification_heads_up_rim_top_left_, lv_color_hex(0xFFFFFF), 0);" in init_body
    assert "lv_obj_set_style_border_opa(notification_heads_up_rim_top_left_, static_cast<lv_opa_t>(153), 0);" in init_body
    assert "static_cast<lv_border_side_t>(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT)" in init_body
    assert "lv_obj_set_style_border_color(notification_heads_up_rim_bottom_right_, lv_color_hex(0xCDD3DC), 0);" in init_body
    assert "lv_obj_set_style_border_opa(notification_heads_up_rim_bottom_right_, static_cast<lv_opa_t>(64), 0);" in init_body
    assert "static_cast<lv_border_side_t>(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT)" in init_body

    assert "lv_obj_set_style_shadow_color(notification_heads_up_layer_, lv_color_hex(0x000000), 0);" in init_body
    assert "lv_obj_set_style_shadow_opa(notification_heads_up_layer_, LV_OPA_10, 0);" in init_body
    assert "lv_obj_set_style_shadow_width(notification_heads_up_layer_, 20, 0);" in init_body
    assert "lv_obj_set_style_shadow_ofs_y(notification_heads_up_layer_, 3, 0);" in init_body

    assert "lv_image_create(notification_heads_up_layer_);" in init_body
    assert "xiaoxin_heads_up_glass_texture" in init_body
    assert "lv_obj_set_style_opa(notification_heads_up_texture_overlay_, static_cast<lv_opa_t>(30), 0);" in init_body

    assert "lv_obj_set_style_bg_color(notification_heads_up_tag_capsule_, lv_color_hex(0x3182CE), 0);" in init_body
    assert "lv_obj_set_style_bg_opa(notification_heads_up_tag_capsule_, static_cast<lv_opa_t>(38), 0);" in init_body
    assert "lv_obj_set_style_radius(notification_heads_up_tag_capsule_, 6, 0);" in init_body
    assert "lv_obj_align(notification_heads_up_tag_capsule_, LV_ALIGN_LEFT_MID, 8, 0);" in init_body

    assert "lv_obj_set_style_text_color(notification_heads_up_title_label_, lv_color_hex(0x111827), 0);" in init_body
    assert "lv_obj_set_style_text_color(notification_heads_up_body_label_, lv_color_hex(0x4A5568), 0);" in init_body
    assert "lv_obj_set_style_text_color(notification_heads_up_tag_label_, lv_color_hex(0x3182CE), 0);" in init_body
    assert "lv_obj_set_width(notification_heads_up_title_label_, 154);" in init_body
    assert "lv_obj_align(notification_heads_up_title_label_, LV_ALIGN_TOP_LEFT, 78, 7);" in init_body
    assert "lv_obj_set_width(notification_heads_up_body_label_, 154);" in init_body
    assert "lv_obj_align(notification_heads_up_body_label_, LV_ALIGN_TOP_LEFT, 78, 32);" in init_body

    assert raise_body.count("RaiseNotificationHeadsUpLayerLocked();") == 2
    early_return = raise_body.index("return;")
    early_raise = raise_body.index("RaiseNotificationHeadsUpLayerLocked();")
    assert early_raise < early_return
    assert raise_body.index("RaiseNotificationHeadsUpLayerLocked();") < raise_body.index(
        "RaiseBootSplashLayerLocked();"
    )
    assert normalize_whitespace(raise_body).endswith("RaiseBootSplashLayerLocked();")
    assert "lv_obj_move_foreground(notification_heads_up_layer_);" in foreground_body

    assert "lv_anim_set_values(&anim, k_notification_heads_up_hidden_y, k_notification_heads_up_visible_y);" in show_body
    assert "lv_anim_set_values(&anim, k_notification_heads_up_visible_y, k_notification_heads_up_hidden_y);" in hide_body
    assert "lv_anim_set_exec_cb(&fade, NotificationHeadsUpSetOpa);" in show_body
    assert "lv_anim_set_exec_cb(&fade, NotificationHeadsUpSetOpa);" in hide_body
    assert "lv_anim_set_values(&fade, LV_OPA_TRANSP, LV_OPA_COVER);" in show_body
    assert "lv_anim_set_values(&fade, LV_OPA_COVER, LV_OPA_TRANSP);" in hide_body

    assert ".cf = LV_COLOR_FORMAT_I8," in texture_source
    assert ".w = 250," in texture_source
    assert ".h = 58," in texture_source
    assert ".stride = 250," in texture_source
    assert ".data_size = sizeof(xiaoxin_heads_up_glass_texture_map)," in texture_source
    assert len(texture_bytes) == 1024 + (250 * 58)

    palette = texture_bytes[:1024]
    pixels = texture_bytes[1024:]
    assert len(pixels) == 250 * 58
    assert palette[:8] == [0x00, 0x00, 0x00, 0xFF, 0x01, 0x01, 0x01, 0xFF]
    assert palette[-8:] == [0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]
    assert min(pixels) >= 0x76
    assert max(pixels) <= 0x8A


def test_notification_upsert_enqueues_heads_up_and_ttl_maintenance():
    source = read_source()
    upsert_body = function_body(source, "void UpsertNotificationEventLocked(const xiaoxin_notification_event_t& event)")
    timer_body = function_body(source, "void RefreshNotificationsFromTimer()")
    enqueue_block = block_after_marker(source, "if (item != nullptr)")
    removed_block = block_after_marker(source, "if (removed > 0)")
    stop_block = block_after_marker(
        source,
        "if (!heads_up_visible && xiaoxin_card_pager_notification_count(&card_pager_) == 0)",
    )

    assert "esp_timer_handle_t notification_maintenance_timer_ = nullptr;" in source
    assert "xiaoxin_card_pager_notification_upsert_event_at(&card_pager_, &event, NowMs())" in upsert_body
    assert "xiaoxin_card_pager_notification_find_by_type(&card_pager_, event.type)" in upsert_body
    assert "xiaoxin_notification_heads_up_enqueue(&notification_heads_up_model_, item, NowMs())" in upsert_body
    assert (
        "xiaoxin_notification_heads_up_enqueue(&notification_heads_up_model_, item, NowMs()); "
        "RefreshNotificationHeadsUpLocked(); StartNotificationMaintenanceTimer();"
    ) in normalize_whitespace(enqueue_block)
    assert "RefreshNotificationHeadsUpLocked();" in enqueue_block
    assert "StartNotificationMaintenanceTimer();" in upsert_body
    assert "xiaoxin_card_pager_notification_expire(&card_pager_, NowMs())" in timer_body
    assert "xiaoxin_notification_heads_up_tick(&notification_heads_up_model_, NowMs())" in timer_body
    assert "RefreshNotificationHeadsUpLocked();" in timer_body
    assert "RefreshNotificationPageIfVisibleLocked();" in removed_block
    assert "StopNotificationMaintenanceTimer();" in stop_block


def test_display_notification_bridge_maps_ota_to_xiaoxin_notification_center():
    display_header = Path("main/display/display.h").read_text(encoding="utf-8")
    source = read_source()
    remove_body = function_body(source, "bool RemoveNotification(const char* id) override")

    assert "virtual bool UpsertNotification(" in display_header
    assert "virtual bool RemoveNotification(" in display_header
    assert 'strcmp(id, "ota_update") == 0' in source
    assert "XIAOXIN_NOTIFICATION_EVENT_OTA_UPDATE" in source
    assert "UpsertNotificationEventLocked(event);" in source
    assert "RemoveNotificationEventLocked(XIAOXIN_NOTIFICATION_EVENT_OTA_UPDATE);" in remove_body
