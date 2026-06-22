from pathlib import Path
import re


BOARD_SOURCE = Path("main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc")
CMAKE_SOURCE = Path("main/CMakeLists.txt")


def read_source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


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
                return source[brace + 1:index]
    raise AssertionError(f"function body not found: {signature}")


def strip_cpp_comments(source: str) -> str:
    return re.sub(r"//.*?$|/\*.*?\*/", "", source, flags=re.MULTILINE | re.DOTALL)


def section_between(source: str, start_marker: str, end_marker: str) -> str:
    start = source.index(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


def test_settings_model_is_included_and_compiled():
    board = read_source(BOARD_SOURCE)
    cmake = read_source(CMAKE_SOURCE)

    assert '#include "xiaoxin_settings_model.h"' in board
    assert "xiaoxin_settings_model.c" in cmake


def test_boot_single_click_closes_settings_before_chat_or_wifi_config():
    source = read_source(BOARD_SOURCE)
    boot_section = section_between(source, "// Boot Button", "// Power Button")

    settings_check = boot_section.index("IsSettingsOpen()")
    close_call = boot_section.index("CloseSettingsOverlay()")
    start_check = boot_section.index("kDeviceStateStarting")
    toggle_call = boot_section.index("ToggleChatState()")

    assert settings_check < close_call < start_check < toggle_call
    assert "return;" in boot_section[close_call:start_check]


def test_boot_long_press_only_opens_settings_from_idle():
    source = read_source(BOARD_SOURCE)
    boot_section = section_between(source, "// Boot Button", "// Power Button")

    assert "OpenSettingsOverlayFromBootButton()" in boot_section
    helper = function_body(source, "void OpenSettingsOverlayFromBootButton()")
    assert "Application::GetInstance()" in helper
    assert "GetDeviceState()" in helper
    assert "XIAOXIN_SETTINGS_RUNTIME_IDLE" in helper
    assert "xiaoxin_settings_can_open" in helper
    assert "OpenSettingsOverlay()" in helper
    assert "ShowNotification(\"璇峰厛缁撴潫瀵硅瘽\"" in helper


def test_settings_overlay_state_is_public_to_board_button_layer():
    source = read_source(BOARD_SOURCE)

    assert "bool IsSettingsOpen()" in source
    assert "void OpenSettingsOverlay()" in source
    assert "void CloseSettingsOverlay()" in source


def test_settings_touch_suppresses_card_pager_gestures():
    body = function_body(read_source(BOARD_SOURCE), "void PollTouch(uint32_t now_ms)")

    assert "if (settings_open_)" in body
    assert "HandleSettingsTouch" in body
    assert body.index("if (settings_open_)") < body.index("xiaoxin_card_pager_press")


def test_brightness_setting_uses_backlight_api_not_direct_settings_write():
    source = read_source(BOARD_SOURCE)
    body = function_body(source, "void ApplySettingsBrightness(uint8_t brightness)")

    assert "xiaoxin_settings_clamp_percent" in body
    assert "Board::GetInstance().GetBacklight()" in body
    assert "SetBrightness(clamped, true)" in body
    assert 'Settings("display"' not in body


def test_brightness_page_exposes_three_presets():
    body = strip_cpp_comments(
        function_body(read_source(BOARD_SOURCE), "void PaopaoPetDisplay::RenderSettingsBrightnessPage()")
    )

    assert "30" in body
    assert "70" in body
    assert "100" in body
    assert "ApplySettingsBrightness(30)" in body
    assert "ApplySettingsBrightness(70)" in body
    assert "ApplySettingsBrightness(100)" in body


def test_wifi_reconfiguration_reuses_existing_entrypoint():
    source = read_source(BOARD_SOURCE)
    body = function_body(source, "void RequestSettingsWifiConfig()")

    assert "CloseSettingsOverlay()" in body
    assert "EnterWifiConfigMode()" in body


def test_wifi_page_calls_board_reconfiguration_request():
    body = strip_cpp_comments(
        function_body(read_source(BOARD_SOURCE), "void PaopaoPetDisplay::RenderSettingsWifiPage()")
    )

    assert "閲嶆柊閰嶇綉" in body
    assert "CustomBoard::Instance()->RequestSettingsWifiConfig()" in body


def test_target_settings_caps_do_not_enable_audio_or_power_save_initially():
    body = function_body(read_source(BOARD_SOURCE), "xiaoxin_settings_caps_t SettingsCaps() const")

    assert ".has_audio_output = false" in body
    assert ".has_vibration = false" in body
    assert ".has_power_save_scheduler = false" in body


def test_about_page_uses_esp_app_description():
    source = read_source(BOARD_SOURCE)
    body = function_body(source, "void RenderSettingsAboutPage()")

    assert "#include <esp_app_desc.h>" in source
    assert "esp_app_get_description()" in body
    assert "Waveshare ESP32-S3 Touch LCD 1.46" in body
