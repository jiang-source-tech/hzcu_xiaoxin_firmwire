# Xiaoxin Pet Mood System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a testable `paopao_pet_mood` policy layer that turns device/user/service events into restrained pet emotion suggestions without consuming the BOOT button for P1.

**Architecture:** Implement a pure C mood module beside the existing `paopao_pet_trigger` module. The mood module updates short-term `energy` and `mood` scores, applies cooldowns for low battery, WiFi changes, voice errors, and service emotions, then outputs existing `paopao_pet_trigger_event_t` values. The Waveshare 1.46 display layer remains responsible for collecting real events and dispatching the mood suggestion through the existing trigger state machine.

**Tech Stack:** ESP-IDF C/C++, LVGL, local GCC C tests, pytest source-path tests.

## Global Constraints

- Do not rewrite `paopao_pet_trigger`; mood suggestions must output `paopao_pet_trigger_event_t`.
- Do not add new mandatory GIF resources in this slice.
- Do not add long-term pet growth, levels, intimacy, cloud personality, or persistence.
- Do not route weather, course, or Overview 待办 data into pet mood.
- Do not make BOOT a required mood input; BOOT may remain reserved for system/debug/settings/product decisions.
- Do not let ordinary mood suggestions bypass trigger protections for error lock, sleeping, listening, thinking, or speaking.
- Keep UI rendering code from directly mapping low battery, WiFi, or voice error to GIF states.

---

## File Structure

| File | Responsibility | Planned Change |
| --- | --- | --- |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.h` | Public mood policy contract | Create. Defines mood events, input, context, suggestion, and API. |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.c` | Pure mood policy logic | Create. Updates scores, edge flags, cooldowns, and suggestions. |
| `tests/paopao_pet_mood_test.c` | Pure C mood regression tests | Create. Covers init, battery, WiFi, voice error, local interaction, service emotion cooldown. |
| `main/CMakeLists.txt` | Firmware source list | Add `paopao_pet_mood.c` for Waveshare 1.46. |
| `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc` | Real event collection and trigger dispatch | Include mood, initialize context, route device/service/local events, remove BOOT pet triggers. |
| `tests/xiaoxin_pet_mood_integration_path_test.py` | Source-path guard tests | Create. Ensures board wiring uses mood and BOOT does not dispatch pet triggers. |
| `docs/xiaoxin-pet-emotion-gif-mapping.zh-CN.md` | User-facing pet emotion mapping docs | Update with mood policy layer and BOOT reservation note. |

---

### Task 1: Specify the Pure Mood Policy Contract

**Files:**
- Create: `tests/paopao_pet_mood_test.c`

**Interfaces:**
- Consumes: planned `paopao_pet_mood.h`.
- Produces: executable expectations for `paopao_pet_mood_init(...)` and `paopao_pet_mood_handle_event(...)`.

- [ ] **Step 1: Write the failing mood policy test**

Create `tests/paopao_pet_mood_test.c` with this full content:

