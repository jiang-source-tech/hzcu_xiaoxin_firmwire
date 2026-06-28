# Task 2 Report: Implement Fruit, Growth, And Capped Length

## Status

DONE

## Summary

Implemented the standby snake fruit lifecycle in `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc` exactly around the Task 2 symbols and constants:

- Replaced the fixed snake length constant with `k_low_power_snake_initial_length`, `k_low_power_snake_max_length`, and `k_low_power_snake_fruits_per_growth`.
- Expanded snake state to track current length, fruit position, fruit readiness, and fruit count.
- Updated body color and opacity helpers to use the current runtime length.
- Switched body collision checks, movement shifts, and draw loops to `low_power_clock_snake_length_`.
- Added `LowPowerSnakeDistanceToFruit(...)`, `GenerateLowPowerSnakeFruitLocked()`, `StartNewLowPowerSnakeGameLocked()`, `HandleLowPowerSnakeFruitLocked(...)`, and `DrawLowPowerSnakeFruitLocked(...)`.
- Made standby entry start a fresh snake game with fruit generation.
- Added fruit-biased movement, eight-fruit growth, capped length at 24, and runtime trap recovery that preserves the current length.

## Test Alignment

Updated `tests/xiaoxin_low_power_clock_visual_path_test.py` only where Task 2 changed exact source strings or helper ordering:

- Reset helper signature assertion
- Runtime reset call assertion
- Body draw helper call signatures
- Fruit helper section slicing
- Fruit-handling call target assertion

The fruit, growth, and capped-length assertions from Task 1 were preserved.

## Verification

Focused test:

```text
pytest tests/xiaoxin_low_power_clock_visual_path_test.py -q
30 passed in 0.49s
```

Broader sanity check:

```text
pytest tests -q
164 passed in 2.86s
```

## Commit

Created commit:

```text
feat: add fruit growth to standby snake
```

## Concerns

None.

## Reviewer Fix

- Preserved the pre-move tail in `MoveLowPowerSnakeLocked(...)` and threaded it through `AdvanceLowPowerSnakeLocked()` into `HandleLowPowerSnakeFruitLocked(...)`.
- On growth below `k_low_power_snake_max_length`, the new body slot now copies `previous_tail` before incrementing `low_power_clock_snake_length_`, so the exposed tail cell is deterministic instead of stale.
- Kept capped-length fruit behavior unchanged: fruit still refreshes, `low_power_clock_snake_length_` does not increase at cap, and no reset/new-game path was added.
- Tightened `tests/xiaoxin_low_power_clock_visual_path_test.py` to require the previous-tail capture, propagation, and growth-slot assignment in the non-cap branch.
- Updated `AdvanceLowPowerSnakeLocked()` so safe moves now keep their own `safe_fruit_distance` and `safe_score`, preferring the closest fruit-targeting safe move before considering larger reachable space.
- Limited safe-move randomness to the final tie-break case where fruit distance and reachable score are both equal, preserving variety without letting a farther safe move displace a closer one.
- Tightened `tests/xiaoxin_low_power_clock_visual_path_test.py` again to require `safe_fruit_distance`, `closer_safe_fruit`, and the safe-block `fruit_distance < safe_fruit_distance` comparison that enforces the fruit bias at source level.
