# Update

## 2026-06-28 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46/1.46C 扬声器输出增强器

#### 背景

Waveshare ESP32-S3 Touch LCD 1.46/1.46C 的外放音量和听感需要在不影响语音输入链路的前提下增强。本轮只在播放端加入 `SpeakerOutputEnhancer`，让云端 TTS、电话类下行音频以及本地 `PlaySound()` OGG 提示音统一经过同一套后处理。

#### 修改内容

- 播放链路接入：
  - `SpeakerOutputEnhancer` 位于 `AudioService::AudioOutputTask()` 和 `AudioCodec::OutputData()` 之间。
  - 云端 TTS/电话类下行音频和 `PlaySound()` OGG 提示音都会在送入 codec 输出前统一处理。
  - 不改变麦克风输入、唤醒词、AEC、VAD、协议封包或上行 Opus 编码路径。
- 音频处理策略：
  - 使用 `esp_audio_effects` 的 `esp_ae_eq` / `esp_ae_drc` 做 EQ/DRC。
  - 在 EQ/DRC 之后增加纯软件后级 limiter，避免增强后样本越界削波。
  - 新增主机侧 C++ limiter 测试，覆盖 `-3 dBFS` 峰值换算、正负样本限幅、空 frame 和处理前后长度保持。
- 配置开关：
  - 新增 `CONFIG_SPEAKER_OUTPUT_ENHANCER`。
  - 默认只对 `BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_46` 启用，避免影响其他板型默认行为。

#### 涉及文件

- `main/audio/speaker_output_enhancer.h`
- `main/audio/speaker_output_enhancer.cc`
- `main/audio/speaker_output_limiter.h`
- `main/audio/speaker_output_limiter.cc`
- `main/audio/audio_service.cc`
- `main/audio/audio_service.h`
- `main/CMakeLists.txt`
- `main/Kconfig.projbuild`
- `tests/speaker_output_limiter_test.cc`
- `docs/update.md`

#### 验证结果

- `$env:PATH='D:\msys64\ucrt64\bin;' + $env:PATH; g++ -std=c++17 -Wall -Wextra -Werror tests\speaker_output_limiter_test.cc main\audio\speaker_output_limiter.cc -I main\audio -o build\speaker_output_limiter_test.exe; .\build\speaker_output_limiter_test.exe`：通过，输出 `speaker_output_limiter tests passed`。
- `git diff --check`：通过，无 whitespace error。
- `D:\Espressif\tools\ninja\1.12.1\ninja.exe -C build esp-idf/main/CMakeFiles/__idf_main.dir/audio/audio_service.cc.obj`：首次在未加载 ESP-IDF export 的 shell 中触发 CMake 重新生成失败，摘要为找不到 `/tools/cmake/project.cmake`，并出现 `Unknown CMake command "idf_build_set_property"`。
- `& 'D:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1'; D:\Espressif\tools\ninja\1.12.1\ninja.exe -C build esp-idf/main/CMakeFiles/__idf_main.dir/audio/audio_service.cc.obj`：通过，成功构建 `esp-idf/main/CMakeFiles/__idf_main.dir/audio/audio_service.cc.obj`。
- `& 'D:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1'; D:\Espressif\tools\ninja\1.12.1\ninja.exe -C build esp-idf/main/CMakeFiles/__idf_main.dir/audio/speaker_output_enhancer.cc.obj`：通过，成功构建 `speaker_output_enhancer.cc.obj`。
- `& 'D:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1'; D:\Espressif\tools\ninja\1.12.1\ninja.exe -C build`：通过，成功生成 `build/ai_pet.bin`。
- 尚未执行实机 flash / smoke test；仍需在 Waveshare ESP32-S3 Touch LCD 1.46/1.46C 上确认外放响度、失真感和提示音听感。

## 2026-06-28 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 待机贪吃蛇成长与低功耗显示增强

#### 背景

低功耗待机时钟页已经加入随机贪吃蛇屏保，但蛇身长度固定，长时间待机时缺少反馈变化；实机也反馈待机屏整体亮度偏低，星期/日期区域在黑底和反光环境下不够醒目。本轮在不改变待机页主结构的前提下，增强贪吃蛇屏保的成长反馈，并提升待机信息可读性。

#### 修改内容

- 贪吃蛇吃果成长：
  - 新增低功耗贪吃蛇果子位置状态，果子只生成在圆形屏幕内、蛇身以外的可用格子。
  - 蛇头吃到果子后记录吃果次数，每吃满 `8` 个果子增长一格。
  - 蛇身最大长度限制为 `24`，达到上限后继续吃果不会越界增长。
  - 增长时保留上一帧尾巴，让视觉上表现为蛇身变长，而不是移动后长度不变。
  - 候选方向选择更偏向靠近果子的安全方向，同时继续保留随机游走感。
  - 果子绘制为高亮青绿色小方块，并随背景绘制对象一起刷新。
- 待机屏亮度提升：
  - `XIAOXIN_LOW_POWER_CLOCK_DEFAULT_BRIGHTNESS` 从 `12%` 提升到 `24%`。
  - 只影响进入低功耗待机时钟页后的专用背光亮度。
  - 退出低功耗待机页后仍调用 `RestoreBrightness()` 恢复用户正常亮度。
- 星期/日期可读性增强：
  - 待机页星期/日期标签从主题提示小字改为 `font_puhui_basic_20_4`。
  - 日期标签透明度从 `80%` 提升到 `90%`。
  - 保持日期位置在顶部区域，不改中心大时间、底部 `POWER 唤醒` 提示和同步状态布局。
- 测试 guardrail 补充：
  - 扩展低功耗时钟模型测试，锁定待机默认亮度为 `24%`。
  - 扩展低功耗时钟视觉路径测试，锁定日期使用更大的专用字体和更高对比度。
  - 保留贪吃蛇成长、果子生成、最大长度、尾巴保留和安全移动相关路径断言。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_low_power_clock_model.h`
- `tests/xiaoxin_low_power_clock_model_test.c`
- `tests/xiaoxin_low_power_clock_visual_path_test.py`
- `docs/superpowers/plans/2026-06-28-xiaoxin-snake-fruit-growth.md`
- `docs/superpowers/specs/2026-06-28-xiaoxin-snake-fruit-growth-design.zh-CN.md`
- `docs/update.md`

#### 验证结果

- `python -m pytest tests\xiaoxin_low_power_clock_visual_path_test.py tests\xiaoxin_power_latch_path_test.py tests\xiaoxin_boot_diagnostics_path_test.py`：通过，52 passed。
- `$env:PATH="D:\Espressif\tools\ccache\4.12.1\ccache-4.12.1-windows-x86_64;$env:PATH"; D:\Espressif\tools\cmake\3.30.2\bin\cmake.exe --build build -j 4`：通过，生成 `build/ai_pet.bin`。
- 构建仍有既有 `esp_lcd_touch_get_coordinates()` deprecated warning、`InitializeCardPagerLayer()` 中 `lvgl_theme` 未使用 warning，以及 `ESP_IDF_VERSION` 环境变量 warning；本轮未处理这些既有警告。
- 尚未执行实机 flash；仍需在硬件上确认吃果成长节奏、24% 待机亮度和日期字号观感。

## 2026-06-28 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 小芯唤醒词音频不上送

#### 背景

前一轮已经把屏幕上的用户 STT 字幕从“小新”归一显示为“小芯”，但实机继续反馈：模型侧仍然“听到”的是“小新”。继续追踪唤醒数据流后确认，`ContinueWakeWordInvoke()` 在 `CONFIG_SEND_WAKE_WORD_DATA` 开启时会先把本地缓存的唤醒词音频片段上传给服务端，再发送结构化的 `listen.detect` 文本。服务端对这段同音唤醒音频做 ASR 时仍会猜成“小新”，因此模型侧上下文仍可能拿到“小新”。

#### 修改内容

- 调整唤醒后的服务端通知路径：
  - 不再从 `audio_service_.PopWakeWordPacket()` 取出唤醒词音频并通过 `protocol_->SendAudio(...)` 上传。
  - 保留 `protocol_->SendWakeWordDetected(wake_word)`，继续向服务端发送结构化 `listen.detect` 文本。
  - 当前自定义唤醒词的 `wake_word` 文本仍来自 `CONFIG_CUSTOM_WAKE_WORD_DISPLAY="小芯"`。
- 保持后续监听流程不变：
  - 仍设置 `play_popup_on_listening_ = true`。
  - 仍进入 `SetListeningMode(GetDefaultListeningMode())`。
  - 不改音频通道打开、TTS/STT 回调、显示层字幕归一化和协议封包格式。
- 扩展路径测试，确认 `ContinueWakeWordInvoke()` 在发送唤醒检测事件时不再上传 wake word 音频，避免模型侧先收到由 ASR 猜测出的“小新”。

#### 涉及文件

- `main/application.cc`
- `tests/xiaoxin_stt_device_name_normalization_test.py`
- `docs/update.md`

#### 验证结果

- `pytest tests\xiaoxin_stt_device_name_normalization_test.py -q`：通过，3 passed。
- `pytest tests\xiaoxin_bottom_subtitle_stream_test.py tests\xiaoxin_error_display_path_test.py -q`：通过，8 passed。
- `git diff --check -- main\application.cc tests\xiaoxin_stt_device_name_normalization_test.py`：通过，仅提示 Git 行尾规则会在工作区保留原样。
- `$env:PATH="D:\Espressif\tools\ccache\4.12.1\ccache-4.12.1-windows-x86_64;$env:PATH"; D:\Espressif\tools\cmake\3.30.2\bin\cmake.exe --build build --target ai_pet.elf -j 4`：通过，`ai_pet.elf` 链接成功。
- 尚未执行实机 flash；仍需在硬件上确认唤醒后模型侧不再把唤醒词记成“小新”。

## 2026-06-28 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 小芯 STT 字幕名称归一化

#### 背景

实机语音交互中，用户呼叫“小芯”后，屏幕上显示的用户识别字幕经常被服务端 ASR 转写为“小新”。本地唤醒词配置使用的是拼音 `xiao xin`，只能判断发音，无法区分“芯/新”等同音汉字；因此需要在字幕显示层对设备名做统一展示，避免用户看到的产品名漂移。

#### 修改内容

- 在 `Application` 入站 STT 路径中新增设备名归一化：
  - 服务端 `type=stt` 返回的用户识别文本在显示前调用 `NormalizeXiaoxinDeviceName()`。
  - 将 `小新`、`晓新` 统一替换为 `小芯`。
  - 原始 ASR 文本仍保留在日志 `ESP_LOGI(TAG, ">> ...")` 中，方便后续排查识别质量。
- 归一化范围保持收敛：
  - 只作用于用户识别字幕 `SetChatMessage("user", ...)`。
  - 不改助手 TTS 字幕、系统消息、唤醒词模型和 WebSocket 协议状态机。
- 新增路径测试，锁定 STT 字幕进入显示前必须经过设备名归一化，并确认助手 TTS 字幕不走该修正路径。

#### 涉及文件

- `main/application.cc`
- `tests/xiaoxin_stt_device_name_normalization_test.py`
- `docs/update.md`

#### 验证结果

- `pytest tests\xiaoxin_stt_device_name_normalization_test.py -q`：通过，2 passed。
- `pytest tests\xiaoxin_bottom_subtitle_stream_test.py tests\xiaoxin_error_display_path_test.py -q`：通过，8 passed。
- `git diff --check -- main\application.cc tests\xiaoxin_stt_device_name_normalization_test.py`：通过，仅提示 Git 行尾规则会在工作区保留原样。
- `$env:PATH="D:\Espressif\tools\ccache\4.12.1\ccache-4.12.1-windows-x86_64;$env:PATH"; D:\Espressif\tools\cmake\3.30.2\bin\cmake.exe --build build --target ai_pet.elf -j 4`：通过，`ai_pet.elf` 链接成功。
- 尚未执行实机 flash；仍需在硬件上确认呼叫“小芯”后的屏幕字幕展示已稳定为“小芯”。

## 2026-06-28 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 低功耗贪吃蛇随机屏保优化

#### 背景

低功耗时钟页的贪吃蛇屏保最初沿固定路径循环，观感容易重复；改成纯随机游走后，又会偶尔走进局部死胡同，随后重置或掉头，视觉上像“从头开始”。本轮把蛇改为带安全评估的随机屏保：保留随机路径感，同时避免主动钻进死胡同。

#### 修改内容

- 贪吃蛇路径从固定扫描路径改为实时随机游走：
  - 移除 `BuildLowPowerSnakePath()` 固定路径生成。
  - 新增蛇身数组、方向状态和轻量随机状态。
  - 每次动画 tick 根据候选方向推进蛇身，而不是沿预生成路径取点。
- 引入 flood-fill 安全评分：
  - 每个候选下一格会先计算后续可达空间。
  - 优先选择可达空间足够大的方向，避免主动走进死胡同。
  - 多个安全方向之间仍随机选择，保留屏保的随机路径感。
  - 没有安全方向时选择可达空间最大的方向，不再运行中随机重置。
- 放开待机文字区域：
  - 蛇可以穿过中心时间、日期同步区域和底部 `POWER 唤醒` 提示区域。
  - 移除原来的文字/底部提示保护区判断，仅保留圆形屏幕边界和蛇身自碰撞限制。
- 蛇身视觉优化：
  - 蛇身颜色从固定几档绿色改为头到尾的渐变色。
  - 蛇头更亮，身体向蓝绿色和更低透明度过渡。
- 动画速度调整：
  - 低功耗时钟刷新周期从 `1000 * 1000` 微秒调整为 `500 * 1000` 微秒。
  - 蛇移动速度约提升为原来的 2 倍。
- 时间显示视觉增强：
  - 增加中心时间发光层。
  - 抽出时间标签配置和刷新 helper，保证主时间和发光层同步刷新。
- 文档补充：
  - 新增随机贪吃蛇渐变屏保设计文档。
  - 新增对应实现计划文档。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `tests/xiaoxin_low_power_clock_visual_path_test.py`
- `docs/superpowers/specs/2026-06-28-xiaoxin-random-snake-gradient-design.zh-CN.md`
- `docs/superpowers/plans/2026-06-28-xiaoxin-random-snake-gradient.md`
- `docs/update.md`

#### 验证结果

- `pytest tests/xiaoxin_low_power_clock_visual_path_test.py -q`：通过，25 passed。
- `pytest tests -q`：通过，149 passed。
- 当前 shell 中未找到 `idf.py`，因此本轮未执行完整 ESP-IDF 固件 build / flash；仍需在 ESP-IDF 环境中实机确认随机路径连续性、穿过时间区域的观感、2 倍速节奏和低功耗显示效果。

## 2026-06-28 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 小芯宠物行为导演与动画自然化

#### 背景

实机对话中服务端 WebSocket 已返回 `{"type":"llm","emotion":"happy"}`，但桌宠常在回答结束后直接回到 `idle.gif`。排查确认 1.46 版本原先通过 `SetEmotion()` 进入 mood 建议，普通 mood 建议在 `SPEAKING / WAITING / THINKING / SLEEPING / FAILING` 等保护状态会被挡住，TTS stop 后又回到 idle，因此用户看不到服务端 emotion。另一方面，idle 长时间只播放固定动画，已有 GIF 资源没有被自然利用。

#### 修改内容

- 新增宠物行为导演模块：
  - 新增 `paopao_pet_behavior.h/.c`，在 display 事件和 `paopao_pet_trigger` 之间负责调度，不直接渲染 GIF。
  - 维护 `pending_service_trigger`、当前 voice state、idle 微动作计时和短期 idle 变体索引。
- 服务端 emotion 收尾播放：
  - `SetEmotion()` 继续更新 mood 分数，同时在同一个显示锁内交给 behavior director。
  - listening / thinking / speaking / sleeping / failing 期间收到的服务端 emotion 会缓存，不打断当前语音动画。
  - 回到 idle 后优先消费 pending emotion，例如回答中收到 `happy`，TTS stop 后播放 `happy.gif` 再回 idle。
  - 连续多个服务端 emotion 只保留最后一个；`neutral` / `none` / `giddy` 不进入 pending。
- idle 表现更丰富：
  - idle 轻量微动作由 behavior director 低频触发，第一版使用 `thinking`、`happy`、`tired` 等轻动作池。
  - 普通 idle 随机不会触发 `failed`、`crying`、`anxiety`、`stamp` 等强烈动画。
  - 语音状态变化、本地触摸、拖动、摇晃和服务端即时反应都会重排 idle 计时，避免小动作立刻覆盖刚发生的反应。
- 触发器职责收敛：
  - `paopao_pet_trigger_tick()` 不再固定在 idle 到点播放 `review`，只负责 reaction 过期和睡眠超时。
  - 本地 tap / hold / drag / shake 的即时反馈保持原路径，并同步刷新 behavior director 的交互时间。
- 1.46 显示接入：
  - `SetStatus()` / `SetChatMessage()` 在同一把显示锁中同时更新 trigger base state 和 behavior voice state，避免 render loop 在两次加锁间提前消费 pending emotion。
  - render loop 中先 tick behavior director，再 tick trigger。
  - behavior director 会从 trigger base state 同步 sleeping / failing 等保护态；busy/connecting 保持 idle-like，避免被下一帧 base-state 同步反复覆盖。
- 文档补充：
  - 新增行为导演设计说明和实施计划，记录边界、优先级、测试策略和验收标准。

#### 涉及文件

- `main/CMakeLists.txt`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_behavior.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_behavior.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c`
- `tests/paopao_pet_behavior_test.c`
- `tests/paopao_pet_trigger_test.c`
- `tests/xiaoxin_pet_mood_integration_path_test.py`
- `docs/superpowers/specs/2026-06-28-xiaoxin-pet-behavior-director-design.zh-CN.md`
- `docs/superpowers/plans/2026-06-28-xiaoxin-pet-behavior-director.md`
- `docs/update.md`