```c
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.h"

static paopao_pet_mood_suggestion_t send_event(
    paopao_pet_mood_context_t* ctx,
    paopao_pet_mood_event_t event,
    uint32_t now_ms
) {
    const paopao_pet_mood_input_t input = {
        .event = event,
        .service_trigger = PAOPAO_PET_TRIGGER_NONE,
    };
    return paopao_pet_mood_handle_event(ctx, &input, now_ms);
}

static paopao_pet_mood_suggestion_t send_service(
    paopao_pet_mood_context_t* ctx,
    paopao_pet_trigger_event_t service_trigger,
    uint32_t now_ms
) {
    const paopao_pet_mood_input_t input = {
        .event = PAOPAO_PET_MOOD_EVENT_SERVICE_EMOTION,
        .service_trigger = service_trigger,
    };
    return paopao_pet_mood_handle_event(ctx, &input, now_ms);
}

static void init_sets_stable_defaults(void) {
    paopao_pet_mood_context_t ctx;
    paopao_pet_mood_init(&ctx, 1000);

    assert(ctx.energy == 70);
    assert(ctx.mood == 60);
    assert(ctx.last_interaction_ms == 1000);
    assert(ctx.last_reaction_ms == 0);
    assert(!ctx.low_battery);
    assert(ctx.wifi_connected);
}

static void low_battery_edge_triggers_tired_once_until_recovery(void) {
    paopao_pet_mood_context_t ctx;
    paopao_pet_mood_init(&ctx, 1000);

    paopao_pet_mood_suggestion_t first =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_BATTERY_LOW, 1100);
    assert(first.has_trigger);
    assert(first.trigger == PAOPAO_PET_TRIGGER_SERVICE_TIRED);
    assert(strcmp(first.text, "有点没电了") == 0);
    assert(ctx.low_battery);
    assert(ctx.energy == 45);

    paopao_pet_mood_suggestion_t repeated =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_BATTERY_LOW, 2000);
    assert(!repeated.has_trigger);
    assert(ctx.low_battery);

    paopao_pet_mood_suggestion_t recovered =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_BATTERY_RECOVERED, 32000);
    assert(recovered.has_trigger);
    assert(recovered.trigger == PAOPAO_PET_TRIGGER_TASK_DONE);
    assert(strcmp(recovered.text, "好多啦") == 0);
    assert(!ctx.low_battery);
    assert(ctx.energy == 60);
}

static void wifi_edges_trigger_anxious_and_done_with_cooldown(void) {
    paopao_pet_mood_context_t ctx;
    paopao_pet_mood_init(&ctx, 1000);

    paopao_pet_mood_suggestion_t disconnected =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_WIFI_DISCONNECTED, 1200);
    assert(disconnected.has_trigger);
    assert(disconnected.trigger == PAOPAO_PET_TRIGGER_SERVICE_ANXIOUS);
    assert(strcmp(disconnected.text, "网络不见了") == 0);
    assert(!ctx.wifi_connected);

    paopao_pet_mood_suggestion_t repeated =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_WIFI_DISCONNECTED, 2000);
    assert(!repeated.has_trigger);

    paopao_pet_mood_suggestion_t connected =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_WIFI_CONNECTED, 25000);
    assert(connected.has_trigger);
    assert(connected.trigger == PAOPAO_PET_TRIGGER_TASK_DONE);
    assert(strcmp(connected.text, "连上啦") == 0);
    assert(ctx.wifi_connected);
}

static void voice_error_has_short_cooldown(void) {
    paopao_pet_mood_context_t ctx;
    paopao_pet_mood_init(&ctx, 1000);

    paopao_pet_mood_suggestion_t first =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_VOICE_ERROR, 1200);
    assert(first.has_trigger);
    assert(first.trigger == PAOPAO_PET_TRIGGER_SERVICE_FAILING);
    assert(strcmp(first.text, "我再想想") == 0);

    paopao_pet_mood_suggestion_t suppressed =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_VOICE_ERROR, 3000);
    assert(!suppressed.has_trigger);

    paopao_pet_mood_suggestion_t later =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_VOICE_ERROR, 4300);
    assert(later.has_trigger);
    assert(later.trigger == PAOPAO_PET_TRIGGER_SERVICE_FAILING);
}

static void local_interactions_update_scores_without_replacing_direct_feedback(void) {
    paopao_pet_mood_context_t ctx;
    paopao_pet_mood_init(&ctx, 1000);

    paopao_pet_mood_suggestion_t tap =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_LOCAL_TAP, 1500);
    assert(!tap.has_trigger);
    assert(ctx.last_interaction_ms == 1500);
    assert(ctx.mood == 68);
    assert(ctx.energy == 72);

    paopao_pet_mood_suggestion_t drag =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_LOCAL_DRAG, 1800);
    assert(!drag.has_trigger);
    assert(ctx.last_interaction_ms == 1800);
    assert(ctx.energy == 76);

    paopao_pet_mood_suggestion_t shake =
        send_event(&ctx, PAOPAO_PET_MOOD_EVENT_LOCAL_SHAKE, 2200);
    assert(!shake.has_trigger);
    assert(ctx.last_interaction_ms == 2200);
    assert(ctx.mood == 65);
}

static void service_emotion_uses_existing_trigger_values_and_cooldown(void) {
    paopao_pet_mood_context_t ctx;
    paopao_pet_mood_init(&ctx, 1000);

    paopao_pet_mood_suggestion_t happy =
        send_service(&ctx, PAOPAO_PET_TRIGGER_SERVICE_HAPPY, 1200);
    assert(happy.has_trigger);
    assert(happy.trigger == PAOPAO_PET_TRIGGER_SERVICE_HAPPY);
    assert(strcmp(happy.text, "收到") == 0);

    paopao_pet_mood_suggestion_t suppressed =
        send_service(&ctx, PAOPAO_PET_TRIGGER_SERVICE_CRYING, 2000);
    assert(!suppressed.has_trigger);

    paopao_pet_mood_suggestion_t crying =
        send_service(&ctx, PAOPAO_PET_TRIGGER_SERVICE_CRYING, 3300);
    assert(crying.has_trigger);
    assert(crying.trigger == PAOPAO_PET_TRIGGER_SERVICE_CRYING);

    paopao_pet_mood_suggestion_t neutral =
        send_service(&ctx, PAOPAO_PET_TRIGGER_SERVICE_NEUTRAL, 6000);
    assert(!neutral.has_trigger);

    paopao_pet_mood_suggestion_t none =
        send_service(&ctx, PAOPAO_PET_TRIGGER_NONE, 9000);
    assert(!none.has_trigger);
}

static void null_input_is_safe_and_has_no_trigger(void) {
    paopao_pet_mood_context_t ctx;
    paopao_pet_mood_init(&ctx, 1000);

    paopao_pet_mood_suggestion_t suggestion =
        paopao_pet_mood_handle_event(&ctx, NULL, 1200);
    assert(!suggestion.has_trigger);
    assert(suggestion.trigger == PAOPAO_PET_TRIGGER_NONE);
}

int main(void) {
    init_sets_stable_defaults();
    low_battery_edge_triggers_tired_once_until_recovery();
    wifi_edges_trigger_anxious_and_done_with_cooldown();
    voice_error_has_short_cooldown();
    local_interactions_update_scores_without_replacing_direct_feedback();
    service_emotion_uses_existing_trigger_values_and_cooldown();
    null_input_is_safe_and_has_no_trigger();
    puts("paopao_pet_mood tests passed");
    return 0;
}
```

