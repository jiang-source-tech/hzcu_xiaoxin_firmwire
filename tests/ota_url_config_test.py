from pathlib import Path


EXPECTED_OTA_URL = "http://124.222.121.103:8003/xiaozhi/ota/"


def read_config_value(path: Path, key: str) -> str:
    prefix = f"{key}="
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix):
            return line.removeprefix(prefix).strip().strip('"')
    raise AssertionError(f"{key} not found in {path}")


def test_ota_url_matches_private_server() -> None:
    repo_root = Path(__file__).resolve().parents[1]

    assert read_config_value(repo_root / "sdkconfig.defaults", "CONFIG_OTA_URL") == EXPECTED_OTA_URL
    assert read_config_value(repo_root / "sdkconfig", "CONFIG_OTA_URL") == EXPECTED_OTA_URL


def test_application_reports_ota_states_to_display_notifications() -> None:
    repo_root = Path(__file__).resolve().parents[1]
    source = (repo_root / "main" / "application.cc").read_text(encoding="utf-8")

    assert 'display->UpsertNotification("ota_update", "OTA 鏇存柊", "鍙戠幇鏂扮増鏈?", "绯荤粺", 4, 0);' in source
    assert 'display->UpsertNotification("ota_update", "OTA 鏇存柊", "姝ｅ湪涓嬭浇骞跺畨瑁呮洿鏂?", "绯荤粺", 4, 0);' in source
    assert 'display->UpsertNotification("ota_update", "OTA 鏇存柊", "鍗囩骇澶辫触锛岃绋嶅悗閲嶈瘯", "绯荤粺", 4, 0);' in source
    assert 'display->RemoveNotification("ota_update");' in source