#### 验证结果

- `pytest -q tests/xiaoxin_pet_mood_integration_path_test.py`：通过，15 passed。
- MSYS2 bash 下运行 `paopao_pet_behavior_test`：通过，覆盖 pending emotion、sleeping/failing 缓存、idle 微动作冷却、长语音结束后延迟 idle 微动作、重复 idle 同步不饿死微动作。
- MSYS2 bash 下运行 `paopao_pet_trigger_test`：通过，确认 trigger 不再拥有固定 idle review 微动作。
- MSYS2 bash 下运行 `paopao_pet_emotion_test`：通过，emotion 到 trigger 映射保持正常。
- `. D:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1; idf.py build`：通过，生成 `build/ai_pet.bin`。
- 尚未执行实机 flash / smoke test；仍需在硬件上确认回答结束 emotion 收尾、idle 微动作频率和本地触摸/摇晃反馈观感。

## 2026-06-27 00:00:00 +08:00

### Speaking GIF 玩偶紫色边缘清理

#### 背景

实机/预览中发现 `speaking_fixed.gif` 玩偶外轮廓周围有一圈紫色杂边，黑底下尤其明显。排查确认该问题来自 GIF 源文件帧内的可见紫色/色键残留像素，而不是截图或显示层级造成的视觉误差。

#### 修改内容

- 清理 `main/assets/images/speaking_fixed.gif` 的 6 帧边缘残留：
  - 从原始 GIF 帧重新生成透明边缘，避免在已二次量化的 GIF 上重复转码。
  - 将外部连通的紫色、洋红色以及明显色键残留像素设为透明。
  - 保留玩偶主体、表情、动作帧、尺寸和帧数不变。
- 未手动修改 `build/` 下的生成产物：
  - 该板级配置通过 `main/assets/images/*.gif` glob 嵌入 GIF 资源。
  - 后续重新构建时，`build` 中的嵌入 `.S/.obj` 会由新的源 GIF 自动生成。

#### 涉及文件

- `main/assets/images/speaking_fixed.gif`
- `docs/update.md`

#### 验证结果

- 使用 Pillow 解码检查：GIF 尺寸保持 `192x208`，帧数保持 `6`。
- 清理前每帧可见紫色像素约 `516-569` 个；清理后每帧可见紫色像素为 `0`。
- 生成黑底 6 帧 contact sheet 进行肉眼检查：外圈紫色框已消失，仅保留玩偶自身的深色/青绿色轮廓。
- 本轮仅做素材像素修复，未执行完整 ESP-IDF 固件 build / flash；仍需在 ESP-IDF 环境中重新构建并实机确认 speaking 状态显示效果。

## 2026-06-27 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 启动动画与手动配网清凭据

#### 背景

本轮恢复并上传此前保存在 `stash@{0}` 的本地改动。该改动包含小芯启动动画资源和启动遮罩层，同时修正手动进入 Wi-Fi 配网时旧凭据仍保留的问题，避免用户主动重新配网后设备继续尝试旧网络。

#### 修改内容

- 新增启动动画资源：
  - 新增 `main/assets/images/boot.gif`。
  - 在 Waveshare 1.46 板级显示中声明 `_binary_boot_gif_start/end` 嵌入符号。
  - `SetupUI()` 完成主界面初始化后调用 `ShowBootSplashLocked()` 显示启动遮罩。
- 启动遮罩播放与清理：
  - 新增 `boot_splash_layer_`、`boot_splash_image_`、`boot_splash_controller_` 和 `boot_splash_timer_`。
  - 启动 GIF 使用白底 RGB565 路径播放，并在前景层显示。
  - 遮罩显示约 `2400ms` 后由 `esp_timer` 回调关闭，并恢复 overlay 层级。
  - `PaopaoPetDisplay` 析构时停止并删除启动遮罩 timer，释放 GIF 控制器。
- 手动 Wi-Fi 重配清理：
  - 新增 `ClearSavedWifiCredentialsForReconfiguration()`。
  - `WifiBoard::EnterWifiConfigMode()` 在停止 station 后、启动配网 AP 前调用 `SsidManager::GetInstance().Clear()`。
  - 静态入口和实例入口两条手动配网路径都覆盖该清理动作。
- 合并处理：
  - 恢复 stash 时保留已合入 `main` 的低功耗贪吃蛇屏保路径，不再带回旧的“main 不包含 snake”测试断言。

#### 涉及文件

