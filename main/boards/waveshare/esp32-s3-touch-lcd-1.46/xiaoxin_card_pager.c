#include "xiaoxin_card_pager.h"

#include <stdlib.h>

static const int16_t k_min_vertical_drag_px = 6;

static const xiaoxin_card_item_t k_notification_items[] = {
  {"低电量", "请尽快充电", "电量", 1, 0},
  {"课程变更", "下一节课教室待确认", "课表", 2, 0},
  {"聊天提醒", "有新的小芯消息", "聊天", 3, 0},
  {"网络异常", "连接不稳定", "网络", 4, 0},
};

static const xiaoxin_card_item_t k_overview_items[] = {
  {"下一节课", "高等数学 10:10", "课程", 1, 0},
  {"校园导航", "打开常用地点入口", "导航", 2, 0},
  {"天气", "多云 26C", "天气", 3, 0},
  {"今日待办", "还有 2 项", "待办", 4, 0},
};

static uint8_t notification_item_count(void) {
  return (uint8_t)(sizeof(k_notification_items) / sizeof(k_notification_items[0]));
}

static bool notification_raw_dismissed(const xiaoxin_card_pager_t* pager, uint8_t raw_index) {
  return pager != NULL && (pager->notification_dismissed_mask & (uint8_t)(1u << raw_index)) != 0;
}

static uint8_t notification_visible_count_for(const xiaoxin_card_pager_t* pager) {
  uint8_t count = 0;
  const uint8_t total = notification_item_count();
  for (uint8_t raw = 0; raw < total; ++raw) {
    if (!notification_raw_dismissed(pager, raw)) {
      count++;
    }
  }
  return count;
}

static int8_t notification_raw_index_for_visible(const xiaoxin_card_pager_t* pager, uint8_t visible_index) {
  uint8_t visible = 0;
  const uint8_t total = notification_item_count();
  for (uint8_t raw = 0; raw < total; ++raw) {
    if (notification_raw_dismissed(pager, raw)) {
      continue;
    }
    if (visible == visible_index) {
      return (int8_t)raw;
    }
    visible++;
  }
  return -1;
}

static void clamp_notification_index(xiaoxin_card_pager_t* pager) {
  if (pager == NULL) {
    return;
  }
  const uint8_t visible = notification_visible_count_for(pager);
  if (visible == 0) {
    pager->notification_index = 0;
  } else if (pager->notification_index >= visible) {
    pager->notification_index = (uint8_t)(visible - 1);
  }
}

static void sync_notification_count(xiaoxin_card_pager_t* pager) {
  if (pager == NULL) {
    return;
  }
  pager->notification_count = notification_visible_count_for(pager);
}

static int16_t abs_i16(int16_t value) {
  return (int16_t)(value < 0 ? -value : value);
}

