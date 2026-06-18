#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  XIAOXIN_CARD_PAGE_HOME = 0,
  XIAOXIN_CARD_PAGE_NOTIFICATIONS,
  XIAOXIN_CARD_PAGE_OVERVIEW,
} xiaoxin_card_page_t;

typedef enum {
  XIAOXIN_CARD_ANIMATION_NONE = 0,
  XIAOXIN_CARD_ANIMATION_SNAP,
  XIAOXIN_CARD_ANIMATION_REBOUND,
} xiaoxin_card_animation_t;

typedef struct {
  const char* title;
  const char* body;
  const char* tag;
  uint32_t priority;
  uint32_t ttl_ms;
} xiaoxin_card_item_t;

typedef struct {
  xiaoxin_card_page_t current_page;
  xiaoxin_card_page_t target_page;
  xiaoxin_card_animation_t animation;
  int16_t screen_height;
  int16_t threshold_px;
  int16_t max_drag_px;
  int16_t start_x;
  int16_t start_y;
  int16_t last_x;
  int16_t last_y;
  int16_t offset_y;
  uint8_t notification_index;
  uint8_t notification_count;
  bool pressed;
  bool dragging;
} xiaoxin_card_pager_t;

void xiaoxin_card_pager_init(xiaoxin_card_pager_t* pager, int16_t screen_height);
void xiaoxin_card_pager_press(xiaoxin_card_pager_t* pager, int16_t x, int16_t y);
void xiaoxin_card_pager_drag(xiaoxin_card_pager_t* pager, int16_t x, int16_t y);
void xiaoxin_card_pager_release(xiaoxin_card_pager_t* pager);

xiaoxin_card_page_t xiaoxin_card_pager_current_page(const xiaoxin_card_pager_t* pager);
xiaoxin_card_page_t xiaoxin_card_pager_target_page(const xiaoxin_card_pager_t* pager);
xiaoxin_card_animation_t xiaoxin_card_pager_animation(const xiaoxin_card_pager_t* pager);
int16_t xiaoxin_card_pager_offset_y(const xiaoxin_card_pager_t* pager);
bool xiaoxin_card_pager_is_dragging(const xiaoxin_card_pager_t* pager);
bool xiaoxin_card_pager_allows_pet_interaction(const xiaoxin_card_pager_t* pager);
xiaoxin_card_page_t xiaoxin_card_pager_visual_page(const xiaoxin_card_pager_t* pager);
uint8_t xiaoxin_card_pager_notification_index(const xiaoxin_card_pager_t* pager);
uint8_t xiaoxin_card_pager_notification_count(const xiaoxin_card_pager_t* pager);
bool xiaoxin_card_pager_notification_next(xiaoxin_card_pager_t* pager);
bool xiaoxin_card_pager_notification_prev(xiaoxin_card_pager_t* pager);
const xiaoxin_card_item_t* xiaoxin_card_pager_current_notification(const xiaoxin_card_pager_t* pager);

const char* xiaoxin_card_page_name(xiaoxin_card_page_t page);
void xiaoxin_card_pager_items(
  xiaoxin_card_page_t page,
  const xiaoxin_card_item_t** items,
  uint8_t* count
);

#ifdef __cplusplus
}
#endif
