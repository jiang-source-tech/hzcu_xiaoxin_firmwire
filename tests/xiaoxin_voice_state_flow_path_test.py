from pathlib import Path


DEVICE_STATE = Path("main/device_state.h")
STATE_MACHINE = Path("main/device_state_machine.cc")
APPLICATION = Path("main/application.cc")
LANG_CONFIG = Path("main/assets/lang_config.h")
PAOPAO_DISPLAY = Path("main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc")


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


def switch_case(source: str, marker: str, next_marker: str) -> str:
    start = source.index(marker)
    end = source.index(next_marker, start)
    return source[start:end]


def test_thinking_state_is_declared_between_listening_and_speaking():
    source = read_source(DEVICE_STATE)

    assert "kDeviceStateListening" in source
    assert "kDeviceStateThinking" in source
    assert "kDeviceStateSpeaking" in source
    assert source.index("kDeviceStateListening") < source.index("kDeviceStateThinking")
    assert source.index("kDeviceStateThinking") < source.index("kDeviceStateSpeaking")


def test_state_machine_names_and_transitions_include_thinking():
    source = read_source(STATE_MACHINE)

    assert '"thinking"' in source

    listening_case = switch_case(source, "case kDeviceStateListening:", "case kDeviceStateThinking:")
    assert "to == kDeviceStateThinking" in listening_case
    assert "to == kDeviceStateSpeaking" in listening_case
    assert "to == kDeviceStateIdle" in listening_case

    thinking_case = switch_case(source, "case kDeviceStateThinking:", "case kDeviceStateSpeaking:")
    assert "to == kDeviceStateSpeaking" in thinking_case
    assert "to == kDeviceStateListening" in thinking_case
    assert "to == kDeviceStateIdle" in thinking_case


def test_lang_config_exposes_thinking_status_text():
    source = read_source(LANG_CONFIG)

    assert 'constexpr const char* THINKING = "思考中...";' in source


def test_application_renders_thinking_state_as_status_and_pet_thinking():
    body = function_body(read_source(APPLICATION), "void Application::HandleStateChangedEvent()")

    thinking_case = switch_case(body, "case kDeviceStateThinking:", "case kDeviceStateSpeaking:")
    assert "display->SetStatus(Lang::Strings::THINKING);" in thinking_case
    assert 'display->SetEmotion("neutral");' in thinking_case
    assert "audio_service_.EnableVoiceProcessing(false);" in thinking_case
    assert "audio_service_.EnableWakeWordDetection(false);" in thinking_case
