#ifndef _DOORBELL_MQTT_H_
#define _DOORBELL_MQTT_H_

#include <string>
#include <atomic>

#include <esp_event.h>
#include <mqtt_client.h>

// 门铃 MQTT 客户端。
//
// 设备空闲/待机时 WebSocket 会断开，服务器就无法主动把消息送到设备。门铃是一条
// 极省电的常驻 MQTT 连接，开机联网后一直挂着，专门用来"被叫醒"：服务器往
// device/{id}/notification 发一条很小的 {"type":"wake"} 消息，设备收到后把
// WebSocket 接上，真正的语音/通知内容仍走 WebSocket 下发。
//
// 本类完全自包含，直接使用 ESP-IDF 原生 esp-mqtt（mqtt_client.h），不依赖项目里
// 走 UDP 音频的那套 MQTT 协议栈，二者互不干扰：
//   - 连接前登记遗嘱（LWT）：device/{id}/status = "offline"（qos1, retained），
//     设备异常掉线时由 broker 自动代发，服务器据此及时判定离线。
//   - 连上后发布 device/{id}/status = "online"（qos1, retained）覆盖遗嘱，
//     并订阅 device/{id}/notification。
//   - 收到 {"type":"wake"} 且设备空闲时，反向建立 WebSocket 语音通道。
//   - 断线后由底层 esp-mqtt 客户端按内置策略自动重连。
class DoorbellMqtt {
public:
    DoorbellMqtt();
    ~DoorbellMqtt();

    // 启动常驻门铃连接。device_id 通常为设备 MAC；credential 为连接凭证
    // （开发期 broker 允许匿名，可为空）。endpoint 形如 "host:port"，留空则用
    // 内置默认地址。多次调用只有首次生效。
    void Start(const std::string& device_id,
               const std::string& credential,
               const std::string& endpoint = "");

    bool IsConnected() const { return connected_.load(); }

private:
    void OnConnected();
    void OnMessage(const std::string& payload);
    static void MqttEventHandler(void* handler_args, esp_event_base_t base,
                                 int32_t event_id, void* event_data);

    std::atomic<bool> connected_{false};
    std::atomic<bool> started_{false};

    esp_mqtt_client_handle_t client_ = nullptr;

    std::string device_id_;
    std::string credential_;
    std::string broker_host_;
    int broker_port_ = 1883;

    std::string status_topic_;        // device/{id}/status
    std::string notification_topic_;  // device/{id}/notification
};

#endif // _DOORBELL_MQTT_H_
