#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/gif/lvgl_gif.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "assets/lang_config.h"

#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_spd2010.h>
#include <esp_lcd_touch.h>
#include <esp_lcd_touch_cst9217.h>
#include <esp_lcd_touch_spd2010.h>
#include <esp_timer.h>
#include "esp_io_expander_tca9554.h"
#include <iot_button.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "paopao_pet_gif_assets.h"
#include "paopao_pet_state.h"
#include "paopao_pet_trigger.h"
}

#define TAG "waveshare_lcd_1_46"

extern const uint8_t assets_images_idle_gif_start[] asm("_binary_idle_gif_start");
extern const uint8_t assets_images_idle_gif_end[] asm("_binary_idle_gif_end");
extern const uint8_t assets_images_working_gif_start[] asm("_binary_working_gif_start");
extern const uint8_t assets_images_working_gif_end[] asm("_binary_working_gif_end");
extern const uint8_t assets_images_speaking_gif_start[] asm("_binary_speaking_gif_start");
extern const uint8_t assets_images_speaking_gif_end[] asm("_binary_speaking_gif_end");
extern const uint8_t assets_images_thinking_gif_start[] asm("_binary_thinking_gif_start");
extern const uint8_t assets_images_thinking_gif_end[] asm("_binary_thinking_gif_end");
extern const uint8_t assets_images_waiting_gif_start[] asm("_binary_waiting_gif_start");
extern const uint8_t assets_images_waiting_gif_end[] asm("_binary_waiting_gif_end");
extern const uint8_t assets_images_done_gif_start[] asm("_binary_done_gif_start");
extern const uint8_t assets_images_done_gif_end[] asm("_binary_done_gif_end");
extern const uint8_t assets_images_sleeping_gif_start[] asm("_binary_sleeping_gif_start");
extern const uint8_t assets_images_sleeping_gif_end[] asm("_binary_sleeping_gif_end");
extern const uint8_t assets_images_jumping_gif_start[] asm("_binary_jumping_gif_start");
extern const uint8_t assets_images_jumping_gif_end[] asm("_binary_jumping_gif_end");
extern const uint8_t assets_images_failed_gif_start[] asm("_binary_failed_gif_start");
extern const uint8_t assets_images_failed_gif_end[] asm("_binary_failed_gif_end");
extern const uint8_t assets_images_giddy_gif_start[] asm("_binary_giddy_gif_start");
extern const uint8_t assets_images_giddy_gif_end[] asm("_binary_giddy_gif_end");
extern const uint8_t assets_images_review_gif_start[] asm("_binary_review_gif_start");
extern const uint8_t assets_images_review_gif_end[] asm("_binary_review_gif_end");

static constexpr uint32_t k_touch_hold_ms = 1200;
static constexpr int16_t k_touch_drag_min_px = 42;
static constexpr uint32_t k_touch_motion_suppress_ms = 600;
static constexpr uint32_t k_motion_poll_ms = 50;
static constexpr uint32_t k_shake_cooldown_ms = 1800;
static constexpr int32_t k_shake_delta_threshold = 12000;
static constexpr uint8_t k_shake_required_samples = 3;
static constexpr uint16_t k_white_rgb565 = 0xFFFF;
static constexpr uint32_t k_pet_image_scale_base = LV_SCALE_NONE;
static constexpr uint16_t k_pet_target_visual_longest = 162;
static constexpr uint8_t k_qmi8658_addr_primary = 0x6B;
static constexpr uint8_t k_qmi8658_addr_secondary = 0x6A;
static constexpr uint8_t k_qmi8658_reg_who_am_i = 0x00;
static constexpr uint8_t k_qmi8658_reg_ctrl1 = 0x02;
static constexpr uint8_t k_qmi8658_reg_ctrl2 = 0x03;
static constexpr uint8_t k_qmi8658_reg_ctrl7 = 0x08;
static constexpr uint8_t k_qmi8658_reg_accel_x_l = 0x35;

class TouchReader {
public:
    virtual ~TouchReader() = default;
    virtual const char* Name() const = 0;
    virtual esp_err_t ReadPoint(uint16_t& x, uint16_t& y, bool& pressed) = 0;
};

class EspLcdTouchReader : public TouchReader {
public:
    explicit EspLcdTouchReader(esp_lcd_touch_handle_t touch) : touch_(touch) {}

    const char* Name() const override {
        return "esp_lcd_touch";
    }

    esp_err_t ReadPoint(uint16_t& x, uint16_t& y, bool& pressed) override {
        pressed = false;
        ESP_RETURN_ON_ERROR(esp_lcd_touch_read_data(touch_), TAG, "touch read failed");

        uint8_t count = 0;
        pressed = esp_lcd_touch_get_coordinates(touch_, &x, &y, nullptr, &count, 1) && count > 0;
        return ESP_OK;
    }

private:
    esp_lcd_touch_handle_t touch_ = nullptr;
};

class Spd2010DirectTouchReader : public TouchReader {
public:
    bool Initialize(i2c_master_bus_handle_t i2c_bus) {
        i2c_device_config_t device_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = ESP_LCD_TOUCH_IO_I2C_SPD2010_ADDRESS,
            .scl_speed_hz = 400 * 1000,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };

        esp_err_t err = i2c_master_bus_add_device(i2c_bus, &device_cfg, &device_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SPD2010 touch add device failed: %s", esp_err_to_name(err));
            return false;
        }

