#include "display_mirror_transport.h"

#include "display_mirror_protocol.h"

#include <driver/usb_serial_jtag.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstring>

#define TAG "DisplayMirrorUSJ"

namespace {

constexpr char kHostHello[] = "TFTM_HOST_V1\n";

class UsbSerialJtagMirrorTransport final : public DisplayMirrorTransport {
public:
    UsbSerialJtagMirrorTransport() {
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
            if (!usb_serial_jtag_is_driver_installed()) {
                usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
                config.tx_buffer_size = 8192;
                config.rx_buffer_size = 256;
                const esp_err_t err = usb_serial_jtag_driver_install(&config);
                if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                    ESP_LOGE(TAG, "usb_serial_jtag_driver_install failed: %s", esp_err_to_name(err));
                    if (mutex_ != nullptr) {
                        xSemaphoreGive(mutex_);
                    }
                    return false;
                }
            }
            started_ = true;
            ESP_LOGI(TAG, "USB Serial/JTAG mirror transport ready");
        }
        if (mutex_ != nullptr) {
            xSemaphoreGive(mutex_);
        }
        return true;
    }

    bool HasClient() const override {
        if (!started_ || !usb_serial_jtag_is_connected()) {
            client_ready_ = false;
            hello_sent_ = false;
            return false;
        }
        PollHostHello();
        return client_ready_;
    }

    bool SendPacket(uint8_t type, const void* payload, size_t payload_size) override {
        if (payload_size > 0 && payload == nullptr) {
            return false;
        }
        if (!HasClient()) {
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
        usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(20));
        if (!ok) {
            client_ready_ = false;
            hello_sent_ = false;
        }
        if (mutex_ != nullptr) {
            xSemaphoreGive(mutex_);
        }
        return ok;
    }

private:
    void PollHostHello() const {
        uint8_t input[64] = {};
        int read = 0;
        do {
            read = usb_serial_jtag_read_bytes(input, sizeof(input), 0);
            for (int i = 0; i < read; ++i) {
                const char expected = kHostHello[host_hello_pos_];
                if (input[i] == expected) {
                    ++host_hello_pos_;
                    if (kHostHello[host_hello_pos_] == '\0') {
                        client_ready_ = true;
                        hello_sent_ = false;
                        host_hello_pos_ = 0;
                    }
                } else {
                    host_hello_pos_ = input[i] == kHostHello[0] ? 1 : 0;
                }
            }
        } while (read == static_cast<int>(sizeof(input)));
    }

    bool MaybeSendHello() const {
        auto* self = const_cast<UsbSerialJtagMirrorTransport*>(this);
        if (!self->HasClient()) {
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
        const tftm_packet_header_t header = tftm_make_header(
            TFTM_PACKET_HELLO,
            self->sequence_++,
            sizeof(hello),
            tftm_crc32(&hello, sizeof(hello))
        );
        self->hello_sent_ = self->WriteBytes(&header, sizeof(header)) && self->WriteBytes(&hello, sizeof(hello));
        usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(20));
        return self->hello_sent_;
    }

    bool WriteBytes(const void* data, size_t size) const {
        const auto* bytes = static_cast<const uint8_t*>(data);
        size_t sent = 0;
        const int64_t deadline = esp_timer_get_time() + 2000000 + static_cast<int64_t>(size) * 20;
        while (sent < size) {
            if (!usb_serial_jtag_is_connected() || esp_timer_get_time() > deadline) {
                return false;
            }
            constexpr size_t kChunkSize = 512;
            const size_t chunk = std::min(kChunkSize, size - sent);
            const int written = usb_serial_jtag_write_bytes(bytes + sent, chunk, pdMS_TO_TICKS(5));
            if (written <= 0) {
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }
            sent += static_cast<size_t>(written);
        }
        return true;
    }

    mutable SemaphoreHandle_t mutex_ = nullptr;
    mutable uint32_t sequence_ = 0;
    uint16_t width_ = 0;
    uint16_t height_ = 0;
    uint16_t flags_ = 0;
    bool started_ = false;
    mutable bool client_ready_ = false;
    mutable bool hello_sent_ = false;
    mutable size_t host_hello_pos_ = 0;
};

UsbSerialJtagMirrorTransport transport;

} // namespace

DisplayMirrorTransport* GetDisplayMirrorTransport() {
    return &transport;
}