- `main/assets/images/boot.gif`
- `main/boards/common/wifi_board.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `tests/wifi_config_status_path_test.py`
- `docs/update.md`

#### 验证结果

- `python -m pytest tests -q`：通过，129 passed。
- 当前 shell 中未找到 `idf.py`，因此本轮未执行完整 ESP-IDF 固件 build / flash；仍需在 ESP-IDF 环境中实机确认启动 GIF 显示时长、遮罩关闭层级、手动配网清凭据和后续重新配网流程。

## 2026-06-27 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 低功耗贪吃蛇屏保合并

#### 背景

本轮将 worktree 分支 `codex/xiaoxin-snake-screensaver` 合并到 `main`，把低功耗时钟页的黑底动态表盘扩展为带贪吃蛇像素背景的 AOD 屏保效果。合并时 `main` 已包含后续的演示控制和低功耗 UI 调整，因此对测试 guardrail 做了适配：保留当前 main 的 4 个低功耗前景文本标签结构，不重新引入分支中较早版本的电池标签。

#### 修改内容

- 低功耗时钟页新增贪吃蛇背景绘制：
  - 新增蛇身网格、圆形屏幕裁剪和底部提示安全区判断。
  - 用单个 `low_power_clock_snake_bg_` LVGL 绘制对象承载背景绘制，避免为每个格子创建独立对象。
  - 通过 `LV_EVENT_DRAW_MAIN` 和 `lv_draw_rect()` 绘制背景格子、蛇头和蛇身。
  - 低功耗刷新 tick 推进蛇身路径，并 invalidate 背景对象形成动画。
- LVGL opacity 兼容修正：
  - 新增 `LowPowerClockOpaPercent()`，将 55%、85%、95% 等透明度转换为 `lv_opa_t` 数值。
  - 避免使用当前 LVGL 版本未提供的 `LV_OPA_55`、`LV_OPA_85`、`LV_OPA_95` 常量。
- 测试 guardrail 加强：
  - 扩展 `tests/xiaoxin_low_power_clock_visual_path_test.py`，覆盖贪吃蛇背景只创建一个绘制对象、路径裁剪圆屏边界、避开底部提示区域和禁用 canvas/多对象格子方案。
  - 合并后将 label 数量断言适配为当前 main 的 4 个前景文本标签，并显式校验时间、日期、同步状态和 POWER 唤醒提示标签。
- 合并处理：
  - 解决 `esp32-s3-touch-lcd-1.46.cc` 中低功耗透明度 helper 和 LVGL opacity 常量的冲突。
  - 保留 `main` 当前低功耗时钟的视觉结构，同时合入 snake 分支的背景绘制和路径 guardrail。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `tests/xiaoxin_low_power_clock_visual_path_test.py`
- `.superpowers/sdd/final-review-fix-report.md`
- `docs/update.md`

#### 验证结果

- `python -m pytest tests/xiaoxin_low_power_clock_visual_path_test.py -q`：通过，21 passed。
- 当前 shell 中未找到 `idf.py`，因此本轮未执行完整 ESP-IDF 固件 build / flash；仍需在 ESP-IDF 环境中实机确认低功耗贪吃蛇背景、低功耗时间页层级和 POWER 唤醒行为。

## 2026-06-25 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 通知悬浮窗调试入口与排版修正

#### 背景

为方便在实机上快速复现通知中心和通知悬浮窗效果，本轮加入串口调试命令 `notify_test`。实机测试后发现测试通知悬浮窗中 `Notify test`、`Debug` 和 `Serial debug notification` 的排版过于拥挤，其中标题与标签区域容易视觉重叠，正文位置也需要和标题保持一致的左边界。

后续实机反馈进一步明确：`Debug` 标签本身的位置不需要调整，只需要把标题和正文排版修正为同一左对齐起点。

#### 修改内容

- 新增串口通知测试命令：
  - 在 Waveshare 1.46 板级代码中接入 ESP-IDF console REPL。
  - 注册 `notify_test` 命令，用于创建或更新一条测试通知。
  - 测试通知内容为标题 `Notify test`、正文 `Serial debug notification`、标签 `Debug`。
  - 命令执行后自动打开通知页，便于同时检查通知页列表和 heads-up 悬浮通知。
- 通知悬浮窗刷新行为修正：
  - 记录当前已渲染的 heads-up 通知快照。
  - 相同通知重复刷新时不再反复播放进入动画。
  - 当前通知结束或无可见通知时清空渲染快照并隐藏悬浮窗。
- 通知悬浮窗排版修正：
  - `Debug` 标签保持原来的左侧居中位置。
  - `Notify test` 标题左边界调整为 `x=78`。
  - `Serial debug notification` 正文左边界同步调整为 `x=78`，与标题左对齐。
  - 标题和正文宽度统一为 `154`，避免后续真实通知走 heads-up 组件时出现同类不齐问题。
- 文档补充：
  - 新增小芯串口调试命令说明文档，记录 `notify_test` 的使用方式、成功输出和 VSCode ESP-IDF Monitor 注意事项。
  - 补充串口通知测试实施计划，保留调试入口的实现约束和验证命令。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `tests/xiaoxin_notification_visual_path_test.py`
- `tests/xiaoxin_serial_debug_command_test.py`
- `docs/xiaoxin-serial-debug-commands.zh-CN.md`
- `docs/superpowers/plans/2026-06-24-xiaoxin-serial-notify-test.md`
- `docs/update.md`

#### 验证结果

- `python -m pytest tests/xiaoxin_notification_visual_path_test.py`：通过，23 passed。
- `python -m pytest tests/xiaoxin_serial_debug_command_test.py`：通过，1 passed。
- 当前 shell 中未执行完整 ESP-IDF 固件 build / flash，因此仍需在 ESP-IDF 环境中实机确认 `notify_test` 命令、通知页打开、heads-up 悬浮窗动画和标题/正文左对齐效果。

## 2026-06-24 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 底部字幕统一流式显示

#### 背景

实机反馈底部字幕观感不统一：部分对话字幕看起来是流式出现，但类似“无法连接服务，请稍后再试”的系统错误提示会一次性静态显示。根因是底部字幕原本在 `LcdDisplay::SetChatMessage()` 中直接 `lv_label_set_text()` 完整内容，而个别业务路径如果想要流式效果只能单独处理，容易漏掉 system/error 等来源。

本轮把流式显示能力下沉到通用底部字幕组件，统一所有落到底部字幕条的消息来源。

#### 修改内容

- 底部字幕统一流式：
  - 在 `LcdDisplay` 中新增 `chat_message_stream_timer_`、`chat_message_stream_text_` 和 `chat_message_stream_offset_`。
  - 非 wechat 底部字幕模式下，所有非空 `SetChatMessage(role, content)` 都调用 `StartChatMessageStreamLocked(content)`。
  - 字幕显示时先清空当前 label，再通过 LVGL timer 按 55ms 节奏逐步追加前缀文本。
  - `system`、`assistant`、`user`、错误提示、升级进度等来源共享同一套流式路径。
- UTF-8 安全：
  - 新增 `NextUtf8CharacterEnd()`，按 UTF-8 字符边界推进，避免中文提示被按字节拆开。
- 清理和取消：
  - 空字幕和 `ClearChatMessages()` 会调用 `StopChatMessageStreamLocked()`，停止当前流式 timer 并隐藏底部栏。
  - `LcdDisplay` 析构时删除 `chat_message_stream_timer_`，避免对象销毁后继续回调。
- 错误提示归一：
  - 保留 `Application` 的错误提示保持逻辑，但不在应用层做单独流式任务。
  - 错误提示仍通过 `Alert(...)->SetChatMessage("system", message)` 进入显示层，由底部字幕组件统一流式显示。
- 回归测试：
  - 新增 `tests/xiaoxin_bottom_subtitle_stream_test.py`，覆盖所有底部字幕角色都走统一流式 helper、空字幕取消流式、LVGL timer 和 UTF-8 边界。
  - 扩展 `tests/xiaoxin_error_display_path_test.py`，防止重新把错误提示流式逻辑放回应用层特例。

#### 涉及文件

- `main/display/lcd_display.cc`
- `main/display/lcd_display.h`
- `tests/xiaoxin_bottom_subtitle_stream_test.py`
- `tests/xiaoxin_error_display_path_test.py`
- `docs/xiaoxin-vertical-card-pager-plan.zh-CN.md`
- `docs/update.md`

#### 验证结果

- `python -m pytest tests`：通过，92 passed。
- `git diff --check -- main\display\lcd_display.cc main\display\lcd_display.h tests\xiaoxin_error_display_path_test.py tests\xiaoxin_bottom_subtitle_stream_test.py`：通过。
- 当前 shell 中未找到 `idf.py`，因此本轮未执行完整 ESP-IDF 固件构建；需要在 ESP-IDF 环境中补跑 `idf.py build` 并在实机确认所有底部字幕来源均为流式显示。

## 2026-06-24 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 动态低功耗时钟、SNTP 与错误提示保持

#### 背景

上一轮低功耗时钟已经能在省电时显示时间，但实机观感仍偏小、偏静态，更像一行调试信息；同时省电调度器原来的 shutdown 阶段会进一步触发关机，不符合当前希望常驻低功耗时钟的产品行为。本轮把低功耗页面改成更醒目的动态表盘：大号居中时间、黑底、青蓝外圈圆弧，并保持 POWER 短按唤醒。

另外，设备联网后的 SNTP 只有单个服务器，弱网或单点失败时校时可靠性不足；错误弹窗路径中，应用回到 idle 或音频通道关闭时可能把底部错误文案清空，导致用户看不到真实失败原因。本轮同步补齐这些稳定性问题。

#### 修改内容

- 低功耗时钟模型与构建接入：
  - 新增/扩展 `xiaoxin_low_power_clock_model` 的动画相位 helper。
  - 默认低功耗显示亮度从 `8%` 调整为 `12%`，提高黑色镜面和反光环境下可读性。
  - 新增 `XIAOXIN_LOW_POWER_CLOCK_ARC_SPAN_DEGREES = 76`，用于外圈圆弧跨度。
  - 将 `xiaoxin_low_power_clock_model.c` 加入 Waveshare 1.46 板级 `CMakeLists.txt` 源文件列表。
- 低功耗时钟 LVGL 页面重做：
  - 移除旧的“小图标在左、时间在右”布局和图标字体 fallback。
  - 新增 `low_power_clock_arc_`，用 LVGL arc 绘制外圈青蓝动态圆弧。
  - 时间改用 `font_puhui_basic_30_4` 并通过 transform 放大，居中略偏上显示。
  - 底部提示保留小号 `POWER 唤醒`，避免抢占中心时间。
  - 进入低功耗页时重置动画 tick，并立即刷新一次圆弧。
  - 专用低功耗刷新 timer 从 `10s` 改为 `1s`，圆弧每秒推进；时间文本仍只在分钟变化时刷新。
- 省电调度器行为调整：
  - `PowerSaveTimer(-1, 60, 300)` 改为 `PowerSaveTimer(-1, 60, -1)`。
  - 移除当前板级的 `OnShutdownRequest()` 关机回调，省电后停留在低功耗时钟页，不再自动关机。
- SNTP 校时增强：
  - 单服务器 `ntp.aliyun.com` 改为三服务器列表：`ntp.aliyun.com`、`cn.pool.ntp.org`、`pool.ntp.org`。
  - `sdkconfig` 和 `sdkconfig.defaults` 增加 `CONFIG_LWIP_SNTP_MAX_SERVERS=3`。
  - 增加 `OnSntpTimeSync()` 同步回调，校时成功后记录本地时间日志。
- 错误提示保持：
  - 新增 `Application::error_message_visible_` 状态位。
  - `MAIN_EVENT_ERROR` 路径在回到 idle 前先标记错误文案可见，避免 idle 刷新立刻清空底部错误消息。
  - 音频通道关闭时，如果错误文案正在显示，不再清空底部 system message。
  - 新会话进入 connecting 时清除旧错误可见状态，避免历史错误污染下一轮连接。
- 扩展 source-path 与模型测试：
  - 覆盖低功耗时钟模型动画相位、亮度默认值和圆弧常量。
  - 覆盖低功耗时钟 CMake 接入、大号居中时间、外圈 arc、1 秒刷新 cadence、移除旧图标布局。
  - 覆盖省电调度器不再触发自动关机。
  - 覆盖 SNTP 三服务器配置和同步回调。
  - 新增错误提示保持路径测试，防止 idle / 音频关闭路径清空可见错误文案。

#### 涉及文件

- `main/CMakeLists.txt`
- `main/application.cc`
- `main/application.h`
- `main/boards/common/wifi_board.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_low_power_clock_model.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_low_power_clock_model.h`
- `sdkconfig`
- `sdkconfig.defaults`
- `tests/wifi_config_status_path_test.py`
- `tests/xiaoxin_error_display_path_test.py`
- `tests/xiaoxin_low_power_clock_model_test.c`
- `tests/xiaoxin_low_power_clock_visual_path_test.py`
- `tests/xiaoxin_settings_path_test.py`
- `docs/superpowers/specs/2026-06-24-xiaoxin-dynamic-low-power-clock-design.zh-CN.md`
- `docs/superpowers/plans/2026-06-24-xiaoxin-dynamic-low-power-clock.md`
- `docs/update.md`

#### 验证结果

- `python -m pytest tests\xiaoxin_low_power_clock_visual_path_test.py tests\xiaoxin_settings_path_test.py tests\wifi_config_status_path_test.py tests\xiaoxin_error_display_path_test.py -q`：通过，54 passed。
- `gcc tests\xiaoxin_low_power_clock_model_test.c main\boards\waveshare\esp32-s3-touch-lcd-1.46\xiaoxin_low_power_clock_model.c -I main\boards\waveshare\esp32-s3-touch-lcd-1.46 -o build\xiaoxin_low_power_clock_model_test.exe; .\build\xiaoxin_low_power_clock_model_test.exe`：通过，退出码 0。
- 当前 shell 中未找到 `idf.py`，因此仍需在 ESP-IDF 环境中执行完整固件 build / flash，并在实机确认低功耗时钟大号时间、外圈动效、12% 亮度、POWER 唤醒、SNTP 日志和错误提示保持行为。

## 2026-06-23 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 省电设置反馈与主页电池省电色

#### 背景

实机反馈设置页中的 `省电` 项点击后没有明显反馈，用户无法判断是否已经按下，也看不到当前省电模式是否启用。进一步设计后，决定让设置列表本身承担状态展示：`省电` 行右侧直接显示 `已开启` / `已关闭` 状态胶囊；同时在主页右上角电池图标上给出轻量省电提示。

本轮不新增额外叶子、月亮等小图标，避免挤占 1.46 寸圆屏右上角空间。省电开启时电池图标改为琥珀黄，但低电/严重低电颜色优先，避免把低电警告语义覆盖掉。

#### 修改内容

- 设置页 `省电` 行增加可见状态：
  - 右侧从普通进入箭头改为状态文案 `已开启` / `已关闭`。
  - 状态文案使用胶囊样式，开启时使用偏琥珀色背景，关闭时使用深色背景。
  - 非省电设置行仍保留原来的 `›` 进入提示。
- 点击 `省电` 行直接切换状态：
  - 新增 `SettingsPowerSaveEnabled()`，读取 `Settings("wifi").GetBool("sleep_mode", true)`。
  - 新增 `ToggleSettingsPowerSaveLocked()`，写入 `Settings("wifi").SetBool("sleep_mode", enabled)`。
  - 切换后调用 `PowerSaveTimer::SetEnabled(enabled)`，让运行时省电调度器同步启停。
  - 切换完成后回到设置列表，只通过右侧状态胶囊变化反馈，不再显示底部提示，避免和 `退出设置` 重叠。
- 主页右上角电池图标增加省电颜色反馈：
  - 新增 `k_battery_meter_power_save = 0xffb84d` 作为省电琥珀黄。
  - 新增 `xiaoxin_settings_power_save_battery_color()`，统一处理颜色优先级。
  - 颜色优先级为：低电/严重低电 > 省电开启 > 普通电量状态。
  - 不新增任何省电小图标。
- 扩展设置模型纯函数：
  - 新增 `xiaoxin_settings_power_save_value_label()`，负责返回 `已开启` / `已关闭`。
  - 新增 `xiaoxin_settings_power_save_battery_color()`，负责省电色和低电色优先级选择。
- 在本变更日志中记录省电设置反馈交互、主页电池省电色、低电优先级和测试策略。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.h`
- `tests/xiaoxin_settings_model_test.c`
- `tests/xiaoxin_settings_path_test.py`
- `docs/update.md`

