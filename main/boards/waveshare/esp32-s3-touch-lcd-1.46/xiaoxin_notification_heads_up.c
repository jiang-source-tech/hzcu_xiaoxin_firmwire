#include "xiaoxin_notification_heads_up.h"

#include <stdio.h>
#include <string.h>

static void copy_text(char* dest, size_t dest_size, const char* text) {
  if (dest == NULL || dest_size == 0) {
    return;
  }

  snprintf(dest, dest_size, "%s", text != NULL ? text : "");
}

static void copy_entry(
  xiaoxin_notification_heads_up_entry_t* dest,
  const xiaoxin_card_item_t* item
) {
  if (dest == NULL) {
    return;
  }

  copy_text(dest->title, sizeof(dest->title), item != NULL ? item->title : NULL);
  copy_text(dest->body, sizeof(dest->body), item != NULL ? item->body : NULL);
  copy_text(dest->tag, sizeof(dest->tag), item != NULL ? item->tag : NULL);
}

static bool entry_matches_item(
  const xiaoxin_notification_heads_up_entry_t* entry,
  const xiaoxin_card_item_t* item
) {
  if (entry == NULL || item == NULL) {
    return false;
  }

  return strcmp(entry->title, item->title != NULL ? item->title : "") == 0 &&
         strcmp(entry->body, item->body != NULL ? item->body : "") == 0 &&
         strcmp(entry->tag, item->tag != NULL ? item->tag : "") == 0;
}

static void clear_entry(xiaoxin_notification_heads_up_entry_t* entry) {
  if (entry == NULL) {
    return;
  }

  entry->title[0] = '\0';
  entry->body[0] = '\0';
  entry->tag[0] = '\0';
}

static void set_active(
  xiaoxin_notification_heads_up_t* model,
  const xiaoxin_card_item_t* item,
  int64_t now_ms
) {
  copy_entry(&model->active, item);
  model->visible = true;
  model->visible_until_ms = now_ms + XIAOXIN_NOTIFICATION_HEADS_UP_DURATION_MS;
}

static void clear_visibility(xiaoxin_notification_heads_up_t* model) {
  if (model == NULL) {
    return;
  }

  model->visible = false;
  model->visible_until_ms = 0;
}

static void shift_queue_left(xiaoxin_notification_heads_up_t* model) {
  if (model == NULL || model->queue_count == 0) {
    return;
  }

  for (uint8_t i = 0; (uint8_t)(i + 1) < model->queue_count; ++i) {
    model->queue[i] = model->queue[i + 1];
  }
  clear_entry(&model->queue[model->queue_count - 1]);
  model->queue_count--;
}

static void promote_next_if_any(xiaoxin_notification_heads_up_t* model, int64_t now_ms) {
  if (model == NULL) {
    return;
  }

  if (model->queue_count == 0) {
    clear_visibility(model);
    return;
  }

  xiaoxin_notification_heads_up_entry_t promoted = model->queue[0];
  xiaoxin_card_item_t item = {
    .title = promoted.title,
    .body = promoted.body,
    .detail = NULL,
    .tag = promoted.tag,
    .priority = 0,
    .ttl_ms = 0,
  };

  shift_queue_left(model);
  set_active(model, &item, now_ms);
}

void xiaoxin_notification_heads_up_init(xiaoxin_notification_heads_up_t* model) {
  if (model == NULL) {
    return;
  }

  memset(model, 0, sizeof(*model));
}

bool xiaoxin_notification_heads_up_enqueue(
  xiaoxin_notification_heads_up_t* model,
  const xiaoxin_card_item_t* item,
  int64_t now_ms
) {
  if (model == NULL || item == NULL || item->title == NULL || item->title[0] == '\0') {
    return false;
  }

  if (model->visible && entry_matches_item(&model->active, item)) {
    return false;
  }

  for (uint8_t i = 0; i < model->queue_count; ++i) {
    if (entry_matches_item(&model->queue[i], item)) {
      return false;
    }
  }

  if (!model->visible) {
    set_active(model, item, now_ms);
    return true;
  }

  if (model->queue_count >= XIAOXIN_NOTIFICATION_HEADS_UP_QUEUE_MAX) {
    shift_queue_left(model);
  }

  copy_entry(&model->queue[model->queue_count], item);
  model->queue_count++;
  return true;
}

bool xiaoxin_notification_heads_up_tick(
  xiaoxin_notification_heads_up_t* model,
  int64_t now_ms
) {
  if (model == NULL) {
    return false;
  }

  if (!model->visible) {
    promote_next_if_any(model, now_ms);
    return model->visible;
  }

  if (now_ms < model->visible_until_ms) {
    return true;
  }

  promote_next_if_any(model, now_ms);
  return model->visible;
}

bool xiaoxin_notification_heads_up_snapshot(
  const xiaoxin_notification_heads_up_t* model,
  xiaoxin_notification_heads_up_snapshot_t* snapshot
) {
  if (model == NULL || snapshot == NULL || !model->visible) {
    return false;
  }

  snapshot->visible = true;
  snapshot->title = model->active.title;
  snapshot->body = model->active.body;
  snapshot->tag = model->active.tag;
  return true;
}
