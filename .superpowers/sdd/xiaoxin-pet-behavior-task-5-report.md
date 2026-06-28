# Task 5 Report: Wire Behavior Director Into the 1.46 Display

## Status

DONE

## What Changed

- Added `paopao_pet_behavior.h` to the 1.46 display and initialized `behavior_` alongside the existing trigger and mood contexts.
- Routed `SetEmotion()` through both mood tracking and the behavior director so service emotions can schedule behavior-driven trigger decisions.
- Added display-side behavior helpers:
  - `DispatchPetBehaviorDecisionLocked(...)`
  - `DispatchPetBehaviorServiceTrigger(...)`
  - `DispatchPetBehaviorTickLocked(...)`
  - `SetPetBehaviorVoiceStateLocked(...)`
  - `RecordPetBehaviorInteractionLocked(...)`
  - plus a non-locked `SetPetBehaviorVoiceState(...)` wrapper for unlocked callers
- Updated `SetStatus()` and `SetChatMessage()` to notify behavior voice state changes without violating the existing lock contract.
- Recorded local pet interactions inside `DispatchLocalPetTriggerLocked(...)` so idle micro-action timing resets on touch/shake inputs.
- Ticked behavior from `RunRenderLoop()` before `paopao_pet_trigger_tick(&trigger_, now_ms);`.
- Added `paopao_pet_behavior.c` to the board-specific source list in `main/CMakeLists.txt`.
- Extended the path test to assert behavior include/init, service routing through behavior, and behavior tick ordering.

## TDD Evidence

1. Updated `tests/xiaoxin_pet_mood_integration_path_test.py` first.
2. Ran:

```powershell
pytest -q tests/xiaoxin_pet_mood_integration_path_test.py
```

3. Observed expected RED failure:
   - missing `#include "paopao_pet_behavior.h"`
   - missing `DispatchPetBehaviorServiceTrigger(event);`
   - missing `DispatchPetBehaviorTickLocked(now_ms);`
4. Implemented the display/CMake changes.
5. Re-ran the same path test to GREEN.

## Verification

Passed:

```powershell
pytest -q tests/xiaoxin_pet_mood_integration_path_test.py
```

```bash
cd /d/AI_Pet/hzcu_xiaoxin_firmwire
export PATH=/ucrt64/bin:$PATH
gcc -std=c11 -Wall -Wextra -I main/boards/waveshare/esp32-s3-touch-lcd-1.46 tests/paopao_pet_behavior_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_behavior.c -o build/paopao_pet_behavior_test.exe
./build/paopao_pet_behavior_test.exe
gcc -std=c11 -Wall -Wextra -I main/boards/waveshare/esp32-s3-touch-lcd-1.46 tests/paopao_pet_trigger_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c -o build/paopao_pet_trigger_test.exe
./build/paopao_pet_trigger_test.exe
```

Observed results:

- `9 passed in 0.09s`
- `paopao_pet_behavior tests passed`
- `paopao_pet_trigger tests passed`

## Notes

- Host GCC invocation from PowerShell/cmd was unreliable with this MSYS2 toolchain, so I used `D:/msys64/usr/bin/bash.exe` with `/ucrt64/bin` on `PATH` to run the required C test compiles and executables successfully.
- Only the three owned task files were staged and committed.

## Review Fix Addendum

- Fixed the Task 5 review race in `SetStatus()` and `SetChatMessage()` by routing voice-trigger and behavior voice-state updates through `DispatchPetVoiceState(...)`, which acquires one `DisplayLockGuard` and then performs mood event dispatch (for chat paths), trigger dispatch, behavior voice-state update, and `ApplyPetStateIfChanged()` in one locked sequence.
- Kept local interaction reset coverage in `DispatchLocalPetTriggerLocked(...)` via `RecordPetBehaviorInteractionLocked(now_ms);`, preserving the idle-timer reset requirement from Task 5.
- Verified the 1.46 board CMake wiring already includes `paopao_pet_behavior.c`, so no further `main/CMakeLists.txt` change was needed for this review fix.
- `DispatchPetBehaviorDecisionLocked(...)` now applies pet state immediately after behavior-directed trigger dispatch, so service-triggered decisions and render-loop behavior ticks update the display state without waiting for a later pass.

### Review Fix Verification

```powershell
pytest -q tests/xiaoxin_pet_mood_integration_path_test.py
```

Output:

- `12 passed in 0.22s`

```powershell
& 'D:/msys64/usr/bin/bash.exe' -lc 'cd /d/AI_Pet/hzcu_xiaoxin_firmwire && export PATH=/ucrt64/bin:/usr/bin:$PATH && mkdir -p build && gcc -std=c11 -Wall -Wextra -I main/boards/waveshare/esp32-s3-touch-lcd-1.46 tests/paopao_pet_behavior_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_behavior.c -o build/paopao_pet_behavior_test.exe && ./build/paopao_pet_behavior_test.exe && gcc -std=c11 -Wall -Wextra -I main/boards/waveshare/esp32-s3-touch-lcd-1.46 tests/paopao_pet_trigger_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c -o build/paopao_pet_trigger_test.exe && ./build/paopao_pet_trigger_test.exe'
```

Output:

- `paopao_pet_behavior tests passed`
- `paopao_pet_trigger tests passed`