#### 验证结果

- 先按 TDD 增加 failing tests：
  - `tests/xiaoxin_settings_model_test.c` 因缺少省电状态文案和电池颜色 helper 失败。
  - `tests/xiaoxin_settings_path_test.py` 因缺少省电切换路径和琥珀色电池路径失败。
- 实现后 `pytest tests\xiaoxin_settings_path_test.py -q`：通过，33 passed。
- 实现后 `gcc -std=c11 -Wall -Wextra -I. tests\xiaoxin_settings_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c -o $env:TEMP\xiaoxin_settings_model_test.exe`：通过。
- `& $env:TEMP\xiaoxin_settings_model_test.exe`：通过，输出 `xiaoxin_settings_model tests passed`。
- `git diff --check`：通过，仅提示当前工作副本中文件下一次由 Git 触碰时会进行 LF/CRLF 行尾转换。
- 当前 shell 中未执行完整 ESP-IDF 固件 build / flash，因此仍需在 ESP-IDF 环境中实机验证设置页胶囊显示、点击切换、省电调度器启停和主页电池琥珀色反馈。

### Waveshare ESP32-S3 Touch LCD 1.46 设置页交互、亮度滑条与配网状态完善

#### 背景

实机反馈设置首页底部的 `退出设置`、亮度页中的 `返回` 等小按钮在 1.46 寸圆屏上不够好点，亮度页的 `30 / 70 / 100` 预设也不够细；同时关于页仍暴露开发板型号，Wi-Fi 配网状态栏图标容易看起来像“已连接”。

本轮把设置页交互继续收束在现有 `PollTouch()` 读取路径里，保留卡片分页和桌宠触摸隔离，同时为小按钮补足独立控件、扩展命中区和动态亮度拖动能力。设置页文案和关于页也同步改为面向用户的小芯产品信息。

#### 修改内容

- 设置页布局和小按钮命中优化：
  - 设置面板高度从 `250` 调整到 `288`，列表行高度和间距收紧，给底部 `退出设置` 留出独立区域。
  - `退出设置` 从普通设置行中拆出为独立 `settings_back_row_`，避免和第四行设置项混在一起。
  - 新增 `PointInObjWithSlop()`，为 `退出设置` 和亮度页 `返回` 增加横向/纵向命中扩展。
  - 新增 `settings_touch_action_consumed_`，避免一次按压在同一帧内重复触发关闭、返回或拖动动作。
- 亮度设置从固定预设改为动态滑条：
  - 新增 `settings_brightness_value_label_`、轨道、填充、滑块、`低/高` 标签和独立 `返回` 按钮。
  - 新增 `xiaoxin_settings_brightness_from_x()`，把触摸横坐标映射到安全亮度区间 `10~100`，避免拖到 0 导致屏幕近似黑屏。
  - 拖动时调用 `Backlight::SetBrightness(value, false)` 做即时预览，松手后调用 `Backlight::SetBrightness(value, true)` 持久化。
  - 增加亮度写入去重，避免拖动过程中同一值重复刷日志和写入链路。
- 设置页进入和产品文案调整：
  - BOOT 长按打开设置页不再限制 idle 状态，连接、聆听、说话、升级等运行态也可进入设置。
  - 修复设置模型中中文标题的 UTF-8 文案，覆盖 `亮度`、`省电`、`关于`、`音量`、`静音`、`提示音`、`震动` 等标题。
  - 关于页从开发板型号改为小芯产品信息，展示 `小芯 D151`、`桌面助手` 和固件版本，并上移正文以适配圆屏底部。
- Wi-Fi 配网状态显示修正：
  - 进入 `kDeviceStateWifiConfiguring` 时立即设置状态文案为 `WIFI_CONFIG_MODE` 并刷新状态栏。
  - `WifiBoard::GetNetworkStateIcon()` 在配网模式下显示 `FONT_AWESOME_WIFI_SLASH`，避免状态栏看起来像已经联网。
- 保留现有设置页触摸边界：
  - 设置页打开时仍先由 `PollTouch()` 读取触摸点，再把事件交给 `HandleSettingsTouch()`。
  - 设置页触摸继续压制卡片分页手势，避免设置 overlay 内拖动亮度时触发通知/总览抽屉。
- 扩展 source-path 测试：
  - 覆盖设置页中文文案、独立退出按钮、命中扩展、按钮首帧可触发和触摸消费状态。
  - 覆盖亮度动态滑条、坐标映射、预览/持久化分离、返回按钮和重复写入去重。
  - 覆盖关于页产品身份、圆屏底部适配、BOOT 长按任意运行态进入设置。
  - 新增 Wi-Fi 配网状态栏和网络图标 source-path 守卫。

#### 涉及文件

- `main/application.cc`
- `main/boards/common/wifi_board.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.h`
- `tests/xiaoxin_settings_model_test.c`
- `tests/xiaoxin_settings_path_test.py`
- `tests/wifi_config_status_path_test.py`
- `docs/update.md`

#### 验证结果

- `python -m pytest tests/xiaoxin_settings_path_test.py tests/wifi_config_status_path_test.py -q`：通过，33 passed。
- `gcc -std=c11 -Wall -Wextra -Werror tests/xiaoxin_settings_model_test.c main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c -o build/xiaoxin_settings_model_test.exe`：通过。
- `.\build\xiaoxin_settings_model_test.exe`：通过，输出 `xiaoxin_settings_model tests passed`。
- `git diff --check`：通过，仅提示当前工作副本中文件下一次由 Git 触碰时会进行 LF/CRLF 行尾转换。
- 当前 shell 中未执行完整 ESP-IDF 固件 build / flash，因此仍需在 ESP-IDF 环境中实机验证 `退出设置`、亮度滑条拖动/持久化、关于页显示和 Wi-Fi 配网状态栏。

### Waveshare ESP32-S3 Touch LCD 1.46 BOOT 长按设置页硬件回归兜底

#### 背景

远程分支已实现 BOOT 长按进入设置页，并补充了按键事件日志；实际硬件重新 build / 烧录后，长按 BOOT 仍无界面反馈，串口中也没有看到 `BOOT press down`、`BOOT long press` 等日志。该现象说明问题发生在更靠前的边界：GPIO0/BOOT 没有稳定进入 `iot_button` 事件链，或者 button 库没有产出 press/long-press 回调。

#### 修改内容

- BOOT/PWR 输入初始化显式启用内部上拉：
  - `BOOT_BUTTON_GPIO` 使用 `GPIO_PULLUP_ONLY`，稳定未按下时的高电平。
  - `PWR_BUTTON_GPIO` 同步启用上拉，避免同类输入漂浮。
- 为 BOOT 增加 GPIO 轮询兜底：
  - 使用 `esp_timer` 每 50ms 读取 `gpio_get_level(BOOT_BUTTON_GPIO)`。
  - GPIO0 低电平持续 2 秒时调用同一个 `HandleBootLongPress()`。
  - 复用 `boot_long_press_handled_`，避免 `iot_button` 和轮询兜底双触发。
- 增加硬件排查日志：
  - `BOOT polling fallback started: gpio=%d level=%d`
  - `BOOT poll press down`
  - `BOOT poll long press fallback`
  - `BOOT poll press up`
- 扩展 source-path 测试：
  - 覆盖 BOOT 上拉配置。
  - 覆盖轮询 timer、GPIO0 低电平判断、2 秒兜底路径和防双触发状态位。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `tests/xiaoxin_settings_path_test.py`
- `docs/update.md`

#### 验证结果

- 先新增 failing source-path test：缺少 `PollBootButtonFallback()` 时失败。
- 实现后 `python -m pytest tests/xiaoxin_settings_path_test.py -q`：通过，15 passed。
- 当前 shell 中未找到 `idf.py`，因此仍需在 ESP-IDF 环境中执行完整固件 build / flash 做硬件验证。

## 2026-06-22 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 BOOT 长按设置页

#### 背景

前一轮已经把 BOOT 按键从宠物动画触发职责中释放出来，后续需要把它作为设备系统入口使用。本轮将 BOOT 长按落地为小芯设置页入口，优先覆盖亮度、Wi-Fi 重新配网、关于设备和受调度器能力保护的省电设置。

设置页必须满足几个边界：只在设备空闲态打开，不打断连接、聆听、说话、配网、升级、激活、音频测试或错误状态；不能把设置页做成第四个卡片分页；目标板默认不暴露音量、静音、提示音和震动设置；省电项只有在目标板真的接入 `PowerSaveTimer` / `SleepTimer` 等调度器后才显示。

#### 修改内容

- 新增纯 C 设置模型：
  - 新增 `xiaoxin_settings_model.h/.c`。
  - 提供设置项枚举、目标能力描述、运行态开关门禁、亮度百分比 clamp 和短标题映射。
  - 默认目标能力只显示亮度、Wi-Fi、关于；音频、震动和省电项按能力开关显示。
- 新增设置模型测试：
  - 覆盖默认项过滤、音频/震动/省电能力门禁、输出容量截断、仅 idle 可打开设置和亮度百分比 clamp。
- 新增设置页 source-path 守卫：
  - 覆盖 BOOT 长按入口、BOOT 短按关闭设置优先级、idle-only 门禁、overlay 而不是第四分页、设置触摸阻断卡片分页手势、亮度 API、Wi-Fi 入口复用、关于页元信息、音频/震动隐藏、省电调度器真实性。
  - 后续 review 中补充了对注释欺骗 path test 的防护：关键断言会去掉 C/C++ 注释后再检查。
  - 新增 Wi-Fi 请求和触摸唤醒的延迟执行守卫，避免在 `DisplayLockGuard` 内同步触发可能重入显示锁的逻辑。
- 在 `PaopaoPetDisplay` 中新增 LVGL 设置 overlay：
  - 设置页挂在当前屏幕 overlay 层，不新增 `xiaoxin_card_page_t` 页面。
  - 长按 BOOT 时从 idle 进入设置列表。
  - 设置打开时，触摸事件先交给设置页处理，不再传给卡片分页拖拽。
  - 设置打开时，BOOT 短按先关闭设置页。
- BOOT 按键行为调整：
  - BOOT 长按调用 `OpenSettingsOverlayFromBootButton()`。
  - 只有 `kDeviceStateIdle` 可打开设置。
  - 连接、聆听和说话状态下长按会提示先结束对话。
  - BOOT 短按在设置关闭时仍保留启动阶段进入 Wi-Fi 配网、其他状态切换聊天的既有语义。
- 新增亮度设置动作：
  - 通过 `xiaoxin_settings_clamp_percent()` 保护输入。
  - 使用 `Board::GetInstance().GetBacklight()->SetBrightness(clamped, true)`，继续走已有显示亮度持久化路径。
  - 当前亮度页先应用中间安全预设，后续可继续做 30 / 70 / 100 的精确触摸命中区域。
- 新增 Wi-Fi 重新配网动作：
  - 通过 `CustomBoard::RequestSettingsWifiConfig()` 复用既有 `EnterWifiConfigMode()`。
  - 最终实现中 Wi-Fi 设置页只设置延迟请求标记，`RunRenderLoop()` 在释放显示锁后再调用板级 Wi-Fi 配网入口，避免显示锁重入。
- 新增关于页：
  - 使用 `esp_app_get_description()` 展示固件版本、项目名、构建日期时间和目标板名称。