        uint8_t version[18] = {};
        err = ReadCommand(0x26, 0x00, version, sizeof(version));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SPD2010 touch version read failed: %s", esp_err_to_name(err));
            return false;
        }

        ESP_LOGI(TAG, "SPD2010 direct touch initialized");
        return true;
    }

    const char* Name() const override {
        return "SPD2010-direct";
    }

    esp_err_t ReadPoint(uint16_t& x, uint16_t& y, bool& pressed) override {
        pressed = false;

        uint8_t status[4] = {};
        ESP_RETURN_ON_ERROR(ReadCommand(0x20, 0x00, status, sizeof(status)), TAG, "Read SPD2010 status failed");

        const bool pt_exist = (status[0] & 0x01) != 0;
        const bool gesture = (status[0] & 0x02) != 0;
        const bool aux = (status[0] & 0x08) != 0;
        const bool cpu_run = (status[1] & 0x08) != 0;
        const bool tic_in_cpu = (status[1] & 0x20) != 0;
        const bool tic_in_bios = (status[1] & 0x40) != 0;
        const uint16_t read_len = (uint16_t)((uint16_t)status[3] << 8 | status[2]);

        if (tic_in_bios) {
            ESP_RETURN_ON_ERROR(WriteCommand4(0x02, 0x00, 0x01, 0x00), TAG, "Clear SPD2010 int failed");
            return WriteCommand4(0x04, 0x00, 0x01, 0x00);
        }
        if (tic_in_cpu) {
            ESP_RETURN_ON_ERROR(WriteCommand4(0x50, 0x00, 0x00, 0x00), TAG, "Set SPD2010 point mode failed");
            ESP_RETURN_ON_ERROR(WriteCommand4(0x46, 0x00, 0x00, 0x00), TAG, "Start SPD2010 touch failed");
            return WriteCommand4(0x02, 0x00, 0x01, 0x00);
        }
        if (cpu_run && read_len == 0) {
            return WriteCommand4(0x02, 0x00, 0x01, 0x00);
        }
        if (!(pt_exist || gesture)) {
            if (cpu_run && aux) {
                return WriteCommand4(0x02, 0x00, 0x01, 0x00);
            }
            return ESP_OK;
        }
        if (read_len < 10 || read_len > sizeof(report_)) {
            return ESP_OK;
        }

        ESP_RETURN_ON_ERROR(ReadCommand(0x00, 0x03, report_, read_len), TAG, "Read SPD2010 report failed");
        const uint8_t check_id = report_[4];
        if (check_id <= 0x0A && pt_exist) {
            const uint8_t weight = report_[8];
            if (weight != 0) {
                x = (uint16_t)(((report_[7] & 0xF0) << 4) | report_[5]);
                y = (uint16_t)(((report_[7] & 0x0F) << 8) | report_[6]);
                ApplyTransform(x, y);
                pressed = true;
            }
        }

        return ClearReportStatus();
    }

private:
    i2c_master_dev_handle_t device_ = nullptr;
    uint8_t report_[4 + (10 * 6)] = {};

    esp_err_t Write(const uint8_t* data, size_t length) {
        return i2c_master_transmit(device_, data, length, pdMS_TO_TICKS(100));
    }

    esp_err_t Receive(uint8_t* data, size_t length) {
        return i2c_master_receive(device_, data, length, pdMS_TO_TICKS(100));
    }

    esp_err_t WriteCommand4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        const uint8_t data[4] = {a, b, c, d};
        esp_err_t err = Write(data, sizeof(data));
        esp_rom_delay_us(200);
        return err;
    }

    esp_err_t ReadCommand(uint8_t a, uint8_t b, uint8_t* data, size_t length) {
        const uint8_t cmd[2] = {a, b};
        ESP_RETURN_ON_ERROR(Write(cmd, sizeof(cmd)), TAG, "SPD2010 command write failed");
        esp_rom_delay_us(200);
        ESP_RETURN_ON_ERROR(Receive(data, length), TAG, "SPD2010 data receive failed");
        esp_rom_delay_us(200);
        return ESP_OK;
    }

    esp_err_t ClearReportStatus() {
        uint8_t hdp_status[8] = {};
        for (uint8_t i = 0; i < 3; i++) {
            ESP_RETURN_ON_ERROR(ReadCommand(0xFC, 0x02, hdp_status, sizeof(hdp_status)), TAG, "Read SPD2010 HDP status failed");
            if (hdp_status[5] == 0x82) {
                return WriteCommand4(0x02, 0x00, 0x01, 0x00);
            }
            if (hdp_status[5] != 0x00) {
                return ESP_OK;
            }

            const uint16_t remain_len = (uint16_t)((uint16_t)hdp_status[3] << 8 | hdp_status[2]);
            if (remain_len == 0 || remain_len > 32) {
                return ESP_OK;
            }
            uint8_t remain[32] = {};
            ESP_RETURN_ON_ERROR(ReadCommand(0x00, 0x03, remain, remain_len), TAG, "Read SPD2010 remain data failed");
        }

        return ESP_OK;
    }

    void ApplyTransform(uint16_t& x, uint16_t& y) {
        if (DISPLAY_SWAP_XY) {
            const uint16_t tmp = x;
            x = y;
            y = tmp;
        }
        if (DISPLAY_MIRROR_X) {
            x = (DISPLAY_WIDTH - 1) - x;
        }
        if (DISPLAY_MIRROR_Y) {
            y = (DISPLAY_HEIGHT - 1) - y;
        }
    }
};

struct PaopaoGifBinary {
    const uint8_t* start;
    const uint8_t* end;
};

