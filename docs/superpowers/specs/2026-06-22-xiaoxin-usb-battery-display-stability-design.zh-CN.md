# 小新 USB 供电电量显示稳定设计

日期：2026-06-22

## 背景

当前 Waveshare ESP32-S3 Touch LCD 1.46 的电量来自 `BAT_ADC -> GPIO8 / ADC1_CHANNEL_7` 的电压采样，再通过 `xiaoxin_battery_percent_from_mv()` 粗略换算为百分比。已有优化为低电状态增加了 EMA 平滑、滞回和持续时间确认，解决了“瞬时低压立刻触发低电通知”的问题。

但 USB 供电时，ADC 读到的是电池/充电电路节点电压，不等价于真实剩余电量。充电路径、负载变化、限流、电池是否接入都会让这个节点出现高压、无效样本、有效/无效交替或快速上升。继续把它当作普通放电电池百分比，会导致右上角电量图标横跳。

第一版不依赖新增硬件引脚；如果后续确认板子有 VBUS、CHG 或 PMIC 状态输出，应将软件推断替换为真实硬件来源。

## 目标

- USB 供电时右上角电量显示稳定，不随不可信 ADC 百分比横跳。
- 低电通知、总览页、电量颜色和宠物 mood 继续使用同一份稳定电量快照。
- 不把 USB 充电/外接供电状态误显示为 100% 满电。
- 不因为 USB 场景下的异常 ADC 样本触发低电通知或低电情绪。
- 真实电池低电时仍能进入 `LOW` / `CRITICAL`，不能被外接供电保护逻辑吞掉。

## 非目标

- 不追求手机级精确 SOC。
- 不显示精确百分比文案。
- 不在第一版新增必须依赖的硬件检测引脚。
- 不修改通用 `LvglDisplay` 的全局电池逻辑，除非小新板级覆盖无法消除双重读数。

## 设计原则

电量模块应成为深模块：调用方只消费稳定快照，不需要理解 ADC 噪声、USB 推断、滞回和异常样本。UI、通知、总览页和宠物 mood 不应分别判断 `voltage_mv` 或 `level <= 20`。

核心原则：

- ADC 估算值和 UI 显示值分离。
- 电池状态和供电来源分离。
- 不可信样本不应直接改变显示百分比。
- 外接供电/未知状态下不触发低电边沿事件。

## 状态模型

保留现有电池健康状态：

```c
typedef enum {
  XIAOXIN_BATTERY_STATE_UNKNOWN = 0,
  XIAOXIN_BATTERY_STATE_NORMAL,
  XIAOXIN_BATTERY_STATE_LOW,
  XIAOXIN_BATTERY_STATE_CRITICAL,
} xiaoxin_battery_state_t;
```

新增供电来源：

```c
typedef enum {
  XIAOXIN_BATTERY_POWER_BATTERY = 0,
  XIAOXIN_BATTERY_POWER_EXTERNAL,
  XIAOXIN_BATTERY_POWER_UNKNOWN,
} xiaoxin_battery_power_source_t;
```

扩展快照：

```c
typedef struct {
  xiaoxin_battery_state_t state;
  xiaoxin_battery_power_source_t power_source;
  int estimated_percent;
  int smoothed_voltage_mv;
  int display_percent;
  uint8_t display_level;
  bool percent_reliable;
  bool low_edge;
  bool critical_edge;
  bool recovered_edge;
} xiaoxin_battery_snapshot_t;
```

字段语义：

- `estimated_percent`：内部 ADC 粗略估算，只用于状态机和诊断。
- `display_percent`：UI 可用的稳定百分比，外接/未知时来自锁定值或固定值。
- `display_level`：UI 推荐使用的稳定档位，范围 0 到 4。
- `percent_reliable`：表示当前百分比是否可当作电池放电估算使用。
- `power_source`：调用方判断外接供电、未知或电池供电的唯一入口。

## 数据流

目标数据流：

```text
ADC 样本
  -> 样本质量判断
  -> 供电来源推断
  -> 电池状态机
  -> 显示快照
  -> 右上角浮层 / 通知中心 / 总览页 / 宠物 mood
```

板级代码继续负责读 ADC，电量状态模块负责把不稳定输入变成稳定快照。UI 只消费 `xiaoxin_battery_snapshot_t`。

## 样本质量判断

将单次输入分为：

