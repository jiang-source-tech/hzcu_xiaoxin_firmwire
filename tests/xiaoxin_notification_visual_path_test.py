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


def test_touch_poll_interval_does_not_exceed_display_refresh_budget():
    source = read_source()

    assert "static constexpr uint32_t k_touch_poll_ms = 16;" in source
