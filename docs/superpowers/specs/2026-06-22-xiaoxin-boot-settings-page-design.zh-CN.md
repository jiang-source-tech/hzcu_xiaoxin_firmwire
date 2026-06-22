# 小芯 BOOT 长按设置页设计规格

> 状态：待评审，已根据首轮评审修订
> 日期：2026-06-22
> 设备：Waveshare ESP32-S3 Touch LCD 1.46 小芯/泡泡宠物固件
> 来源：`docs/xiaoxin-feature-roadmap.zh-CN.md` 的 P2「设置页」

## 1. 背景

当前固件已经具备宠物首页、通知中心、总览页、系统浮层、电量读取和电源控制能力。后续需要补齐“本机可配置能力”，让用户不依赖重新烧录或 Web 配置面板，也能在设备上完成高频设置。

设置页入口已确定为：

- BOOT 键长按进入设置界面，作为本机设置主入口。
- BOOT 键短按继续保留现有聊天/启动阶段配网语义。
- 电源键继续专注电源控制，不承载设置入口。

代码现状中 `BOOT_BUTTON_GPIO` 的长按回调已经预留给系统/设置行为，适合作为第一阶段设置入口。

## 2. 目标

第一版设置页聚焦目标硬件上真实有意义、低输入成本的本机配置：

- 让用户在正常空闲状态下通过 BOOT 长按稳定进入设置界面。
- 提供亮度、Wi-Fi 状态/重新配网、睡眠/省电开关、关于设备等第一版设置项。
- 亮度变更通过现有 Backlight API 写入现有 `Settings`/NVS 存储，重启后保持。
- 设置页打开时不破坏现有 Home、Notifications、Overview 的上下滑分页体验。
- 不改变 BOOT 短按聊天/启动配网语义，也不改变 PWR 长按关机/休眠语义。
- 对音量、静音、提示音、震动等依赖硬件能力的设置做能力门控：目标板不确认具备对应硬件时不显示。

## 3. 非目标

第一版不做这些事情：

- 不在小圆屏上输入 Wi-Fi 密码、服务器地址、课程表、提醒内容等复杂文本。
- 不实现完整 Web 配置面板；Web 配置面板仍作为独立 P2 方向。
- 不实现主题资源管理、宠物资源上传或动画商店。
- 不把设置页塞进现有卡片分页的 Home / Notifications / Overview 序列。
- 不改变通知中心、总览页或宠物情绪系统的数据模型。
- 不为 Waveshare ESP32-S3 Touch LCD 1.46 默认显示音量、静音、提示音或震动设置；这些仅在硬件能力明确存在时作为跨板扩展项。

## 4. 交互设计

### 4.1 入口

BOOT 键长按 2 秒触发设置页。

触发规则：

- `kDeviceStateIdle`：BOOT 长按打开设置页，这是第一版必须保证的主路径。
- `kDeviceStateStarting`：BOOT 短按仍进入 Wi-Fi 配网；第一版不要求启动阶段响应 BOOT 长按设置入口，避免和初始化流程冲突。
- `kDeviceStateConnecting`、`kDeviceStateListening`、`kDeviceStateSpeaking`：不打开设置页，避免覆盖或打断活跃对话。用户应先用现有 BOOT 短按语义停止/中断对话，再长按进入设置。
- `kDeviceStateWifiConfiguring`、`kDeviceStateAudioTesting`、`kDeviceStateActivating`、`kDeviceStateUpgrading`、`kDeviceStateFatalError`：第一版不打开设置页，只保留现有状态流程。
- 设置页已打开时：BOOT 短按关闭设置页，返回打开前页面；BOOT 长按保持幂等，不重复创建设置 UI。

### 4.2 BOOT 短按事件分发

设置页关闭行为必须在现有 BOOT 单击处理器内优先拦截，而不是依赖 LVGL 事件捕获或额外注册更高优先级按钮回调。

目标板当前 BOOT 单击处理器位于 `esp32-s3-touch-lcd-1.46.cc` 的 `BUTTON_SINGLE_CLICK` 回调中，现有顺序是：

```text
Starting -> EnterWifiConfigMode()
其他状态 -> ToggleChatState()
```

实施时应调整为：

