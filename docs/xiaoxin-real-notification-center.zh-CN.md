# 小新真实通知中心触发说明

> 日期：2026-06-19
> 范围：Waveshare ESP32-S3 Touch LCD 1.46 的小新通知分页卡
> 分支：`codex/xiaoxin-real-notifications`

## 目标

本轮改动把通知页从固定样板数组改成可注入的事件通知中心。通知页默认不再展示静态演示内容，只有设备状态或业务事件进入通知仓库后，通知卡片才会出现。

通知中心复用现有通知页交互：

- 下滑进入通知页。
- 上下拖动浏览通知卡。
- 左滑单条通知可清除。
- `全部清理` 清空当前通知。
- 通知清空后显示空状态。

## 数据入口

核心入口在：

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`

事件结构：

```c
typedef struct {
  xiaoxin_notification_event_type_t type;
  const char* title;
  const char* body;
  const char* tag;
  uint32_t priority;
  uint32_t ttl_ms;
} xiaoxin_notification_event_t;
```

写入通知：

```c
bool xiaoxin_card_pager_notification_upsert_event(
  xiaoxin_card_pager_t* pager,
  const xiaoxin_notification_event_t* event
);
```

移除状态型通知：

```c
bool xiaoxin_card_pager_notification_remove_event(
  xiaoxin_card_pager_t* pager,
  xiaoxin_notification_event_type_t type
);
```

`upsert` 的含义是：同一 `type` 的通知只保留一条，再次写入会更新标题、正文、标签、优先级和 TTL，不会生成重复卡片。

## 通知类型和触发方式

### 低电量

事件类型：

```c
XIAOXIN_NOTIFICATION_EVENT_LOW_BATTERY
```

当前自动触发位置：

- `PaopaoPetDisplay::UpdateStatusBar()`
- `SyncLowBatteryNotificationLocked(int level)`

触发条件：

- `NotificationBatteryLevelPercent()` 返回的电量 `<= 20`。

通知内容：

- 标题默认：`低电量`
- 标签默认：`电量`
- 正文示例：`剩余 18%，请尽快充电`

恢复条件：

- 电量回到 `> 20` 时，调用 `xiaoxin_card_pager_notification_remove_event()` 移除低电量通知。

### WiFi 断开或配网中

事件类型：

```c
XIAOXIN_NOTIFICATION_EVENT_WIFI_DISCONNECTED
```

当前自动触发位置：

- `PaopaoPetDisplay::ApplySystemOverlayNetworkStyle()`
- `SyncNetworkNotificationLocked(xiaoxin_system_overlay_network_state_t state)`

触发条件：

- `SystemOverlayNetworkState()` 返回 `XIAOXIN_SYSTEM_OVERLAY_NETWORK_DISCONNECTED`。
- 或返回 `XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONFIGURING`。

通知内容：

- 标题默认：`WiFi 断开`
- 标签默认：`网络`
- 断开正文：`WiFi 已断开，正在重新连接`
- 配网正文：`正在配网或等待连接`

恢复条件：

- 网络状态回到 `XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED` 时，移除 WiFi 通知。

### OTA 更新

事件类型：

```c
XIAOXIN_NOTIFICATION_EVENT_OTA_UPDATE
```

当前状态：

- 已在通知中心数据层预留事件类型和默认文案。
- 当前显示层尚未自动接入 OTA 检测回调。

建议触发位置：

- OTA 检测到新固件时写入该事件。
- OTA 开始下载、下载失败、等待重启时可以继续 upsert 同一类型，更新正文。

默认通知内容：

- 标题默认：`OTA 更新`
- 标签默认：`系统`
- 正文默认：`发现新固件`

建议调用示例：

```c
const xiaoxin_notification_event_t event = {
  .type = XIAOXIN_NOTIFICATION_EVENT_OTA_UPDATE,
  .title = NULL,
  .body = "发现新固件，等待升级",
  .tag = NULL,
  .priority = 0,
  .ttl_ms = 0,
};
xiaoxin_card_pager_notification_upsert_event(&card_pager, &event);
```

### 语音识别失败

事件类型：

```c
XIAOXIN_NOTIFICATION_EVENT_VOICE_RECOGNITION_FAILED
```

当前自动触发位置：

- `PaopaoPetDisplay::SetStatus(const char* status)`
- `AddVoiceFailureNotificationLocked(const char* status)`

触发条件：

- 显示状态等于 `Lang::Strings::ERROR`。
- 或状态文本包含 `Error` / `error`。

通知内容：

- 标题默认：`语音识别失败`
- 标签默认：`语音`
- 正文使用传入的错误状态文本；如果未来直接调用事件入口，默认正文为 `没听清，请再说一次`。
- 默认 TTL：`8000ms`

说明：

- 当前映射偏保守：显示层只能看到通用错误状态，所以先把错误状态作为语音失败通知入口。
- 如果后续语音识别模块提供更明确的 ASR 失败回调，应直接调用 `XIAOXIN_NOTIFICATION_EVENT_VOICE_RECOGNITION_FAILED`，避免把所有错误都归类为语音失败。

### 聊天回复

事件类型：

```c
XIAOXIN_NOTIFICATION_EVENT_CHAT_REPLY
```

当前自动触发位置：

- `PaopaoPetDisplay::SetChatMessage(const char* role, const char* content)`
- `AddChatReplyNotificationLocked(const char* content)`

触发条件：

- `role == "assistant"`。
- `content` 非空。

通知内容：

- 标题默认：`聊天回复`
- 标签默认：`聊天`
- 正文使用 assistant 的回复内容。

更新规则：

- 新的 assistant 回复会 upsert 同一类型通知，因此通知中心里只保留最新一条聊天回复。

### 定时提醒

事件类型：

```c
XIAOXIN_NOTIFICATION_EVENT_REMINDER
```

当前状态：

- 已在通知中心数据层预留事件类型和默认文案。
- 当前项目尚未把具体定时提醒模块接到该入口。

默认通知内容：

- 标题默认：`定时提醒`
- 标签默认：`提醒`
- 正文默认：`有一个提醒到时间了`

建议触发方式：

- 本地闹钟、课程提醒、日程提醒到点时写入该事件。
- 如果存在多个并行提醒，后续可以把事件模型扩展为 `type + source_id`，避免同类提醒互相覆盖。

## 优先级排序

通知中心按 `priority` 从小到大排序。默认优先级如下：

| 类型 | 默认优先级 |
|---|---:|
| 低电量 | 1 |
| WiFi 断开 | 2 |
| OTA 更新 | 3 |
| 语音识别失败 | 4 |
| 聊天回复 | 5 |
| 定时提醒 | 6 |

调用方可以传入非 0 的 `priority` 覆盖默认值。

## 清除和恢复规则

- 左滑清除：从当前通知仓库中移除被命中的可见通知。
- `全部清理`：清空当前通知仓库。
- 状态恢复：低电量恢复和 WiFi 连接恢复会主动移除对应状态通知。
- 再次触发：即使用户清除了某条通知，后续同类事件再次 upsert 时仍会重新出现。

## 容量限制

当前通知中心最多保留：

```c
#define XIAOXIN_CARD_NOTIFICATION_MAX 6
```

这与当前 6 类通知一一对应。通知标题、正文、标签会复制到 pager 内部固定缓冲区，调用方传入的临时字符串在函数返回后可以释放。

## 测试覆盖

测试文件：

- `tests/xiaoxin_card_pager_test.c`

覆盖内容：

- 通知中心初始化为空。
- 事件注入后生成真实通知。
- 同一事件类型重复注入会更新，不会重复。
- 多类型通知按优先级排序。
- 状态型通知可以按事件类型移除。
- 左滑 dismiss 和全部清理仍保持原行为。

验证命令：

```powershell
& 'D:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1'
idf.py build
```

本轮已用 ESP-IDF 构建验证通过。