- [ ] **Step 2: Run the test and verify it fails because the module does not exist**

Run from repo root:

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_mood_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.c -o build/paopao_pet_mood_test.exe
```

Expected result:

```text
fatal error: ../main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.h: No such file or directory
```

- [ ] **Step 3: Commit the failing contract test**

```powershell
git add -- tests/paopao_pet_mood_test.c
git commit -m "test: specify paopao pet mood policy"
```

---

### Task 2: Implement the Pure Mood Module

**Files:**
- Create: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.h`
- Create: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.c`
- Modify: `main/CMakeLists.txt`
- Test: `tests/paopao_pet_mood_test.c`

**Interfaces:**
- Consumes: `paopao_pet_trigger_event_t` from `paopao_pet_trigger.h`.
- Produces:
  - `void paopao_pet_mood_init(paopao_pet_mood_context_t* ctx, uint32_t now_ms)`
  - `paopao_pet_mood_suggestion_t paopao_pet_mood_handle_event(paopao_pet_mood_context_t* ctx, const paopao_pet_mood_input_t* input, uint32_t now_ms)`

- [ ] **Step 1: Add the mood header**

Create `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.h` with this full content:

```c
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "paopao_pet_trigger.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PAOPAO_PET_MOOD_EVENT_NONE = 0,
  PAOPAO_PET_MOOD_EVENT_TICK,
  PAOPAO_PET_MOOD_EVENT_LOCAL_TAP,
  PAOPAO_PET_MOOD_EVENT_LOCAL_HOLD,
  PAOPAO_PET_MOOD_EVENT_LOCAL_DRAG,
  PAOPAO_PET_MOOD_EVENT_LOCAL_SHAKE,
  PAOPAO_PET_MOOD_EVENT_BATTERY_LOW,
  PAOPAO_PET_MOOD_EVENT_BATTERY_RECOVERED,
  PAOPAO_PET_MOOD_EVENT_WIFI_DISCONNECTED,
  PAOPAO_PET_MOOD_EVENT_WIFI_CONNECTED,
  PAOPAO_PET_MOOD_EVENT_VOICE_ERROR,
  PAOPAO_PET_MOOD_EVENT_CHAT_STARTED,
  PAOPAO_PET_MOOD_EVENT_ASSISTANT_REPLY,
  PAOPAO_PET_MOOD_EVENT_SERVICE_EMOTION
} paopao_pet_mood_event_t;

typedef struct {
  paopao_pet_mood_event_t event;
  paopao_pet_trigger_event_t service_trigger;
} paopao_pet_mood_input_t;

typedef struct {
  int8_t energy;
  int8_t mood;
  uint32_t last_interaction_ms;
  uint32_t last_reaction_ms;
  uint32_t low_battery_last_ms;
  uint32_t wifi_alert_last_ms;
  uint32_t voice_error_last_ms;
  uint32_t service_emotion_last_ms;
  bool low_battery;
  bool wifi_connected;
} paopao_pet_mood_context_t;

typedef struct {
  bool has_trigger;
  paopao_pet_trigger_event_t trigger;
  const char* text;
  uint8_t priority;
  uint32_t cooldown_ms;
} paopao_pet_mood_suggestion_t;

void paopao_pet_mood_init(paopao_pet_mood_context_t* ctx, uint32_t now_ms);

paopao_pet_mood_suggestion_t paopao_pet_mood_handle_event(
  paopao_pet_mood_context_t* ctx,
  const paopao_pet_mood_input_t* input,
  uint32_t now_ms
);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: Add the mood implementation**

Create `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.c` with this full content:

