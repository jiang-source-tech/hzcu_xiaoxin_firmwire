#include "xiaoxin_battery_level.h"

uint8_t xiaoxin_battery_percent_from_mv(int voltage_mv) {
  if (voltage_mv >= 4200) {
    return 100;
  }
  if (voltage_mv <= 3300) {
    return 0;
  }

  int percent = (-1 * voltage_mv * voltage_mv + 9016 * voltage_mv - 19189000) / 10000;
  if (percent > 100) {
    percent = 100;
  }
  if (percent < 0) {
    percent = 0;
  }
  return (uint8_t)percent;
}
