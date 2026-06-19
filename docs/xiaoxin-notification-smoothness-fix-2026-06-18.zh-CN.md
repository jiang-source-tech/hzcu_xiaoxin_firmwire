# 小芯通知页滑动卡顿与黑线残影修复记录

> 日期：2026-06-18
> 分支：`codex/xiaoxin-card-pager-smoothness`
> 范围：Waveshare ESP32-S3 Touch LCD 1.46 圆屏通知页

## 问题现象

实机在通知页滑动通知卡片时仍有周期性卡顿感；滑动过程中屏幕左侧会出现多段短黑线。截图形态像移动对象留下的局部刷新残影，而不是固定 UI 元素。

## 根因判断

该板子的屏幕走 `SpiLcdDisplay`，LVGL 使用约 20 行高的局部刷新 buffer，并非全屏刷新。通知卡片在拖动过程中包含半透明圆角、边框和较重的外阴影，移动时会触发较大的旧区域擦除和新区域绘制。

在 QSPI 局部刷新场景下，移动阴影尤其容易造成两个问题：

- 局部脏区擦除不完整时，屏幕边缘会出现短线状残影。
- 每一帧都重绘阴影、层级或指示点样式，会增加刷新负担，表现为滑动卡顿。

## 本次修改

修改文件：

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `tests/xiaoxin_notification_visual_path_test.py`

具体调整：

- 通知卡片容器不再绘制外阴影，避免移动中的大面积半透明阴影参与局部刷新。
- 通知滚动释放动画每帧继续走轻量视觉路径，只更新卡片位置和透明度，不刷新圆点、层级和重样式。
- 轻量拖动路径不再每帧更新通知指示点样式。
- 触摸按住期间不再每秒输出一次 `Touch point` 日志，只在按下瞬间记录一次。
- 触摸轮询间隔从 `10ms` 调整为 `16ms`，将拖动刷新频率从约 100Hz 降到接近 60Hz 的显示预算。
- 新增源码级回归测试，约束通知页热路径不要重新引入移动阴影、高频触摸轮询和完整动画刷新路径。

## 取舍

移除通知卡外阴影会让卡片的悬浮感略微降低，但可以显著降低圆屏 QSPI 局部刷新时的残影风险和每帧绘制成本。当前保留卡片背景透明度、边框、圆角和内部 icon 阴影，视觉层次仍然存在。

## 验证

已执行：

```powershell
python -m pytest tests\xiaoxin_notification_visual_path_test.py
gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_card_pager_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_card_pager.c -o build\xiaoxin_card_pager_test.exe
.\build\xiaoxin_card_pager_test.exe
& 'D:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1'
idf.py build
```

结果：

- `xiaoxin_notification_visual_path_test.py`：4 passed
- `xiaoxin_card_pager_test`：passed
- `idf.py build`：通过，生成 `build/ai_pet.bin`

构建中仍有既有的 `esp_lcd_touch_get_coordinates` deprecated warning，与本次通知页卡顿和残影修复无关。

## 后续观察

刷入固件后重点观察：

- 通知卡片滑动时左侧短黑线是否消失。
- 通知卡片连续拖动是否仍有周期性停顿。
- Home 页下方流式字幕在通知页滑动时是否仍能平稳刷新。

如果残影消失但仍有卡顿，下一步应打开已有 UI perf trace，记录 `drag` 与 `touch_loop` 的 `max_us` 峰值，再定位是否还有其他热路径参与重绘。