```c
#include "paopao_pet_mood.h"

#include <stddef.h>
#include <string.h>

static const uint32_t k_low_battery_cooldown_ms = 30000;
static const uint32_t k_battery_recovered_cooldown_ms = 10000;
static const uint32_t k_wifi_alert_cooldown_ms = 20000;
static const uint32_t k_wifi_recovered_cooldown_ms = 10000;
static const uint32_t k_voice_error_cooldown_ms = 3000;
static const uint32_t k_service_emotion_cooldown_ms = 1800;

static int8_t clamp_score(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return (int8_t)value;
}

static bool time_reached(uint32_t now_ms, uint32_t target_ms) {
  return (int32_t)(now_ms - target_ms) >= 0;
}

static bool cooldown_elapsed(uint32_t last_ms, uint32_t now_ms, uint32_t cooldown_ms) {
  return last_ms == 0 || time_reached(now_ms, last_ms + cooldown_ms);
}

static paopao_pet_mood_suggestion_t no_suggestion(void) {
  const paopao_pet_mood_suggestion_t suggestion = {
    .has_trigger = false,
    .trigger = PAOPAO_PET_TRIGGER_NONE,
    .text = "",
    .priority = 0,
    .cooldown_ms = 0,
  };
  return suggestion;
}

static paopao_pet_mood_suggestion_t make_suggestion(
  paopao_pet_mood_context_t* ctx,
  paopao_pet_trigger_event_t trigger,
  const char* text,
  uint8_t priority,
  uint32_t cooldown_ms,
  uint32_t now_ms
) {
  if (ctx != NULL) {
    ctx->last_reaction_ms = now_ms;
  }

  const paopao_pet_mood_suggestion_t suggestion = {
    .has_trigger = trigger != PAOPAO_PET_TRIGGER_NONE,
    .trigger = trigger,
    .text = text != NULL ? text : "",
    .priority = priority,
    .cooldown_ms = cooldown_ms,
  };
  return suggestion;
}

static void record_interaction(paopao_pet_mood_context_t* ctx, uint32_t now_ms) {
  if (ctx == NULL) {
    return;
  }
  ctx->last_interaction_ms = now_ms;
}

static paopao_pet_mood_suggestion_t handle_service_emotion(
  paopao_pet_mood_context_t* ctx,
  paopao_pet_trigger_event_t trigger,
  uint32_t now_ms
) {
  if (ctx == NULL ||
      trigger == PAOPAO_PET_TRIGGER_NONE ||
      trigger == PAOPAO_PET_TRIGGER_SERVICE_NEUTRAL) {
    return no_suggestion();
  }

  if (!cooldown_elapsed(ctx->service_emotion_last_ms, now_ms, k_service_emotion_cooldown_ms)) {
    return no_suggestion();
  }

  ctx->service_emotion_last_ms = now_ms;
  if (trigger == PAOPAO_PET_TRIGGER_SERVICE_HAPPY) {
    ctx->mood = clamp_score(ctx->mood + 6);
  } else if (trigger == PAOPAO_PET_TRIGGER_SERVICE_CRYING ||
             trigger == PAOPAO_PET_TRIGGER_SERVICE_ANXIOUS ||
             trigger == PAOPAO_PET_TRIGGER_SERVICE_ANGRY ||
             trigger == PAOPAO_PET_TRIGGER_SERVICE_FAILING) {
    ctx->mood = clamp_score(ctx->mood - 4);
  }

  return make_suggestion(ctx, trigger, "收到", 40, k_service_emotion_cooldown_ms, now_ms);
}

void paopao_pet_mood_init(paopao_pet_mood_context_t* ctx, uint32_t now_ms) {
  if (ctx == NULL) {
    return;
  }

  memset(ctx, 0, sizeof(*ctx));
  ctx->energy = 70;
  ctx->mood = 60;
  ctx->last_interaction_ms = now_ms;
  ctx->wifi_connected = true;
}

paopao_pet_mood_suggestion_t paopao_pet_mood_handle_event(
  paopao_pet_mood_context_t* ctx,
  const paopao_pet_mood_input_t* input,
  uint32_t now_ms
) {
  if (ctx == NULL || input == NULL) {
    return no_suggestion();
  }

  switch (input->event) {
    case PAOPAO_PET_MOOD_EVENT_NONE:
    case PAOPAO_PET_MOOD_EVENT_TICK:
      break;
    case PAOPAO_PET_MOOD_EVENT_LOCAL_TAP:
      record_interaction(ctx, now_ms);
      ctx->mood = clamp_score(ctx->mood + 8);
      ctx->energy = clamp_score(ctx->energy + 2);
      break;
    case PAOPAO_PET_MOOD_EVENT_LOCAL_HOLD:
      record_interaction(ctx, now_ms);
      ctx->energy = clamp_score(ctx->energy - 2);
      break;
    case PAOPAO_PET_MOOD_EVENT_LOCAL_DRAG:
      record_interaction(ctx, now_ms);
      ctx->energy = clamp_score(ctx->energy + 4);
      break;
    case PAOPAO_PET_MOOD_EVENT_LOCAL_SHAKE:
      record_interaction(ctx, now_ms);
      ctx->mood = clamp_score(ctx->mood - 3);
      break;
    case PAOPAO_PET_MOOD_EVENT_BATTERY_LOW:
      if (ctx->low_battery) {
        return no_suggestion();
      }
      ctx->low_battery = true;
      ctx->energy = clamp_score(ctx->energy - 25);
      ctx->mood = clamp_score(ctx->mood - 5);
      if (!cooldown_elapsed(ctx->low_battery_last_ms, now_ms, k_low_battery_cooldown_ms)) {
        return no_suggestion();
      }
      ctx->low_battery_last_ms = now_ms;
      return make_suggestion(
        ctx,
        PAOPAO_PET_TRIGGER_SERVICE_TIRED,
        "有点没电了",
        80,
        k_low_battery_cooldown_ms,
        now_ms
      );
    case PAOPAO_PET_MOOD_EVENT_BATTERY_RECOVERED:
      if (!ctx->low_battery) {
        return no_suggestion();
      }
      ctx->low_battery = false;
      ctx->energy = clamp_score(ctx->energy + 15);
      if (!cooldown_elapsed(ctx->low_battery_last_ms, now_ms, k_battery_recovered_cooldown_ms)) {
        return no_suggestion();
      }
      ctx->low_battery_last_ms = now_ms;
      return make_suggestion(
        ctx,
        PAOPAO_PET_TRIGGER_TASK_DONE,
        "好多啦",
        60,
        k_battery_recovered_cooldown_ms,
        now_ms
      );
    case PAOPAO_PET_MOOD_EVENT_WIFI_DISCONNECTED:
      if (!ctx->wifi_connected) {
        return no_suggestion();
      }
      ctx->wifi_connected = false;
      ctx->mood = clamp_score(ctx->mood - 6);
      if (!cooldown_elapsed(ctx->wifi_alert_last_ms, now_ms, k_wifi_alert_cooldown_ms)) {
        return no_suggestion();
      }
      ctx->wifi_alert_last_ms = now_ms;
      return make_suggestion(
        ctx,
        PAOPAO_PET_TRIGGER_SERVICE_ANXIOUS,
        "网络不见了",
        70,
        k_wifi_alert_cooldown_ms,
        now_ms
      );
    case PAOPAO_PET_MOOD_EVENT_WIFI_CONNECTED:
      if (ctx->wifi_connected) {
        return no_suggestion();
      }
      ctx->wifi_connected = true;
      ctx->mood = clamp_score(ctx->mood + 4);
      if (!cooldown_elapsed(ctx->wifi_alert_last_ms, now_ms, k_wifi_recovered_cooldown_ms)) {
        return no_suggestion();
      }
      ctx->wifi_alert_last_ms = now_ms;
      return make_suggestion(
        ctx,
        PAOPAO_PET_TRIGGER_TASK_DONE,
        "连上啦",
        55,
        k_wifi_recovered_cooldown_ms,
        now_ms
      );
    case PAOPAO_PET_MOOD_EVENT_VOICE_ERROR:
      ctx->mood = clamp_score(ctx->mood - 3);
      if (!cooldown_elapsed(ctx->voice_error_last_ms, now_ms, k_voice_error_cooldown_ms)) {
        return no_suggestion();
      }
      ctx->voice_error_last_ms = now_ms;
      return make_suggestion(
        ctx,
        PAOPAO_PET_TRIGGER_SERVICE_FAILING,
        "我再想想",
        75,
        k_voice_error_cooldown_ms,
        now_ms
      );
    case PAOPAO_PET_MOOD_EVENT_CHAT_STARTED:
      record_interaction(ctx, now_ms);
      ctx->energy = clamp_score(ctx->energy + 1);
      break;
    case PAOPAO_PET_MOOD_EVENT_ASSISTANT_REPLY:
      record_interaction(ctx, now_ms);
      ctx->mood = clamp_score(ctx->mood + 3);
      break;
    case PAOPAO_PET_MOOD_EVENT_SERVICE_EMOTION:
      return handle_service_emotion(ctx, input->service_trigger, now_ms);
  }

  return no_suggestion();
}
```

