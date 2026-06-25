# 小新串口调试命令

本文档记录当前小新 1.46 固件可用的串口调试命令，以及在 VSCode ESP-IDF Monitor 里使用时需要注意的点。

## 使用方式

1. 烧录并启动固件后，打开 VSCode 的 ESP-IDF Monitor。
2. 看到提示符 `xiaoxin>` 后，输入一条命令并按回车。
3. 每次只输入一条完整命令，例如：

```text
notify_test
```

不要连续粘贴成 `notify_testnotify_test`，否则 REPL 会把它当成一条不存在的命令，并显示 `Unrecognized command`。

## 当前命令列表

| 命令 | 用途 | 成功输出 | 备注 |
| --- | --- | --- | --- |
| `notify_test` | 创建一条测试通知，并切换到通知页面。 | `notify_test: opened notification page` | 每执行一次命令，触发一次测试通知悬浮窗。 |

## `notify_test`

`notify_test` 用来快速检查通知中心和通知悬浮窗效果。

执行后固件会做这些事情：

1. 创建或更新一条测试通知。
2. 通知标题为 `Notify test`。
3. 通知正文为 `Serial debug notification`。
4. 通知标签为 `Debug`。
5. 打开通知页面。
6. 显示一次通知悬浮窗。

悬浮窗的显示时长目前约为 3 秒。到时间后，维护定时器会在下一次刷新时触发隐藏动画。

如果显示：

```text
notify_test: display is not ready
```

说明命令执行时显示对象还没有准备好，通常等固件启动完成后再输入一次即可。

## VSCode Monitor 注意事项

当前小新固件的交互控制台配置在 USB Serial/JTAG 上。VSCode 里需要选中实际连接小新设备的串口，例如之前使用的 `COM4`。

如果输入命令时出现类似提示：

```text
Warning: Writing to serial is timing out
```

一般表示 Monitor 没有连到可交互的控制台端口，或者当前选择的 console 类型不对。确认点：

1. VSCode ESP-IDF Monitor 选择的是小新设备对应的串口。
2. 固件已经重新烧录了包含串口命令的版本。
3. Monitor 里能看到 `xiaoxin>` 提示符后再输入命令。

串口日志可能会插入到命令行附近，例如电池电压日志会周期性打印。输入命令时以 `xiaoxin>` 后面的内容为准，输完一条命令就按回车。

## 其他板卡命令

仓库里的 `sensecap-watcher` 板卡代码还注册了 `reboot`、`shutdown`、`battery`、`factory_reset`、`read_mac`、`version` 等命令。这些命令属于 SenseCAP Watcher 板卡，不属于当前小新 1.46 固件；在小新 Monitor 里不要按这些命令来测试。

## 维护位置

小新串口命令注册位置：

```text
main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc
```

当前命令在 `InitializeDebugConsole()` 中注册。以后新增串口调试命令时，请同步更新本文档。