- 有效电池样本：`3300mV <= voltage_mv <= 4400mV`。下界对齐 `xiaoxin_battery_percent_from_mv()` 的 0% 映射点（3300mV），低于此值在百分比曲线上已无区分度。
- 极低电压样本：`0mV < voltage_mv < 3300mV`。可能是严重过放的电池或 ADC 噪声，不直接当作正常放电电池使用，但也不清空状态。
- 高压不可信样本：`voltage_mv > 4400mV`。
- 无效样本：读取失败或 `voltage_mv <= 0`。

高压不可信样本不表示满电；它更可能表示 USB/充电路径影响了 ADC 节点。

**关键认知**：USB 稳态充电时 Li-Po 电池节点电压通常在 4100–4300mV，这落在"有效电池样本"范围内。因此**不能仅靠 `> 4400mV` 检测外接供电**，必须结合电压变化趋势和持续时间（见下一节）。

有效电池样本继续进入 EMA 平滑：

```text
smoothed = smoothed * 0.85 + raw * 0.15
```

高压、极低、无效样本不更新 EMA，不进入状态机，只更新样本质量计数和供电来源推断。

## 供电来源推断

在没有真实 VBUS/CHG 信号时，使用保守启发式。所有推断都需要**确认计数**（连续满足条件 N 次后才切换），进入和退出同样谨慎。

### 进入 EXTERNAL 的条件（满足任一 + 确认计数 ≥ 3 次）

**条件 A — 高压持续**：连续 3 次采样 `voltage_mv > 4400mV`，每次间隔约 1 秒。

**条件 B — 快速上升**：在 60 秒窗口内，`smoothed_voltage_mv` 上升超过 **300mV**，且最终值 **> 4050mV**。这捕获 USB 插入瞬间的充电爬升——电压从 3700mV 跳到 4200mV 的过程。

**条件 C — 稳态高电压**：`smoothed_voltage_mv` 持续 **> 4080mV** 超过 **120 秒**，且期间未见正常的放电波动（峰值-谷值 < 50mV 意味着没有真实负载变化，更像外接供电维持的高平台）。

**条件 D — 样本交替**：在 10 秒窗口内，有效/高压/无效样本类型交替 ≥ **3 次**。这表示 ADC 节点被充电路径干扰。

### 进入 UNKNOWN 的条件

- ADC 连续 **10 次**读取失败或返回 `voltage_mv <= 0`（约 10 秒无有效数据）。
- 或 `power_source == EXTERNAL` 状态下，所有样本类型（高压/有效/无效）在 30 秒内都无法形成稳定判断。

### 退出到 BATTERY 的条件

- `power_source == EXTERNAL`：需要连续 **20 秒**的有效电池样本（`3300–4400mV`），且 `smoothed_voltage_mv < 4080mV`，且电压波动正常（峰值-谷值 ≥ 50mV / 60s 窗口）。
- `power_source == UNKNOWN`：需要连续 **10 秒**的有效电池样本，且满足上述正常放电特征。

### 状态保持与防抖

- **进入确认**：任何 `BATTERY → EXTERNAL` 或 `BATTERY → UNKNOWN` 的候选条件需要连续满足 **3 次**采样（约 3 秒）才执行切换。
- **最小保持**：`EXTERNAL` 至少保持 **30 秒**，避免 USB 接触或充电波动导致闪烁。
- **UNKNOWN 显示保持**：进入 `UNKNOWN` 后的前 **10 秒**，`display_level` 维持上一个已知值；超时后才降为灰色 0/1 档，避免短暂 ADC 噪声引起视觉闪烁。

### 边沿事件约束

供电来源从 `EXTERNAL` 或 `UNKNOWN` 回到 `BATTERY` 时：
- `display_level` 可以**立即**反映当前真实电池状态（因为确实已经回到电池供电）。
- `low_edge` / `critical_edge` / `recovered_edge` 必须**重新满足状态机的持续确认规则**后才能触发，不能因为拔掉 USB 瞬间的电压跌落而误触发。
- 低电通知同样需要重新确认。

### 硬件信号 fallback

如果未来接入真实硬件信号：

- VBUS 存在时直接进入 `EXTERNAL`（无需确认计数）。
- CHG/PMIC 可用时可进一步区分充电中、满电、外接无电池。
- 软件推断只作为硬件信号缺失时的 fallback。

## 电池状态机

现有 `NORMAL` / `LOW` / `CRITICAL` 的阈值、滞回和持续时间规则继续保留，但只在 `power_source == BATTERY` 且样本可信时允许产生低电或恢复边沿。

外接供电或未知时：

- 不触发 `low_edge`。
- 不触发 `critical_edge`。
- 不触发 `recovered_edge`。
- 不新增或更新低电通知。
- 保留最近稳定电池状态作为诊断参考，但 UI 以 `power_source` 为主。