- 新增省电调度器接入：
  - 目标板新增 `PowerSaveTimer(-1, 60, 300)`。
  - 进入 sleep 时调用 `Display::SetPowerSaveMode(true)`。
  - 退出 sleep 时调用 `Display::SetPowerSaveMode(false)`。
  - shutdown 请求走现有电源关闭路径。
  - 设置页是否显示省电项由真实 `PowerSaveTimer*` 是否存在决定。
- 补齐本地活动唤醒：
  - BOOT 单击、BOOT 长按、PWR 单击、PWR 长按都会唤醒/重置 `PowerSaveTimer`。
  - 触摸活动只在显示锁内记录唤醒请求，真正 `WakeUp()` 延迟到显示锁释放后执行，避免 `PowerSaveTimer::WakeUp()` 同步触发显示回调造成锁重入。
#### 涉及文件

- `main/CMakeLists.txt`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_settings_model.c`
- `tests/xiaoxin_settings_model_test.c`
- `tests/xiaoxin_settings_path_test.py`
- `docs/update.md`

#### 验证结果

- `xiaoxin_settings_model_test`：通过，输出 `xiaoxin_settings_model tests passed`。
- `python -m pytest tests/xiaoxin_settings_path_test.py -q`：通过，13 passed。
- `python -m pytest tests/xiaoxin_notification_visual_path_test.py tests/xiaoxin_pet_mood_integration_path_test.py -q`：通过，27 passed。
- `xiaoxin_card_pager_test`：通过。
- `xiaoxin_system_overlay_test`：通过。
- 最终代码审查通过：Wi-Fi 配网和触摸唤醒都已改为显示锁外延迟执行，未发现 Critical / Important 问题。
- 当前 shell 中未找到 `idf.py`，因此本轮未执行完整 ESP-IDF 固件构建。
- `tests/wifi_config_status_path_test.py` 在本 worktree 中不存在；它只存在于 main checkout 的未跟踪用户文件中，本轮按要求未复制、未编辑。

## 2026-06-21 21:22:40 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 BOOT 按键宠物动画触发释放

#### 背景

P1 宠物情绪系统接入后，BOOT 按键需要从宠物情绪和宠物动画触发路径中闲置出来，避免它继续承担屏幕触摸之外的宠物交互职责。该按键后续可保留给系统、调试、设置入口或产品层面的独立决策。

#### 修改内容

- BOOT 单击不再触发宠物本地点击动画：
  - 不再派发 `PAOPAO_PET_TRIGGER_LOCAL_TAP`。
  - 不再播放 `done.gif` 作为 BOOT 单击反馈。
- BOOT 长按不再触发宠物本地长按动画：
  - 不再派发 `PAOPAO_PET_TRIGGER_LOCAL_HOLD`。
  - 不再通过 BOOT 长按切换 `sleeping.gif`。
- BOOT 单击保留既有系统行为：
  - 启动阶段仍可进入 WiFi 配置。
  - 其他状态仍可切换聊天状态。
- BOOT 长按当前预留给未来系统、设置或调试入口。
- 宠物本地即时反馈继续由屏幕触摸、拖动和 IMU 摇晃触发。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `tests/xiaoxin_pet_mood_integration_path_test.py`
- `docs/xiaoxin-pet-emotion-gif-mapping.zh-CN.md`
- `docs/update.md`

#### 验证结果

- `tests/xiaoxin_pet_mood_integration_path_test.py` 已覆盖 BOOT 区段不包含 `DispatchPetTrigger`、`PAOPAO_PET_TRIGGER_LOCAL_TAP` 和 `PAOPAO_PET_TRIGGER_LOCAL_HOLD`。
- 当前实现中 BOOT 不作为 `paopao_pet_mood` 输入，也不直接触发宠物 GIF 切换。
- `python -m pytest tests/xiaoxin_pet_mood_integration_path_test.py tests/xiaoxin_notification_visual_path_test.py -q`：通过，21 passed。
- `paopao_pet_mood_test`、`paopao_pet_trigger_test`、`paopao_pet_emotion_test`、`paopao_pet_gif_assets_test`：均已通过 host C 测试。
- 当前 shell 未找到 `idf.py`，因此本轮未执行完整 ESP-IDF 固件构建。

## 2026-06-19 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 宠物核心情绪 GIF 映射

#### 背景

当前桌宠已有多组 GIF 动画资源，但服务端 `llm.emotion` 到 GIF 的映射仍偏粗糙：开心复用完成反馈，伤心、生气、哭泣等负向情绪容易落到 `failed.gif`，导致“真实错误失败”和“宠物情绪表达”混在一起。

本轮先按“少量核心情绪稳定运行”的原则接入核心情绪，不追求所有 emotion 字符串都拥有独立动画。同时考虑当前开发板 512 KB SRAM、384 KB ROM、8 MB PSRAM、16 MB Flash 的资源约束，继续保持同一时间只播放一个宠物 GIF，不叠加额外特效。

#### 修改内容

- 新增服务端情绪归一模块：
  - 新增 `paopao_pet_emotion.h/.c`。
  - 将 `SetEmotion()` 中的 emotion 字符串判断迁出显示类，统一由 `paopao_pet_trigger_for_emotion()` 处理。
  - emotion 匹配大小写不敏感，按子串匹配。
- 新增核心情绪状态：
  - `PAOPAO_PET_STATE_HAPPY` -> `happy.gif`
  - `PAOPAO_PET_STATE_CRYING` -> `crying.gif`
  - `PAOPAO_PET_STATE_ANXIETY` -> `anxiety.gif`
  - `PAOPAO_PET_STATE_TIRED` -> `tired.gif`
  - `PAOPAO_PET_STATE_STAMP` -> `stamp.gif`
- 调整服务端 emotion 映射：
  - `happy`, `laugh`, `loving`, `excited`, `cool` -> `happy.gif`
  - `cry`, `sad`, `unhappy`, `upset`, `lonely` -> `crying.gif`
  - `angry`, `annoyed`, `frustrated`, `mad`, `impatient` -> `stamp.gif`
  - `anxious`, `worried`, `nervous`, `scared`, `afraid` -> `anxiety.gif`
  - `tired`, `weak`, `low_battery` -> `tired.gif`
  - `sleep`, `sleepy`, `sleeping` -> `sleeping.gif`
  - `think`, `confused`, `curious` -> `thinking.gif`
  - `error`, `fail`, `shock` -> `failed.gif`
  - `neutral`, `calm`, `relaxed`, `microchip` 不打断当前状态。
- 明确 `stamp.gif` 表达“跺脚、生气、不满、抗议”，用于 angry 类 emotion。
- 保留 `failed.gif` 给真实错误、失败、识别失败，不再泛化表示伤心或生气。
- 修复 `unhappy` 被 `happy` 子串误判的问题：负向哭泣类关键词优先于开心类关键词匹配。
- 接入新 GIF 资源：
  - 新增 `main/assets/images/crying.gif`。
  - `happy.gif`、`anxiety.gif`、`tired.gif`、`stamp.gif` 从已有资源接入状态机。
- 更新显示层二进制资源映射和视觉尺寸表：
  - 新增 `happy`、`crying`、`anxiety`、`tired`、`stamp` 的嵌入符号映射。
  - 所有已接入状态机 GIF 按前景最长边统一缩放到 `162px`。
  - 复测后将 `anxiety.gif` 的前景最长边从 `172` 校准为实测 `173`。
  - 未接入的 `waving.gif` 也已测量，后续接入时建议使用前景最长边 `138`。
- 新增正式说明文档 `docs/xiaoxin-pet-emotion-gif-mapping.zh-CN.md`：
  - 记录不同 GIF 的情绪含义。
  - 记录服务端 `emotion -> trigger -> GIF` 映射。
  - 记录每个 GIF 当前真实触发方式。
  - 记录资源约束、状态优先级、动画持续时间和后续新增 GIF 的步骤。

#### 当前 GIF 触发规则

| GIF | 当前触发方式 |
| --- | --- |
| `idle.gif` | 初始状态；`SetStatus(standby)`；短反应结束后回到 idle |
| `waiting.gif` | `SetStatus(listening)`；`PAOPAO_PET_TRIGGER_WAKE` |
| `thinking.gif` | `SetStatus(thinking)`；`SetChatMessage(role=user)`；服务端 emotion 包含 `think`, `confused`, `curious` |
| `speaking_fixed.gif` | `SetStatus(speaking)`；`SetChatMessage(role=assistant)` |
| `done.gif` | 屏幕短点按；Boot 按键单击；`PAOPAO_PET_TRIGGER_TASK_DONE` |
| `sleeping.gif` | idle 安静约 60 秒；屏幕或 Boot 按键长按；服务端 emotion 包含 `sleep`, `sleepy`, `sleeping` |
| `jumping.gif` | 屏幕横向拖动 |
| `failed.gif` | `SetStatus(error)`；服务端 emotion 包含 `error`, `fail`, `shock` |
| `giddy.gif` | IMU 检测到设备摇晃；服务端 `giddy` 当前不会触发 |
| `review.gif` | idle 一段时间后的空闲小动作 |
| `happy.gif` | 服务端开心类 emotion |
| `crying.gif` | 服务端伤心/哭泣类 emotion |
| `anxiety.gif` | 服务端焦虑类 emotion |
| `tired.gif` | 服务端疲惫/低电量类 emotion |
| `stamp.gif` | 服务端生气/不满类 emotion |
| `working.gif` | 状态和资源已存在，但当前状态机没有实际入口触发 |
| `waving.gif` | 资源已存在，但当前尚未接入状态机 |

#### 涉及文件

- `main/assets/images/crying.gif`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_emotion.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_emotion.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_gif_assets.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_state.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_state.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_trigger.c`
- `tests/paopao_pet_emotion_test.c`
- `tests/paopao_pet_trigger_test.c`
- `tests/paopao_pet_gif_assets_test.c`
- `tests/paopao_gif_probe_decode_test.c`
- `docs/xiaoxin-pet-emotion-gif-mapping.zh-CN.md`
- `docs/update.md`

#### 验证结果

- 已用 Python/Pillow 遍历 GIF 帧透明前景包围盒，确认所有已接入状态机的 GIF 缩放后前景最长边均为 `162px`。
- 当前 `main/assets/images/*.gif` 共 17 个，总大小约 `1,541,074 bytes`。
- `git diff --check`：通过，仅提示既有 CRLF/LF 行尾转换 warning。
- 当前 shell 未找到 `idf.py` 和 `ninja`，无法执行完整 ESP-IDF 构建。
- 当前本机 `gcc` 连最小 C 程序也返回退出码 `1`，因此本轮未能运行 host C 测试二进制。

### Waveshare ESP32-S3 Touch LCD 1.46 真实总览数据路径与电量状态文案

#### 背景

总览页前一版已经把四张卡片做成了更高信息密度的样式，但内容仍来自硬编码示例。用户希望保留当前 UI 形态，让真实天气、课程、待办和设备状态能够接到同一组卡片上显示；在真实数据尚未接入、未联网或未配置时，要显示诚实的空状态，而不是继续显示示例内容。

同时，设备电量来自板载 ADC 估算。精确百分比容易让用户误以为读数很准，因此总览页设备状态不再显示百分比，只显示粗粒度状态文案。

#### 修改内容

- 新增独立总览数据模型 `xiaoxin_overview_model`：
  - 输入 `xiaoxin_overview_state_t`，包含时间、网络、电量、天气、课程和待办摘要字段。
  - 输出 `xiaoxin_overview_snapshot_t`，即一次总览页渲染所需的时间/日期文本和四张卡片条目。
  - LVGL 渲染层只消费快照，不再直接拼接天气、课程、待办业务文案。
- 总览页顶部新增时间日期区域：
  - 时间有效时显示 `HH:MM` 和 `M月D日 周X`。
  - 时间未同步时显示 `--:--` / `时间未同步`。
- 移除 `xiaoxin_card_pager.c` 中的 Overview 静态示例数组：
  - `xiaoxin_card_pager_items(XIAOXIN_CARD_PAGE_OVERVIEW, ...)` 不再返回假天气、假课程、假待办。
  - Notifications 分页的数据仓库、清理、空状态、手势和指示器不属于本轮修改范围。
- 天气、课程、待办当前使用真实接入前的降级状态：
  - 天气未联网时显示 `天气未同步` / `连接网络后更新`，已联网但未配置位置时显示 `未配置位置`。
  - 课程未配置时显示 `暂无课程` / `在配置中添加课表`，有课表但当天无课时显示 `今日无课`。
  - 待办未配置或为空时显示 `暂无待办` / `添加提醒后显示`。
