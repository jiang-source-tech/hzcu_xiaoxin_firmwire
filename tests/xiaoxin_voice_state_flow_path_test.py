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


def test_stt_text_moves_listening_to_thinking_before_user_subtitle():
    body = function_body(read_source(APPLICATION), "void Application::InitializeProtocol()")
    stt_section = switch_case(body, 'strcmp(type->valuestring, "stt") == 0', 'strcmp(type->valuestring, "llm") == 0')

    assert "if (GetDeviceState() == kDeviceStateListening && !IsSttThinkingSuppressed())" in stt_section
    assert "SetDeviceState(kDeviceStateThinking);" in stt_section
    assert 'display->SetChatMessage("user", message.c_str());' in stt_section
    assert stt_section.index("SetDeviceState(kDeviceStateThinking);") < stt_section.index('display->SetChatMessage("user", message.c_str());')


def test_tts_start_moves_thinking_to_speaking_and_tts_stop_leaves_speaking():
    body = function_body(read_source(APPLICATION), "void Application::InitializeProtocol()")
    tts_section = switch_case(body, 'strcmp(type->valuestring, "tts") == 0', 'strcmp(type->valuestring, "stt") == 0')

    assert "SetDeviceState(kDeviceStateSpeaking);" in tts_section
    assert "if (GetDeviceState() == kDeviceStateSpeaking)" in tts_section
    assert "SetDeviceState(kDeviceStateListening);" in tts_section
    assert "SetDeviceState(kDeviceStateIdle);" in tts_section


def test_wake_word_listening_restart_arms_stt_thinking_suppression_before_send_start_listening():
    body = function_body(read_source(APPLICATION), "void Application::HandleWakeWordDetectedEvent()")
    listening_restart = switch_case(body, "if (state == kDeviceStateListening) {", "} else {")

    assert "SuppressSttThinkingFor(kSttThinkingSuppressionWindowUs);" in listening_restart
    assert "protocol_->SendStartListening(GetDefaultListeningMode());" in listening_restart
    assert listening_restart.index("SuppressSttThinkingFor(kSttThinkingSuppressionWindowUs);") < listening_restart.index(
        "protocol_->SendStartListening(GetDefaultListeningMode());"
    )