- [ ] **Step 3: Register the source for firmware builds**

In `main/CMakeLists.txt`, inside the existing `if(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_46)` block, add `paopao_pet_mood.c` immediately after `paopao_pet_emotion.c`:

```cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_emotion.c
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.c
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_power_control.c
```

- [ ] **Step 4: Run the mood policy test and verify it passes**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_mood_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.c -o build/paopao_pet_mood_test.exe
.\build\paopao_pet_mood_test.exe
```

Expected output:

```text
paopao_pet_mood tests passed
```

- [ ] **Step 5: Run existing pet tests to verify no trigger/emotion regression**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_trigger_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c -o build/paopao_pet_trigger_test.exe
.\build\paopao_pet_trigger_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_emotion_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_emotion.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c -o build/paopao_pet_emotion_test.exe
.\build\paopao_pet_emotion_test.exe
```

Expected output:

```text
paopao_pet_trigger tests passed
paopao pet emotion tests passed
```

- [ ] **Step 6: Commit the pure mood module**

```powershell
git add -- main/CMakeLists.txt main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.h main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.c tests/paopao_pet_mood_test.c
git commit -m "feat: add paopao pet mood policy"
```

---

### Task 3: Specify Board Integration and BOOT Reservation

**Files:**
- Create: `tests/xiaoxin_pet_mood_integration_path_test.py`

**Interfaces:**
- Consumes: board source at `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`.
- Produces: source-path guards for mood wiring and BOOT not dispatching pet triggers.

- [ ] **Step 1: Write source-path integration tests**

Create `tests/xiaoxin_pet_mood_integration_path_test.py` with this full content:

```python
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
```

- [ ] **Step 2: Run the integration tests and verify they fail**

```powershell
python -m pytest tests/xiaoxin_pet_mood_integration_path_test.py -q
```

Expected result includes these failures:

```text
FAILED tests/xiaoxin_pet_mood_integration_path_test.py::test_board_includes_and_initializes_pet_mood
FAILED tests/xiaoxin_pet_mood_integration_path_test.py::test_service_emotion_routes_through_mood_cooldown
FAILED tests/xiaoxin_pet_mood_integration_path_test.py::test_touch_and_motion_update_mood_but_boot_button_does_not
```

- [ ] **Step 3: Commit the failing integration tests**

```powershell
git add -- tests/xiaoxin_pet_mood_integration_path_test.py
git commit -m "test: specify xiaoxin pet mood board wiring"
```

---

### Task 4: Wire Mood Into the Waveshare 1.46 Board

