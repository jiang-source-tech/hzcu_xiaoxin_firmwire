#include "doorbell_mqtt.h"
#include "application.h"
#include "settings.h"

#include <cstring>
#include <esp_log.h>
#include <cJSON.h>

#define TAG "Doorbell"

// 门铃连接保活间隔（秒）。取较长值以省电；broker 在约 1.5 倍间隔内收不到心跳即判定
// 该连接失效并代发遗嘱。
#define DOORBELL_KEEPALIVE_SECONDS 240

// 开发期默认 broker 地址（明文 1883）。可由 Start() 传入的 endpoint 覆盖。
static const char* kDefaultBrokerHost = "124.222.121.103";
static const int kDefaultBrokerPort = 1883;

DoorbellMqtt::DoorbellMqtt() {}

DoorbellMqtt::~DoorbellMqtt() {
    if (client_ != nullptr) {
        esp_mqtt_client_stop(client_);
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
    }
}

void DoorbellMqtt::Start(const std::string& device_id,
                         const std::string& credential,
                         const std::string& endpoint) {
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) {
        ESP_LOGW(TAG, "doorbell already started");
        return;
    }

    if (device_id.empty()) {
        ESP_LOGW(TAG, "device_id empty; doorbell not started");
        started_.store(false);
        return;
    }

    device_id_ = device_id;
    credential_ = credential;

    // 解析 broker 地址：优先用入参 endpoint，其次用内置默认。
    broker_host_ = kDefaultBrokerHost;
    broker_port_ = kDefaultBrokerPort;
    if (!endpoint.empty()) {
        size_t pos = endpoint.find(':');
        if (pos != std::string::npos) {
            broker_host_ = endpoint.substr(0, pos);
            broker_port_ = std::stoi(endpoint.substr(pos + 1));
        } else {
            broker_host_ = endpoint;
        }
    }

    status_topic_ = "device/" + device_id_ + "/status";
    notification_topic_ = "device/" + device_id_ + "/notification";

    std::string uri = "mqtt://" + broker_host_ + ":" + std::to_string(broker_port_);

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = uri.c_str();
    cfg.credentials.client_id = device_id_.c_str();
    cfg.credentials.username = device_id_.c_str();
    if (!credential_.empty()) {
        cfg.credentials.authentication.password = credential_.c_str();
    }
    cfg.session.keepalive = DOORBELL_KEEPALIVE_SECONDS;
    // 遗嘱（LWT）：异常掉线时 broker 代发，使服务器及时判定离线。
    cfg.session.last_will.topic = status_topic_.c_str();
    cfg.session.last_will.msg = "offline";
    cfg.session.last_will.msg_len = 7;
    cfg.session.last_will.qos = 1;
    cfg.session.last_will.retain = 1;

    client_ = esp_mqtt_client_init(&cfg);
    if (client_ == nullptr) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        started_.store(false);
        return;
    }

    esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, MqttEventHandler, this);

    ESP_LOGI(TAG, "starting doorbell MQTT to %s:%d", broker_host_.c_str(), broker_port_);
    esp_err_t err = esp_mqtt_client_start(client_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start failed: %s", esp_err_to_name(err));
    }
    // 连接失败/断线由 esp-mqtt 内置自动重连处理，无需自管定时器。
}

void DoorbellMqtt::MqttEventHandler(void* handler_args, esp_event_base_t base,
                                    int32_t event_id, void* event_data) {
    (void)base;
    DoorbellMqtt* self = static_cast<DoorbellMqtt*>(handler_args);
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
    case MQTT_EVENT_CONNECTED:
        self->connected_.store(true);
        self->OnConnected();
        break;
    case MQTT_EVENT_DISCONNECTED:
        self->connected_.store(false);
        ESP_LOGI(TAG, "doorbell disconnected; will auto-reconnect");
        break;
    case MQTT_EVENT_DATA: {
        // 仅处理门铃 topic（本连接只订阅了它，这里再按需收窄即可）。
        std::string payload(event->data, event->data_len);
        self->OnMessage(payload);
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "doorbell mqtt error event");
        break;
    default:
        break;
    }
}

void DoorbellMqtt::OnConnected() {
    // 发布在线状态（retained）覆盖遗嘱，并订阅门铃 topic。
    int mid_pub = esp_mqtt_client_publish(client_, status_topic_.c_str(), "online", 6, 1, 1);
    int mid_sub = esp_mqtt_client_subscribe(client_, notification_topic_.c_str(), 1);
    if (mid_pub < 0 || mid_sub < 0) {
        ESP_LOGW(TAG, "online publish/subscribe failed (pub=%d sub=%d)", mid_pub, mid_sub);
        return;
    }
    ESP_LOGI(TAG, "doorbell connected: online published + notification subscribed");
}

void DoorbellMqtt::OnMessage(const std::string& payload) {
    cJSON* root = cJSON_Parse(payload.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "failed to parse doorbell payload: %s", payload.c_str());
        return;
    }

    cJSON* type = cJSON_GetObjectItem(root, "type");
    // 先判空再比较，避免对缺失/非字符串字段解引用导致崩溃。
    if (!cJSON_IsString(type)) {
        ESP_LOGW(TAG, "doorbell 'type' missing or not a string; discarding");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(type->valuestring, "wake") == 0) {
        auto& app = Application::GetInstance();
        // 仅在空闲时反向建 WebSocket；正在对话则不打断。
        if (app.GetDeviceState() == kDeviceStateIdle) {
            ESP_LOGI(TAG, "doorbell 'wake' received; reverse-opening WebSocket");
            app.ToggleChatState();  // 线程安全：内部投递事件到主任务
        } else {
            ESP_LOGI(TAG, "doorbell 'wake' ignored; device not idle");
        }
    } else {
        ESP_LOGI(TAG, "doorbell type '%s' is not 'wake'; ignoring", type->valuestring);
    }

    cJSON_Delete(root);
}
