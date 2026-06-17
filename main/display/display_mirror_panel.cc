#include "display_mirror_panel.h"

#include "display_mirror_protocol.h"
#include "display_mirror_transport.h"

#include <esp_heap_caps.h>
#include <esp_lcd_panel_interface.h>
#include <esp_lcd_panel_ops.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <algorithm>
#include <cstring>
#include <new>
#include <vector>

#define TAG "DisplayMirror"

namespace {

class MirrorLock {
public:
    explicit MirrorLock(SemaphoreHandle_t mutex) : mutex_(mutex) {
        if (mutex_ != nullptr) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
        }
    }

    ~MirrorLock() {
        if (mutex_ != nullptr) {
            xSemaphoreGive(mutex_);
        }
    }

private:
    SemaphoreHandle_t mutex_;
};

struct DisplayMirrorPanel {
    esp_lcd_panel_t base;
    esp_lcd_panel_handle_t real_panel;
    DisplayMirrorTransport* transport;
    uint8_t* framebuffer;
    SemaphoreHandle_t mutex;
    uint16_t width;
    uint16_t height;
    uint16_t flags;
    uint32_t frame_id;
    uint32_t frames_since_full;
    bool need_full_frame;
};

DisplayMirrorPanel* s_last_mirror = nullptr;

DisplayMirrorPanel* to_mirror(esp_lcd_panel_t* panel) {
    return reinterpret_cast<DisplayMirrorPanel*>(panel);
}

bool is_mirror_panel(esp_lcd_panel_handle_t panel) {
    return s_last_mirror != nullptr && panel == &s_last_mirror->base;
}

void delete_wrapper_only(DisplayMirrorPanel* mirror) {
    if (mirror == nullptr) {
        return;
    }
    if (s_last_mirror == mirror) {
        s_last_mirror = nullptr;
    }
    if (mirror->framebuffer != nullptr) {
        heap_caps_free(mirror->framebuffer);
        mirror->framebuffer = nullptr;
    }
    if (mirror->mutex != nullptr) {
        vSemaphoreDelete(mirror->mutex);
        mirror->mutex = nullptr;
    }
    delete mirror;
}

bool send_full_frame_locked(DisplayMirrorPanel* mirror) {
    if (mirror->transport == nullptr || !mirror->transport->HasClient() || mirror->framebuffer == nullptr) {
        return false;
    }

    const size_t framebuffer_size = static_cast<size_t>(mirror->width) * mirror->height * sizeof(uint16_t);
    std::vector<uint8_t> payload(sizeof(uint32_t) + framebuffer_size);
    std::memcpy(payload.data(), &mirror->frame_id, sizeof(mirror->frame_id));
    std::memcpy(payload.data() + sizeof(uint32_t), mirror->framebuffer, framebuffer_size);

    const bool ok = mirror->transport->SendPacket(TFTM_PACKET_FULL_FRAME, payload.data(), payload.size());
    if (ok) {
        mirror->need_full_frame = false;
        mirror->frames_since_full = 0;
    }
    return ok;
}

void send_frame_end_locked(DisplayMirrorPanel* mirror, uint32_t rect_count) {
    if (mirror->transport == nullptr || !mirror->transport->HasClient() || mirror->framebuffer == nullptr) {
        mirror->need_full_frame = true;
        return;
    }

    if (mirror->need_full_frame) {
        send_full_frame_locked(mirror);
    }

    const size_t framebuffer_size = static_cast<size_t>(mirror->width) * mirror->height * sizeof(uint16_t);
    tftm_frame_end_payload_t payload = {
        .frame_id = mirror->frame_id,
        .rect_count = rect_count,
        .framebuffer_crc32 = tftm_crc32(mirror->framebuffer, framebuffer_size),
    };
    if (!mirror->transport->SendPacket(TFTM_PACKET_FRAME_END, &payload, sizeof(payload))) {
        mirror->need_full_frame = true;
        return;
    }

    ++mirror->frames_since_full;
#if CONFIG_DISPLAY_MIRROR_FULL_FRAME_INTERVAL > 0
    if (mirror->frames_since_full >= CONFIG_DISPLAY_MIRROR_FULL_FRAME_INTERVAL) {
        mirror->need_full_frame = true;
    }
#endif
}