static int16_t clamp_i16(int16_t value, int16_t min_value, int16_t max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static xiaoxin_card_page_t target_for_delta(
  xiaoxin_card_page_t current_page,
  int16_t dy
) {
  if (dy > 0) {
    return current_page == XIAOXIN_CARD_PAGE_OVERVIEW
      ? XIAOXIN_CARD_PAGE_HOME
      : XIAOXIN_CARD_PAGE_NOTIFICATIONS;
  }
  if (dy < 0) {
    return current_page == XIAOXIN_CARD_PAGE_NOTIFICATIONS
      ? XIAOXIN_CARD_PAGE_HOME
      : XIAOXIN_CARD_PAGE_OVERVIEW;
  }
  return current_page;
}

void xiaoxin_card_pager_init(xiaoxin_card_pager_t* pager, int16_t screen_height) {
  if (pager == NULL) {
    return;
  }

  pager->current_page = XIAOXIN_CARD_PAGE_HOME;
  pager->target_page = XIAOXIN_CARD_PAGE_HOME;
  pager->animation = XIAOXIN_CARD_ANIMATION_NONE;
  pager->screen_height = screen_height;
  pager->threshold_px = (int16_t)((screen_height * 20) / 100);
  pager->max_drag_px = screen_height;
  pager->start_x = 0;
  pager->start_y = 0;
  pager->last_x = 0;
  pager->last_y = 0;
  pager->offset_y = 0;
  pager->notification_index = 0;
  pager->notification_dismissed_mask = 0;
  sync_notification_count(pager);
  pager->pressed = false;
  pager->dragging = false;
}

void xiaoxin_card_pager_press(xiaoxin_card_pager_t* pager, int16_t x, int16_t y) {
  if (pager == NULL) {
    return;
  }

  pager->start_x = x;
  pager->start_y = y;
  pager->last_x = x;
  pager->last_y = y;
  pager->target_page = pager->current_page;
  pager->animation = XIAOXIN_CARD_ANIMATION_NONE;
  pager->offset_y = 0;
  pager->pressed = true;
  pager->dragging = false;
}

void xiaoxin_card_pager_drag(xiaoxin_card_pager_t* pager, int16_t x, int16_t y) {
  if (pager == NULL || !pager->pressed) {
    return;
  }

  pager->last_x = x;
  pager->last_y = y;

  const int16_t dx = (int16_t)(x - pager->start_x);
  const int16_t dy = (int16_t)(y - pager->start_y);
  const int16_t abs_dx = abs_i16(dx);
  const int16_t abs_dy = abs_i16(dy);
  if (abs_dy < k_min_vertical_drag_px || abs_dx > abs_dy) {
    pager->dragging = false;
    pager->target_page = pager->current_page;
    pager->offset_y = 0;
    return;
  }

  pager->dragging = true;
  pager->target_page = target_for_delta(pager->current_page, dy);
  pager->offset_y = clamp_i16(dy, (int16_t)-pager->max_drag_px, pager->max_drag_px);
}

void xiaoxin_card_pager_release(xiaoxin_card_pager_t* pager) {
  if (pager == NULL || !pager->pressed) {
    return;
  }

  if (pager->dragging && abs_i16(pager->offset_y) >= pager->threshold_px) {
    pager->current_page = pager->target_page;
    pager->animation = XIAOXIN_CARD_ANIMATION_SNAP;
  } else {
    pager->target_page = pager->current_page;
    pager->animation = pager->dragging
      ? XIAOXIN_CARD_ANIMATION_REBOUND
      : XIAOXIN_CARD_ANIMATION_NONE;
  }

  pager->offset_y = 0;
  pager->pressed = false;
  pager->dragging = false;
}

xiaoxin_card_page_t xiaoxin_card_pager_current_page(const xiaoxin_card_pager_t* pager) {
  return pager != NULL ? pager->current_page : XIAOXIN_CARD_PAGE_HOME;
}

xiaoxin_card_page_t xiaoxin_card_pager_target_page(const xiaoxin_card_pager_t* pager) {
  return pager != NULL ? pager->target_page : XIAOXIN_CARD_PAGE_HOME;
}

xiaoxin_card_animation_t xiaoxin_card_pager_animation(const xiaoxin_card_pager_t* pager) {
  return pager != NULL ? pager->animation : XIAOXIN_CARD_ANIMATION_NONE;
}

int16_t xiaoxin_card_pager_offset_y(const xiaoxin_card_pager_t* pager) {
  return pager != NULL ? pager->offset_y : 0;
}

bool xiaoxin_card_pager_is_dragging(const xiaoxin_card_pager_t* pager) {
  return pager != NULL && pager->dragging;
}

bool xiaoxin_card_pager_allows_pet_interaction(const xiaoxin_card_pager_t* pager) {
  return pager == NULL || pager->current_page == XIAOXIN_CARD_PAGE_HOME;
}

xiaoxin_card_page_t xiaoxin_card_pager_visual_page(const xiaoxin_card_pager_t* pager) {
  if (pager == NULL) {
    return XIAOXIN_CARD_PAGE_HOME;
  }

  if (!pager->dragging) {
    return pager->current_page;
  }

  return pager->target_page == XIAOXIN_CARD_PAGE_HOME
    ? pager->current_page
    : pager->target_page;
}

uint8_t xiaoxin_card_pager_notification_index(const xiaoxin_card_pager_t* pager) {
  return pager != NULL ? pager->notification_index : 0;
}

uint8_t xiaoxin_card_pager_notification_count(const xiaoxin_card_pager_t* pager) {
  return pager != NULL ? notification_visible_count_for(pager) : notification_item_count();
}

const xiaoxin_card_item_t* xiaoxin_card_pager_notification_at(
  const xiaoxin_card_pager_t* pager,
  uint8_t visible_index
) {
  const int8_t raw = notification_raw_index_for_visible(pager, visible_index);
  if (raw < 0 || raw >= (int8_t)notification_item_count()) {
    return NULL;
  }
  return &k_notification_items[raw];
}

bool xiaoxin_card_pager_notification_dismiss(xiaoxin_card_pager_t* pager, uint8_t visible_index) {
  if (pager == NULL) {
    return false;
  }
  const int8_t raw = notification_raw_index_for_visible(pager, visible_index);
  if (raw < 0) {
    return false;
  }
  pager->notification_dismissed_mask |= (uint8_t)(1u << (uint8_t)raw);
  clamp_notification_index(pager);
  sync_notification_count(pager);
  return true;
}

void xiaoxin_card_pager_notification_clear_all(xiaoxin_card_pager_t* pager) {
  if (pager == NULL) {
    return;
  }
  const uint8_t total = notification_item_count();
  uint8_t mask = 0;
  for (uint8_t raw = 0; raw < total; ++raw) {
    mask |= (uint8_t)(1u << raw);
  }
  pager->notification_dismissed_mask = mask;
  pager->notification_index = 0;
  sync_notification_count(pager);
}

bool xiaoxin_card_pager_notification_empty(const xiaoxin_card_pager_t* pager) {
  return xiaoxin_card_pager_notification_count(pager) == 0;
}

bool xiaoxin_card_pager_notification_next(xiaoxin_card_pager_t* pager) {
  if (pager == NULL || xiaoxin_card_pager_notification_empty(pager)) {
    return false;
  }

  if ((uint8_t)(pager->notification_index + 1) >= xiaoxin_card_pager_notification_count(pager)) {
    return false;
  }

  pager->notification_index++;
  return true;
}

bool xiaoxin_card_pager_notification_prev(xiaoxin_card_pager_t* pager) {
  if (pager == NULL || xiaoxin_card_pager_notification_empty(pager) || pager->notification_index == 0) {
    return false;
  }

  pager->notification_index--;
  return true;
}

const xiaoxin_card_item_t* xiaoxin_card_pager_current_notification(const xiaoxin_card_pager_t* pager) {
  if (pager == NULL || xiaoxin_card_pager_notification_empty(pager)) {
    return NULL;
  }

  uint8_t index = pager->notification_index;
  const uint8_t visible_count = xiaoxin_card_pager_notification_count(pager);
  if (index >= visible_count) {
    index = (uint8_t)(visible_count - 1);
  }
  return xiaoxin_card_pager_notification_at(pager, index);
}

const char* xiaoxin_card_page_name(xiaoxin_card_page_t page) {
  switch (page) {
    case XIAOXIN_CARD_PAGE_HOME:
      return "home";
    case XIAOXIN_CARD_PAGE_NOTIFICATIONS:
      return "notifications";
    case XIAOXIN_CARD_PAGE_OVERVIEW:
      return "overview";
    default:
      return "home";
  }
}

void xiaoxin_card_pager_items(
  xiaoxin_card_page_t page,
  const xiaoxin_card_item_t** items,
  uint8_t* count
) {
  if (items == NULL || count == NULL) {
    return;
  }

  switch (page) {
    case XIAOXIN_CARD_PAGE_NOTIFICATIONS:
      *items = k_notification_items;
      *count = notification_item_count();
      break;
    case XIAOXIN_CARD_PAGE_OVERVIEW:
      *items = k_overview_items;
      *count = (uint8_t)(sizeof(k_overview_items) / sizeof(k_overview_items[0]));
      break;
    case XIAOXIN_CARD_PAGE_HOME:
    default:
      *items = NULL;
      *count = 0;
      break;
  }
}
