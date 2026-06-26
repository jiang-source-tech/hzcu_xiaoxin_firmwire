from pathlib import Path


WIFI_BOARD_SOURCE = Path("main/boards/common/wifi_board.cc")
APPLICATION_SOURCE = Path("main/application.cc")
SDKCONFIG_SOURCE = Path("sdkconfig")
SDKCONFIG_DEFAULTS_SOURCE = Path("sdkconfig.defaults")


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


def test_wifi_connection_starts_time_synchronization():
    source = read_source(WIFI_BOARD_SOURCE)
    body = function_body(source, "void WifiBoard::OnNetworkEvent(NetworkEvent event, const std::string& data)")
    connected_block = block_after(body, "case NetworkEvent::Connected:", length=420)

    assert "#include <esp_sntp.h>" in source
    assert "StartTimeSynchronization();" in connected_block
    assert "static constexpr const char* NTP_SERVERS[]" in source
    assert '"ntp.aliyun.com"' in source
    assert '"ntp.tencent.com"' in source
    assert '"ntp.ntsc.ac.cn"' in source
    assert '"cn.pool.ntp.org"' not in source
    assert '"pool.ntp.org"' not in source
    assert "k_ntp_server_count" in source
    assert 'static constexpr char DEFAULT_TIMEZONE[] = "CST-8";' in source
    assert '#include <stdlib.h>' in source
    assert '#include <sys/time.h>' in source
    assert '#include <time.h>' in source
    assert 'setenv("TZ", DEFAULT_TIMEZONE, 1);' in source
    assert "tzset();" in source
    assert "esp_sntp_set_time_sync_notification_cb(OnSntpTimeSync);" in source
    assert "for (size_t i = 0; i < k_ntp_server_count; ++i)" in source
    assert "esp_sntp_setservername(i, NTP_SERVERS[i]);" in source
    assert "esp_sntp_init();" in source


def test_sntp_config_allows_three_servers():
    sdkconfig = read_source(SDKCONFIG_SOURCE)
    sdkconfig_defaults = read_source(SDKCONFIG_DEFAULTS_SOURCE)

    assert "CONFIG_LWIP_SNTP_MAX_SERVERS=3" in sdkconfig
    assert "CONFIG_LWIP_SNTP_MAX_SERVERS=3" in sdkconfig_defaults


def test_wifi_time_sync_updates_shared_status():
    source = read_source(WIFI_BOARD_SOURCE)
    cmake = Path("main/CMakeLists.txt").read_text(encoding="utf-8")

    assert '#include "time_sync_status.h"' in source
    assert "MarkTimeSyncStarted();" in source
    assert "MarkTimeSyncSucceeded();" in source
    assert '"boards/common/time_sync_status.cc"' in cmake
