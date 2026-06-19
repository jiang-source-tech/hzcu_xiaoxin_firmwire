# Update

## 2026-06-19 00:00:00 +08:00

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
- 总览静态数据从“标题 + 单行正文”扩展为“标题 + 主信息 + 辅助信息”：
  - 下一节课：`高数 10:10` / `教2-301 · 还有24分`。
  - 校园导航：`常用地点` / `教学楼 / 食堂 / 图书馆`。
  - 天气：`多云 26C` / `湿度72% · 东风2级`。
  - 今日待办：`2 项待办` / `实验报告 · 晚自习`。
- 通知卡片不使用 `detail` 字段，通知渲染仍只显示标题和正文。
- 总览行渲染新增第三行 detail 标签，并同步放大文本容器，降低三行文字贴底或裁切风险。
- 根据实机反馈继续放大总览卡片：宽度调整为 `270`，高度调整为 `58`，行间距调整为 `61`，整体起始位置下移到 `70`，文本容器增加到 `54px` 高，让三行信息不再挤在一起。

#### 涉及文件

- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.h`
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/xiaoxin_card_pager.c`
- `tests/xiaoxin_card_pager_test.c`
- `docs/superpowers/plans/2026-06-19-xiaoxin-overview-card-density.md`
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
