# Xiaoxin Pet Behavior Director Final Fix Report

## Status

Fixed the two final review findings and added regression coverage.

## Changes

1. Service emotions now defer idle micro-actions when they play immediately from idle.
   - `paopao_pet_behavior_handle_service_trigger()` now refreshes `last_interaction_ms` and `next_idle_variant_ms` before returning an immediate idle playback decision.
2. Service emotions are now preserved while the pet is in protected base states, including sleeping and failing.
   - The board display layer now syncs behavior voice state from `trigger_.base_state` before service-trigger handling and before behavior ticks.
   - `SetEmotion()` now routes mood + behavior service handling under one display lock through `DispatchPetServiceEmotionLocked(...)`, which also answers the open interleaving question in a practical way.
3. Added regression tests.
   - Behavior tests now cover protected-state caching for sleeping/failing and the idle-frame overwrite case.
   - Integration path tests now cover the single-lock service-emotion path and protected-state sync helper wiring.

## Verification

- `pytest -q tests/xiaoxin_pet_mood_integration_path_test.py`
- MSYS2 bash:
  - `paopao_pet_behavior_test`
  - `paopao_pet_trigger_test`
  - `paopao_pet_emotion_test`

## Concerns

- No full IDF build was run, per request.
- The protected-state sync helper currently mirrors `WAITING`, `THINKING`, `SPEAKING`, `SLEEPING`, and `FAILING` from trigger base state into behavior voice state before service/tick processing. That keeps behavior protection aligned with the currently rendered base state without broader refactoring.
