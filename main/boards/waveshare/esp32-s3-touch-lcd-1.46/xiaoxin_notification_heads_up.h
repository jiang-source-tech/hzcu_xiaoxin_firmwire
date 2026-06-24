#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "xiaoxin_card_pager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XIAOXIN_NOTIFICATION_HEADS_UP_QUEUE_MAX 3
#define XIAOXIN_NOTIFICATION_HEADS_UP_DURATION_MS 3000

typedef struct {
  char title[XIAOXIN_CARD_NOTIFICATION_TITLE_MAX];
  char body[XIAOXIN_CARD_NOTIFICATION_BODY_MAX];
  char tag[XIAOXIN_CARD_NOTIFICATION_TAG_MAX];
} xiaoxin_notification_heads_up_entry_t;

typedef struct {
  bool visible;
  int64_t visible_until_ms;
  uint8_t queue_count;
  xiaoxin_notification_heads_up_entry_t active;
  xiaoxin_notification_heads_up_entry_t queue[XIAOXIN_NOTIFICATION_HEADS_UP_QUEUE_MAX];
} xiaoxin_notification_heads_up_t;

typedef struct {
  bool visible;
  const char* title;
  const char* body;
  const char* tag;
} xiaoxin_notification_heads_up_snapshot_t;

void xiaoxin_notification_heads_up_init(xiaoxin_notification_heads_up_t* model);
bool xiaoxin_notification_heads_up_enqueue(
  xiaoxin_notification_heads_up_t* model,
  const xiaoxin_card_item_t* item,
  int64_t now_ms
);
bool xiaoxin_notification_heads_up_tick(
  xiaoxin_notification_heads_up_t* model,
  int64_t now_ms
);
bool xiaoxin_notification_heads_up_snapshot(
  const xiaoxin_notification_heads_up_t* model,
  xiaoxin_notification_heads_up_snapshot_t* snapshot
);

#ifdef __cplusplus
}
#endif
