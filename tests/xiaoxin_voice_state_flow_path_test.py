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


def branch_block(source: str, marker: str) -> str:
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
                return source[start : index + 1]
    raise AssertionError(f"branch block not found: {marker}")


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


def test_vad_true_while_listening_clears_stt_thinking_suppression_before_led_refresh():
    body = function_body(read_source(APPLICATION), "void Application::Run()")
    vad_section = switch_case(body, "if (bits & MAIN_EVENT_VAD_CHANGE) {", "if (bits & MAIN_EVENT_SCHEDULE) {")

    assert "if (GetDeviceState() == kDeviceStateListening) {" in vad_section
    assert "if (audio_service_.IsVoiceDetected()) {" in vad_section
    assert "ClearSttThinkingSuppression();" in vad_section
    assert "led->OnStateChanged();" in vad_section
    assert vad_section.index("ClearSttThinkingSuppression();") < vad_section.index("led->OnStateChanged();")


def test_manual_start_listening_from_speaking_arms_suppression_before_listening_state():
    body = function_body(read_source(APPLICATION), "void Application::HandleStartListeningEvent()")
    speaking_branch = body[body.index("else if (state == kDeviceStateSpeaking) {") :]

    assert "AbortSpeaking(kAbortReasonNone);" in speaking_branch
    assert "SuppressSttThinkingFor(kSttThinkingSuppressionWindowUs);" in speaking_branch
    assert "SetListeningMode(kListeningModeManualStop);" in speaking_branch
    assert speaking_branch.index("SuppressSttThinkingFor(kSttThinkingSuppressionWindowUs);") < speaking_branch.index(
        "SetListeningMode(kListeningModeManualStop);"
    )


def test_toggle_chat_cancels_thinking_to_idle_like_active_listening():
    body = function_body(read_source(APPLICATION), "void Application::HandleToggleChatEvent()")
    cancel_branch = branch_block(body, "state == kDeviceStateListening || state == kDeviceStateThinking")

    assert "protocol_->CloseAudioChannel();" in cancel_branch
    assert "SetDeviceState(kDeviceStateIdle);" in cancel_branch


def test_stop_listening_cancels_thinking_to_idle():
    body = function_body(read_source(APPLICATION), "void Application::HandleStopListeningEvent()")
    cancel_branch = branch_block(body, "state == kDeviceStateListening || state == kDeviceStateThinking")

    assert "protocol_->SendStopListening();" in cancel_branch
    assert "SetDeviceState(kDeviceStateIdle);" in cancel_branch


def test_wake_word_speaking_restart_arms_suppression_before_listening_state():
    body = function_body(read_source(APPLICATION), "void Application::HandleWakeWordDetectedEvent()")
    speaking_start = body.index("} else {", body.index("if (state == kDeviceStateListening) {"))
    speaking_restart = body[speaking_start:]

    assert "play_popup_on_listening_ = true;" in speaking_restart
    assert "SuppressSttThinkingFor(kSttThinkingSuppressionWindowUs);" in speaking_restart
    assert "SetListeningMode(GetDefaultListeningMode());" in speaking_restart
    assert speaking_restart.index("SuppressSttThinkingFor(kSttThinkingSuppressionWindowUs);") < speaking_restart.index(
        "SetListeningMode(GetDefaultListeningMode());"
    )


def test_wake_word_thinking_restart_arms_suppression_before_listening_state():
    body = function_body(read_source(APPLICATION), "void Application::HandleWakeWordDetectedEvent()")

    assert "state == kDeviceStateSpeaking || state == kDeviceStateListening || state == kDeviceStateThinking" in body

    thinking_start = body.index("} else {", body.index("if (state == kDeviceStateListening) {"))
    thinking_restart = body[thinking_start:]

    assert "play_popup_on_listening_ = true;" in thinking_restart
    assert "SuppressSttThinkingFor(kSttThinkingSuppressionWindowUs);" in thinking_restart
    assert "SetListeningMode(GetDefaultListeningMode());" in thinking_restart
    assert thinking_restart.index("SuppressSttThinkingFor(kSttThinkingSuppressionWindowUs);") < thinking_restart.index(
        "SetListeningMode(GetDefaultListeningMode());"
    )


def test_paopao_status_recognizes_thinking_status():
    body = function_body(read_source(PAOPAO_DISPLAY), "virtual void SetStatus(const char* status) override")

    assert "StatusEquals(status, Lang::Strings::THINKING)" in body
    assert "PAOPAO_PET_TRIGGER_THINKING" in body
    assert "PAOPAO_PET_BEHAVIOR_VOICE_THINKING" in body


def test_assistant_subtitle_only_sets_speaking_pet_when_device_is_speaking():
    body = function_body(read_source(PAOPAO_DISPLAY), "virtual void SetChatMessage(const char* role, const char* content) override")
    assistant_section = branch_block(body, 'std::strcmp(role, "assistant") == 0')

    assert "Application::GetInstance().GetDeviceState() == kDeviceStateSpeaking" in assistant_section
    assert "PAOPAO_PET_TRIGGER_SPEAKING" in assistant_section
    assert "} else {" in assistant_section
    fallback_section = assistant_section.split("} else {", 1)[1]
    assert "DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_ASSISTANT_REPLY);" in fallback_section