真实低电池场景仍按现有规则进入 `LOW` / `CRITICAL`：

- `NORMAL -> LOW`：估算电量低于阈值并持续确认。
- `LOW -> CRITICAL`：估算电量低于临界阈值并持续确认。
- `LOW/CRITICAL -> NORMAL`：需要超过恢复阈值并持续确认。

## 显示快照

`display_level` 由模块统一生成，**含滞回防止边界抖动**：

```text
BATTERY + NORMAL:
  display_percent >= 70%  → level 4  （降到 65% 才回 level 3）
  display_percent >= 40%  → level 3  （降到 35% 才回 level 2）
  display_percent >= 15%  → level 2  （降到 10% 才回 level 1）
  display_percent <  15%  → level 1  （升到 20% 才回 level 2）

BATTERY + LOW:
  固定为 1 档，警告色。

BATTERY + CRITICAL:
  固定为 0 档，严重警告色。

EXTERNAL:
  固定为 4 档，正常色，可配合充电/插电图标。
  （如果后续有充电图标资源，改为 level 4 + 闪电符号）

UNKNOWN:
  进入 UNKNOWN 的前 10 秒保持上一个 display_level（避免短暂噪声闪灰）。
  10 秒后固定为 0 档，灰色，不使用红色/警告色。
```

右上角浮层优先使用 `display_level`，而不是每次按 `estimated_percent` 计算连续宽度。这样即便内部估算百分比有小幅变化，图标也不会横跳。

## UI 行为

### 右上角电量浮层

- 电池供电：显示稳定档位和状态颜色。
- 外接供电：显示固定插电/充电样式，颜色正常，不显示红色。
- 未知：显示灰色，不弹低电提示。

第一版如果图标资源有限，可以先不新增闪电图标，只将外接供电显示为固定 4 档正常色。

### 通知中心

- 只有 `power_source == BATTERY` 且状态稳定为 `LOW` 或 `CRITICAL` 时才展示低电通知。
- `EXTERNAL` 或 `UNKNOWN` 移除低电通知。
- 文案继续使用定性描述，不出现精确百分比。

### 总览页

总览页根据 `power_source` 和 `state` 显示定性文案：

```text
BATTERY + NORMAL:   电量正常
BATTERY + LOW:      电量偏低
BATTERY + CRITICAL: 请尽快充电
EXTERNAL:           外接供电中
UNKNOWN:            电量状态未知
```

### 宠物 mood

- 只响应电池供电下的 `low_edge`、`critical_edge` 和 `recovered_edge`。
- 外接供电和未知状态不触发低电情绪。
- 从外接供电回到电池供电后，必须重新满足持续确认规则才触发低电情绪。

## 接入点

需要修改的主要位置：

- `xiaoxin_battery_state.h`：扩展供电来源和快照字段。
- `xiaoxin_battery_state.c`：增加样本质量判断、供电来源推断、显示快照生成。
- `esp32-s3-touch-lcd-1.46.cc`：
  - `RefreshBatterySnapshotLocked()` 继续读取 ADC，但只把原始电压交给状态模块。
  - `ApplyBatteryOverlayLevel()` 改为使用 `display_level` 和 `power_source`。
  - `SyncLowBatteryNotificationLocked()` 加上 `power_source == BATTERY` 条件。
  - `BuildOverviewState()` 传递供电来源或直接传递更丰富的电量状态。
- `xiaoxin_overview_model.*`：增加外接供电和未知文案。

## 测试策略

新增或扩展 `xiaoxin_battery_state_test.c`：

**样本质量与供电来源推断**：
- `4500mV` 高压样本不更新 EMA，不映射为 100% 满电。
- `2000mV` 极低样本不更新 EMA，不进入状态机。
- `3900 -> 4500 -> 4100 -> 4500` 不导致 `display_level` 来回跳。
- 连续 3 次 `> 4400mV` 进入 `EXTERNAL`（确认计数生效）。
- 单次 `> 4400mV` 尖峰不进入 `EXTERNAL`（确认计数拦截噪声）。
- 60 秒内从 3700mV 升至 4200mV（上升 500mV）进入 `EXTERNAL`（快速上升检测）。
- `smoothed_voltage_mv` 持续 > 4080mV 超过 120 秒进入 `EXTERNAL`（稳态高电压检测）。
- 4200mV 稳态（USB 充电典型值）不误判为 `BATTERY + NORMAL` 满电。

