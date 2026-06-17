# 小芯动画与 GIF 显示更新记录

记录时间：2026-06-17  
工作分支：`codex/paopao-pet-triggers`

## 当前结论

Waveshare ESP32-S3-Touch-LCD-1.46C 的小芯桌宠显示已经从旧的 PNG 帧直绘方案，切换为 **LVGL 直接播放 GIF 资源** 的正式方案。单点验证中 `giddy.gif` 在真机上的显示效果满足预期，因此后续不再把 `paopao_pet_renderer` 作为主显示路径。

## 显示方案

- `PaopaoPetDisplay` 继承 `SpiLcdDisplay`，复用项目已有 LVGL 显示栈。
- 屏幕背景为纯黑，中心通过 LVGL `lv_image` 显示当前 GIF。
- GIF 解码使用项目已有 `LvglGif + gifdec`。
- GIF 资源统一放在 `main/assets/images/*.gif`。
- `main/CMakeLists.txt` 会把 `main/assets/images/*.gif` 作为嵌入资源打包。
- 旧的 `paopao_pet_assets.c` 和 `paopao_pet_renderer.c` 已从 Waveshare 1.46C 的构建源列表中剔除。
- 单点验证开关 `CONFIG_PAOPAO_LVGL_GIF_PROBE` 已移除，`main/paopao_giddy_probe.gif` 已删除。

## 状态映射

| 桌宠状态 | GIF 资源 |
| --- | --- |
| `idle` | `idle.gif` |
| `working` | `working.gif` |
| `thinking` | `thinking.gif` |
| `waiting` | `waiting.gif` |
| `done` | `done.gif` |
| `sleeping` | `sleeping.gif` |
| `jumping` | `jumping.gif` |
| `failing` | `failed.gif` |
| `giddy` | `giddy.gif` |
| `review` | `review.gif` |

说明：

- 原 `running-left` / `running-right` 不再作为桌宠状态保留。
- 左右拖动仍是两个输入事件：`LOCAL_DRAG_LEFT` 和 `LOCAL_DRAG_RIGHT`。
- 两个拖动事件都会触发统一的 `PAOPAO_PET_STATE_JUMPING`，显示 `jumping.gif`。

## 触发规则

- `SetStatus()`、`SetEmotion()`、`SetChatMessage()` 不直接播放资源，而是派发到 `paopao_pet_trigger`。
- `paopao_pet_trigger` 统一管理主状态、临时反应、idle 自主动作和睡眠超时。
- `CONNECTING`、注册网络、加载协议、检测模块、升级、激活等后台忙碌状态不再抢占当前桌宠动画。
- `neutral` 服务端情绪不清除 `listening` 或 `speaking`。
- `failing` 是锁定状态，普通服务端情绪建议不能覆盖。
- `giddy` 只由剧烈摇晃对应的 `LOCAL_SHAKE` 触发。
- idle 低频自主动作使用 `review`，不触发 `giddy`。
- 60 秒无交互进入 `sleeping`。

## 输入支持

- SPD2010 触摸轮询：
  - 点按触发 `LOCAL_TAP`。
  - 长按 1200ms 触发 `LOCAL_HOLD`。
  - 左右拖动超过 42px 触发 `LOCAL_DRAG_LEFT` / `LOCAL_DRAG_RIGHT`，显示 `jumping.gif`。
- QMI8658 运动轮询：
  - 每 50ms 读取加速度。
  - 达到剧烈摇晃阈值并满足冷却后触发 `LOCAL_SHAKE`。
  - `LOCAL_SHAKE` 显示 `giddy.gif`。

## 涉及文件

- `main/assets/images/*.gif`
- `main/CMakeLists.txt`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_gif_assets.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_gif_assets.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_state.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_state.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c`
- `tests/paopao_gif_probe_decode_test.c`
- `tests/paopao_pet_gif_assets_test.c`
- `tests/paopao_pet_trigger_test.c`

## 已验证

宿主机 GIF 解码验证：

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I tests/stubs -I main/display/lvgl_display/gif tests/paopao_gif_probe_decode_test.c main/display/lvgl_display/gif/gifdec.c -o build/paopao_gif_probe_decode_test.exe
.\build\paopao_gif_probe_decode_test.exe
```

输出：

```text
paopao GIF asset decode passed: 10 GIFs sampled
```

状态到 GIF 映射验证：

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I main/boards/waveshare/esp32-s3-touch-lcd-1.46 tests/paopao_pet_gif_assets_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_gif_assets.c -o build/paopao_pet_gif_assets_test.exe
.\build\paopao_pet_gif_assets_test.exe
```

输出：

```text
paopao pet GIF asset mapping tests passed
```

触发状态机验证：

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I main/boards/waveshare/esp32-s3-touch-lcd-1.46 tests/paopao_pet_trigger_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_state.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c -o build/paopao_pet_trigger_test.exe
.\build\paopao_pet_trigger_test.exe
```

输出：

```text
paopao_pet_trigger tests passed
```

格式检查：

```powershell
git diff --check
```

结果：无空白错误，仅有 Git 对 LF/CRLF 转换的提示。

## 尚未在当前终端验证

当前 PowerShell 环境找不到 `idf.py`，因此完整 ESP-IDF 构建仍需在 ESP-IDF 终端里执行：

```powershell
idf.py reconfigure
idf.py build
idf.py -p COMx flash monitor
```

真机重点确认：

- 开机是否直接进入黑底 GIF 桌宠界面。
- 语音状态、触摸、长按、拖动、剧烈摇晃是否按映射切换 GIF。
- `jumping.gif` 是否作为左右拖动统一反馈。
- `paopao_pet_renderer` 直绘路径是否不再抢屏。