static PaopaoGifBinary PaopaoGifAssetForState(paopao_pet_state_t state) {
    switch (state) {
        case PAOPAO_PET_STATE_IDLE:
            return {assets_images_idle_gif_start, assets_images_idle_gif_end};
        case PAOPAO_PET_STATE_WORKING:
            return {assets_images_working_gif_start, assets_images_working_gif_end};
        case PAOPAO_PET_STATE_SPEAKING:
            return {assets_images_speaking_gif_start, assets_images_speaking_gif_end};
        case PAOPAO_PET_STATE_THINKING:
            return {assets_images_thinking_gif_start, assets_images_thinking_gif_end};
        case PAOPAO_PET_STATE_WAITING:
            return {assets_images_waiting_gif_start, assets_images_waiting_gif_end};
        case PAOPAO_PET_STATE_DONE:
            return {assets_images_done_gif_start, assets_images_done_gif_end};
        case PAOPAO_PET_STATE_SLEEPING:
            return {assets_images_sleeping_gif_start, assets_images_sleeping_gif_end};
        case PAOPAO_PET_STATE_JUMPING:
            return {assets_images_jumping_gif_start, assets_images_jumping_gif_end};
        case PAOPAO_PET_STATE_FAILING:
            return {assets_images_failed_gif_start, assets_images_failed_gif_end};
        case PAOPAO_PET_STATE_GIDDY:
            return {assets_images_giddy_gif_start, assets_images_giddy_gif_end};
        case PAOPAO_PET_STATE_REVIEW:
            return {assets_images_review_gif_start, assets_images_review_gif_end};
        default:
            return {assets_images_idle_gif_start, assets_images_idle_gif_end};
    }
};

static uint16_t PaopaoGifVisualLongestForState(paopao_pet_state_t state) {
    switch (state) {
        case PAOPAO_PET_STATE_IDLE:
            return 162;
        case PAOPAO_PET_STATE_WORKING:
            return 165;
        case PAOPAO_PET_STATE_SPEAKING:
            return 182;
        case PAOPAO_PET_STATE_THINKING:
            return 159;
        case PAOPAO_PET_STATE_WAITING:
            return 151;
        case PAOPAO_PET_STATE_DONE:
            return 150;
        case PAOPAO_PET_STATE_SLEEPING:
            return 163;
        case PAOPAO_PET_STATE_JUMPING:
            return 125;
        case PAOPAO_PET_STATE_FAILING:
            return 112;
        case PAOPAO_PET_STATE_GIDDY:
            return 238;
        case PAOPAO_PET_STATE_REVIEW:
            return 126;
        default:
            return k_pet_target_visual_longest;
    }
}

static uint32_t PaopaoImageScaleForVisualSize(paopao_pet_state_t state) {
    const uint16_t visual_longest = PaopaoGifVisualLongestForState(state);
    if (visual_longest == 0) {
        return k_pet_image_scale_base;
    }

    return (((uint32_t)k_pet_target_visual_longest * k_pet_image_scale_base) + visual_longest / 2) / visual_longest;
}

class PaopaoPetDisplay : public SpiLcdDisplay {
public:
    PaopaoPetDisplay(
        esp_lcd_panel_io_handle_t panel_io,
        esp_lcd_panel_handle_t panel
    ) : SpiLcdDisplay(
            panel_io,
            panel,
            DISPLAY_WIDTH,
            DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X,
            DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X,
            DISPLAY_MIRROR_Y,
            DISPLAY_SWAP_XY
        ) {
    }

    virtual void SetupUI() override {
        LcdDisplay::SetupUI();

        DisplayLockGuard lock(this);

        lv_obj_t* screen = lv_screen_active();
        lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

        if (emoji_label_ != nullptr) {
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (emoji_image_ != nullptr) {
            lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        }

        InitializePetFrameBuffer();
        if (pet_frame_buffer_ != nullptr) {
            pet_image_ = lv_image_create(screen);
            lv_image_set_src(pet_image_, &pet_frame_dsc_);
            lv_obj_align(pet_image_, LV_ALIGN_CENTER, 0, 0);
            lv_obj_move_to_index(pet_image_, 1);
        }
        RaiseOverlayObjects();
        lv_obj_invalidate(screen);

        const uint32_t now_ms = NowMs();
        paopao_pet_trigger_init(&trigger_, now_ms);
        current_state_ = trigger_.displayed_state;
        PlayGifState(current_state_);

        if (render_task_ == nullptr) {
            xTaskCreatePinnedToCore(RenderTask, "paopao_pet", 4096, this, 3, &render_task_, 1);
        }
    }

    virtual void SetStatus(const char* status) override {
        if (status == nullptr) {
            return;
        }

        LcdDisplay::SetStatus(status);

        if (StatusEquals(status, Lang::Strings::LISTENING) ||
            Contains(status, "Listening") || Contains(status, "listening")) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_LISTENING);
        } else if (StatusEquals(status, Lang::Strings::SPEAKING) ||
                   Contains(status, "Speaking") || Contains(status, "speaking")) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_SPEAKING);
        } else if (Contains(status, "Thinking") || Contains(status, "thinking")) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_THINKING);
        } else if (StatusEquals(status, Lang::Strings::STANDBY) ||
                   Contains(status, "Standby") || Contains(status, "standby")) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_IDLE);
        } else if (StatusEquals(status, Lang::Strings::ERROR) ||
                   Contains(status, "Error") || Contains(status, "error")) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_ERROR);
        } else if (IsBusyStatus(status)) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_CONNECTING);
        }
    }

    virtual void SetEmotion(const char* emotion) override {
        if (emotion == nullptr) {
            return;
        }

        if (std::strstr(emotion, "happy") || std::strstr(emotion, "laugh") ||
            std::strstr(emotion, "loving") || std::strstr(emotion, "cool")) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_SERVICE_HAPPY);
        } else if (std::strstr(emotion, "think")) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_SERVICE_THINKING);
        } else if (std::strstr(emotion, "sleep")) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_SERVICE_SLEEP);
        } else if (std::strstr(emotion, "sad") || std::strstr(emotion, "angry") ||
                   std::strstr(emotion, "cry") || std::strstr(emotion, "shock")) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_SERVICE_FAILING);
        } else if (std::strstr(emotion, "neutral") || std::strstr(emotion, "microchip")) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_SERVICE_NEUTRAL);
        }
    }

    virtual void SetChatMessage(const char* role, const char* content) override {
        if (role == nullptr || content == nullptr) {
            return;
        }
        if (content[0] == '\0') {
            LcdDisplay::SetChatMessage(role, content);
            return;
        }

        if (std::strcmp(role, "user") == 0) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_THINKING);
        } else if (std::strcmp(role, "assistant") == 0) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_SPEAKING);
        }

        LcdDisplay::SetChatMessage(role, content);
    }

    virtual void ClearChatMessages() override {
        LcdDisplay::ClearChatMessages();
    }

    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override {
        LcdDisplay::ShowNotification(notification, duration_ms);
    }

    virtual void UpdateStatusBar(bool update_all = false) override {
        LcdDisplay::UpdateStatusBar(update_all);
    }

    virtual void SetPowerSaveMode(bool on) override {
        LvglDisplay::SetPowerSaveMode(on);
    }

    void DispatchPetTrigger(paopao_pet_trigger_event_t event) {
        DisplayLockGuard lock(this);
        paopao_pet_trigger_dispatch(&trigger_, event, NowMs());
        ApplyPetStateIfChanged();
    }

    void PlayLocalReaction(paopao_pet_state_t state, uint32_t duration_ms) {
        DisplayLockGuard lock(this);
        paopao_pet_trigger_play_reaction(&trigger_, state, duration_ms, NowMs());
        ApplyPetStateIfChanged();
    }

    void AttachTouch(TouchReader* touch) {
        DisplayLockGuard lock(this);
        touch_ = touch;
        ESP_LOGI(TAG, "Touch reader attached: %s", touch_ != nullptr ? touch_->Name() : "none");
    }

    bool IsTouchMotionSuppressed(uint32_t now_ms) {
        DisplayLockGuard lock(this);
        return touch_pressed_ ||
               (touch_last_active_ms_ != 0 &&
                now_ms - touch_last_active_ms_ <= k_touch_motion_suppress_ms);
    }

