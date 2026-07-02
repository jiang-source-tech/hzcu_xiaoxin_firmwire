# Task 2 Report: Drive Listening To Thinking From STT/Input Completion

## Scope

- Worktree: `D:\AI_Pet\hzcu_xiaoxin_firmwire\.worktrees\xiaoxin-voice-state-flow`
- Owned files:
  - `main/application.cc`
  - `tests/xiaoxin_voice_state_flow_path_test.py`

## Requirements Applied

Implemented the Task 2 brief exactly:

1. Added the two protocol path tests to `tests/xiaoxin_voice_state_flow_path_test.py`.
2. Ran the focused pytest command and confirmed the new STT ordering test failed first.
3. Updated the STT scheduled task in `main/application.cc` so `Listening -> Thinking` occurs before the user subtitle is rendered.
4. Re-ran the focused pytest command and confirmed both tests passed.
5. Prepared the requested commit.

## TDD Record

### Red

Command:

```powershell
python -m pytest tests/xiaoxin_voice_state_flow_path_test.py::test_stt_text_moves_listening_to_thinking_before_user_subtitle tests/xiaoxin_voice_state_flow_path_test.py::test_tts_start_moves_thinking_to_speaking_and_tts_stop_leaves_speaking -q
```

Result:

- `test_stt_text_moves_listening_to_thinking_before_user_subtitle` failed.
- Failure was the expected missing line: `SetDeviceState(kDeviceStateThinking);`
- `test_tts_start_moves_thinking_to_speaking_and_tts_stop_leaves_speaking` already passed.

### Green

Command:

```powershell
python -m pytest tests/xiaoxin_voice_state_flow_path_test.py::test_stt_text_moves_listening_to_thinking_before_user_subtitle tests/xiaoxin_voice_state_flow_path_test.py::test_tts_start_moves_thinking_to_speaking_and_tts_stop_leaves_speaking -q
```

Result:

- `2 passed in 0.01s`

## Code Change

In `main/application.cc`, the STT schedule block now:

- captures `this`
- checks `if (GetDeviceState() == kDeviceStateListening)`
- calls `SetDeviceState(kDeviceStateThinking);`
- then renders the normalized user subtitle via `display->SetChatMessage("user", message.c_str());`

This keeps the transition scoped to the existing STT completion path and does not alter the existing TTS start/stop flow.

## Commit

Requested commit message:

```powershell
git commit -m "fix: move xiaoxin to thinking after stt"
```

## Concerns

- None from this focused task.

---

## Follow-up Fix: Interruption STT Guard

### Reviewer Finding Addressed

Late STT from an interrupted turn could still push a freshly restarted listening session into `kDeviceStateThinking` because the Task 2 gate only checked `GetDeviceState() == kDeviceStateListening`.

### Change Summary

- Added an `Application`-owned suppression deadline field: `suppress_stt_thinking_until_us_`.
- Added local helpers:
  - `SuppressSttThinkingFor(int64_t duration_us)`
  - `IsSttThinkingSuppressed() const`
- Set a bounded `800 ms` suppression window before listening restarts caused by interruption flows:
  - wake word detected while already listening, before `SendStartListening(GetDefaultListeningMode())`
  - wake word detected while speaking, before `SetListeningMode(GetDefaultListeningMode())`
  - manual start-listening while speaking, before `SetListeningMode(kListeningModeManualStop)`
- Updated the STT scheduled task to enter `kDeviceStateThinking` only when the app is still listening and the suppression window has expired. The user subtitle still renders either way.

### Added Path Coverage

- Verified the STT transition now checks `!IsSttThinkingSuppressed()`.
- Verified the wake-word listening restart path arms suppression before `SendStartListening(GetDefaultListeningMode())`.

### Verification

Red:

```powershell
python -m pytest tests/xiaoxin_voice_state_flow_path_test.py::test_stt_text_moves_listening_to_thinking_before_user_subtitle tests/xiaoxin_voice_state_flow_path_test.py::test_wake_word_listening_restart_arms_stt_thinking_suppression_before_send_start_listening -q
```

Result:

- `2 failed` for the expected missing suppression guard and missing wake-word restart arming.

Green:

```powershell
python -m pytest tests/xiaoxin_voice_state_flow_path_test.py::test_stt_text_moves_listening_to_thinking_before_user_subtitle tests/xiaoxin_voice_state_flow_path_test.py::test_tts_start_moves_thinking_to_speaking_and_tts_stop_leaves_speaking tests/xiaoxin_voice_state_flow_path_test.py::test_wake_word_listening_restart_arms_stt_thinking_suppression_before_send_start_listening -q
python -m pytest tests/xiaoxin_voice_state_flow_path_test.py -q
```

Result:

- Focused Task 2 plus interruption guard tests: `3 passed`
- Full Task 2 path file: `7 passed`

---

## Re-review Fix: Clear Suppression On Real New Speech

### Reviewer Finding Addressed

The fixed `800 ms` STT-thinking suppression window could also block a legitimate fast next-turn STT after a restart. The requested improvement was to keep the stale-STT guard, but drop it as soon as actual new speech is detected.

### Change Summary

- Added `ClearSttThinkingSuppression()` in `Application` to reset `suppress_stt_thinking_until_us_` back to `0`.
- Updated the existing `MAIN_EVENT_VAD_CHANGE` handling in `Application::Run()` so that when the device is still in `kDeviceStateListening` and `audio_service_.IsVoiceDetected()` is true, the app clears the suppression before `led->OnStateChanged()`.
- Left the suppression arming branches intact:
  - manual start-listening while speaking
  - wake-word restart while listening
  - wake-word restart while speaking

This keeps the stale interrupted-turn STT blocked during restart, while letting real fresh speech re-enable the normal `Listening -> Thinking` path immediately.

### Added Path Coverage

- Verified VAD-true while listening clears suppression before LED refresh.
- Verified manual start-listening from speaking still arms suppression before `SetListeningMode(kListeningModeManualStop)`.
- Verified wake-word restart from speaking still arms suppression before `SetListeningMode(GetDefaultListeningMode())`.

### Verification

Focused tests:

```powershell
python -m pytest tests/xiaoxin_voice_state_flow_path_test.py::test_vad_true_while_listening_clears_stt_thinking_suppression_before_led_refresh tests/xiaoxin_voice_state_flow_path_test.py::test_manual_start_listening_from_speaking_arms_suppression_before_listening_state tests/xiaoxin_voice_state_flow_path_test.py::test_wake_word_listening_restart_arms_stt_thinking_suppression_before_send_start_listening tests/xiaoxin_voice_state_flow_path_test.py::test_wake_word_speaking_restart_arms_suppression_before_listening_state -q
```

Result:

- `4 passed`

Requested full file:

```powershell
python -m pytest tests/xiaoxin_voice_state_flow_path_test.py -q
```

Result:

- `10 passed`

### Concerns

- None for this bounded follow-up.
