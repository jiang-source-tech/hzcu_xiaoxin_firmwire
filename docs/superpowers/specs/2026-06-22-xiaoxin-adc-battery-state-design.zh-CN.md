# 小芯 ADC 电量状态系统设计

日期：2026-06-22

## 背景

小芯当前 Waveshare ESP32-S3 Touch LCD 1.46 板子的电量来自 `BAT_ADC -> GPIO8 / ADC1_CHANNEL_7` 的电压采样，再通过 `xiaoxin_battery_percent_from_mv()` 粗略换算为百分比。这个百分比目前会直接影响右上角电池颜色、低电量通知和宠物 mood。

实机反馈显示，在 USB 供电或语音链路工作时，电压采样可能短时下陷，导致设备突然显示低电量。对消费级产品来说，这种“瞬时采样值直接改变 UI 状态”的行为不合理：用户看到的应该是可信、稳定、经过确认的电量状态，而不是某一毫秒 ADC 读数的波动。

本设计限定第一版硬件只使用 ADC，不接入电量计或 PMIC 的 SOC 输出。

## 目标

- 让右上角电池显示稳定，不因一次瞬时电压波动变红。
- 保留 ADC 方案，但不把 ADC 估算百分比包装成精确电量。
- 建立统一的电量状态机，让右上角浮层、通知中心、总览页和宠物 mood 使用同一份电量状态。
- 在语音对话、播放等高负载场景下避免误触发低电量 UI。
- 为未来接入更好的硬件电量来源保留接口，但第一版不依赖新硬件。

## 非目标

- 不显示精确电量百分比，例如“剩余 18%”。
- 不追求手机级 SOC 精度。
- 不在底部字幕区域显示低电量警告。
- 不让各 UI 模块自行判断 `level <= 20` 这类阈值。

## 总体架构

新增一个独立的 ADC 电量状态模块，建议命名为：

```text
main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.h
main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_battery_state.c
```

链路如下：

```text
ADC 原始电压
  -> 单次采样去极值平均
  -> 慢速平滑
  -> 内部粗略百分比
  -> 电量状态机
  -> 电量快照
  -> 右上角浮层 / 通知中心 / 总览页 / 宠物 mood
```

板级代码继续负责实际 ADC 读取。新增模块只接收 `voltage_mv`、当前时间和当前负载场景，输出稳定的电量快照。

## 数据模型

建议状态：

```c
typedef enum {
  XIAOXIN_BATTERY_STATE_UNKNOWN,
  XIAOXIN_BATTERY_STATE_NORMAL,
  XIAOXIN_BATTERY_STATE_LOW,
  XIAOXIN_BATTERY_STATE_CRITICAL,
} xiaoxin_battery_state_t;
```

建议负载场景：

```c
typedef enum {
  XIAOXIN_BATTERY_LOAD_IDLE,
  XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE,
} xiaoxin_battery_load_t;
```

建议快照：

```c
typedef struct {
  xiaoxin_battery_state_t state;
  int estimated_percent;
  int smoothed_voltage_mv;
  bool low_edge;
  bool critical_edge;
  bool recovered_edge;
} xiaoxin_battery_snapshot_t;
```

`estimated_percent` 只供内部状态判断、图标格数或测试使用。UI 文案不展示精确百分比。

## 采样与平滑策略

板级 ADC 读取保持每次多样本采样：

- 每次更新读取 8 到 10 个 ADC 样本。
- 去掉最高值和最低值。
- 对剩余样本求平均，得到本轮 `raw_voltage_mv`。
- 如果采样失败或电压明显不可信，状态机进入或保持 `UNKNOWN`。

状态模块对 `raw_voltage_mv` 做慢速平滑。建议使用 EMA：

```text
smoothed = smoothed * 0.85 + raw * 0.15
```

第一轮有效采样直接初始化 `smoothed_voltage_mv`，避免启动时从 0 慢慢爬升。

如果后续实机发现语音播放时电压塌陷明显，可以在 `VOICE_ACTIVE` 场景下降低低电压样本权重，或者只允许降级计时、不立即改变状态。

## 状态机规则

状态变化必须满足阈值和持续时间两个条件。

建议第一版阈值：

```text
UNKNOWN -> NORMAL:
  有效采样连续存在，并且估算电量 >= 25%，持续 5 秒

NORMAL -> LOW:
  平滑估算电量 <= 20%，持续 20 秒

LOW -> NORMAL:
  平滑估算电量 >= 30%，持续 10 秒

LOW -> CRITICAL:
  平滑估算电量 <= 10%，持续 10 秒

CRITICAL -> LOW:
  平滑估算电量 >= 18%，持续 15 秒

任意状态 -> UNKNOWN:
  ADC 连续采样失败，或电压长期处于不可信范围
```

规则原则：

- 进入更差状态要慢，避免瞬时电压波动吓用户。
- 退出更差状态要有滞回，避免在边界附近来回跳。
- `LOW` 是温和提示，`CRITICAL` 才是强低电量状态。

## 语音高负载抑制

语音链路工作时，WiFi、麦克风、喇叭、屏幕和 CPU 可能同时拉高负载，电池电压短时下陷是正常现象。

当设备处于 listening、thinking 或 speaking 状态时：