private:
    TaskHandle_t render_task_ = nullptr;
    lv_obj_t* pet_image_ = nullptr;
    lv_img_dsc_t pet_frame_dsc_ = {};
    uint16_t* pet_frame_buffer_ = nullptr;
    lv_img_dsc_t gif_source_dsc_ = {};
    std::unique_ptr<LvglGif> pet_gif_controller_ = nullptr;
    TouchReader* touch_ = nullptr;
    paopao_pet_trigger_context_t trigger_;
    paopao_pet_state_t current_state_ = PAOPAO_PET_STATE_IDLE;
    bool touch_pressed_ = false;
    uint16_t touch_start_x_ = 0;
    uint16_t touch_start_y_ = 0;
    uint16_t touch_last_x_ = 0;
    uint16_t touch_last_y_ = 0;
    uint32_t touch_start_ms_ = 0;
    uint32_t touch_last_active_ms_ = 0;
    uint32_t touch_last_error_log_ms_ = 0;
    uint32_t touch_last_point_log_ms_ = 0;

    static uint32_t NowMs() {
        return (uint32_t)(esp_timer_get_time() / 1000ULL);
    }

    static bool Contains(const char* text, const char* needle) {
        return text != nullptr && needle != nullptr && std::strstr(text, needle) != nullptr;
    }

    static bool StatusEquals(const char* status, const char* expected) {
        return status != nullptr && expected != nullptr && std::strcmp(status, expected) == 0;
    }

    static bool IsBusyStatus(const char* status) {
        return StatusEquals(status, Lang::Strings::CONNECTING) ||
               StatusEquals(status, Lang::Strings::REGISTERING_NETWORK) ||
               StatusEquals(status, Lang::Strings::DETECTING_MODULE) ||
               StatusEquals(status, Lang::Strings::LOADING_PROTOCOL) ||
               StatusEquals(status, Lang::Strings::CHECKING_NEW_VERSION) ||
               StatusEquals(status, Lang::Strings::UPGRADING) ||
               StatusEquals(status, Lang::Strings::ACTIVATION) ||
               StatusEquals(status, Lang::Strings::WIFI_CONFIG_MODE) ||
               Contains(status, "Connecting") ||
               Contains(status, "connecting") ||
               Contains(status, "Loading") ||
               Contains(status, "loading") ||
               Contains(status, "Upgrading") ||
               Contains(status, "upgrading");
    }

    void ApplyPetStateIfChanged() {
        if (current_state_ == trigger_.displayed_state) {
            return;
        }
        current_state_ = trigger_.displayed_state;
        PlayGifState(current_state_);
    }

    void InitializePetFrameBuffer() {
        if (pet_frame_buffer_ == nullptr) {
            const size_t frame_bytes = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
            pet_frame_buffer_ = static_cast<uint16_t*>(
                heap_caps_malloc(frame_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
            );
            if (pet_frame_buffer_ == nullptr) {
                pet_frame_buffer_ = static_cast<uint16_t*>(heap_caps_malloc(frame_bytes, MALLOC_CAP_8BIT));
            }
            if (pet_frame_buffer_ == nullptr) {
                ESP_LOGE(TAG, "Failed to allocate Paopao full-screen frame buffer");
                return;
            }
        }

        std::fill_n(pet_frame_buffer_, DISPLAY_WIDTH * DISPLAY_HEIGHT, k_white_rgb565);
        pet_frame_dsc_ = {};
        pet_frame_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
        pet_frame_dsc_.header.flags = LV_IMAGE_FLAGS_MODIFIABLE;
        pet_frame_dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
        pet_frame_dsc_.header.w = DISPLAY_WIDTH;
        pet_frame_dsc_.header.h = DISPLAY_HEIGHT;
        pet_frame_dsc_.header.stride = DISPLAY_WIDTH * sizeof(uint16_t);
        pet_frame_dsc_.data_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        pet_frame_dsc_.data = reinterpret_cast<const uint8_t*>(pet_frame_buffer_);
    }

    void RaiseOverlayObjects() {
        if (top_bar_ != nullptr) {
            lv_obj_move_foreground(top_bar_);
        }
        if (status_bar_ != nullptr) {
            lv_obj_move_foreground(status_bar_);
        }
        if (bottom_bar_ != nullptr) {
            lv_obj_move_foreground(bottom_bar_);
        }
        if (low_battery_popup_ != nullptr) {
            lv_obj_move_foreground(low_battery_popup_);
        }
    }

    void CopyPetFrameToScreen(const lv_img_dsc_t* frame, uint32_t image_scale) {
        if (pet_frame_buffer_ == nullptr || frame == nullptr || frame->data == nullptr) {
            return;
        }

        std::fill_n(pet_frame_buffer_, DISPLAY_WIDTH * DISPLAY_HEIGHT, k_white_rgb565);

        const uint16_t src_w = frame->header.w;
        const uint16_t src_h = frame->header.h;
        if (src_w == 0 || src_h == 0 || image_scale == 0) {
            return;
        }

        uint32_t draw_w = ((uint32_t)src_w * image_scale + 128) / 256;
        uint32_t draw_h = ((uint32_t)src_h * image_scale + 128) / 256;
        if (draw_w == 0 || draw_h == 0) {
            return;
        }

        const int32_t dst_start_x = ((int32_t)DISPLAY_WIDTH - (int32_t)draw_w) / 2;
        const int32_t dst_start_y = ((int32_t)DISPLAY_HEIGHT - (int32_t)draw_h) / 2;
        const uint16_t* src = reinterpret_cast<const uint16_t*>(frame->data);
        const size_t src_stride_pixels = frame->header.stride / sizeof(uint16_t);

        for (uint32_t dy = 0; dy < draw_h; ++dy) {
            const int32_t screen_y = dst_start_y + (int32_t)dy;
            if (screen_y < 0 || screen_y >= DISPLAY_HEIGHT) {
                continue;
            }
            const uint32_t sy = (dy * 256) / image_scale;
            if (sy >= src_h) {
                continue;
            }
            for (uint32_t dx = 0; dx < draw_w; ++dx) {
                const int32_t screen_x = dst_start_x + (int32_t)dx;
                if (screen_x < 0 || screen_x >= DISPLAY_WIDTH) {
                    continue;
                }
                const uint32_t sx = (dx * 256) / image_scale;
                if (sx >= src_w) {
                    continue;
                }
                pet_frame_buffer_[screen_y * DISPLAY_WIDTH + screen_x] =
                    src[sy * src_stride_pixels + sx];
            }
        }
    }

    void PlayGifState(paopao_pet_state_t state) {
        if (pet_image_ == nullptr) {
            return;
        }

        if (pet_gif_controller_) {
            pet_gif_controller_->Stop();
            pet_gif_controller_.reset();
        }

        const PaopaoGifBinary asset = PaopaoGifAssetForState(state);
        const size_t gif_size = (size_t)(asset.end - asset.start);
        gif_source_dsc_ = {};
        gif_source_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
        gif_source_dsc_.header.cf = LV_COLOR_FORMAT_RAW_ALPHA;
        gif_source_dsc_.header.w = 0;
        gif_source_dsc_.header.h = 0;
        gif_source_dsc_.header.stride = 0;
        gif_source_dsc_.data_size = gif_size;
        gif_source_dsc_.data = asset.start;

        pet_gif_controller_ = std::make_unique<LvglGif>(&gif_source_dsc_, true, 0xFFFFFF, true);
        if (!pet_gif_controller_->IsLoaded()) {
            ESP_LOGE(
                TAG,
                "Failed to load Paopao GIF state=%s size=%u",
                paopao_pet_gif_asset_name(state),
                (unsigned)gif_size
            );
            pet_gif_controller_.reset();
            return;
        }

        const uint32_t image_scale = PaopaoImageScaleForVisualSize(state);
        pet_gif_controller_->SetFrameCallback([this, image_scale]() {
            CopyPetFrameToScreen(pet_gif_controller_->image_dsc(), image_scale);
            lv_image_set_src(pet_image_, &pet_frame_dsc_);
            lv_obj_invalidate(pet_image_);
        });
        CopyPetFrameToScreen(pet_gif_controller_->image_dsc(), image_scale);
        lv_image_set_src(pet_image_, &pet_frame_dsc_);
        lv_image_set_scale(pet_image_, 256);
        lv_obj_align(pet_image_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_invalidate(pet_image_);
        pet_gif_controller_->Start();
        ESP_LOGI(
            TAG,
            "Playing Paopao GIF state=%s size=%u scale=%u visual=%u",
            paopao_pet_gif_asset_name(state),
            (unsigned)gif_size,
            (unsigned)image_scale,
            (unsigned)PaopaoGifVisualLongestForState(state)
        );
    }

    void DispatchPetTriggerLocked(paopao_pet_trigger_event_t event, uint32_t now_ms) {
        paopao_pet_trigger_dispatch(&trigger_, event, now_ms);
        ApplyPetStateIfChanged();
    }

    void HandleTouchRelease(uint32_t now_ms) {
        const int16_t dx = (int16_t)touch_last_x_ - (int16_t)touch_start_x_;
        const int16_t dy = (int16_t)touch_last_y_ - (int16_t)touch_start_y_;
        const int16_t abs_dx = (int16_t)std::abs(dx);
        const int16_t abs_dy = (int16_t)std::abs(dy);
        if (abs_dx >= k_touch_drag_min_px && abs_dx > abs_dy) {
            DispatchPetTriggerLocked(
                dx < 0 ? PAOPAO_PET_TRIGGER_LOCAL_DRAG_LEFT : PAOPAO_PET_TRIGGER_LOCAL_DRAG_RIGHT,
                now_ms
            );
        } else if (now_ms - touch_start_ms_ >= k_touch_hold_ms) {
            DispatchPetTriggerLocked(PAOPAO_PET_TRIGGER_LOCAL_HOLD, now_ms);
        } else {
            DispatchPetTriggerLocked(PAOPAO_PET_TRIGGER_LOCAL_TAP, now_ms);
        }
    }

    void PollTouch(uint32_t now_ms) {
        if (touch_ == nullptr) {
            return;
        }

        bool pressed = false;
        uint16_t x = 0;
        uint16_t y = 0;
        const esp_err_t touch_err = touch_->ReadPoint(x, y, pressed);
        if (touch_err != ESP_OK) {
            if (touch_pressed_) {
                HandleTouchRelease(now_ms);
                touch_last_active_ms_ = now_ms;
                touch_pressed_ = false;
            }
            if (now_ms - touch_last_error_log_ms_ >= 1000) {
                touch_last_error_log_ms_ = now_ms;
                ESP_LOGW(TAG, "Touch read failed: %s", esp_err_to_name(touch_err));
            }
            return;
        }

        if (pressed) {
            if (!touch_pressed_ || now_ms - touch_last_point_log_ms_ >= 1000) {
                touch_last_point_log_ms_ = now_ms;
                ESP_LOGI(TAG, "Touch point x=%u y=%u", x, y);
            }
            touch_last_x_ = x;
            touch_last_y_ = y;
            touch_last_active_ms_ = now_ms;
            if (!touch_pressed_) {
                touch_start_x_ = x;
                touch_start_y_ = y;
                touch_start_ms_ = now_ms;
            }
        } else if (touch_pressed_) {
            HandleTouchRelease(now_ms);
            touch_last_active_ms_ = now_ms;
        }

        touch_pressed_ = pressed;
    }

    void RunRenderLoop() {
        while (true) {
            {
                DisplayLockGuard lock(this);
                const uint32_t now_ms = NowMs();
                PollTouch(now_ms);
                paopao_pet_trigger_tick(&trigger_, now_ms);
                ApplyPetStateIfChanged();
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    static void RenderTask(void* arg) {
        static_cast<PaopaoPetDisplay*>(arg)->RunRenderLoop();
    }
};

enum class TouchControllerType {
    Spd2010,
    Cst9217,
};

class Qmi8658Motion {
public:
    bool Initialize(i2c_master_bus_handle_t i2c_bus) {
        uint8_t address = k_qmi8658_addr_primary;
        esp_err_t err = i2c_master_probe(i2c_bus, address, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            address = k_qmi8658_addr_secondary;
            err = i2c_master_probe(i2c_bus, address, pdMS_TO_TICKS(100));
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "QMI8658 not found");
            return false;
        }

        i2c_device_config_t device_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = 400 * 1000,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        err = i2c_master_bus_add_device(i2c_bus, &device_cfg, &device_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "QMI8658 add device failed: %s", esp_err_to_name(err));
            return false;
        }

        uint8_t chip_id = 0;
        if (ReadReg(k_qmi8658_reg_who_am_i, &chip_id) != ESP_OK) {
            ESP_LOGW(TAG, "QMI8658 WHO_AM_I read failed");
            return false;
        }

        WriteReg(k_qmi8658_reg_ctrl1, 0x60);
        WriteReg(k_qmi8658_reg_ctrl2, 0x23);
        WriteReg(k_qmi8658_reg_ctrl7, 0x01);

        ESP_LOGI(TAG, "QMI8658 initialized at 0x%02x, chip_id=0x%02x", address, chip_id);
        return true;
    }

    bool ReadAcceleration(int16_t& x, int16_t& y, int16_t& z) {
        uint8_t data[6] = {};
        if (ReadRegs(k_qmi8658_reg_accel_x_l, data, sizeof(data)) != ESP_OK) {
            return false;
        }

        x = (int16_t)((uint16_t)data[1] << 8 | data[0]);
        y = (int16_t)((uint16_t)data[3] << 8 | data[2]);
        z = (int16_t)((uint16_t)data[5] << 8 | data[4]);
        return true;
    }

private:
    i2c_master_dev_handle_t device_ = nullptr;

    esp_err_t WriteReg(uint8_t reg, uint8_t value) {
        uint8_t buffer[2] = {reg, value};
        return i2c_master_transmit(device_, buffer, sizeof(buffer), pdMS_TO_TICKS(100));
    }

    esp_err_t ReadReg(uint8_t reg, uint8_t* value) {
        return i2c_master_transmit_receive(device_, &reg, 1, value, 1, pdMS_TO_TICKS(100));
    }

    esp_err_t ReadRegs(uint8_t reg, uint8_t* buffer, size_t length) {
        return i2c_master_transmit_receive(device_, &reg, 1, buffer, length, pdMS_TO_TICKS(100));
    }
};

class CustomBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    esp_io_expander_handle_t io_expander = NULL;
    Display* display_ = nullptr;
    esp_lcd_panel_io_handle_t touch_io_handle_ = nullptr;
    esp_lcd_touch_handle_t touch_handle_ = nullptr;
    TouchReader* touch_reader_ = nullptr;
    Spd2010DirectTouchReader spd2010_touch_reader_;
    Qmi8658Motion motion_;
    TaskHandle_t motion_task_ = nullptr;
    button_handle_t boot_btn, pwr_btn;
    button_driver_t* boot_btn_driver_ = nullptr;
    button_driver_t* pwr_btn_driver_ = nullptr;
    static CustomBoard* instance_;

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = I2C_SDA_IO,
            .scl_io_num = I2C_SCL_IO,
            .clk_source = I2C_CLK_SRC_DEFAULT,
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }
    
    void InitializeTca9554(void) {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, I2C_ADDRESS, &io_expander);
        if(ret != ESP_OK)
            ESP_LOGE(TAG, "TCA9554 create returned error");        

        // uint32_t input_level_mask = 0;
        // ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_INPUT);               // 设置引脚 EXIO0 和 EXIO1 模式为输入 
        // ret = esp_io_expander_get_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, &input_level_mask);             // 获取引脚 EXIO0 和 EXIO1 的电平状态,存放在 input_level_mask 中

        // ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_3, IO_EXPANDER_OUTPUT);              // 设置引脚 EXIO2 和 EXIO3 模式为输出
        // ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_3, 1);                             // 将引脚电平设置为 1
        // ret = esp_io_expander_print_state(io_expander);                                                                             // 打印引脚状态

        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_OUTPUT);                 // 设置引脚 EXIO0 和 EXIO1 模式为输出
        ESP_ERROR_CHECK(ret);
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 1);                                // 复位 LCD 与 TouchPad
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(300));
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 0);                                // 复位 LCD 与 TouchPad
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(300));
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 1);                                // 复位 LCD 与 TouchPad
        ESP_ERROR_CHECK(ret);
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize QSPI bus");

        const spi_bus_config_t bus_config = TAIJIPI_SPD2010_PANEL_BUS_QSPI_CONFIG(QSPI_PIN_NUM_LCD_PCLK,
                                                                        QSPI_PIN_NUM_LCD_DATA0,
                                                                        QSPI_PIN_NUM_LCD_DATA1,
                                                                        QSPI_PIN_NUM_LCD_DATA2,
                                                                        QSPI_PIN_NUM_LCD_DATA3,
                                                                        QSPI_LCD_H_RES * 80 * sizeof(uint16_t));
        ESP_ERROR_CHECK(spi_bus_initialize(QSPI_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));
    }

    void InitializeSpd2010Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGI(TAG, "Install panel IO");
        
        const esp_lcd_panel_io_spi_config_t io_config = SPD2010_PANEL_IO_QSPI_CONFIG(QSPI_PIN_NUM_LCD_CS, NULL, NULL);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)QSPI_LCD_HOST, &io_config, &panel_io));

        ESP_LOGI(TAG, "Install SPD2010 panel driver");
        
        spd2010_vendor_config_t vendor_config = {
            .flags = {
                .use_qspi_interface = 1,
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = QSPI_PIN_NUM_LCD_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
            .bits_per_pixel = QSPI_LCD_BIT_PER_PIXEL,    // Implemented by LCD command `3Ah` (16/18)
            .vendor_config = &vendor_config,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_spd2010(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_disp_on_off(panel, true);
        if (DISPLAY_SWAP_XY) {
            esp_lcd_panel_swap_xy(panel, true);
        }
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new PaopaoPetDisplay(panel_io, panel);
    }

    bool TryInitializeTouchController(TouchControllerType type, const esp_lcd_touch_config_t& touch_cfg) {
        const uint8_t address = type == TouchControllerType::Spd2010
            ? ESP_LCD_TOUCH_IO_I2C_SPD2010_ADDRESS
            : ESP_LCD_TOUCH_IO_I2C_CST9217_ADDRESS;
        const char* name = type == TouchControllerType::Spd2010 ? "SPD2010" : "CST9217";

        esp_err_t err = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Touch controller %s not found at 0x%02x: %s", name, address, esp_err_to_name(err));
            return false;
        }

        if (type == TouchControllerType::Spd2010) {
            if (!spd2010_touch_reader_.Initialize(i2c_bus_)) {
                return false;
            }
            touch_reader_ = &spd2010_touch_reader_;
            static_cast<PaopaoPetDisplay*>(display_)->AttachTouch(touch_reader_);
            ESP_LOGI(TAG, "Touch controller initialized: %s at 0x%02x", touch_reader_->Name(), address);
            return true;
        }

        esp_lcd_panel_io_i2c_config_t touch_io_config = {};
        touch_io_config = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
        touch_io_config.scl_speed_hz = 400 * 1000;

        esp_lcd_panel_io_handle_t touch_io_handle = nullptr;
        err = esp_lcd_new_panel_io_i2c(i2c_bus_, &touch_io_config, &touch_io_handle);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Touch panel IO init failed for %s: %s", name, esp_err_to_name(err));
            return false;
        }

        esp_lcd_touch_handle_t touch_handle = nullptr;
        err = esp_lcd_touch_new_i2c_cst9217(touch_io_handle, &touch_cfg, &touch_handle);

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Touch controller %s init failed: %s", name, esp_err_to_name(err));
            esp_lcd_panel_io_del(touch_io_handle);
            return false;
        }

        touch_io_handle_ = touch_io_handle;
        touch_handle_ = touch_handle;
        touch_reader_ = new EspLcdTouchReader(touch_handle_);
        static_cast<PaopaoPetDisplay*>(display_)->AttachTouch(touch_reader_);
        ESP_LOGI(TAG, "Touch controller initialized: %s at 0x%02x", name, address);
        return true;
    }

    void InitializeTouch() {
        esp_lcd_touch_config_t touch_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = TP_PIN_NUM_RST,
            .int_gpio_num = TP_PIN_NUM_INT,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = DISPLAY_SWAP_XY,
                .mirror_x = DISPLAY_MIRROR_X,
                .mirror_y = DISPLAY_MIRROR_Y,
            },
        };

        if (TryInitializeTouchController(TouchControllerType::Spd2010, touch_cfg)) {
            return;
        }
        if (TryInitializeTouchController(TouchControllerType::Cst9217, touch_cfg)) {
            return;
        }

        ESP_LOGW(TAG, "No supported touch controller found");
    }

    void InitializeMotion() {
        if (!motion_.Initialize(i2c_bus_)) {
            return;
        }

        xTaskCreatePinnedToCore(
            MotionTask,
            "paopao_motion",
            3072,
            this,
            3,
            &motion_task_,
            1
        );
    }

    void RunMotionLoop() {
        int16_t last_x = 0;
        int16_t last_y = 0;
        int16_t last_z = 0;
        bool has_last = false;
        uint32_t last_shake_ms = 0;
        uint8_t shake_sample_count = 0;

        while (true) {
            int16_t x = 0;
            int16_t y = 0;
            int16_t z = 0;
            const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
            if (motion_.ReadAcceleration(x, y, z)) {
                if (has_last) {
                    const int32_t delta =
                        std::abs((int32_t)x - last_x) +
                        std::abs((int32_t)y - last_y) +
                        std::abs((int32_t)z - last_z);

                    const bool touch_suppressed =
                        static_cast<PaopaoPetDisplay*>(display_)->IsTouchMotionSuppressed(now_ms);
                    if (touch_suppressed) {
                        shake_sample_count = 0;
                    } else if (delta >= k_shake_delta_threshold) {
                        if (shake_sample_count < k_shake_required_samples) {
                            shake_sample_count++;
                        }
                    } else {
                        shake_sample_count = 0;
                    }

                    if (shake_sample_count >= k_shake_required_samples &&
                        now_ms - last_shake_ms >= k_shake_cooldown_ms) {
                        last_shake_ms = now_ms;
                        shake_sample_count = 0;
                        static_cast<PaopaoPetDisplay*>(display_)->DispatchPetTrigger(PAOPAO_PET_TRIGGER_LOCAL_SHAKE);
                    }
                }

                last_x = x;
                last_y = y;
                last_z = z;
                has_last = true;
            }

            vTaskDelay(pdMS_TO_TICKS(k_motion_poll_ms));
        }
    }

    static void MotionTask(void* arg) {
        static_cast<CustomBoard*>(arg)->RunMotionLoop();
    }
 
    void InitializeButtonsCustom() {
        gpio_reset_pin(BOOT_BUTTON_GPIO);                                     
        gpio_set_direction(BOOT_BUTTON_GPIO, GPIO_MODE_INPUT);   
        gpio_reset_pin(PWR_BUTTON_GPIO);                                     
        gpio_set_direction(PWR_BUTTON_GPIO, GPIO_MODE_INPUT);   
        gpio_reset_pin(PWR_Control_PIN);                                     
        gpio_set_direction(PWR_Control_PIN, GPIO_MODE_OUTPUT);     
        // gpio_set_level(PWR_Control_PIN, false);
        gpio_set_level(PWR_Control_PIN, true);
    }

    void InitializeButtons() {
        instance_ = this;
        InitializeButtonsCustom();

        // Boot Button
        button_config_t boot_btn_config = {
            .long_press_time = 2000,
            .short_press_time = 0
        };
        boot_btn_driver_ = (button_driver_t*)calloc(1, sizeof(button_driver_t));
        boot_btn_driver_->enable_power_save = false;
        boot_btn_driver_->get_key_level = [](button_driver_t *button_driver) -> uint8_t {
            return !gpio_get_level(BOOT_BUTTON_GPIO);
        };
        ESP_ERROR_CHECK(iot_button_create(&boot_btn_config, boot_btn_driver_, &boot_btn));
        iot_button_register_cb(boot_btn, BUTTON_SINGLE_CLICK, nullptr, [](void* button_handle, void* usr_data) {
            auto self = static_cast<CustomBoard*>(usr_data);
            auto& app = Application::GetInstance();
            static_cast<PaopaoPetDisplay*>(self->display_)->DispatchPetTrigger(PAOPAO_PET_TRIGGER_LOCAL_TAP);
            if (app.GetDeviceState() == kDeviceStateStarting) {
                self->EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        }, this);
        iot_button_register_cb(boot_btn, BUTTON_LONG_PRESS_START, nullptr, [](void* button_handle, void* usr_data) {
            auto self = static_cast<CustomBoard*>(usr_data);
            static_cast<PaopaoPetDisplay*>(self->display_)->DispatchPetTrigger(PAOPAO_PET_TRIGGER_LOCAL_HOLD);
        }, this);

        // Power Button
        button_config_t pwr_btn_config = {
            .long_press_time = 5000,
            .short_press_time = 0
        };
        pwr_btn_driver_ = (button_driver_t*)calloc(1, sizeof(button_driver_t));
        pwr_btn_driver_->enable_power_save = false;
        pwr_btn_driver_->get_key_level = [](button_driver_t *button_driver) -> uint8_t {
            return !gpio_get_level(PWR_BUTTON_GPIO);
        };
        ESP_ERROR_CHECK(iot_button_create(&pwr_btn_config, pwr_btn_driver_, &pwr_btn));
        iot_button_register_cb(pwr_btn, BUTTON_SINGLE_CLICK, nullptr, [](void* button_handle, void* usr_data) {
            // 短按无处理
        }, this);
        iot_button_register_cb(pwr_btn, BUTTON_LONG_PRESS_START, nullptr, [](void* button_handle, void* usr_data) {
            auto self = static_cast<CustomBoard*>(usr_data);
            if(self->GetBacklight()->brightness() > 0) {
                self->GetBacklight()->SetBrightness(0);
                gpio_set_level(PWR_Control_PIN, false);
            }
            else {
                self->GetBacklight()->RestoreBrightness();
                gpio_set_level(PWR_Control_PIN, true);
            }
        }, this);
    }

public:
    CustomBoard() { 
        InitializeI2c();
        InitializeTca9554();
        InitializeSpi();
        InitializeSpd2010Display();
        InitializeTouch();
        InitializeButtons();
        InitializeMotion();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_LEFT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_RIGHT); // I2S_STD_SLOT_LEFT / I2S_STD_SLOT_RIGHT / I2S_STD_SLOT_BOTH

        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(CustomBoard);

CustomBoard* CustomBoard::instance_ = nullptr;