- 今日待办的后续真实功能边界：
  - 服务端作为待办事实来源，负责创建、编辑、完成、过期和筛选今日待办。
  - 硬件端第一阶段只同步只读摘要，例如 `todo_configured`、`todo_count`、`todo_detail`，以后可增加最近同步时间。
  - 离线时优先显示已缓存摘要；没有缓存或从未配置时显示明确空状态。
- 设备状态卡片改为定性电量文案：
  - `>= 60` 显示 `电量充足`。
  - `30-59` 显示 `电量正常`。
  - `15-29` 显示 `电量偏低`。
  - `< 15` 显示 `请尽快充电`。
  - 电量不可读取时显示 `电量未知`。
- `UpdateStatusBar()` 在总览页可见时会刷新当前总览快照，避免时间、网络和电量状态停留在旧值。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_overview_model.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`
- `main/CMakeLists.txt`
- `tests/xiaoxin_overview_model_test.c`
- `tests/xiaoxin_card_pager_test.c`
- `tests/xiaoxin_notification_visual_path_test.py`
- `tests/xiaoxin_card_pager_threshold_test.py`
- `docs/update.md`

#### 验证结果

- `xiaoxin_overview_model_test`：通过，覆盖离线默认状态、联网未配置、真实注入数据、未知电量和电量分档。
- `xiaoxin_card_pager_test`：通过，确认 Overview 数据已从 pager 静态数组迁出，通知分页行为保持可用。
- `python -m pytest tests/xiaoxin_card_pager_threshold_test.py tests/xiaoxin_notification_visual_path_test.py -q`：通过，15 passed。
- 当前 shell 中未找到 `idf.py` / `cmake`，因此本轮未在本机完成完整 ESP-IDF 固件构建。

### Waveshare ESP32-S3 Touch LCD 1.46 真实通知中心、空状态视觉与返回手势阈值优化

#### 背景

本轮通知页不再只是静态 UI 样板，而是接入了真实通知事件仓库。通知页初始为空，只有低电量、WiFi、OTA、语音失败、课程提醒等事件进入后才生成通知卡。

实机反馈通知页为空时视觉重心不够明确，分页层完全透明时空状态文字容易和 Home 背景混在一起。同时，通知页或总览页已经打开后，反向滑动回 Home 仍沿用 Home 进入分页的 20% 高阈值，轻扫收回不够顺手。

#### 修改内容

- 落地真实通知中心：
  - 新增通知事件模型 `xiaoxin_notification_event_t`，支持低电量、WiFi 断开、OTA 更新、语音识别失败、聊天回复兼容值和课程提醒。
  - 通知页启动时不再填充静态假通知，`notification_count` 初始为 `0`。
  - 新增 `xiaoxin_card_pager_notification_upsert_event()`，同类状态通知按类型更新，避免重复堆积。
  - 新增 `xiaoxin_card_pager_notification_remove_event()`，用于外部状态恢复后移除对应通知。
  - 新增 `xiaoxin_card_pager_notification_clear_all()` 和单条 dismiss，支持用户清理当前通知仓库。
  - 新增课程提醒 helper，在提醒窗口内生成“上课提醒”通知，并按课程事件更新。
  - 通知按优先级排序：课程提醒、低电量、WiFi、OTA、语音失败依次展示。
  - 聊天回复事件保留兼容枚举，但不再渲染成通知卡，避免聊天内容刷屏通知中心。
  - 低电量状态通过显示层同步为通知事件，恢复正常后可移除。
- 调整分页层背景：
  - `card_layer_` 不再完全透明，改为使用 `0xe9edf3` 的低透明度浅色背景。
  - 背景透明度为 `18`，保留 Home 静止画面透出，同时让空状态和页面控件更稳定可读。
- 调整页面标题与通知页标题行为：
  - 页面标题颜色改为近黑色 `0x111111`。
  - 通知页不再显示顶部“通知”标题，减少和清理按钮、通知卡组之间的视觉拥挤。
  - 总览页继续显示“总览”，并保持居中对齐。
- 优化通知空状态：
  - 新增 `notification_empty_panel_`，将“暂无通知”放入半透明白色圆角面板。
  - 空通知时隐藏“全部清理”按钮，显示空状态面板。
  - 有通知时隐藏空状态面板，恢复“全部清理”入口。
- 优化打开页返回 Home 的释放阈值：
  - 新增 `release_threshold_px()`。
  - 当目标页为 Home 且当前页不是 Home 时，释放阈值从通用 20% 改为屏幕高度的 8%。
  - Home 进入通知页/总览页仍沿用原有阈值，避免误触进入分页。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`
- `tests/xiaoxin_card_pager_test.c`
- `tests/xiaoxin_card_pager_threshold_test.py`
- `tests/xiaoxin_notification_visual_path_test.py`
- `docs/update.md`

#### 验证结果

- `python -m pytest tests\xiaoxin_notification_visual_path_test.py tests\xiaoxin_card_pager_threshold_test.py`：通过，12 passed。
- `git diff --check`：通过，仅提示既有 CRLF/LF 行尾转换 warning。
- `xiaoxin_card_pager_test`：已新增真实通知仓库、事件 upsert、状态移除、课程提醒、优先级排序、聊天回复忽略、单条 dismiss、全部清理和打开页短反向滑动返回 Home 用例；当前本机 `gcc` 连最小 C 程序也返回退出码 1 且无诊断输出，因此本轮未完成 C 测试二进制编译运行。

### Waveshare ESP32-S3 Touch LCD 1.46 总览卡片信息密度优化

#### 背景

实机总览页的四张卡片布局已经可用，但每张卡片只有标题和一行正文，信息量偏少。用户希望暂时保留当前总览页形态，只让单张卡片承载更完整的信息。

#### 修改内容

- 保留总览页四张卡片、图标、右侧箭头和整体分页交互。
- `xiaoxin_card_item_t` 新增 `detail` 字段，用于总览卡片的辅助信息。
- 通知卡片复用同一个 `xiaoxin_card_item_t` 结构，但在通知仓库绑定时将 `detail` 显式置为 `NULL`，因此通知页仍保持原来的标题/正文渲染，不会多出第三行。
- 总览静态数据从“标题 + 单行正文”扩展为“标题 + 主信息 + 辅助信息”：
  - 下一节课：`高数 10:10` / `教2-301 · 还有24分`。
  - 校园导航：`常用地点` / `教学楼 / 食堂 / 图书馆`。
  - 天气：`多云 26C` / `湿度72% · 东风2级`。
  - 今日待办：`2 项待办` / `实验报告 · 晚自习`。
- 总览行渲染新增第三行 `detail` 标签，正文行使用强调色，辅助信息使用弱化色，三行都使用 `LV_LABEL_LONG_MODE_DOTS` 防止长文本撑破卡片。
- 删除旧的 `OverviewCompactBodyText()` 硬编码压缩文案，让总览页直接使用数据源里的 `body` 和 `detail`。

#### 最终布局参数

- 卡片宽度：`252` -> `270`。
- 卡片高度：`48` -> `58`。
- 卡片间距：`51` -> `61`。
- 起始位置：`k_overview_y_start = 70`，让第一张卡片避开顶部标题和电量区域。
- 文本区域：宽度 `176`，高度 `54`。
- 三行文字位置：标题 `y=0`，主信息 `y=17`，辅助信息 `y=34`。
- 右侧箭头位置：`k_overview_arrow_x = 244`，配合放大后的卡片宽度继续留在安全区内。

#### 实现边界

- 未改变 Home / Notifications / Overview 的分页状态机。
- 未改变通知中心的排序、清理、空状态和左滑逻辑。
- 未接入真实课程、天气或待办后端，本轮只把现有总览样例数据改成更完整的展示内容。
- 未新增小屏深层操作入口，当前总览页仍是只读摘要页。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`
- `tests/xiaoxin_card_pager_test.c`
- `docs/update.md`

#### 验证结果

- `git diff --check`：通过，仅提示既有 CRLF/LF 行尾转换 warning。
- `ninja -C build esp-idf/main/CMakeFiles/__idf_main.dir/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc.obj`：通过，仅保留既有 `esp_lcd_touch_get_coordinates` deprecated warning。
- `xiaoxin_card_pager_test` 已新增总览 detail 数据契约用例；当前本机 `gcc` 的 `cc1.exe` 运行时失败，未能产出测试二进制。
- `ninja -C build`：通过，生成 `build/ai_pet.bin`。

## 2026-06-18 00:00:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 全局 WiFi / 电量安全区浮层

#### 背景

实机反馈原本设计右上角有电量显示，但通知卡片不拖动时看不到。排查后确认当前实现已经不再需要通知卡片内部四格电量，电量应作为全局浮层显示；问题在于全局电量位置过于贴近圆屏右上角，容易落入圆形屏幕不可见区域。同时，分页页会隐藏原始顶部状态栏，导致原 `network_label_` WiFi 图标在卡片页不可见。

#### 修改内容

- 保留全局电量显示，不恢复通知卡片内部电量。
- 新增圆屏安全区系统浮层 `system_overlay_`：
  - 尺寸 `76 x 24`。
  - 位置为 `LV_ALIGN_TOP_RIGHT, -76, 50`，让 WiFi 和电量避开圆屏右上裁切区。
  - 浮层挂在 `lv_screen_active()`，不是 `card_layer_` 子对象。
- 将 WiFi 标志迁移到安全区浮层：
  - 复用原 `network_label_` 更新链路，继续由 `LvglDisplay::UpdateStatusBar()` 写入网络状态图标。
  - 隐藏原顶部栏里的旧 `network_label_`，避免重复显示。
- 将全局电量对象放入同一个安全区浮层：
  - `battery_overlay_`、`battery_overlay_box_`、`battery_overlay_fill_`、`battery_overlay_cap_` 现在作为 `system_overlay_` 子对象。
  - 电量填充仍按真实 `GetBatteryLevel()` 结果更新。
  - 低电量仍显示红色，正常电量显示青绿色 `0x168a73`。
- 修改 `RaiseOverlayObjects()`：
  - 卡片页显示时，先置顶 `card_layer_`，再置顶 `system_overlay_`。
  - 因此分页页隐藏原顶部/底部系统栏时，WiFi 和电量仍保持可见。
- `UpdateStatusBar()` 后同步调用 `ApplyBatteryOverlayLevel()`，保证 Home 页和卡片页的全局电量都能持续刷新。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `docs/update.md`
- `docs/xiaoxin-system-notification-card-progress.zh-CN.md`
- `docs/xiaoxin-vertical-card-pager-plan.zh-CN.md`

#### 验证结果

- `xiaoxin_card_pager_test`：通过。
- `idf.py build`：通过，生成 `build/ai_pet.bin`。
- 构建中仅保留既有的 `esp_lcd_touch_get_coordinates` deprecated warning。

#### 实机观察说明

- 当前安全区位置偏保守，目的是先确保用户能稳定看到 WiFi 和电量。
- 如实机上觉得图标太靠中间，可在确认不会被圆屏边缘裁切后，微调 `k_system_overlay_right` 和 `k_system_overlay_top`。

### Waveshare ESP32-S3 Touch LCD 1.46 分页体验、电量与 PWR 电源键优化

#### 背景

实机反馈卡片分页“不够跟手”，通知卡片右侧信息区域需要更像手表通知中心，并希望使用开发板 PWR 键实现长按开关机。同时，通知卡片新增四格电量后，最初因为板级未接真实电量采样，接电池仍固定显示一格；后续又接入了 BAT_ADC 真实采样。

#### 修改内容

- 分页跟手优化：
  - 新增 `xiaoxin_card_pager_visual_page()`，将拖动时应显示的 visual page 从渲染层抽回状态机。
  - `PaopaoPetDisplay` 增加卡片页渲染缓存，连续拖动同一页时不再反复执行 `RenderCardPage()`，热路径只更新卡片层 `y/opacity`。
  - 触摸轮询从 `20ms` 调整为 `10ms`，提升拖动响应频率。
- 通知卡片布局优化：
  - 第一张通知卡右上角新增四格电量仪表，每格约代表 25%。
  - 低电量时使用红色填充，和左侧低电量状态点呼应。
  - 其它通知卡保留简化 tag/箭头状态区，避免所有卡片视觉权重一致。
- PWR 电源键：
  - 新增 `xiaoxin_power_control` 纯逻辑模块。
  - 启动时拉高 `PWR_Control_PIN`，保持硬件电源锁存。
  - 长按 PWR 后不再切换背光亮灭，而是关闭背光、拉低 `PWR_Control_PIN` 请求关机。
  - USB 供电无法被软件切断时，等待松开 PWR 后进入 deep sleep，并用 PWR 键作为唤醒源。
- 电池电量采样：
  - 根据 Waveshare PDF 原理图网络标注，确认 `BAT_ADC -> IO8`、`BAT_Control -> IO7`、`Key_BAT -> IO6`。
  - GPIO8 对应 ESP32-S3 `ADC_CHANNEL_7`，新增 ADC1 采样、电压校准和三倍分压还原。
  - 新增 `xiaoxin_battery_level` 模块，将电池电压 mV 映射为百分比。
  - `GetBatteryLevel()` 现在返回真实采样结果，通知卡片四格电量不再固定兜底为 25%。
  - `main/CMakeLists.txt` 增加 `esp_adc` 组件依赖。

#### 涉及文件

- `main/CMakeLists.txt`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_power_control.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_power_control.c`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_level.c`
- `tests/xiaoxin_card_pager_test.c`
- `tests/xiaoxin_power_control_test.c`
- `tests/xiaoxin_battery_level_test.c`
- `docs/update.md`

#### 验证结果

- `xiaoxin_card_pager_test`：通过。
- `xiaoxin_power_control_test`：通过。
- `xiaoxin_battery_level_test`：通过。
- `git diff --check`：通过。
- 当前 shell 中未找到 `idf.py`，因此尚未在本机执行完整 ESP-IDF 固件构建。

#### 实机观察说明

- 四格电量为粗粒度显示：4 格代表约 76% 到 100%，不等于精确 100%。
- USB 供电或电池接近满电时，BAT_ADC 采样电压可能显示 4 格，这是正常现象。
- 如果需要继续校准，应观察串口日志中的 `Battery voltage=xxxxmV level=xx%`，再决定是否调整分压倍数或电压百分比曲线。

## 2026-06-17 15:35:54 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 小芯分页卡片 UI 错位修复

#### 背景

实机反馈分页卡片 UI 中的文字位置错乱，并且进入分页页后仍能看到系统顶部状态栏“配网模式”和底部字幕/访问提示压在卡片层上方。问题本质不是视觉风格需要重做，而是分页层与系统栏层级、显隐状态和卡片内部布局约束存在 bug。

#### 问题原因

- `RenderCardPage()` 末尾先将 `card_layer_` 置前，随后又调用 `RaiseOverlayObjects()`，导致 `top_bar_`、`status_bar_`、`bottom_bar_` 再次被提升到分页层上方。
- 分页页可见期间，系统状态更新或聊天文本更新仍可能重新显示顶部/底部栏，造成分页内容被遮挡。
- 通知卡片和总览行内部使用横向 flex 布局，多个 label、tag 和箭头共同参与宽度分配；在 1.46 寸圆屏和中文文本场景下，标题/正文容易被挤压成窄列或发生错位。

#### 修改内容

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`：
  - 新增分页层可见状态判断，`card_layer_` 非隐藏时视为分页 UI 正在覆盖主界面。
  - 新增系统栏隐藏状态缓存：分页层显示时隐藏 `top_bar_`、`status_bar_`、`bottom_bar_`；回到 Home 后按进入分页前的状态恢复。
  - 修改 `RaiseOverlayObjects()`：分页层可见时保持 `card_layer_` 在系统栏之上，仅允许低电量弹窗继续置顶。
  - 在 `SetStatus()`、`SetChatMessage()`、`ClearChatMessages()`、`ShowNotification()`、`UpdateStatusBar()` 和分页动画完成回调后统一重新应用层级/显隐规则，避免状态更新重新把系统栏带到分页页上。
  - 通知卡片内部改为固定坐标布局，明确状态点、文本区域、分类 tag 和箭头的位置与宽度。
  - 总览行内部改为固定坐标布局，明确图标、文本区域和箭头的位置与宽度，避免中文 label 被 flex 压缩。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `docs/update.md`

