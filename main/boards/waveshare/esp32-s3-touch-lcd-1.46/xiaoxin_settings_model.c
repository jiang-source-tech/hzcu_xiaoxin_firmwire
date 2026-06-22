#include "xiaoxin_settings_model.h"

static void append_item(
  xiaoxin_settings_item_t item,
  xiaoxin_settings_item_t* out,
  uint8_t max_count,
  uint8_t* count
) {
  if (out != 0 && *count < max_count) {
    out[*count] = item;
  }
  if (*count < max_count) {
    *count = (uint8_t)(*count + 1);
  }
}

uint8_t xiaoxin_settings_visible_items(
  xiaoxin_settings_caps_t caps,
  xiaoxin_settings_item_t* out,
  uint8_t max_count
) {
  uint8_t count = 0;
  append_item(XIAOXIN_SETTINGS_ITEM_BRIGHTNESS, out, max_count, &count);
  append_item(XIAOXIN_SETTINGS_ITEM_WIFI, out, max_count, &count);
  if (caps.has_power_save_scheduler) {
    append_item(XIAOXIN_SETTINGS_ITEM_POWER_SAVE, out, max_count, &count);
  }
  append_item(XIAOXIN_SETTINGS_ITEM_ABOUT, out, max_count, &count);
  if (caps.has_audio_output) {
    append_item(XIAOXIN_SETTINGS_ITEM_VOLUME, out, max_count, &count);
    append_item(XIAOXIN_SETTINGS_ITEM_MUTE, out, max_count, &count);
    append_item(XIAOXIN_SETTINGS_ITEM_PROMPT_SOUND, out, max_count, &count);
  }
  if (caps.has_vibration) {
    append_item(XIAOXIN_SETTINGS_ITEM_VIBRATION, out, max_count, &count);
  }
  return count;
}

bool xiaoxin_settings_can_open(xiaoxin_settings_runtime_state_t runtime_state) {
  return runtime_state == XIAOXIN_SETTINGS_RUNTIME_IDLE;
}

uint8_t xiaoxin_settings_clamp_percent(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return (uint8_t)value;
}

const char* xiaoxin_settings_item_title(xiaoxin_settings_item_t item) {
  switch (item) {
    case XIAOXIN_SETTINGS_ITEM_BRIGHTNESS:
      return "浜害";
    case XIAOXIN_SETTINGS_ITEM_WIFI:
      return "Wi-Fi";
    case XIAOXIN_SETTINGS_ITEM_POWER_SAVE:
      return "鐪佺數";
    case XIAOXIN_SETTINGS_ITEM_ABOUT:
      return "鍏充簬";
    case XIAOXIN_SETTINGS_ITEM_VOLUME:
      return "闊抽噺";
    case XIAOXIN_SETTINGS_ITEM_MUTE:
      return "闈欓煶";
    case XIAOXIN_SETTINGS_ITEM_PROMPT_SOUND:
      return "鎻愮ず闊?";
    case XIAOXIN_SETTINGS_ITEM_VIBRATION:
      return "闇囧姩";
    default:
      return "";
  }
}