esp_err_t mirror_panel_del(esp_lcd_panel_t* panel) {
    auto mirror = to_mirror(panel);
    esp_lcd_panel_handle_t real_panel = mirror->real_panel;
    delete_wrapper_only(mirror);
    return real_panel != nullptr ? esp_lcd_panel_del(real_panel) : ESP_OK;
}

esp_err_t mirror_panel_reset(esp_lcd_panel_t* panel) {
    return esp_lcd_panel_reset(to_mirror(panel)->real_panel);
}

esp_err_t mirror_panel_init(esp_lcd_panel_t* panel) {
    return esp_lcd_panel_init(to_mirror(panel)->real_panel);
}

esp_err_t mirror_panel_draw_bitmap(esp_lcd_panel_t* panel, int x_start, int y_start, int x_end, int y_end, const void* color_data) {
    auto mirror = to_mirror(panel);
    const int rect_w = x_end - x_start;
    const int rect_h = y_end - y_start;

    {
        MirrorLock lock(mirror->mutex);
        if (color_data != nullptr &&
            rect_w > 0 && rect_h > 0 &&
            tftm_rect_in_bounds(static_cast<uint16_t>(x_start), static_cast<uint16_t>(y_start),
                                static_cast<uint16_t>(rect_w), static_cast<uint16_t>(rect_h),
                                mirror->width, mirror->height)) {
            const auto* pixels = static_cast<const uint8_t*>(color_data);
            const size_t row_bytes = static_cast<size_t>(rect_w) * sizeof(uint16_t);
            for (int row = 0; row < rect_h; ++row) {
                const size_t dst = (static_cast<size_t>(y_start + row) * mirror->width + x_start) * sizeof(uint16_t);
                std::memcpy(mirror->framebuffer + dst, pixels + static_cast<size_t>(row) * row_bytes, row_bytes);
            }

            ++mirror->frame_id;
            if (mirror->transport != nullptr && mirror->transport->HasClient()) {
                tftm_rect_payload_header_t rect_header = {
                    .frame_id = mirror->frame_id,
                    .x = static_cast<uint16_t>(x_start),
                    .y = static_cast<uint16_t>(y_start),
                    .w = static_cast<uint16_t>(rect_w),
                    .h = static_cast<uint16_t>(rect_h),
                    .stride_pixels = static_cast<uint16_t>(rect_w),
                };
                std::vector<uint8_t> payload(sizeof(rect_header) + static_cast<size_t>(rect_h) * row_bytes);
                std::memcpy(payload.data(), &rect_header, sizeof(rect_header));
                std::memcpy(payload.data() + sizeof(rect_header), color_data, static_cast<size_t>(rect_h) * row_bytes);
                if (!mirror->transport->SendPacket(TFTM_PACKET_RECT, payload.data(), payload.size())) {
                    mirror->need_full_frame = true;
                }
                send_frame_end_locked(mirror, 1);
            } else {
                mirror->need_full_frame = true;
            }
        } else {
            ESP_LOGW(TAG, "Skipping out-of-bounds mirror rect: (%d,%d)-(%d,%d)", x_start, y_start, x_end, y_end);
        }
    }

    return esp_lcd_panel_draw_bitmap(mirror->real_panel, x_start, y_start, x_end, y_end, color_data);
}

esp_err_t mirror_panel_invert_color(esp_lcd_panel_t* panel, bool invert_color_data) {
    return esp_lcd_panel_invert_color(to_mirror(panel)->real_panel, invert_color_data);
}

esp_err_t mirror_panel_mirror(esp_lcd_panel_t* panel, bool mirror_x, bool mirror_y) {
    return esp_lcd_panel_mirror(to_mirror(panel)->real_panel, mirror_x, mirror_y);
}