#### 验证结果

- `xiaoxin_card_pager_test`：通过，分页状态机行为未受影响。
- `git diff --check`：通过。
- 当前 shell 中未找到 `idf.py`、`cmake`、`ninja`，因此尚未在本机执行完整 ESP-IDF 固件构建。

## 2026-06-17 15:30:00 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 小芯卡片分页跟手抽屉化

#### 背景

1.46 寸圆屏需要在原有“小芯桌宠主页”之外增加竖向卡片分页。早期版本虽然能通过上/下滑切换到卡片页，但体验更接近“检测到滑动后触发分页”，页面没有智能手表通知中心/控制中心那种跟随手指拖动的抽屉感。实机观察还发现，完全拉下并松手后偶尔会出现一帧淡色背景，再切到全黑卡片背景，原因是释放吸附时重新渲染并触发卡片淡入，导致卡片内容短暂透明、底层白色桌宠背景透出。

#### 当前交互行为

- Home 页下拉：通知页从屏幕上方跟随手指露出。
- Home 页上拉：总览页从屏幕下方跟随手指露出。
- 通知页/总览页反向拖动：当前卡片页跟随手指滑出，回到 Home。
- 松手后才判断吸附或回弹：
  - 拖动距离达到屏幕高度 20% 阈值时吸附到目标页。
  - 未达到阈值时回弹到原页面或屏幕外隐藏位置。
- 非 Home 卡片页会接管触摸交互，长按可回到 Home。

#### UI 当前状态

- 通知页使用暗色玻璃卡片样式，显示 3 条示例通知。
- 每张通知卡片拆分为状态点、标题、正文、分类 tag 和右箭头，不再使用单个 `title\nbody` 文本标签堆叠。
- 总览页使用分类色块、单字图标、标题正文两行和细分隔线，显示 4 条示例信息。
- 卡片层为深色全屏背景；拖拽释放过程中保持不透明，避免透出底层白色桌宠背景。

#### 修改内容

- 新增 `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h` 和 `xiaoxin_card_pager.c`：
  - 管理 Home、通知页、总览页三态。
  - 记录 press/drag/release、目标页、拖动偏移和吸附/回弹动画状态。
  - 竖向拖动启动阈值为 `6px`，最大拖动距离为整屏高度，保证页面能跟随手指拉满一屏。
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`：
  - 增加卡片分页层 `card_layer_`，并在桌宠层上方渲染卡片页。
  - 新增通知玻璃卡片、总览行、指示条、页面标题等 LVGL 对象。
  - 新增 `DragCardLayerY()`、`HiddenCardLayerY()` 和 `DragCardLayerOpacity()`，将拖动偏移映射为卡片层真实屏幕位置。
  - 移除拖动过程中的 `offset / 4` 式轻微晃动，改为抽屉式跟手移动。
  - 拖拽释放使用独立完成回调，不再触发卡片二次淡入动画。
  - 释放吸附动画保持 `card_layer_` 不透明，修复“淡色背景一帧后再变黑”的闪烁。
- 新增 `tests/xiaoxin_card_pager_test.c`：
  - 覆盖下拉进入通知页、上拉进入总览页、反向拖回 Home、短拖回弹、横向拖动不触发分页、长拖可跟随整屏、卡片项排序和非 Home 页触摸接管。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`
- `tests/xiaoxin_card_pager_test.c`
- `docs/update.md`

#### 验证结果

- `xiaoxin_card_pager_test`：通过，分页状态机和长距离跟手拖动逻辑正常。
- `git diff --check`：通过。
- 当前 shell 中未找到 `idf.py`，因此尚未在本机执行完整 ESP-IDF 固件构建。

## 2026-06-17 12:40:49 +08:00

### Speaking 修复版动画资源替换

#### 背景

需要将 speaking 状态使用的动画资源替换为修复后的 speaking 动画，并同步修改资源文件名，确保嵌入符号、状态映射、测试期望和显示尺寸统一逻辑都指向修复版文件。

#### 资源来源

- 修复版来源文件：`D:\Learn\paopao_ui\firmware\paopao_pet\assets\speaking_pet\speaking.gif`
- 项目内新文件名：`main/assets/images/speaking_fixed.gif`
- 旧项目文件名：`main/assets/images/speaking.gif` 已移除，不再作为 speaking 状态资源。
- SHA256 校验：修复版来源文件与替换前项目内 `speaking.gif` 内容一致，均为 `47ED5189DB7C0904AE60C989C6F7241487AAA984C21F414D0295C706B60301C8`。本次仍按修复版文件重新接入，并使用新文件名让状态映射明确指向修复版资源。

#### 修改内容

- `main/assets/images/speaking_fixed.gif`：新增修复版 speaking GIF。
- `main/assets/images/speaking.gif`：删除旧文件名，避免继续引用旧资源名。
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`：
  - 嵌入符号从 `_binary_speaking_gif_start/end` 改为 `_binary_speaking_fixed_gif_start/end`。
  - `PAOPAO_PET_STATE_SPEAKING` 的二进制资源改为 `assets_images_speaking_fixed_gif_start/end`。
  - speaking 的前景最长边仍为 `182px`，继续按统一视觉尺寸缩放到 `162px`。
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/paopao_pet_gif_assets.c`：
  - `PAOPAO_PET_STATE_SPEAKING` 返回值从 `speaking.gif` 改为 `speaking_fixed.gif`。
- `tests/paopao_gif_probe_decode_test.c`：
  - speaking 解码测试路径从 `main/assets/images/speaking.gif` 改为 `main/assets/images/speaking_fixed.gif`。
- `tests/paopao_pet_gif_assets_test.c`：
  - speaking 状态映射期望从 `speaking.gif` 改为 `speaking_fixed.gif`。
- `docs/update.md`：记录本次资源替换、文件名变化、尺寸测量和验证结果。

#### 尺寸测量

| GIF | 画布 | 帧数 | 玩偶前景尺寸 | 前景最长边 | 显示缩放 | 缩放后前景最长边 |
| --- | --- | ---: | --- | ---: | ---: | ---: |
| `speaking_fixed.gif` | `192x208` | 6 | `143x182` | 182 | `228/256` | 162 |

结论：修复版 speaking 动画接入后仍满足当前统一尺寸规则，显示出来的玩偶前景最长边与其他状态一致，为 `162px`。

#### 验证结果

- `paopao_gif_probe_decode_test`：通过，11 个 GIF 均可解码采样。
- `paopao_pet_gif_assets_test`：通过，`PAOPAO_PET_STATE_SPEAKING` 映射到 `speaking_fixed.gif`。
- `paopao_pet_trigger_test`：通过，speaking 状态触发和恢复逻辑正常。

## 2026-06-17 12:11:57 +08:00

### Wi-Fi 默认配置清理

#### 背景

重新烧录后设备会自动写入默认 Wi-Fi `Jiang`，导致设备没有进入配网模式，并可能在错误网络环境下进行 OTA 检查，出现 `ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT` / `code=32774`。

#### 修改内容

- 移除 `main/boards/common/wifi_board.cc` 中自动写入默认 Wi-Fi 密码的逻辑。
- 保留默认 SSID 名称仅用于识别旧记录，不再保存默认密码。
- 启动连接 Wi-Fi 前，如果 NVS 中仍存在旧的默认 SSID `Jiang`，自动删除该条记录。
- 删除旧默认记录后，如果没有其他已保存 Wi-Fi，设备会进入配网模式。

#### 涉及文件

- `main/boards/common/wifi_board.cc`
- `docs/update.md`

## 2026-06-17 10:21:37 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 桌宠动画显示尺寸统一

#### 背景

用户反馈不同 GIF 动画显示出来的玩偶大小不一致，尤其 `giddy.gif` 眩晕动画看起来比 `idle.gif` 待机动画大很多。排查后确认问题不只是 GIF 画布大小不同，而是每个 GIF 内部玩偶前景区域占比不同。

#### 尺寸测量

测量方式：遍历 `main/assets/images/*.gif` 的所有帧，合并所有非背景像素的前景包围盒，以包围盒最长边作为玩偶本体视觉尺寸。

已映射到桌宠状态机的动画尺寸：

| GIF | 状态 | 画布 | 玩偶前景尺寸 | 前景最长边 |
| --- | --- | --- | --- | ---: |
| `idle.gif` | `IDLE` | `256x256` | `130x162` | 162 |
| `working.gif` | `WORKING` | `256x256` | `165x154` | 165 |
| `speaking_fixed.gif` | `SPEAKING` | `192x208` | `143x182` | 182 |
| `thinking.gif` | `THINKING` | `256x256` | `150x159` | 159 |
| `waiting.gif` | `WAITING` | `256x256` | `124x151` | 151 |
| `done.gif` | `DONE` | `256x256` | `134x150` | 150 |
| `sleeping.gif` | `SLEEPING` | `256x256` | `163x110` | 163 |
| `jumping.gif` | `JUMPING` | `256x256` | `115x125` | 125 |
| `failed.gif` | `FAILING` | `256x256` | `107x112` | 112 |
| `giddy.gif` | `GIDDY` | `256x256` | `207x238` | 238 |
| `review.gif` | `REVIEW` | `256x256` | `104x126` | 126 |