- 继续采样和平滑。
- 不允许一次短时下陷直接触发 `NORMAL -> LOW` 或 `LOW -> CRITICAL`。
- 如果低电压持续超过更长确认时间，可以允许状态变化，避免真的低电量被完全忽略。

建议高负载确认时间：

```text
NORMAL -> LOW during VOICE_ACTIVE:
  <= 20%，持续 45 秒

LOW -> CRITICAL during VOICE_ACTIVE:
  <= 10%，持续 30 秒
```

对话结束后，状态机按普通规则继续确认。如果电压恢复，则不触发低电量 UI。

## UI 行为

### 右上角电池浮层

右上角电池只消费 `xiaoxin_battery_snapshot_t.state`：

```text
UNKNOWN:  灰色或空心
NORMAL:   当前青绿色
LOW:      橙色或黄色
CRITICAL: 红色
```

右上角不展示精确百分比。可继续展示格数，但格数来自稳定快照，而不是瞬时 ADC。

### 通知中心

通知中心使用同一状态机：

```text
LOW:
  注入或更新“电量偏低，请尽快充电”

CRITICAL:
  注入或更新“电量很低，请尽快充电”

NORMAL:
  移除低电量通知

UNKNOWN:
  不注入低电量通知
```

低电量通知不应弹出覆盖底部字幕，不应打断正在进行的对话。

### 总览页

总览页设备状态使用定性文案：

```text
UNKNOWN:  电量未知
NORMAL:   电量正常 / 电量充足
LOW:      电量偏低
CRITICAL: 请尽快充电
```

总览页不展示 ADC 推算出的精确百分比。

### 宠物 mood

宠物 mood 只响应状态边沿：

```text
NORMAL -> LOW:
  触发一次 tired/weak 类型反馈

LOW -> CRITICAL:
  可触发一次更明显的疲惫反馈

LOW/CRITICAL -> NORMAL:
  触发一次恢复反馈
```

持续低电量不重复触发情绪动画。CRITICAL 状态如果需要周期提醒，应使用 5 到 10 分钟级别的冷却。

## 异常处理

ADC 读数可能因为无电池、USB 直供、接触不良或硬件差异而失真。第一版统一处理为“不可信则 UNKNOWN”，不要显示红色低电量。

建议不可信范围：

```text
voltage_mv <= 0:
  采样失败

voltage_mv < 3000 且持续异常:
  可能无电池或采样异常，进入 UNKNOWN

voltage_mv > 4400 且持续异常:
  可能采样异常，进入 UNKNOWN
```

这些范围需要实机校准，第一版可以保守处理：异常读数只影响 `UNKNOWN`，不触发 `LOW/CRITICAL`。

## 接入点

现有调用需要收敛到状态模块：

- `NotificationBatteryLevelPercent()` 不再作为 UI 状态源。
- `xiaoxin_system_overlay_style()` 不再用 `battery_level_percent <= 20` 判断红色。
- `SyncLowBatteryNotificationLocked()` 改为消费电量状态，而不是瞬时百分比。
- `SyncPetMoodDeviceStateLocked()` 改为消费电量状态边沿，而不是瞬时百分比。
- `BuildOverviewState()` 使用电量快照的定性状态。

第一版可以保留 `xiaoxin_battery_percent_from_mv()` 作为内部估算函数，但它不直接驱动 UI。

## 测试策略

新增纯 C 单测覆盖电量状态模块：

- 初始化后为 `UNKNOWN`。
- 连续有效采样进入 `NORMAL`。
- 单次低电压不会进入 `LOW`。
- 低电压持续达到确认时间才进入 `LOW`。
- `LOW` 状态下电量回到 21% 不会立刻恢复。
- 电量持续高于 30% 才恢复 `NORMAL`。
- `LOW -> CRITICAL` 需要持续确认。
- `VOICE_ACTIVE` 下低电量确认时间更长。
- ADC 失败或异常读数进入 `UNKNOWN`，不触发低电量通知边沿。

保留源码路径测试，确保板级 UI 不再直接判断 `level <= 20`：

- 右上角浮层使用电量状态。
- 通知中心使用电量状态。
- 宠物 mood 使用状态边沿。
- 底部字幕区域不显示低电量 popup。

## 分阶段落地

### 阶段一：稳定状态机

- 新增 `xiaoxin_battery_state` 模块。
- 接入 ADC 平滑、滞回和时间确认。
- 右上角 LOW 用橙色，CRITICAL 才红色。
- 通知中心只响应稳定后的 LOW/CRITICAL。

### 阶段二：语音高负载抑制

- 将设备状态映射为 `IDLE` 或 `VOICE_ACTIVE`。
- 语音期间延长低电量确认时间。
- 对话结束后继续按普通规则确认。

### 阶段三：实机校准

- 采集满电、中等电量、低电量、USB 直供、无电池等场景的 ADC 日志。
- 微调阈值、EMA 权重和异常范围。
- 如能稳定识别无电池/外接供电，再增加独立 `EXTERNAL` 状态。

## 成功标准

- 语音对话中一次瞬时电压下陷不会让右上角电池突然变红。
- 低电量状态需要持续确认才出现。
- 电量在阈值附近不会红绿闪烁。
- 通知中心不会因为瞬时 ADC 波动反复增删低电量卡片。
- 底部流式字幕不被低电量提示覆盖。
- 所有 UI 模块使用同一电量状态快照。