```text
如果 SettingsOverlay 已打开 -> CloseSettingsOverlay(); return
如果 Starting -> EnterWifiConfigMode(); return
否则 -> ToggleChatState()
```

这样可以保证设置页打开时 BOOT 短按不会误触发聊天切换或启动阶段配网。

### 4.3 页面层级

设置页作为独立系统层显示在宠物首页和卡片分页之上，不参与 `xiaoxin_card_pager_t` 的三页状态机。

建议页面层级：

```text
Home / Notifications / Overview
  ↓ BOOT 长按
Settings Overlay
  ├─ 设置列表
  ├─ 单项编辑页
  └─ 确认/提示状态
```

设置层打开时：

- 暂停外层卡片分页手势处理，避免上下滑同时切页和滚动设置。
- 系统浮层可继续显示，但需要移到前景或避开设置标题区。
- 宠物动画可以继续运行，也可以被半透明遮罩弱化；第一版推荐暗色遮罩，突出设置层。

### 4.4 导航方式

设置首页采用纵向列表，每次显示 4 到 5 个设置项。

基础手势：

- 上下滑：滚动设置列表。
- 点击设置项：进入单项编辑页或执行动作。
- 从单项编辑页点击返回项或 BOOT 短按：返回设置列表。
- 在设置首页 BOOT 短按：关闭设置页，返回打开前页面。

如果后续需要无触摸兜底，可以增加 BOOT 短按移动焦点、BOOT 长按确认的按键模式，但第一版不作为必做项。

### 4.5 设置项形式

目标板第一版必须实现的设置项：

| 设置项 | 形式 | 保存/动作策略 |
| --- | --- | --- |
| 亮度 | 滑条或三档按钮：低/中/高 | 通过 `Backlight::SetBrightness(brightness, true)` 即时生效，并写入 `Settings("display")` 的 `brightness` |
| Wi-Fi | 状态行 + 重新配网按钮 | 点击重新配网后关闭设置页并进入现有 `EnterWifiConfigMode()` 流程 |
| 省电/睡眠 | 开关 | 第一版只映射现有省电开关，不新增可变睡眠时长；目标板接入省电 timer 前不显示 |
| 关于设备 | 只读信息页 | 不保存 |

硬件能力存在时才显示的扩展项：

| 设置项 | 显示条件 | 集成策略 |
| --- | --- | --- |
| 音量 | 板级声明具备可听音频输出 | 复用 `AudioCodec::SetOutputVolume()` 和 `Settings("audio")` 的 `output_volume` |
| 静音 | 板级声明具备可听音频输出 | 第一版不新增 `audio.muted`；如需要静音，作为音量 0 的 UI 快捷行为处理 |
| 提示音 | 板级声明具备可听音频输出且已有提示音播放路径 | 不在 Waveshare 1.46 默认显示 |
| 震动 | 板级声明具备震动马达或触觉反馈硬件 | 不在 Waveshare 1.46 默认显示 |

说明：Waveshare ESP32-S3 Touch LCD 1.46 当前使用 `NoAudioCodecSimplex`。即使通用 `AudioCodec` 抽象提供音量字段，也不能默认假设该目标板存在用户可感知的扬声器输出。因此目标板第一版设置页不显示音量、静音、提示音设置，除非实现时补充明确的板级能力判断。

### 4.6 关于设备页内容

关于设备页至少显示：

- 固件版本：来自 `esp_app_get_description()` 的 `version`。
- 项目名或应用名：来自 `esp_app_get_description()` 的 `project_name`。
- 设备型号：`Waveshare ESP32-S3 Touch LCD 1.46`。
- 板级能力摘要：触摸、背光、电量 ADC、Wi-Fi、音频输出能力是否启用。
- 编译日期和时间：来自 `esp_app_get_description()` 的 `date` / `time`。

如果字段过多，一屏只显示 3 到 4 行，通过上下滑查看剩余内容。

## 5. 视觉设计

设置页沿用当前“小芯卡片 UI”的深色科技风：

- 背景：深色半透明遮罩，保留一点宠物首页氛围。
- 主容器：圆角玻璃卡片，蓝色细边框。
- 标题：`设置`，放在顶部，字号略大于普通卡片标题。
- 列表项：图标 + 名称 + 当前值/状态 + 右箭头。
- 当前状态：使用蓝色表示可操作，橙色/红色表示需要注意。