未映射到当前桌宠状态机的 GIF 也已测量，后续如果接入状态机，需要同步加入显示尺寸表：

| GIF | 画布 | 玩偶前景尺寸 | 前景最长边 |
| --- | --- | --- | ---: |
| `anxiety.gif` | `256x256` | `173x172` | 173 |
| `happy.gif` | `256x256` | `201x187` | 201 |
| `stamp.gif` | `256x256` | `135x166` | 166 |
| `tired.gif` | `256x256` | `134x164` | 164 |
| `waving.gif` | `256x256` | `115x138` | 138 |

#### 修改内容

- 在 `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc` 中保留固定显示区域作为桌宠显示基准。
- 新增 `k_pet_target_visual_longest = 162`，以 `idle.gif` 的玩偶前景最长边作为统一显示基准。
- 新增 `PaopaoGifVisualLongestForState()`，记录每个已映射桌宠状态对应 GIF 的实测玩偶前景最长边。
- 新增 `PaopaoImageScaleForVisualSize()`，按 `目标最长边 / 当前动画前景最长边` 计算 LVGL 缩放值。
- 缩放值使用四舍五入计算，避免整数除法向下取整导致个别动画显示最长边落到 `161px`。
- `PlayGifState()` 切换动画时统一应用缩放，并保持玩偶居中。
- `giddy.gif` 从前景最长边 `238px` 缩放到约 `162px`，不再比 `idle.gif` 大。
- `speaking_fixed.gif` 虽然画布是 `192x208`，也按前景最长边 `182px` 缩放到统一视觉尺寸。

#### 统一后的显示结果

| 状态 | GIF | 显示缩放 | 缩放后前景最长边 |
| --- | --- | ---: | ---: |
| `IDLE` | `idle.gif` | `256/256` | 162 |
| `WORKING` | `working.gif` | `251/256` | 162 |
| `SPEAKING` | `speaking_fixed.gif` | `228/256` | 162 |
| `THINKING` | `thinking.gif` | `261/256` | 162 |
| `WAITING` | `waiting.gif` | `275/256` | 162 |
| `DONE` | `done.gif` | `276/256` | 162 |
| `SLEEPING` | `sleeping.gif` | `254/256` | 162 |
| `JUMPING` | `jumping.gif` | `332/256` | 162 |
| `FAILING` | `failed.gif` | `370/256` | 162 |
| `GIDDY` | `giddy.gif` | `174/256` | 162 |
| `REVIEW` | `review.gif` | `329/256` | 162 |

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `docs/update.md`

#### 验证结果

- `paopao_gif_probe_decode_test`：通过，11 个 GIF 均可解码采样。
- `paopao_pet_gif_assets_test`：通过。
- `paopao_pet_trigger_test`：通过。

## 2026-06-17 10:14:18 +08:00

### Waveshare ESP32-S3 Touch LCD 1.46 GIF 显示与黑短线修复

#### 背景

当前 1.46 寸屏版本使用 LVGL 显示小芯 GIF 动画。恢复原有 LVGL 顶部状态栏和底部字幕后，设备屏幕左侧出现一列黑色短线。用户反馈黑短线只在加入 GIF 资源并播放后出现，位置接近 GIF 动画显示区域的左边界。

#### 排查记录

1. 检查原始 `idle.gif` 帧，确认左侧没有对应黑色短线，排除素材本身问题。
2. 尝试给 GIF 对象增加不透明白色背景层，黑短线仍存在。
3. 尝试在 `LvglGif` 中将透明像素预合成到白底，黑短线仍存在。
4. 尝试将 GIF 输出改成 `RGB565`，黑短线仍存在。
5. 最终方案：使用全屏 RGB565 背景帧合成。每帧先填充白底，再将 GIF 当前帧按当前视觉缩放居中拷贝到全屏缓冲，最后让 LVGL 显示这张全屏 image。用户实机确认黑色短线消失。

#### 修改内容

- `PaopaoPetDisplay::SetupUI()` 改为先调用 `LcdDisplay::SetupUI()`，恢复顶部状态栏、通知栏、电量/网络图标和底部字幕栏。
- `SetStatus()` 先调用 `LcdDisplay::SetStatus(status)`，再根据状态触发桌宠动画。
- `SetChatMessage()` 保留动画触发，同时调用 `LcdDisplay::SetChatMessage(role, content)` 显示字幕。
- `ShowNotification()`、`UpdateStatusBar()`、`ClearChatMessages()` 恢复调用基类实现。
- `LvglGif` 构造函数新增 `force_opaque_background`、`background_rgb`、`output_rgb565` 参数。
- `LvglGif` 新增 `ApplyOpaqueBackground()`，用于将透明/半透明像素合成到指定背景色。
- `LvglGif` 新增 `UpdateImageData()`，用于在 `output_rgb565` 模式下将 GIF canvas 转换为 `RGB565` 输出缓冲。
- 板级显示类新增全屏帧缓冲 `pet_frame_dsc_` 和 `pet_frame_buffer_`。
- 新增 `InitializePetFrameBuffer()`，分配并初始化全屏 `RGB565` 图像缓冲。
- 新增 `CopyPetFrameToScreen()`，每帧先填充白底，再将 GIF 当前帧按缩放比例居中贴到全屏缓冲。
- `PlayGifState()` 改为使用全屏 `pet_frame_dsc_` 刷新 LVGL image，不再直接显示 GIF 小图对象。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/display/lvgl_display/gif/lvgl_gif.h`
- `main/display/lvgl_display/gif/lvgl_gif.cc`

#### 验证结果

- 小芯 GIF 正常播放。
- 左侧黑色短线消失。
- 原 LVGL 顶部状态栏恢复，可显示例如“配网模式”。
- 原 LVGL 底部字幕/流式文字恢复。

#### 后续注意

- 当前全屏缓冲约占用 `412 * 412 * 2 = 339488` 字节，约 331.5 KiB，优先分配到 PSRAM。
- 如果后续切换为非白色背景，需要同步调整白底填充值、`LvglGif` 的 `background_rgb` 和屏幕背景色。
- 如果未来 LVGL 或 LCD 驱动修复了小图动态刷新边界问题，可以重新评估是否退回直接显示 GIF 小图对象。
- 后续继续调整 GIF 尺寸、缩放或显示层级时，应优先保留“全屏背景帧合成”策略，避免黑短线问题回归。
## 2026-06-18 竖向通知卡连续跟手滚动

### Waveshare ESP32-S3 Touch LCD 1.46 通知页交互更新

#### 背景

实机反馈希望通知页像手机通知中心一样，在一屏内通过滑动查看多条通知，并且要有“跟手”的动态滑动效果。中间曾尝试过横向卡片切换和竖向分页切换，但最终需求明确为：不要“切换一张”的分页感，而要通知卡组随手指连续滚动。

同时，通知页的收回规则需要更严格：只有通知内容已经滑到最底部后，再继续自下而上滑动，才允许把通知页收回到主界面。

#### 修改内容

- 通知卡从单张当前卡扩展为一组竖向通知卡：
  - `k_card_glass_count` 从 3 调整为 4，可同时承载当前静态通知列表中的 4 条通知。
  - 通知卡统一使用大玻璃卡样式、系统感 icon、右上角“现在”、标题和正文。
  - 通知卡竖向间距使用 `k_notification_slide_pitch = 116`，让多条通知形成一组可滚动内容。
- 新增通知页内部连续滚动状态：
  - `notification_scroll_y_` 记录通知卡组当前滚动偏移。
  - `notification_drag_start_scroll_y_` 记录本次触摸开始时的滚动位置。
  - 新增 `NotificationMinScrollY()`、`ClampNotificationScrollY()`、`NotificationScrollDisplayY()`、`NotificationIndexForScroll()` 等辅助函数。
- 通知页视觉更新为“跟手滚动”：
  - 拖动过程中直接调用 `ApplyNotificationScrollVisual()`，卡片位置按像素跟随手指移动。
  - 松手后不再强制跳到上一条/下一条通知，只保存当前位置。
  - 顶部/底部越界时使用 1/3 阻尼显示，松手后通过 `AnimateNotificationScroll()` 回弹到合法范围。
  - 底部圆点指示器仍保留，但当前点由连续滚动位置推导，而不是由一次性分页切换决定。
- 通知页收回规则调整：
  - 普通上/下滑优先滚动通知卡组。
  - 只有当 `notification_scroll_y_` 已经到达最后一条通知的底部，再继续上滑，才把手势交给外层 `xiaoxin_card_pager_drag()`，触发通知页整体上收回主页。
  - 通知页长按返回主页逻辑被取消，避免绕过“必须滑到底再上滑收回”的规则。
- 保留状态机里的通知索引接口：
  - `notification_index` / `notification_count` 仍保留，用于测试、圆点状态和后续真实通知数据源接入。
  - 但当前主要视觉交互不再依赖 `notification_next()` / `notification_prev()` 做切换动画。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`
- `tests/xiaoxin_card_pager_test.c`
- `docs/update.md`
- `docs/xiaoxin-system-notification-card-progress.zh-CN.md`
- `docs/xiaoxin-vertical-card-pager-plan.zh-CN.md`

#### 当前交互规则

| 场景 | 手势 | 行为 |
|---|---|---|
| Home | 下滑 | 进入通知页 |
| 通知页 | 上/下滑，未到底 | 通知卡组连续跟手滚动 |
| 通知页 | 顶部或底部越界 | 阻尼跟手，松手回弹 |
| 通知页 | 已到最底部后继续上滑 | 收回通知页，返回 Home |
| 通知页 | 横向滑动 | 不切换通知，不触发主页返回 |
| 总览页 | 下滑 | 返回 Home |

#### 验证结果

- `xiaoxin_card_pager_test`：通过。
- `xiaoxin_battery_level_test`：通过。
- 当前 PowerShell 中仍未找到 `idf.py`，因此本轮未执行完整 ESP-IDF 固件构建。

## 2026-06-18 透明分页背景与栈溢出修复

### Waveshare ESP32-S3 Touch LCD 1.46 通知卡背景行为修正

#### 背景

实机上下滑进入分页卡片时出现 `***ERROR*** A stack overflow in task paopao_pet has been detected.`，设备随后 panic 重启。排查确认不是分页状态机主动重启，而是 `paopao_pet` 任务在卡片显示热路径中栈溢出。

同时，通知卡背景需求进一步明确：通知卡片背景不应来自 LVGL snapshot 或另行生成的背景图，而应让主页本身停在当前一帧，作为分页卡片下方的真实背景。

#### 修改内容

- 修复 `paopao_pet` 任务栈溢出：
  - 新增 `k_pet_render_task_stack_bytes = 12 * 1024`。
  - `paopao_pet` 渲染/触摸任务栈从 `4096` 提升到 `12 KiB`。
- 移除分页背景图链路：
  - 删除 `card_background_buffer_`、`card_background_image_`、`card_background_dsc_` 等背景图状态。
  - 删除 `CaptureCardBackgroundSnapshot()`、`DimCardBackgroundSnapshot()`、`HideCardBackgroundSnapshot()` 等伪 snapshot 逻辑。
  - 确认当前代码中不再引用 `snapshot`、`card_background` 或 `lv_snapshot`。
- 调整分页显示方式：
  - `card_layer_` 保持 `LV_OPA_TRANSP`，分页层本身透明。
  - 卡片页可见时调用 `pet_gif_controller_->Pause()`，让主页桌宠停在当前帧。
  - 卡片页回到 Home 后调用 `pet_gif_controller_->Resume()`，恢复主页桌宠动画。
  - 分页层可见期间 `ApplyPetStateIfChanged()` 暂缓切换桌宠状态，避免背景突然跳到其它 GIF 的第一帧。
- 调整视觉透明度：
  - 通知玻璃卡背景不透明度降低，使主页静止画面能够透出。
  - 总览行背景不透明度同步降低，维持与通知页一致的透明覆盖感。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `docs/update.md`
- `docs/xiaoxin-system-notification-card-progress.zh-CN.md`

#### 验证结果

- `xiaoxin_card_pager_test`：通过。
- `idf.py build`：通过，已生成 `build/ai_pet.bin`。
- 构建仅保留既有 `esp_lcd_touch_get_coordinates()` deprecated warning。
