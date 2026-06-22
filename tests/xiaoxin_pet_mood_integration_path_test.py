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


def test_device_status_refresh_syncs_mood_edges_from_battery_snapshot():
    body = function_body(source=read_source(), signature="virtual void UpdateStatusBar(bool update_all = false) override")

    assert "RefreshBatterySnapshotLocked();" in body
    assert "SyncPetMoodDeviceStateLocked();" in body


def test_pet_mood_uses_battery_state_edges_not_percent_thresholds():
    body = function_body(source=read_source(), signature="void SyncPetMoodDeviceStateLocked()")

    assert "battery_snapshot_.low_edge" in body
    assert "battery_snapshot_.critical_edge" in body
    assert "battery_snapshot_.recovered_edge" in body
    assert "battery_level <=" not in body


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


def test_protected_state_helper_blocks_ordinary_mood_suggestions():
    body = function_body(
        source=read_source(),
        signature="bool ShouldDispatchMoodSuggestionLocked() const"
    )

    assert "switch (trigger_.base_state)" in body
    assert "case PAOPAO_PET_STATE_FAILING:" in body
    assert "case PAOPAO_PET_STATE_SLEEPING:" in body
    assert "case PAOPAO_PET_STATE_WAITING:" in body
    assert "case PAOPAO_PET_STATE_THINKING:" in body
    assert "case PAOPAO_PET_STATE_SPEAKING:" in body
    assert "return false;" in body
    assert "return true;" in body


def test_dispatch_pet_mood_input_updates_context_before_guarding_trigger_dispatch():
    body = function_body(source=read_source(), signature="void DispatchPetMoodInputLocked(const paopao_pet_mood_input_t& input, uint32_t now_ms)")

    assert "paopao_pet_mood_handle_event(&mood_, &input, now_ms);" in body
    assert "suggestion.has_trigger && ShouldDispatchMoodSuggestionLocked()" in body
    assert "paopao_pet_trigger_dispatch(&trigger_, suggestion.trigger, now_ms);" in body