**Files:**
- Modify: `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- Test: `tests/xiaoxin_pet_mood_integration_path_test.py`
- Test: `tests/paopao_pet_mood_test.c`

**Interfaces:**
- Consumes:
  - `paopao_pet_mood_init(...)`
  - `paopao_pet_mood_handle_event(...)`
  - `paopao_pet_trigger_dispatch(...)`
- Produces:
  - `DispatchPetMoodEvent(...)`
  - `SyncPetMoodDeviceStateLocked(...)`
  - `DispatchLocalPetTriggerLocked(...)`
  - `DispatchLocalPetTrigger(...)`

- [ ] **Step 1: Include the mood header**

In the `extern "C"` block of `esp32-s3-touch-lcd-1.46.cc`, add:

```cpp
#include "paopao_pet_mood.h"
```

immediately after:

```cpp
#include "paopao_pet_emotion.h"
```

- [ ] **Step 2: Add mood context and low-battery threshold**

Near the other file-scope constants, add:

```cpp
static constexpr int k_pet_mood_low_battery_percent = 20;
```

In the `PaopaoPetDisplay` private fields near `paopao_pet_trigger_context_t trigger_ = {};`, add:

```cpp
    paopao_pet_mood_context_t mood_ = {};
```

- [ ] **Step 3: Initialize mood with the trigger**

In the display constructor/init path, immediately after:

```cpp
        paopao_pet_trigger_init(&trigger_, now_ms);
```

add:

```cpp
        paopao_pet_mood_init(&mood_, now_ms);
```

- [ ] **Step 4: Add locked mood dispatch helpers**

Inside `PaopaoPetDisplay`, replace the current `DispatchPetTriggerLocked(...)` function:

```cpp
    void DispatchPetTriggerLocked(paopao_pet_trigger_event_t event, uint32_t now_ms) {
        paopao_pet_trigger_dispatch(&trigger_, event, now_ms);
        ApplyPetStateIfChanged();
    }
```

with:

```cpp
    void DispatchPetTriggerLocked(paopao_pet_trigger_event_t event, uint32_t now_ms) {
        paopao_pet_trigger_dispatch(&trigger_, event, now_ms);
        ApplyPetStateIfChanged();
    }

    void DispatchPetMoodInputLocked(const paopao_pet_mood_input_t& input, uint32_t now_ms) {
        const paopao_pet_mood_suggestion_t suggestion =
            paopao_pet_mood_handle_event(&mood_, &input, now_ms);
        if (suggestion.has_trigger) {
            paopao_pet_trigger_dispatch(&trigger_, suggestion.trigger, now_ms);
            ApplyPetStateIfChanged();
        }
    }

    void DispatchPetMoodEventLocked(
        paopao_pet_mood_event_t event,
        paopao_pet_trigger_event_t service_trigger,
        uint32_t now_ms
    ) {
        const paopao_pet_mood_input_t input = {
            .event = event,
            .service_trigger = service_trigger,
        };
        DispatchPetMoodInputLocked(input, now_ms);
    }

    void DispatchLocalPetTriggerLocked(
        paopao_pet_trigger_event_t trigger_event,
        paopao_pet_mood_event_t mood_event,
        uint32_t now_ms
    ) {
        DispatchPetMoodEventLocked(mood_event, PAOPAO_PET_TRIGGER_NONE, now_ms);
        paopao_pet_trigger_dispatch(&trigger_, trigger_event, now_ms);
        ApplyPetStateIfChanged();
    }
```

- [ ] **Step 5: Add public mood dispatch wrappers**

Near the existing public `DispatchPetTrigger(...)` method, add:

```cpp
    void DispatchPetMoodEvent(
        paopao_pet_mood_event_t event,
        paopao_pet_trigger_event_t service_trigger = PAOPAO_PET_TRIGGER_NONE
    ) {
        DisplayLockGuard lock(this);
        DispatchPetMoodEventLocked(event, service_trigger, NowMs());
    }

    void DispatchLocalPetTrigger(
        paopao_pet_trigger_event_t trigger_event,
        paopao_pet_mood_event_t mood_event
    ) {
        DisplayLockGuard lock(this);
        DispatchLocalPetTriggerLocked(trigger_event, mood_event, NowMs());
    }
```

Keep the existing `DispatchPetTrigger(...)` for non-mood callers and for future system-level direct trigger use.

- [ ] **Step 6: Route service emotion through mood cooldown**

Replace `SetEmotion(...)` with:

```cpp
    virtual void SetEmotion(const char* emotion) override {
        const paopao_pet_trigger_event_t event = paopao_pet_trigger_for_emotion(emotion);
        if (event == PAOPAO_PET_TRIGGER_NONE) {
            return;
        }
        DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_SERVICE_EMOTION, event);
    }
```

- [ ] **Step 7: Route voice error and chat edges into mood**

In `SetStatus(...)`, replace:

```cpp
        } else if (status_error) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_ERROR);
        } else if (IsBusyStatus(status)) {
```

with:

```cpp
        } else if (status_error) {
            DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_VOICE_ERROR);
        } else if (IsBusyStatus(status)) {
```

In `SetChatMessage(...)`, inside the non-empty content branch, change:

```cpp
        if (std::strcmp(role, "user") == 0) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_THINKING);
        } else if (std::strcmp(role, "assistant") == 0) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_SPEAKING);
        }
```

to:

```cpp
        if (std::strcmp(role, "user") == 0) {
            DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_CHAT_STARTED);
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_THINKING);
        } else if (std::strcmp(role, "assistant") == 0) {
            DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_ASSISTANT_REPLY);
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_SPEAKING);
        }
