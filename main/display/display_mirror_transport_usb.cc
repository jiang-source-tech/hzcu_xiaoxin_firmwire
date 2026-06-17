#include "display_mirror_transport.h"

#include "display_mirror_protocol.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <tinyusb.h>
#include <tinyusb_default_config.h>
#include <tinyusb_cdc_acm.h>

#include <algorithm>
#include <cstring>

#define TAG "DisplayMirrorUsb"

namespace {

volatile bool s_cdc_connected = false;

void MirrorCdcLineStateChangedCallback(int, cdcacm_event_t* event) {
    s_cdc_connected = event != nullptr && event->line_state_changed_data.dtr != 0;
}

class UsbCdcMirrorTransport final : public DisplayMirrorTransport {
public:
    UsbCdcMirrorTransport() {
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
            const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
            esp_err_t err = tinyusb_driver_install(&tusb_cfg);
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(err));
                if (mutex_ != nullptr) {
                    xSemaphoreGive(mutex_);
                }
                return false;
            }
            tinyusb_config_cdcacm_t acm_cfg = {
                .cdc_port = TINYUSB_CDC_ACM_0,
                .callback_rx = nullptr,
                .callback_rx_wanted_char = nullptr,
                .callback_line_state_changed = MirrorCdcLineStateChangedCallback,
                .callback_line_coding_changed = nullptr,
            };
            err = tinyusb_cdcacm_init(&acm_cfg);
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGE(TAG, "tinyusb_cdcacm_init failed: %s", esp_err_to_name(err));
                if (mutex_ != nullptr) {
                    xSemaphoreGive(mutex_);
                }
                return false;
            }
            started_ = true;
            ESP_LOGI(TAG, "USB CDC mirror interface ready");
        }
        if (mutex_ != nullptr) {
            xSemaphoreGive(mutex_);
        }
        return MaybeSendHello();
    }

    bool HasClient() const override {
        return started_ && s_cdc_connected;
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
        tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
        if (mutex_ != nullptr) {
            xSemaphoreGive(mutex_);
        }
        return ok;
    }

private:
    bool MaybeSendHello() const {
        auto* self = const_cast<UsbCdcMirrorTransport*>(this);
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
        const tftm_packet_header_t header = tftm_make_header(TFTM_PACKET_HELLO, self->sequence_++, sizeof(hello), tftm_crc32(&hello, sizeof(hello)));
        const bool ok = self->WriteBytes(&header, sizeof(header)) && self->WriteBytes(&hello, sizeof(hello));
        tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
        if (self->mutex_ != nullptr) {
            xSemaphoreGive(self->mutex_);
        }
        self->hello_sent_ = ok;
        return ok;
    }

    bool WriteBytes(const void* data, size_t size) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        size_t sent = 0;
        const int64_t deadline = esp_timer_get_time() + 2000;
        while (sent < size) {
            if (!HasClient() || esp_timer_get_time() > deadline) {
                return false;
            }
            constexpr size_t kChunkSize = 512;
            const size_t chunk = std::min(kChunkSize, size - sent);
            const size_t queued = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, bytes + sent, chunk);
            if (queued == 0) {
                tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }
            sent += queued;
        }
        return true;
    }

    mutable SemaphoreHandle_t mutex_ = nullptr;
    uint32_t sequence_ = 0;
    uint16_t width_ = 0;
    uint16_t height_ = 0;
    uint16_t flags_ = 0;
    bool started_ = false;
    mutable bool hello_sent_ = false;
};

UsbCdcMirrorTransport transport;

} // namespace

DisplayMirrorTransport* GetDisplayMirrorTransport() {
    return &transport;
}