圆屏约束：

- 每屏不要超过 5 行。
- 文案尽量短，例如 `亮度`、`Wi-Fi`、`省电`、`关于`。
- 不使用复杂表格或多列布局。
- 单项编辑页只呈现一个主要控件，避免误触。

## 6. 架构设计

### 6.1 模块边界

建议新增一个本机设置模型模块，而不是把设置逻辑直接写进按钮回调。

推荐模块：

```text
xiaoxin_settings_model
  - 定义设置项、当前值、可选项、保存/读取接口
  - 负责和 Settings/NVS 交互
  - 负责判断板级能力是否允许显示某个设置项
  - 不依赖 LVGL

目标板设置 UI 层
  - Waveshare 1.46 当前具体落点可以是 esp32-s3-touch-lcd-1.46.cc 内的 PaopaoPetDisplay
  - 跨板设计应描述为 Display/LvglDisplay/LcdDisplay 派生显示层
  - 创建设置页对象，渲染列表、开关、滑条和提示
  - 处理触摸命中和动画

CustomBoard / 按键层
  - BOOT 长按在允许状态下触发打开设置页
  - BOOT 短按在设置页打开时优先关闭设置页
  - 设置页关闭时继续保持原有聊天/配网行为
```

说明：`PaopaoPetDisplay` 是目标板文件中的真实类名，不是全项目通用显示抽象。文档和实现注释中应避免把它误写成所有开发板都存在的类。

### 6.2 设置页状态

UI 层可以维护一个轻量状态机：

```c
typedef enum {
  XIAOXIN_SETTINGS_VIEW_CLOSED = 0,
  XIAOXIN_SETTINGS_VIEW_LIST,
  XIAOXIN_SETTINGS_VIEW_EDIT_BRIGHTNESS,
  XIAOXIN_SETTINGS_VIEW_WIFI,
  XIAOXIN_SETTINGS_VIEW_POWER_SAVE,
  XIAOXIN_SETTINGS_VIEW_ABOUT,
} xiaoxin_settings_view_t;
```

核心字段建议：

```c
typedef struct {
  xiaoxin_settings_view_t view;
  uint8_t selected_index;
  int16_t scroll_y;
  bool dirty;
  bool saving;
} xiaoxin_settings_ui_state_t;
```

`dirty` 表示当前编辑值尚未确认写入；`saving` 用于避免重复点击造成多次写入。

### 6.3 与现有页面状态机的关系

设置页不扩展 `xiaoxin_card_page_t`。

原因：

- Home / Notifications / Overview 是日常信息分页。
- Settings 是系统级覆盖层，语义更像手表侧键呼出的系统菜单。
- 如果把设置页加入上下滑分页，会增加误触概率，也会让通知/总览手势变复杂。

设置层关闭后，外层仍保持打开前页面。

### 6.4 与聊天/语音状态的关系

第一版采用“空闲态才允许进入设置”的策略。

- Idle：允许打开设置。
- Connecting / Listening / Speaking：不打开设置，不暂停、不覆盖、不自动中断对话。
- 用户要进入设置时，应先使用现有 BOOT 短按语义结束或中断当前聊天流程，再长按 BOOT 进入设置。

这样可以避免设置页和语音状态机互相抢占，也避免在说话/聆听时出现“UI 打开了但音频状态仍在后台变化”的尴尬半状态。

### 6.5 设置页打开期间的通知处理

设置页打开期间，底层通知模型仍然更新，但通知中心 UI 不主动抢占设置页。

具体规则：

- 低电量、Wi-Fi 断开、OTA 等状态型事件继续进入通知模型。
- 设置页不弹出通知卡片、不自动跳转到 Notifications。
- 系统浮层中的网络/电量状态继续更新；低电量颜色等关键状态仍可见。
- 设置页关闭后，用户再进入 Notifications 时看到最新通知列表。
- 不丢弃通知事件，也不阻塞事件源。

这让设置页保持稳定，同时不牺牲状态驱动通知的正确性。

## 7. 数据流

### 7.1 打开设置页