```

- [ ] **Step 8: Add device-state mood edge sync**

Add this private method near `ApplySystemOverlayNetworkStyle()`:

```cpp
    void SyncPetMoodDeviceStateLocked(int battery_level) {
        const uint32_t now_ms = NowMs();
        const bool low_battery = battery_level <= k_pet_mood_low_battery_percent;
        if (low_battery != mood_.low_battery) {
            DispatchPetMoodEventLocked(
                low_battery
                    ? PAOPAO_PET_MOOD_EVENT_BATTERY_LOW
                    : PAOPAO_PET_MOOD_EVENT_BATTERY_RECOVERED,
                PAOPAO_PET_TRIGGER_NONE,
                now_ms
            );
        }

        const bool wifi_connected =
            SystemOverlayNetworkState() == XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED;
        if (wifi_connected != mood_.wifi_connected) {
            DispatchPetMoodEventLocked(
                wifi_connected
                    ? PAOPAO_PET_MOOD_EVENT_WIFI_CONNECTED
                    : PAOPAO_PET_MOOD_EVENT_WIFI_DISCONNECTED,
                PAOPAO_PET_TRIGGER_NONE,
                now_ms
            );
        }
    }
```

In `UpdateStatusBar(...)`, after:

```cpp
            SyncLowBatteryNotificationLocked(battery_level);
```

add:

```cpp
            SyncPetMoodDeviceStateLocked(battery_level);
```

- [ ] **Step 9: Route touch local feedback through local mood updates**

In `HandleTouchRelease(...)`, replace:

```cpp
            DispatchPetTriggerLocked(
                dx < 0 ? PAOPAO_PET_TRIGGER_LOCAL_DRAG_LEFT : PAOPAO_PET_TRIGGER_LOCAL_DRAG_RIGHT,
                now_ms
            );
        } else if (now_ms - touch_start_ms_ >= k_touch_hold_ms) {
            DispatchPetTriggerLocked(PAOPAO_PET_TRIGGER_LOCAL_HOLD, now_ms);
        } else {
            DispatchPetTriggerLocked(PAOPAO_PET_TRIGGER_LOCAL_TAP, now_ms);
        }
```

with:

```cpp
            DispatchLocalPetTriggerLocked(
                dx < 0 ? PAOPAO_PET_TRIGGER_LOCAL_DRAG_LEFT : PAOPAO_PET_TRIGGER_LOCAL_DRAG_RIGHT,
                PAOPAO_PET_MOOD_EVENT_LOCAL_DRAG,
                now_ms
            );
        } else if (now_ms - touch_start_ms_ >= k_touch_hold_ms) {
            DispatchLocalPetTriggerLocked(
                PAOPAO_PET_TRIGGER_LOCAL_HOLD,
                PAOPAO_PET_MOOD_EVENT_LOCAL_HOLD,
                now_ms
            );
        } else {
            DispatchLocalPetTriggerLocked(
                PAOPAO_PET_TRIGGER_LOCAL_TAP,
                PAOPAO_PET_MOOD_EVENT_LOCAL_TAP,
                now_ms
            );
        }
```

- [ ] **Step 10: Route shake through local mood updates**

In `CustomBoard::RunMotionLoop()`, replace:

```cpp
                        static_cast<PaopaoPetDisplay*>(display_)->DispatchPetTrigger(PAOPAO_PET_TRIGGER_LOCAL_SHAKE);
```

with:

```cpp
                        static_cast<PaopaoPetDisplay*>(display_)->DispatchLocalPetTrigger(
                            PAOPAO_PET_TRIGGER_LOCAL_SHAKE,
                            PAOPAO_PET_MOOD_EVENT_LOCAL_SHAKE
                        );
```

- [ ] **Step 11: Keep BOOT out of pet mood and pet trigger**

In the BOOT single-click callback, remove this line:

```cpp
            static_cast<PaopaoPetDisplay*>(self->display_)->DispatchPetTrigger(PAOPAO_PET_TRIGGER_LOCAL_TAP);