**状态保持与防抖**：
- 外接供电状态下不产生 `low_edge` / `critical_edge` / `recovered_edge`。
- 外接供电状态下低电通知应被移除或保持不显示。
- 从外接供电回到电池供电需要连续 20 秒有效样本 + 正常放电特征。
- 从 `UNKNOWN` 回到 `BATTERY` 需要连续 10 秒有效样本。
- `EXTERNAL` 最小保持 30 秒，即使条件消失也不提前退出。

**边沿事件时机**：
- 拔掉 USB 后 `display_level` 可以立即反映电池实际状态。
- 拔掉 USB 后 `low_edge` / `critical_edge` 必须重新满足持续确认才触发。
- 真实低电池样本持续存在时仍能进入 `LOW`。

**显示快照**：
- `display_percent >= 70%` → `display_level = 4`，跌到 65% 才降为 3（滞回生效）。
- `UNKNOWN` 前 10 秒保持上一个 `display_level`，超时才降为 0 档灰色。

保留路径测试：

- UI 不直接使用 `estimated_percent` 计算右上角连续宽度。
- 通知中心不直接判断 `estimated_percent <= 20`。
- 宠物 mood 只响应状态机边沿。

## 分阶段落地

### 阶段一：软件推断和稳定显示

- 扩展状态模型和快照。
- 增加外接/未知推断。
- 右上角浮层使用 `display_level`。
- 通知和宠物 mood 只响应电池供电下的稳定状态。

### 阶段二：总览页文案完善

- 增加 `外接供电中` 和 `电量状态未知`。
- 确保总览页不显示 ADC 精确百分比。

### 阶段三：硬件信号接入

- 确认是否有 VBUS、CHG 或 PMIC 状态脚。
- 如果存在，新增板级电源状态读取 adapter。
- 让电量状态模块优先使用真实硬件状态，软件推断作为 fallback。

## 实现进度（2026-06-22）

当前分支：`codex/xiaoxin-pet-mood-system`

本版已经完成：

- 电量快照扩展：新增 `power_source`、`display_percent`、`display_level`、`percent_reliable`。
- 样本质量判断：有效样本、极低电压、无效样本、高压不可信样本分流处理；不可信样本不进入 EMA 和电池状态机。
- USB/外接供电推断：支持高压持续、快速上升、稳态高电压低波动、样本类型交替，并统一使用确认计数。
- `UNKNOWN` 和 `EXTERNAL` 状态管理：包含 UNKNOWN 显示保持、EXTERNAL 30 秒最小保持、连续有效样本退出确认。
- 正常放电特征收口为 60 秒窗口，避免旧的峰谷差长期残留导致误退出外接供电。
- UI 接入：右上角电量浮层使用稳定 `display_level`；外接供电使用正常色，未知状态使用灰色。
- 低电量通知和宠物情绪：只在确认 `BATTERY` 供电时响应低电/恢复边沿。
- 总览页：增加外接供电和未知电量状态的定性文案。
- LOW/CRITICAL 显示规则：确认 `BATTERY + LOW` 固定为 level 1，确认 `BATTERY + CRITICAL` 固定为 level 0，包含 steady-state 更新路径。

本版验证结果：

- `xiaoxin_battery_level_test`：通过。
- `xiaoxin_battery_state_test`：通过。
- `xiaoxin_system_overlay_test`：通过。
- `xiaoxin_overview_model_test`：通过。
- Python 路径测试：`28 passed`。
- `rg` 审计确认：
  - `ApplyBatteryOverlayLevel()` 使用 `display_level`。
  - `SyncLowBatteryNotificationLocked()` 和 `SyncPetMoodDeviceStateLocked()` 检查 `power_source == XIAOXIN_BATTERY_POWER_BATTERY`。
  - Xiaoxin UI 路径没有继续用 `estimated_percent <= 20` 或 `level <= 20` 作为低电来源。

当前限制：

- 当前 shell 中没有 `idf.py`，因此本地尚未运行 ESP-IDF 固件构建；需要在加载 ESP-IDF 环境后补跑 `idf.py build`。

## 成功标准

- 插 USB 后右上角电量不再在多档之间横跳（包括稳态充电电压 4100–4300mV 场景）。
- USB 供电时不会误弹低电通知。
- USB 供电时不会触发宠物低电情绪。
- ADC 高压样本（> 4400mV）和稳态高电压（> 4080mV 持续）都不会被显示为普通满电。
- ADC 偶发噪声尖峰不会导致供电来源切换（确认计数拦截）。
- 拔掉 USB 后，`display_level` 立即反映真实电量；低电通知和情绪需重新确认后才触发。
- 真实低电池仍能被识别并进入低电状态。
- `display_level` 在各档位边界处不发生抖动（滞回生效）。