```text
BOOT 长按
  -> CustomBoard 回调
  -> 检查 Application::GetDeviceState() 是否为 kDeviceStateIdle
  -> 检查 SettingsOverlay 是否已打开
  -> 目标显示层 OpenSettingsOverlay()
  -> 设置模型读取当前值和板级能力
  -> UI 渲染设置列表
```

### 7.2 关闭设置页

```text
BOOT 短按
  -> CustomBoard 现有 BUTTON_SINGLE_CLICK 回调
  -> 优先检查 SettingsOverlay 是否已打开
  -> CloseSettingsOverlay()
  -> return，阻止 EnterWifiConfigMode() / ToggleChatState()
```

### 7.3 修改亮度

```text
点击/滑动亮度设置
  -> UI 更新临时值
  -> 设置模型夹紧到 0..100
  -> Board::GetInstance().GetBacklight()->SetBrightness(value, true)
  -> Backlight 写入 Settings("display").SetInt("brightness", value)
  -> UI 显示新亮度
```

亮度设置必须复用现有 `Backlight::SetBrightness()` / `Backlight::RestoreBrightness()` 语义，不另写一套 NVS 保存逻辑。

### 7.4 重新配网

```text
点击 Wi-Fi 重新配网
  -> 设置页显示确认提示
  -> 用户确认
  -> 关闭设置页
  -> 调用现有 EnterWifiConfigMode()
```

第一版不在设置页内完成 Wi-Fi 凭据输入。

### 7.5 可选音量集成

音量设置不属于 Waveshare 1.46 第一版必做项。若后续用于具备可听音频输出的板级目标：

```text
点击/滑动音量设置
  -> UI 更新临时值
  -> 设置模型夹紧到 0..100
  -> Board::GetInstance().GetAudioCodec()->SetOutputVolume(value)
  -> AudioCodec 写入 Settings("audio").SetInt("output_volume", value)
```

音量设置必须复用 `AudioCodec::SetOutputVolume()`，不要新增并行的 `audio.volume` 键。

## 8. 存储设计

第一版必须复用现有键，不引入点分隔的新键名。

| 设置 | Settings 命名空间 | 键名 | 写入路径 |
| --- | --- | --- | --- |
| 亮度 | `display` | `brightness` | `Backlight::SetBrightness(value, true)` |
| 音量，硬件支持时可选 | `audio` | `output_volume` | `AudioCodec::SetOutputVolume(value)` |
| 省电/睡眠开关 | `wifi` | `sleep_mode` | 沿用现有省电开关语义 |

不新增这些键：

- `display.brightness`
- `audio.volume`
- `audio.muted`
- `vendor.prompt_sound`
- `vendor.vibration`

如果后续要加入静音、提示音、震动或可变睡眠时长，应另写小规格补充键名和迁移策略。当前规格不定义这些新增持久化字段。

默认值策略：

- 亮度默认使用 `Backlight::RestoreBrightness()` 中 `Settings("display")` 的 `brightness`，默认值为 75，过低时夹到 10。
- 音量扩展项默认使用 `AudioCodec::Start()` 中 `Settings("audio")` 的 `output_volume`，默认值来自 `AudioCodec::output_volume_`。
- 省电/睡眠开关默认沿用现有 `Settings("wifi")` 的 `sleep_mode` 读取逻辑。

## 9. 省电/睡眠协调

第一版只做“省电开关”，不做可配置睡眠时长。

原因：

- 现有代码已有 `Display::SetPowerSaveMode(bool)` / `LvglDisplay::SetPowerSaveMode(bool)` 这类即时显示省电入口。
- 现有 `sleep_timer` / `power_save_timer` 读取 `Settings("wifi")` 的 `sleep_mode` 作为是否启用省电的开关。
- 可变睡眠时长需要和触摸、按键唤醒、动画暂停、网络任务协同，范围比设置页第一版更大。

第一版设置页的省电项应只读写现有 `Settings("wifi")` 的 `sleep_mode`，并在必要时调用现有显示层 `SetPowerSaveMode()` 做即时反馈。可变睡眠时长作为后续独立切片。

实施切片 3 前必须先确认目标板是否已经初始化 `PowerSaveTimer`、`SleepTimer` 或等价省电调度器。当前通用 timer 类会读取 `Settings("wifi").GetBool("sleep_mode", true)`，但如果目标板未接入 timer，设置页不得显示一个只改 NVS、没有运行时效果的省电开关；应先接入 timer，再开放该设置项。

