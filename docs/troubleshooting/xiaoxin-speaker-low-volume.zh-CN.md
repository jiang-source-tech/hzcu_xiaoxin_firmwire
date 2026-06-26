# 小新喇叭输出音量过小排查记录

## 问题现象

开发板自带喇叭音量旋钮已经调到最大，但设备播放出来的声音仍然偏小。

这次确认的问题是喇叭输出音量小，不是麦克风输入音量小。此前尝试过麦克风 slot 和输入增益方向，其中 `I2S_STD_SLOT_LEFT` 会导致设备听不见人声，输入增益也不解决喇叭小声问题，因此相关修改已撤回。

## 当前音频输出链路

当前板型是 Waveshare ESP32-S3 Touch LCD 1.46，音频 codec 在这里创建：

```text
main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
```

板级代码使用 `NoAudioCodecSimplex` 输出到 I2S 喇叭链路：

```cpp
static NoAudioCodecSimplex audio_codec(
    AUDIO_INPUT_SAMPLE_RATE,
    AUDIO_OUTPUT_SAMPLE_RATE,
    AUDIO_I2S_SPK_GPIO_BCLK,
    AUDIO_I2S_SPK_GPIO_LRCK,
    AUDIO_I2S_SPK_GPIO_DOUT,
    I2S_STD_SLOT_LEFT,
    AUDIO_I2S_MIC_GPIO_SCK,
    AUDIO_I2S_MIC_GPIO_WS,
    AUDIO_I2S_MIC_GPIO_DIN,
    I2S_STD_SLOT_RIGHT);
```

喇叭相关引脚与板卡资料一致：

```cpp
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_47
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_48
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_38
```

## 关键发现

`NoAudioCodec::Write()` 会按软件音量计算输出幅度：

```cpp
int32_t volume_factor = pow(double(output_volume_) / 100.0, 2) * 65536;
```

默认 `output_volume_` 是 `70`，平方曲线后实际幅度约为 `49%`。所以即使硬件旋钮打满，如果软件音量仍是 70，整体输出仍会明显偏小。

## 当前修改

1. 在 Waveshare ESP32-S3 Touch LCD 1.46 的 `GetAudioCodec()` 中强制设置：

```cpp
audio_codec.SetOutputVolume(100);
```

用户实测后反馈声音已经变大。

2. 曾尝试增加 `1.25x` 和 `1.5x` 输出数字增益，但用户实测判断数字增益没有明显效果，因此已撤回该方案。

3. 当前板级试验值只保留：

```cpp
audio_codec.SetOutputVolume(100);
```

## 验证建议

每次只调整一个值并重新烧录测试：

1. 先确认单独 `SetOutputVolume(100)` 是否稳定保留此前听到的音量提升。
2. 听是否出现破音、爆音、明显失真或长时间播放发热异常。
3. 如果仍然偏小，下一步优先检查硬件输出链路，而不是继续做数字增益。
4. 数字增益已验证收益不明显，且可能牺牲动态范围，不能替代功放、电源、喇叭或硬件旋钮链路本身的排查。

## 注意事项

- 这个方案只增强喇叭输出，不改变麦克风输入。
- `SetOutputVolume(100)` 是软件音量满幅。
- 数字增益方案已撤回；当前不再保留 `SetOutputBoost()` 或 `output_boost_`。
- 如果 `100` 仍不够，说明软件侧空间有限，下一步应检查功放供电、喇叭规格、硬件音量电位器和外壳开孔等硬件因素。
