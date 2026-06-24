#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h"
#include "../main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_notification_heads_up.h"

static xiaoxin_card_item_t item(const char* title, const char* body, const char* tag) {
  xiaoxin_card_item_t value = {
    .title = title,
    .body = body,
    .detail = NULL,
    .tag = tag,
    .priority = 1,
    .ttl_ms = 0,
  };
  return value;
}

static void enqueue_starts_visible_banner_for_three_seconds(void) {
  xiaoxin_notification_heads_up_t model;
  xiaoxin_notification_heads_up_init(&model);

  assert(XIAOXIN_NOTIFICATION_HEADS_UP_DURATION_MS == 3000);

  xiaoxin_card_item_t voice = item("Voice recognition failed", "Could not hear that clearly", "voice");
  assert(xiaoxin_notification_heads_up_enqueue(&model, &voice, 1000));

  xiaoxin_notification_heads_up_snapshot_t snapshot = {};
  assert(xiaoxin_notification_heads_up_snapshot(&model, &snapshot));
  assert(snapshot.visible);
  assert(strcmp(snapshot.title, "Voice recognition failed") == 0);
  assert(strcmp(snapshot.body, "Could not hear that clearly") == 0);
  assert(strcmp(snapshot.tag, "voice") == 0);

  assert(xiaoxin_notification_heads_up_tick(&model, 3999));
  assert(xiaoxin_notification_heads_up_snapshot(&model, &snapshot));

  assert(!xiaoxin_notification_heads_up_tick(&model, 4000));
  assert(!xiaoxin_notification_heads_up_snapshot(&model, &snapshot));
}

static void queued_notifications_show_in_order(void) {
  xiaoxin_notification_heads_up_t model;
  xiaoxin_notification_heads_up_init(&model);

  xiaoxin_card_item_t wifi = item("WiFi disconnected", "Please check the network", "network");
  xiaoxin_card_item_t ota = item("OTA update", "New version available", "system");
  assert(xiaoxin_notification_heads_up_enqueue(&model, &wifi, 5000));
  assert(xiaoxin_notification_heads_up_enqueue(&model, &ota, 5100));

  xiaoxin_notification_heads_up_snapshot_t snapshot = {};
  assert(xiaoxin_notification_heads_up_snapshot(&model, &snapshot));
  assert(strcmp(snapshot.title, "WiFi disconnected") == 0);

  assert(xiaoxin_notification_heads_up_tick(&model, 8000));
  assert(xiaoxin_notification_heads_up_snapshot(&model, &snapshot));
  assert(strcmp(snapshot.title, "OTA update") == 0);
}

static void duplicate_visible_content_is_not_queued_again(void) {
  xiaoxin_notification_heads_up_t model;
  xiaoxin_notification_heads_up_init(&model);

  xiaoxin_card_item_t first = item("WiFi disconnected", "Please check the network", "network");
  xiaoxin_card_item_t same = item("WiFi disconnected", "Please check the network", "network");
  assert(xiaoxin_notification_heads_up_enqueue(&model, &first, 1000));
  assert(!xiaoxin_notification_heads_up_enqueue(&model, &same, 1100));
}

static void overflow_drops_oldest_queued_banner(void) {
  xiaoxin_notification_heads_up_t model;
  xiaoxin_notification_heads_up_init(&model);

  xiaoxin_card_item_t active = item("Low battery", "Please charge soon", "battery");
  xiaoxin_card_item_t first = item("WiFi disconnected", "first queued", "network");
  xiaoxin_card_item_t second = item("OTA update", "second queued", "system");
  xiaoxin_card_item_t third = item("Class reminder", "third queued", "course");
  xiaoxin_card_item_t newest = item("Voice recognition failed", "newest queued", "voice");

  assert(xiaoxin_notification_heads_up_enqueue(&model, &active, 1000));
  assert(xiaoxin_notification_heads_up_enqueue(&model, &first, 1100));
  assert(xiaoxin_notification_heads_up_enqueue(&model, &second, 1200));
  assert(xiaoxin_notification_heads_up_enqueue(&model, &third, 1300));
  assert(xiaoxin_notification_heads_up_enqueue(&model, &newest, 1400));

  xiaoxin_notification_heads_up_snapshot_t snapshot = {};
  assert(xiaoxin_notification_heads_up_snapshot(&model, &snapshot));
  assert(strcmp(snapshot.title, "Low battery") == 0);

  assert(xiaoxin_notification_heads_up_tick(&model, 4000));
  assert(xiaoxin_notification_heads_up_snapshot(&model, &snapshot));
  assert(strcmp(snapshot.title, "OTA update") == 0);

  assert(xiaoxin_notification_heads_up_tick(&model, 7000));
  assert(xiaoxin_notification_heads_up_snapshot(&model, &snapshot));
  assert(strcmp(snapshot.title, "Class reminder") == 0);

  assert(xiaoxin_notification_heads_up_tick(&model, 10000));
  assert(xiaoxin_notification_heads_up_snapshot(&model, &snapshot));
  assert(strcmp(snapshot.title, "Voice recognition failed") == 0);
}

static void empty_body_snapshot_keeps_title_only(void) {
  xiaoxin_notification_heads_up_t model;
  xiaoxin_notification_heads_up_init(&model);

  xiaoxin_card_item_t low = item("Low battery", "", "battery");
  assert(xiaoxin_notification_heads_up_enqueue(&model, &low, 1000));

  xiaoxin_notification_heads_up_snapshot_t snapshot = {};
  assert(xiaoxin_notification_heads_up_snapshot(&model, &snapshot));
  assert(strcmp(snapshot.title, "Low battery") == 0);
  assert(strcmp(snapshot.body, "") == 0);
}

int main(void) {
  enqueue_starts_visible_banner_for_three_seconds();
  queued_notifications_show_in_order();
  duplicate_visible_content_is_not_queued_again();
  overflow_drops_oldest_queued_banner();
  empty_body_snapshot_keeps_title_only();
  puts("xiaoxin_notification_heads_up tests passed");
  return 0;
}
