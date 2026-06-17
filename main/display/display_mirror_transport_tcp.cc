#include "display_mirror_transport.h"

#include "display_mirror_protocol.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <lwip/tcp.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

#define TAG "DisplayMirrorTcp"

namespace {

class TcpMirrorTransport final : public DisplayMirrorTransport {
public:
    TcpMirrorTransport() {
        mutex_ = xSemaphoreCreateMutex();
    }

    bool Start(uint16_t width, uint16_t height, uint16_t flags) override {
        if (mutex_ != nullptr) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
        }
        width_ = width;
        height_ = height;
        flags_ = flags;
        hello_sent_ = false;
        if (!started_) {
            started_ = xTaskCreate(ServerTaskEntry, "tft_mirror_tcp", 4096, this, 4, nullptr) == pdPASS;
            if (!started_) {
                ESP_LOGE(TAG, "failed to start TCP server task");
            }
        }
        if (mutex_ != nullptr) {
            xSemaphoreGive(mutex_);
        }
        return started_;
    }

    bool HasClient() const override {
        return client_socket_ >= 0;
    }

    bool SendPacket(uint8_t type, const void* payload, size_t payload_size) override {
        if (payload_size > 0 && payload == nullptr) {
            return false;
        }
        if (!HasClient()) {
            hello_sent_ = false;
            return false;
        }
        if (!hello_sent_ && type != TFTM_PACKET_HELLO && !MaybeSendHello()) {
            return false;
        }

        if (mutex_ != nullptr) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
        }
        const uint32_t crc = tftm_crc32(payload, payload_size);
        const tftm_packet_header_t header = tftm_make_header(type, sequence_++, payload_size, crc);
        const bool ok = WriteBytes(&header, sizeof(header)) && WriteBytes(payload, payload_size);
        if (!ok) {
            CloseClientLocked();
        }
        if (mutex_ != nullptr) {
            xSemaphoreGive(mutex_);
        }
        return ok;
    }

private:
    static void ServerTaskEntry(void* arg) {
        static_cast<TcpMirrorTransport*>(arg)->ServerTask();
    }

    void ServerTask() {
        while (true) {
            int listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            if (listen_socket < 0) {
                ESP_LOGE(TAG, "socket failed: errno %d", errno);
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }

            int yes = 1;
            setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

            sockaddr_in bind_addr = {};
            bind_addr.sin_family = AF_INET;
            bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            bind_addr.sin_port = htons(CONFIG_DISPLAY_MIRROR_TCP_PORT);

            if (bind(listen_socket, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) != 0) {
                ESP_LOGE(TAG, "bind port %d failed: errno %d", CONFIG_DISPLAY_MIRROR_TCP_PORT, errno);
                close(listen_socket);
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }

            if (listen(listen_socket, 1) != 0) {
                ESP_LOGE(TAG, "listen failed: errno %d", errno);
                close(listen_socket);
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }

            ESP_LOGI(TAG, "listening on TCP port %d", CONFIG_DISPLAY_MIRROR_TCP_PORT);
            while (true) {
                sockaddr_in source_addr = {};
                socklen_t addr_len = sizeof(source_addr);
                int client = accept(listen_socket, reinterpret_cast<sockaddr*>(&source_addr), &addr_len);
                if (client < 0) {
                    ESP_LOGW(TAG, "accept failed: errno %d", errno);
                    break;
                }

                ConfigureClient(client);
                char addr_str[INET_ADDRSTRLEN] = {};
                inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str));

                if (mutex_ != nullptr) {
                    xSemaphoreTake(mutex_, portMAX_DELAY);
                }
                CloseClientLocked();
                client_socket_ = client;
                hello_sent_ = false;
                sequence_ = 0;
                if (mutex_ != nullptr) {
                    xSemaphoreGive(mutex_);
                }

                ESP_LOGI(TAG, "client connected: %s", addr_str);
            }

            close(listen_socket);
        }
    }

    static void ConfigureClient(int client) {
        int yes = 1;
        setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

        timeval timeout = {};
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    }

    bool MaybeSendHello() const {
        auto* self = const_cast<TcpMirrorTransport*>(this);
        if (!self->HasClient()) {
            self->hello_sent_ = false;
            return false;
        }
        if (self->hello_sent_) {
            return true;
        }

        tftm_hello_payload_t hello = {
            .width = self->width_,
            .height = self->height_,
            .color_format = TFTM_COLOR_RGB565,
            .flags = self->flags_,
        };

        if (self->mutex_ != nullptr) {
            xSemaphoreTake(self->mutex_, portMAX_DELAY);
        }
        const tftm_packet_header_t header = tftm_make_header(
            TFTM_PACKET_HELLO,
            self->sequence_++,
            sizeof(hello),
            tftm_crc32(&hello, sizeof(hello))
        );
        const bool ok = self->WriteBytes(&header, sizeof(header)) && self->WriteBytes(&hello, sizeof(hello));
        if (!ok) {
            self->CloseClientLocked();
        }
        if (self->mutex_ != nullptr) {
            xSemaphoreGive(self->mutex_);
        }
        self->hello_sent_ = ok;
        return ok;
    }

    bool WriteBytes(const void* data, size_t size) const {
        const auto* bytes = static_cast<const uint8_t*>(data);
        size_t sent = 0;
        const int64_t deadline = esp_timer_get_time() + 250000 + static_cast<int64_t>(size) * 8;
        while (sent < size) {
            const int client = client_socket_;
            if (client < 0 || esp_timer_get_time() > deadline) {
                return false;
            }
            constexpr size_t kChunkSize = 1460;
            const size_t chunk = std::min(kChunkSize, size - sent);
            const int written = send(client, bytes + sent, chunk, 0);
            if (written <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    vTaskDelay(pdMS_TO_TICKS(1));
                    continue;
                }
                return false;
            }
            sent += static_cast<size_t>(written);
        }
        return true;
    }

    void CloseClientLocked() const {
        if (client_socket_ >= 0) {
            shutdown(client_socket_, SHUT_RDWR);
            close(client_socket_);
            client_socket_ = -1;
            hello_sent_ = false;
            ESP_LOGI(TAG, "client disconnected");
        }
    }

    mutable SemaphoreHandle_t mutex_ = nullptr;
    mutable int client_socket_ = -1;
    mutable uint32_t sequence_ = 0;
    uint16_t width_ = 0;
    uint16_t height_ = 0;
    uint16_t flags_ = 0;
    bool started_ = false;
    mutable bool hello_sent_ = false;
};

TcpMirrorTransport transport;

} // namespace

DisplayMirrorTransport* GetDisplayMirrorTransport() {
    return &transport;
}
