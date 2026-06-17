# 小芯硬件端 LVGL GIF 显示方案

版本：2026-06-17
目标板：Waveshare ESP32-S3-Touch-LCD-1.46C
当前状态：单点 GIF 真机验证满意，正式切换为 LVGL GIF 显示路径。

## 一句话结论

小芯硬件端当前采用 **LVGL + GIF** 方案：桌宠资源放在 `main/assets/images`，固件通过 `LvglGif + gifdec` 解码 GIF，再用 LVGL `lv_image` 显示。旧的 PNG 帧直绘路径不再作为 Waveshare 1.46C 的主显示方式。

## 为什么切换

前期讨论过“LVGL 背景 + 直绘顶层”的混合方案，是为了解决动画盖住 LVGL UI 或被 LVGL 覆盖的问题。但单点验证表明，当前 GIF 资源直接走 LVGL 播放时，真机效果已经可以接受。

因此当前选择更简单、更符合项目显示栈的路线：

- 不再维护独立 LCD 直绘帧循环。
- 不再让 `esp_lcd_panel_draw_bitmap` 和 LVGL 抢屏。
- 继续复用 `SpiLcdDisplay`、`lv_image`、LVGL 锁和 `esp_lvgl_port`。
- 后续新增状态时只需要补 GIF 文件和状态映射。

## 资源目录

正式 GIF 资源目录：

```text
main/assets/images
```

当前目录中包含：

```text
anxiety.gif
done.gif
failed.gif
giddy.gif
happy.gif
idle.gif
jumping.gif
review.gif
sleeping.gif
stamp.gif
thinking.gif
tired.gif
waiting.gif
waving.gif
working.gif
```

当前固件状态机直接使用其中 10 个 GIF：`idle`、`working`、`thinking`、`waiting`、`done`、`sleeping`、`jumping`、`failed`、`giddy`、`review`。

## 当前状态映射

| 内部状态 | 显示资源 | 说明 |
| --- | --- | --- |
| `PAOPAO_PET_STATE_IDLE` | `idle.gif` | 默认待机 |
| `PAOPAO_PET_STATE_WORKING` | `working.gif` | 说话/工作中 |
| `PAOPAO_PET_STATE_THINKING` | `thinking.gif` | 思考中 |
| `PAOPAO_PET_STATE_WAITING` | `waiting.gif` | 聆听/等待用户输入 |
| `PAOPAO_PET_STATE_DONE` | `done.gif` | 短反馈/完成 |
| `PAOPAO_PET_STATE_SLEEPING` | `sleeping.gif` | 休眠 |
| `PAOPAO_PET_STATE_JUMPING` | `jumping.gif` | 左右拖动统一反馈 |
| `PAOPAO_PET_STATE_FAILING` | `failed.gif` | 错误/失败 |
| `PAOPAO_PET_STATE_GIDDY` | `giddy.gif` | 剧烈摇晃 |
| `PAOPAO_PET_STATE_REVIEW` | `review.gif` | idle 低频自主动作 |

说明：原 `running-left` / `running-right` 不再保留为状态，左右拖动统一触发 `jumping`。

## 构建集成

`main/CMakeLists.txt` 针对 Waveshare 1.46C 做了两件事：

- `file(GLOB PAOPAO_GIF_FILES ... assets/images/*.gif)` 收集 GIF 资源并加入 `EMBED_FILES`。
- 从该板子的源文件列表中剔除旧的 `paopao_pet_assets.c` 和 `paopao_pet_renderer.c`，避免旧 PNG 帧直绘资源继续进入固件。

单点验证开关 `CONFIG_PAOPAO_LVGL_GIF_PROBE` 已移除。正式路径不再区分“探针模式”和“正常模式”。

## 显示实现

核心文件：

```text
main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
```

当前实现要点：

- `PaopaoPetDisplay` 继承 `SpiLcdDisplay`。
- `SetupUI()` 创建黑色 LVGL 背景和居中的 `pet_image_`。
- `PlayGifState()` 根据当前桌宠状态选择嵌入 GIF 二进制。
- `LvglGif` 解码 GIF 到 ARGB8888 帧。
- 每帧回调里调用 `lv_image_set_src(pet_image_, gif_controller_->image_dsc())` 刷新显示。
- 状态变化时停止旧 GIF，加载并启动新 GIF。

## 触发体系

核心文件：

```text
main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.h
main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c
```

触发层负责管理：

- 主状态：语音/系统长期状态。
- 临时反应：点击、拖动、完成、服务端情绪等短反馈。
- idle 自主动作：低频触发 `review`。
- 睡眠超时：60 秒无交互进入 `sleeping`。
- 锁定状态：`failing` 不被普通表情建议覆盖。

## 本地输入

触摸：

- 点按：`LOCAL_TAP`，短暂显示 `done.gif`。
- 长按 1200ms：`LOCAL_HOLD`，切换 `sleeping` / `idle`。
- 左右拖动超过 42px：`LOCAL_DRAG_LEFT` / `LOCAL_DRAG_RIGHT`，统一显示 `jumping.gif`。

运动：

- QMI8658 每 50ms 轮询。
- 剧烈摇晃触发 `LOCAL_SHAKE`。
- `LOCAL_SHAKE` 显示 `giddy.gif`。

## 已通过的本地验证

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I tests/stubs -I main/display/lvgl_display/gif tests/paopao_gif_probe_decode_test.c main/display/lvgl_display/gif/gifdec.c -o build/paopao_gif_probe_decode_test.exe
.\build\paopao_gif_probe_decode_test.exe
```

```text
paopao GIF asset decode passed: 10 GIFs sampled
```

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I main/boards/waveshare/esp32-s3-touch-lcd-1.46 tests/paopao_pet_gif_assets_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_gif_assets.c -o build/paopao_pet_gif_assets_test.exe
.\build\paopao_pet_gif_assets_test.exe
```

```text
paopao pet GIF asset mapping tests passed
```

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I main/boards/waveshare/esp32-s3-touch-lcd-1.46 tests/paopao_pet_trigger_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_state.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c -o build/paopao_pet_trigger_test.exe
.\build\paopao_pet_trigger_test.exe
```

```text
paopao_pet_trigger tests passed
```

## 真机构建与烧录

当前普通 PowerShell 中找不到 `idf.py`，完整构建需要在 ESP-IDF 终端运行：

```powershell
idf.py reconfigure
idf.py build
idf.py -p COMx flash monitor
```

真机验收重点：

- 黑底 GIF 桌宠是否开机直接显示。
- 所有状态 GIF 是否能正确切换。
- 左右拖动是否统一显示 `jumping.gif`。
- 剧烈摇晃是否显示 `giddy.gif`，普通移动不误触发。
- 旧 PNG 直绘不再抢屏。
