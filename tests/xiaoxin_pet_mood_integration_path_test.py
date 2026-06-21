from pathlib import Path


SOURCE = Path("main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc")


def read_source() -> str:
    return SOURCE.read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace_start = source.index("{", start)
    depth = 0
    for index in range(brace_start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace_start : index + 1]
    raise AssertionError(f"function body not found for {signature}")


def test_board_includes_and_initializes_pet_mood():
    source = read_source()

    assert '#include "paopao_pet_mood.h"' in source
    assert "paopao_pet_mood_context_t mood_ = {};" in source
    assert "paopao_pet_mood_init(&mood_, now_ms);" in source


def test_service_emotion_routes_through_mood_cooldown():
    body = function_body(source=read_source(), signature="virtual void SetEmotion(const char* emotion) override")

    assert "paopao_pet_trigger_for_emotion(emotion)" in body
    assert "DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_SERVICE_EMOTION, event);" in body
    assert "DispatchPetTrigger(event);" not in body


def test_status_and_chat_events_update_mood_without_bypassing_trigger_state():
    source = read_source()
    status_body = function_body(source, "virtual void SetStatus(const char* status) override")
    chat_body = function_body(source, "virtual void SetChatMessage(const char* role, const char* content) override")

    assert "DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_VOICE_ERROR);" in status_body
    assert "DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_CHAT_STARTED);" in chat_body
    assert "DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_ASSISTANT_REPLY);" in chat_body


def test_device_status_refresh_syncs_mood_edges():
    body = function_body(source=read_source(), signature="virtual void UpdateStatusBar(bool update_all = false) override")

    assert "SyncPetMoodDeviceStateLocked(battery_level);" in body


def test_touch_and_motion_update_mood_but_boot_button_does_not():
    source = read_source()

    assert "DispatchLocalPetTriggerLocked(" in source
    assert "PAOPAO_PET_MOOD_EVENT_LOCAL_TAP" in source
    assert "PAOPAO_PET_MOOD_EVENT_LOCAL_HOLD" in source
    assert "PAOPAO_PET_MOOD_EVENT_LOCAL_DRAG" in source
    assert "DispatchLocalPetTrigger(PAOPAO_PET_TRIGGER_LOCAL_SHAKE, PAOPAO_PET_MOOD_EVENT_LOCAL_SHAKE);" in source

    boot_start = source.index("// Boot Button")
    power_start = source.index("// Power Button")
    boot_section = source[boot_start:power_start]

    assert "DispatchPetTrigger" not in boot_section
    assert "PAOPAO_PET_TRIGGER_LOCAL_TAP" not in boot_section
    assert "PAOPAO_PET_TRIGGER_LOCAL_HOLD" not in boot_section