esp_err_t mirror_panel_swap_xy(esp_lcd_panel_t* panel, bool swap_axes) {
    return esp_lcd_panel_swap_xy(to_mirror(panel)->real_panel, swap_axes);
}

esp_err_t mirror_panel_set_gap(esp_lcd_panel_t* panel, int x_gap, int y_gap) {
    return esp_lcd_panel_set_gap(to_mirror(panel)->real_panel, x_gap, y_gap);
}

esp_err_t mirror_panel_disp_on_off(esp_lcd_panel_t* panel, bool on_off) {
    return esp_lcd_panel_disp_on_off(to_mirror(panel)->real_panel, on_off);
}

esp_err_t mirror_panel_disp_sleep(esp_lcd_panel_t* panel, bool sleep) {
    return esp_lcd_panel_disp_sleep(to_mirror(panel)->real_panel, sleep);
}

esp_err_t mirror_panel_set_brightness(esp_lcd_panel_t* panel, int brightness) {
    return to_mirror(panel)->real_panel->set_brightness != nullptr
        ? to_mirror(panel)->real_panel->set_brightness(to_mirror(panel)->real_panel, brightness)
        : ESP_ERR_NOT_SUPPORTED;
}

} // namespace

esp_lcd_panel_handle_t display_mirror_wrap_panel(
    esp_lcd_panel_handle_t real_panel,
    uint16_t width,
    uint16_t height,
    uint16_t flags
) {
    if (real_panel == nullptr || width == 0 || height == 0) {
        return real_panel;
    }

    auto mirror = new (std::nothrow) DisplayMirrorPanel();
    if (mirror == nullptr) {
        ESP_LOGE(TAG, "failed to allocate panel wrapper");
        return real_panel;
    }

    mirror->real_panel = real_panel;
    mirror->transport = GetDisplayMirrorTransport();
    mirror->width = width;
    mirror->height = height;
    mirror->flags = flags;
    mirror->frame_id = 0;
    mirror->frames_since_full = 0;
    mirror->need_full_frame = true;
    mirror->mutex = xSemaphoreCreateMutex();

    const size_t framebuffer_size = static_cast<size_t>(width) * height * sizeof(uint16_t);
    mirror->framebuffer = static_cast<uint8_t*>(heap_caps_malloc(framebuffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (mirror->framebuffer == nullptr) {
        mirror->framebuffer = static_cast<uint8_t*>(heap_caps_malloc(framebuffer_size, MALLOC_CAP_8BIT));
    }
    if (mirror->framebuffer == nullptr || mirror->mutex == nullptr) {
        ESP_LOGE(TAG, "failed to allocate mirror framebuffer: %ux%u RGB565", width, height);
        delete_wrapper_only(mirror);
        return real_panel;
    }
    std::memset(mirror->framebuffer, 0, framebuffer_size);

    mirror->base.reset = mirror_panel_reset;
    mirror->base.init = mirror_panel_init;
    mirror->base.del = mirror_panel_del;
    mirror->base.draw_bitmap = mirror_panel_draw_bitmap;
    mirror->base.invert_color = mirror_panel_invert_color;
    mirror->base.mirror = mirror_panel_mirror;
    mirror->base.swap_xy = mirror_panel_swap_xy;
    mirror->base.set_gap = mirror_panel_set_gap;
    mirror->base.disp_on_off = mirror_panel_disp_on_off;
    mirror->base.disp_sleep = mirror_panel_disp_sleep;
    mirror->base.set_brightness = mirror_panel_set_brightness;

    if (mirror->transport != nullptr) {
        mirror->transport->Start(width, height, flags);
    }
    ESP_LOGI(TAG, "framebuffer allocated: %ux%u RGB565", width, height);
    s_last_mirror = mirror;
    return &mirror->base;
}

esp_lcd_panel_handle_t display_mirror_unwrap_panel(esp_lcd_panel_handle_t panel) {
    if (!is_mirror_panel(panel)) {
        return panel;
    }
    return reinterpret_cast<DisplayMirrorPanel*>(panel)->real_panel;
}
