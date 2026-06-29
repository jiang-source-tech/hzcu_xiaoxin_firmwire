from pathlib import Path


CMAKE_SOURCE = Path("main/CMakeLists.txt")
RUNTIME_HEALTH_HEADER = Path("main/boards/common/runtime_health.h")
RUNTIME_HEALTH_SOURCE = Path("main/boards/common/runtime_health.cc")
APPLICATION_SOURCE = Path("main/application.cc")
WAVESHARE_146_SOURCE = Path(
    "main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc"
)


def read_source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def assert_ordered(source: str, *needles: str):
    cursor = 0
    for needle in needles:
        index = source.find(needle, cursor)
        assert index != -1, f"missing ordered source fragment: {needle}"
        cursor = index + len(needle)


def test_runtime_health_service_is_compiled_and_uses_esp_nvs():
    cmake = read_source(CMAKE_SOURCE)
    header = read_source(RUNTIME_HEALTH_HEADER)
    source = read_source(RUNTIME_HEALTH_SOURCE)

    assert '"boards/common/runtime_health.cc"' in cmake
    assert '"boards/common/runtime_health_model.c"' in cmake
    assert 'static constexpr const char* k_namespace = "runtime_health";' in source
    assert "esp_reset_reason()" in source
    assert "nvs_get_u32" in source
    assert "nvs_set_u32" in source
    assert "RuntimeHealthStart(bool on_battery)" in header
    assert "RuntimeHealthMaybeCheckpoint(void)" in header
    assert "RuntimeHealthReadSnapshot" in header


def test_runtime_health_is_wired_into_startup_tick_reboot_and_poweroff():
    application = read_source(APPLICATION_SOURCE)
    board = read_source(WAVESHARE_146_SOURCE)

    assert '#include "runtime_health.h"' in application
    assert '#include "runtime_health.h"' in board

    assert_ordered(
        board,
        "DetectPowerSourceEarly();",
        "RuntimeHealthStart(on_battery_);",
        "BootDiagnosticsStart(on_battery_);",
    )
    assert_ordered(
        application,
        "if (bits & MAIN_EVENT_CLOCK_TICK)",
        "clock_ticks_++;",
        "RuntimeHealthMaybeCheckpoint();",
    )
    assert_ordered(
        application,
        "void Application::Reboot()",
        "RuntimeHealthForceCheckpoint();",
        "esp_restart();",
    )
    assert_ordered(
        board,
        "void RequestPowerOff()",
        "RuntimeHealthForceCheckpoint();",
        "gpio_set_level(PWR_Control_PIN, xiaoxin_power_control_power_hold(&power_control_));",
    )
