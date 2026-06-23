from pathlib import Path


WIFI_BOARD_SOURCE = Path("main/boards/common/wifi_board.cc")
APPLICATION_SOURCE = Path("main/application.cc")


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
                return source[brace + 1 : index]
    raise AssertionError(f"function body not found: {signature}")


def block_after(source: str, marker: str, length: int = 240) -> str:
    start = source.index(marker)
    return source[start : start + length]


def braced_block_after(source: str, marker: str) -> str:
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


def test_wifi_config_mode_network_icon_does_not_claim_connected():
    body = function_body(read_source(WIFI_BOARD_SOURCE), "const char* WifiBoard::GetNetworkStateIcon()")
    config_mode_block = braced_block_after(body, "if (wifi.IsConfigMode())")

    assert "FONT_AWESOME_WIFI_SLASH" in config_mode_block
    assert "return FONT_AWESOME_WIFI;" not in config_mode_block


def test_wifi_config_state_refreshes_status_bar_immediately():
    body = function_body(read_source(APPLICATION_SOURCE), "void Application::HandleStateChangedEvent()")
    wifi_config_block = block_after(body, "case kDeviceStateWifiConfiguring:")

    assert "display->SetStatus(Lang::Strings::WIFI_CONFIG_MODE);" in wifi_config_block
    assert "display->UpdateStatusBar(true);" in wifi_config_block
