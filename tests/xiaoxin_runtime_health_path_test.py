from pathlib import Path


CMAKE_SOURCE = Path("main/CMakeLists.txt")
RUNTIME_HEALTH_HEADER = Path("main/boards/common/runtime_health.h")
RUNTIME_HEALTH_SOURCE = Path("main/boards/common/runtime_health.cc")


def read_source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


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
