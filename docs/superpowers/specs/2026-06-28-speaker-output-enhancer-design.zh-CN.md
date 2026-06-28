# 扬声器输出增强器设计

## 背景

当前固件的播放链路集中在 `AudioService`：

- 云端 TTS/电话类下行音频进入 `audio_decode_queue_`。
- `OpusCodecTask` 解码为 16-bit PCM，必要时重采样到 codec 输出采样率。
- `AudioOutputTask` 从 `audio_playback_queue_` 取 PCM 并调用 `AudioCodec::OutputData()`。
- `PlaySound()` 播放的 OGG 提示音也会 demux 成 Opus 包，再走同一解码和播放队列。

因此第一版增强应放在 `AudioOutputTask` 和 `AudioCodec::OutputData()` 之间，对所有扬声器播放内容统一生效，同时不影响麦克风采集、唤醒词、AEC、VAD 和上行 Opus 编码。

## 目标

在小尺寸手表喇叭上提升提示音、TTS 和电话类人声的听感响度，避免只是粗暴增大音量导致削波、破音或低频浪费。

第一版目标：

- 削减小喇叭难以有效重放的低频能量。
- 适度突出人声和提示音可懂度频段。
- 压缩动态范围，让较小的人声细节更容易听见。
- 对最终 PCM 做限幅，避免增强后超过 `int16_t` 安全范围。
- 保持实现可关闭、参数集中，方便后续实机调音。

## 非目标

- 不改麦克风输入链路。
- 不改唤醒词、语音识别、服务端协议和 AEC 行为。
- 不引入 SpeexDSP、WebRTC APM 或 npm/Web Audio 依赖。
- 不在第一版做离线重制 OGG 素材；离线素材处理可作为后续补充。
- 不实现复杂的多板级自动声学校准。

## 方案

新增一个播放端模块 `SpeakerOutputEnhancer`，由 `AudioService` 持有并在播放前调用：

```text
Opus/OGG packet
  -> Opus decoder
  -> optional output resampler
  -> SpeakerOutputEnhancer::Process()
  -> AudioCodec::OutputData()
  -> I2S / speaker
```

模块职责：

- 初始化并封装乐鑫 `esp_audio_effects` 中的 EQ / DRC 或 ALC 能力。
- 在输出采样率变化时按采样率重新配置处理器。
- 对每个 PCM frame 原地处理，减少额外分配。
- 如果效果器初始化失败，自动退回到轻量软件限幅路径，保证播放不中断。
- 暴露一个小的参数结构体，集中管理默认调音参数。

第一版采用保守的单通道配置，因为当前下行 Opus 解码和输出重采样路径按 mono PCM 处理。

## 初始调音

默认参数以“更响但不刺耳”为目标，具体数值需要实机确认后再微调：

- High-pass / low-shelf：削减约 150 Hz 以下或 200 Hz 附近低频。
- Presence boost：在 2 kHz 左右适度提升人声存在感。
- Clarity boost：在 3.5 kHz 到 4 kHz 左右轻微提升提示音穿透力。
- DRC / ALC：采用中等压缩，补偿少量 makeup gain。
- Limiter：最终峰值限制在约 -1 dBFS 到 -2 dBFS，对应 `int16_t` 留出安全余量。

参数文件不分散到业务代码里。若使用 `esp_audio_effects` API 时某些滤波器模式或曲线点需要更具体的结构体配置，实现阶段以组件头文件和示例为准，但语义保持上述目标。

## 配置与降级

新增编译期配置建议：

- `CONFIG_SPEAKER_OUTPUT_ENHANCER`：默认开启。

新增运行时行为：

- `SpeakerOutputEnhancer::Initialize(sample_rate)` 失败时记录日志，并进入 bypass/limiter-only 模式。
- `Process()` 接收空 buffer 时直接返回。
- 如果输出采样率与已初始化采样率不同，重新初始化处理器。
- `ResetDecoder()` 不需要重置增强器状态；增强器只处理播放端连续 PCM，不持有协议状态。

## 错误处理

- 效果器打开失败：不阻断音频，打印一次错误日志，继续播放原始 PCM。
- 效果器处理失败：打印限频警告，当前 frame 走软件限幅，后续 frame 继续尝试或进入 bypass。
- PCM 溢出：所有软件增益或 fallback 路径都使用饱和裁剪到 `int16_t` 范围。
- 采样率异常：跳过效果器，只做限幅。

## 测试策略

主机侧单元测试优先覆盖不依赖 ESP-IDF 二进制库的部分：

- 软件限幅不会溢出 `int16_t`。
- 空数据、正常语音幅度、超大幅度输入都能稳定返回。
- 增强器 disabled/bypass 路径不改变 frame 长度。

固件构建验证：

- 至少编译 `audio_service.cc` 相关目标，确认新增头文件、组件依赖和 Kconfig 配置可用。
- 如果当前 shell 没有 ESP-IDF 环境，则记录无法完整 `idf.py build` 的原因。

实机验证：

- 播放启动成功音、popup、错误提示音，确认无爆音。
- 播放 TTS 中文人声，确认主观响度提升且齿音不过度。
- 在最大音量下确认没有持续削波或明显破音。
- 切换聆听/说话状态，确认麦克风、唤醒词和对话状态不回归。

## 后续可选增强

- 为 Waveshare 1.46 目标板单独保存一组实机调音参数。
- 增加设置页中的“响度增强”开关。
- 用 FFmpeg/SoX 批量生成手表喇叭版 OGG 提示音，减少实时处理负担。
- 如果未来硬件换成 smart amp，保留软件 EQ/DRC 作为前级或改为板级可选。
