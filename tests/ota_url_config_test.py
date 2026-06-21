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