```

Keep the existing system behavior:

```cpp
            if (app.GetDeviceState() == kDeviceStateStarting) {
                self->EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
```

In the BOOT long-press callback, remove the pet trigger line:

```cpp
            static_cast<PaopaoPetDisplay*>(self->display_)->DispatchPetTrigger(PAOPAO_PET_TRIGGER_LOCAL_HOLD);
```

Leave the callback body empty except for a comment:

```cpp
            // BOOT long press is reserved for future system/settings behavior.
```

- [ ] **Step 12: Run source-path integration tests**

```powershell
python -m pytest tests/xiaoxin_pet_mood_integration_path_test.py -q
```

Expected output:

```text
5 passed
```

- [ ] **Step 13: Run mood and pet regression tests**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_mood_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.c -o build/paopao_pet_mood_test.exe
.\build\paopao_pet_mood_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_trigger_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c -o build/paopao_pet_trigger_test.exe
.\build\paopao_pet_trigger_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_emotion_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_emotion.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c -o build/paopao_pet_emotion_test.exe
.\build\paopao_pet_emotion_test.exe
```

Expected output:

```text
paopao_pet_mood tests passed
paopao_pet_trigger tests passed
paopao pet emotion tests passed
```

- [ ] **Step 14: Commit board integration**

```powershell
git add -- main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc tests/xiaoxin_pet_mood_integration_path_test.py
git commit -m "feat: wire xiaoxin pet mood events"
```

---

### Task 5: Update Emotion Mapping Documentation

**Files:**
- Modify: `docs/xiaoxin-pet-emotion-gif-mapping.zh-CN.md`

**Interfaces:**
- Consumes: implemented mood module and board wiring.
- Produces: human-readable behavior notes for P1 mood policy.

- [ ] **Step 1: Add mood policy section**

In `docs/xiaoxin-pet-emotion-gif-mapping.zh-CN.md`, after section `7. 动画持续时间`, add:

```markdown
## 7.1 情绪策略层

P1 情绪系统在 `paopao_pet_trigger` 前增加 `paopao_pet_mood` 策略层。该层不直接选择 GIF 文件，而是把设备和用户事件转换为现有 `paopao_pet_trigger_event_t`：

- 低电量进入：建议 `PAOPAO_PET_TRIGGER_SERVICE_TIRED`，冷却 30 秒。
- 电量恢复：建议 `PAOPAO_PET_TRIGGER_TASK_DONE`，冷却 10 秒。
- WiFi 断开或配网中：建议 `PAOPAO_PET_TRIGGER_SERVICE_ANXIOUS`，冷却 20 秒。
- WiFi 恢复：建议 `PAOPAO_PET_TRIGGER_TASK_DONE`，冷却 10 秒。
- 语音错误：建议 `PAOPAO_PET_TRIGGER_SERVICE_FAILING`，冷却 3 秒。
- 服务端 emotion：先由 `paopao_pet_trigger_for_emotion()` 归一，再由 mood 层做 1.8 秒冷却。
- 触摸、拖动、摇晃：继续走本地即时 trigger，同时更新 mood 分数。

BOOT 按键不作为 P1 情绪系统输入。当前实现不要求 BOOT 触发宠物动画；该按键可保留给系统、调试、设置入口或后续产品决策。
```

- [ ] **Step 2: Update the testing section**

In section `9. 测试覆盖`, add one bullet:

```markdown
- `tests/paopao_pet_mood_test.c`：验证情绪策略层的分数、冷却、低电量、WiFi、语音错误、本地交互和服务端 emotion 归一后触发。
```

- [ ] **Step 3: Commit documentation**

```powershell
git add -- docs/xiaoxin-pet-emotion-gif-mapping.zh-CN.md
git commit -m "docs: describe xiaoxin pet mood policy"
```

---

### Task 6: Final Verification

**Files:**
- Verify only

**Interfaces:**
- Consumes: all previous tasks.
- Produces: evidence for completion or a precise limitation note.

- [ ] **Step 1: Run all local pet C tests**

```powershell
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_mood_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_mood.c -o build/paopao_pet_mood_test.exe
.\build\paopao_pet_mood_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_trigger_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c -o build/paopao_pet_trigger_test.exe
.\build\paopao_pet_trigger_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_emotion_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_emotion.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c -o build/paopao_pet_emotion_test.exe
.\build\paopao_pet_emotion_test.exe
gcc -std=c11 -Wall -Wextra -I. tests/paopao_pet_gif_assets_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_gif_assets.c -o build/paopao_pet_gif_assets_test.exe
.\build\paopao_pet_gif_assets_test.exe
```

Expected output includes:

```text
paopao_pet_mood tests passed
paopao_pet_trigger tests passed
paopao pet emotion tests passed
paopao pet gif assets tests passed
```

- [ ] **Step 2: Run pytest guards**

```powershell
python -m pytest tests/xiaoxin_pet_mood_integration_path_test.py tests/xiaoxin_notification_visual_path_test.py -q
```

Expected output:

```text
all selected tests passing
```

- [ ] **Step 3: Run firmware build if ESP-IDF is available**

```powershell
idf.py build
```

Expected result:

```text
Project build complete.
```

If `idf.py` is unavailable in the current shell, record that limitation in the final handoff.

- [ ] **Step 4: Inspect BOOT and mood diff**

```powershell
git diff main -- main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
```

Confirm:

```text
BOOT section contains no DispatchPetTrigger call.
SetEmotion routes through DispatchPetMoodEvent.
UpdateStatusBar calls SyncPetMoodDeviceStateLocked.
Touch and shake still produce direct local pet feedback.
```

---

## Self-Review

1. **Spec coverage:** Tasks cover pure mood module, low battery, WiFi, voice error, service emotion cooldown, local touch/drag/shake score updates, BOOT reservation, tests, docs, and final verification.
2. **Placeholder scan:** No unresolved placeholder markers or unspecified validation steps remain. Optional `idf.py build` is explicitly tied to ESP-IDF availability.
3. **Type consistency:** `paopao_pet_mood_input_t`, `paopao_pet_mood_context_t`, `paopao_pet_mood_suggestion_t`, `paopao_pet_mood_init`, and `paopao_pet_mood_handle_event` are defined in Task 2 and used consistently in later tasks.
4. **Boundary check:** Mood never returns GIF filenames or `paopao_pet_state_t`; it returns existing `paopao_pet_trigger_event_t`. BOOT is explicitly excluded from mood and pet trigger wiring.