## 10. 错误处理

- NVS 写入失败：保留当前运行时值，显示短提示 `保存失败`，不崩溃。
- 亮度越界：设置模型夹紧到 0..100。
- Wi-Fi 重新配网触发失败：保持设置页可见，显示 `配网启动失败`。
- 设置页打开时对象创建失败：记录日志并保持原页面，不影响聊天和电源功能。
- 活跃对话状态下 BOOT 长按：不打开设置页，可以显示短提示 `请先结束对话`，也可以静默忽略；第一版推荐短提示。
- 动画运行中重复按 BOOT：忽略重复打开请求，避免重复创建 UI 对象。

## 11. 测试计划

### 11.1 纯逻辑测试

新增设置模型测试，覆盖：

- 默认值加载。
- 亮度范围夹紧。
- 板级能力门控：无音频能力时不返回音量、静音、提示音设置项。
- 省电开关读写字段为现有 `Settings("wifi")` 的 `sleep_mode`。
- NVS 写入失败时返回错误。

### 11.2 UI 路径测试

如果继续使用现有 Python 路径测试风格，可覆盖：

- Idle 状态下 BOOT 长按打开设置页。
- Listening / Speaking 状态下 BOOT 长按不打开设置页。
- 设置页打开后外层卡片分页不响应上下滑。
- 设置页打开时 BOOT 短按关闭设置页，且不会触发 `ToggleChatState()`。
- 点击亮度设置项进入单项编辑页。
- 点击 Wi-Fi 重新配网触发现有配网入口。
- 设置页打开期间注入通知事件，关闭后通知中心仍能看到该事件。

### 11.3 实机验收

- 正常 Idle 状态下 BOOT 长按 2 秒能进入设置页。
- BOOT 短按在非设置页仍能切换聊天状态。
- 启动阶段 BOOT 短按仍能进入 Wi-Fi 配网。
- 活跃聊天状态下 BOOT 长按不覆盖聊天 UI。
- 亮度设置能即时生效，重启后保持。
- Waveshare 1.46 第一版不显示无实际硬件意义的音量、静音、提示音、震动项。
- 设置页滚动、点击、返回无明显卡顿。
- PWR 长按关机/休眠行为不受影响。

## 12. 验收标准

### 12.1 第一版可交付标准

- [ ] Roadmap 中设置页入口明确为 BOOT 长按。
- [ ] Idle 状态下 BOOT 长按可以打开设置页。
- [ ] 设置页不加入 Home / Notifications / Overview 三页分页状态机。
- [ ] 设置页打开时 BOOT 短按优先关闭设置页，不触发聊天/配网逻辑。
- [ ] BOOT 短按原有聊天/启动配网语义在设置页关闭时保留。
- [ ] 实现亮度、Wi-Fi 状态/重新配网、省电开关、关于设备四个目标板第一版设置项。
- [ ] 亮度能通过现有 Backlight API 写入 NVS 并在重启后保持。
- [ ] 设置页打开时外层卡片分页手势被抑制。
- [ ] 本地测试覆盖设置模型核心逻辑和 BOOT 入口路径。

### 12.2 非第一版验收项

这些不阻塞第一版交付：

- 音量、静音、提示音设置。
- 震动设置。
- 可变睡眠超时时长。
- Web 配置面板。
- 主题/宠物资源管理。

## 13. 第一版实施切片建议

建议分三步落地：

1. 垂直最小切片：BOOT 长按打开设置页、BOOT 短按关闭设置页、显示设置列表和关于设备页，同时实现亮度设置的读取、即时生效和 NVS 保存。
2. 接入 Wi-Fi 状态/重新配网入口，并确保重新配网复用现有 `EnterWifiConfigMode()`。
3. 接入省电开关：先确认或补齐目标板的 `PowerSaveTimer` / `SleepTimer` / 等价调度器接入，再复用现有 `Settings("wifi")` 的 `sleep_mode` 和显示层省电接口，并补齐通知遮罩期间的 UI 路径测试。

这样第一版从一开始就包含至少一个真实可写设置项，避免“只有空 UI 但没有配置闭环”的假进展。
