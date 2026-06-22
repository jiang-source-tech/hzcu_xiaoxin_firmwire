#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/gif/lvgl_gif.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "assets/lang_config.h"

#include <esp_check.h>
#include <esp_app_desc.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <font_awesome.h>
#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_spd2010.h>
#include <esp_lcd_touch.h>
#include <esp_lcd_touch_cst9217.h>
#include <esp_lcd_touch_spd2010.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include "esp_io_expander_tca9554.h"
#include <iot_button.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <time.h>

extern "C" {
#include "paopao_pet_emotion.h"
#include "paopao_pet_mood.h"
#include "paopao_pet_gif_assets.h"
#include "paopao_pet_state.h"
#include "paopao_pet_trigger.h"
#include "xiaoxin_battery_level.h"
#include "xiaoxin_battery_state.h"
#include "xiaoxin_card_pager.h"
#include "xiaoxin_overview_model.h"
#include "xiaoxin_power_control.h"
#include "xiaoxin_settings_model.h"
#include "xiaoxin_system_overlay.h"
}

#define TAG "waveshare_lcd_1_46"

extern const uint8_t assets_images_idle_gif_start[] asm("_binary_idle_gif_start");
extern const uint8_t assets_images_idle_gif_end[] asm("_binary_idle_gif_end");
extern const uint8_t assets_images_working_gif_start[] asm("_binary_working_gif_start");
extern const uint8_t assets_images_working_gif_end[] asm("_binary_working_gif_end");
extern const uint8_t assets_images_speaking_fixed_gif_start[] asm("_binary_speaking_fixed_gif_start");
extern const uint8_t assets_images_speaking_fixed_gif_end[] asm("_binary_speaking_fixed_gif_end");
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
extern const uint8_t assets_images_happy_gif_start[] asm("_binary_happy_gif_start");
extern const uint8_t assets_images_happy_gif_end[] asm("_binary_happy_gif_end");

class CustomBoard;
static int BoardBatteryVoltageMv();
extern const uint8_t assets_images_crying_gif_start[] asm("_binary_crying_gif_start");
extern const uint8_t assets_images_crying_gif_end[] asm("_binary_crying_gif_end");
extern const uint8_t assets_images_anxiety_gif_start[] asm("_binary_anxiety_gif_start");
extern const uint8_t assets_images_anxiety_gif_end[] asm("_binary_anxiety_gif_end");
extern const uint8_t assets_images_tired_gif_start[] asm("_binary_tired_gif_start");
extern const uint8_t assets_images_tired_gif_end[] asm("_binary_tired_gif_end");
extern const uint8_t assets_images_stamp_gif_start[] asm("_binary_stamp_gif_start");
extern const uint8_t assets_images_stamp_gif_end[] asm("_binary_stamp_gif_end");

static constexpr uint32_t k_touch_hold_ms = 1200;
static constexpr int16_t k_touch_drag_min_px = 42;
static constexpr uint32_t k_touch_motion_suppress_ms = 600;
static constexpr uint32_t k_touch_poll_ms = 16;
static constexpr uint32_t k_pet_render_task_stack_bytes = 12 * 1024;
static constexpr int k_pet_mood_low_battery_percent = 20;
static constexpr uint32_t k_power_off_release_poll_ms = 20;
static constexpr adc_channel_t k_battery_adc_channel = ADC_CHANNEL_7;
static constexpr uint8_t k_battery_adc_samples = 10;
static constexpr int k_battery_voltage_divider = 3;
static constexpr uint32_t k_motion_poll_ms = 50;
static constexpr uint32_t k_shake_cooldown_ms = 1800;
static constexpr int32_t k_shake_delta_threshold = 12000;
static constexpr uint8_t k_shake_required_samples = 3;
static constexpr bool k_ui_perf_trace_enabled = false;
static constexpr uint32_t k_ui_perf_log_interval_ms = 1000;
static constexpr uint16_t k_white_rgb565 = 0xFFFF;
static constexpr uint32_t k_pet_image_scale_base = LV_SCALE_NONE;
static constexpr uint16_t k_pet_target_visual_longest = 162;
static constexpr uint16_t k_card_snap_min_anim_ms = 150;
static constexpr uint16_t k_card_snap_max_anim_ms = 240;
static constexpr uint8_t k_card_glass_count = XIAOXIN_CARD_NOTIFICATION_MAX;
static constexpr uint8_t k_notification_indicator_dot_count = XIAOXIN_CARD_NOTIFICATION_MAX;
static constexpr uint8_t k_overview_row_count = 4;
static constexpr uint8_t k_overview_sep_count = 3;
static constexpr uint8_t k_settings_item_max_count = 6;
static constexpr int16_t k_settings_panel_w = 264;
static constexpr int16_t k_settings_panel_h = 250;
static constexpr int16_t k_settings_panel_radius = 28;
static constexpr int16_t k_settings_title_y = 22;
static constexpr int16_t k_settings_row_x = 22;
static constexpr int16_t k_settings_row_y = 64;
static constexpr int16_t k_settings_row_w = 220;
static constexpr int16_t k_settings_row_h = 38;
static constexpr int16_t k_settings_row_pitch = 42;
static constexpr uint32_t k_settings_panel_bg = 0x111827;
static constexpr uint32_t k_settings_panel_border = 0x4a9eff;
static constexpr uint32_t k_settings_text_primary = 0xe8eaed;
static constexpr uint32_t k_settings_text_secondary = 0x7d9cc6;
static constexpr uint32_t k_card_layer_bg_color = 0xe9edf3;
static constexpr lv_opa_t k_card_layer_bg_opa = static_cast<lv_opa_t>(18);
static constexpr uint32_t k_page_title_color = 0x111111;
static constexpr uint32_t k_card_bg_color = 0x17181d;
static constexpr uint32_t k_card_border_color = 0x5a5f6b;
static constexpr uint32_t k_card_shadow_color = 0x000000;
static constexpr lv_opa_t k_glass_bg_opa[k_card_glass_count] = {
    static_cast<lv_opa_t>(174),
    static_cast<lv_opa_t>(124),
    static_cast<lv_opa_t>(82),
    static_cast<lv_opa_t>(52),
    static_cast<lv_opa_t>(36),
    static_cast<lv_opa_t>(24)
};
static constexpr lv_opa_t k_glass_border_opa[k_card_glass_count] = {
    static_cast<lv_opa_t>(44),
    static_cast<lv_opa_t>(20),
    static_cast<lv_opa_t>(12),
    static_cast<lv_opa_t>(8),
    static_cast<lv_opa_t>(6),
    static_cast<lv_opa_t>(4)
};
static constexpr int16_t k_glass_radius = 34;
static constexpr int16_t k_glass_width = 268;
static constexpr int16_t k_glass_height = 150;
static constexpr int16_t k_glass_pad_v = 18;
static constexpr int16_t k_glass_pad_h = 22;
static constexpr int16_t k_glass_text_x = 24;
static constexpr int16_t k_glass_text_y = 82;
static constexpr int16_t k_glass_text_w = 218;
static constexpr int16_t k_glass_tag_x = 154;
static constexpr int16_t k_glass_tag_w = 38;
static constexpr int16_t k_glass_arrow_x = 200;
static constexpr int16_t k_battery_meter_x = 154;
static constexpr int16_t k_battery_meter_y = 8;
static constexpr int16_t k_battery_meter_w = 48;
static constexpr int16_t k_battery_meter_h = 18;
static constexpr int16_t k_battery_segment_w = 8;
static constexpr int16_t k_battery_segment_h = 10;
static constexpr int16_t k_battery_segment_gap = 2;
static constexpr int16_t k_system_battery_w = 34;
static constexpr int16_t k_system_battery_h = 16;
static constexpr int16_t k_system_battery_tip_w = 3;
static constexpr int16_t k_system_overlay_w = 76;
static constexpr int16_t k_system_overlay_h = 24;
static constexpr int16_t k_system_overlay_right = 76;
static constexpr int16_t k_system_overlay_top = 50;
static constexpr int16_t k_system_wifi_w = 24;
static constexpr int16_t k_system_battery_x = 32;
static constexpr int16_t k_notification_icon_size = 46;
static constexpr int16_t k_notification_indicator_dot_size = 5;
static constexpr int16_t k_dot_size = 8;
static constexpr uint32_t k_dot_color_urgent = 0xff5e5b;
static constexpr uint32_t k_dot_color_warning = 0xffb84d;
static constexpr uint32_t k_dot_color_info = 0x4fc3f7;
static constexpr uint32_t k_battery_meter_border = XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR;
static constexpr uint32_t k_battery_meter_fill = XIAOXIN_SYSTEM_OVERLAY_ACTIVE_COLOR;
static constexpr uint32_t k_battery_meter_low = XIAOXIN_SYSTEM_OVERLAY_LOW_BATTERY_COLOR;
static constexpr uint32_t k_battery_meter_empty = 0x27413a;
static constexpr int16_t k_ov_icon_size = 30;
static constexpr int16_t k_ov_icon_radius = 10;
static constexpr int16_t k_overview_row_w = 270;
static constexpr int16_t k_overview_row_h = 58;
static constexpr int16_t k_overview_icon_x = 13;
static constexpr int16_t k_overview_text_x = 56;
static constexpr int16_t k_overview_text_w = 176;
static constexpr int16_t k_overview_arrow_x = 244;
static constexpr int16_t k_overview_sep_w = 206;
static constexpr uint32_t k_ov_icon_bg_course = 0x4fc3f7;
static constexpr uint32_t k_ov_icon_bg_nav = 0xa0d468;
static constexpr uint32_t k_ov_icon_bg_weather = 0xffb84d;
static constexpr uint32_t k_ov_icon_bg_todo = 0xac92ec;
static constexpr uint32_t k_title_accent = 0x77c7ff;
static constexpr uint32_t k_text_primary = 0xe8eaed;
static constexpr uint32_t k_text_secondary = 0x7d9cc6;
static constexpr uint32_t k_text_dimmed = 0xa9b8ca;
static constexpr uint32_t k_tag_text = 0x9fd8ff;
static constexpr uint32_t k_tag_bg = 0x1d3654;
static constexpr uint32_t k_indicator_top = 0x2a4a6b;
static constexpr uint32_t k_indicator_home = 0x1e3350;
static constexpr uint32_t k_separator_color = 0x30597f;
static constexpr lv_opa_t k_separator_opa = static_cast<lv_opa_t>(46);
static constexpr lv_opa_t k_ov_icon_bg_opa = static_cast<lv_opa_t>(132);
static constexpr uint32_t k_entry_fade_ms = 120;
static constexpr uint32_t k_entry_stagger_ms = 50;
static constexpr uint32_t k_notification_switch_anim_ms = 160;
static constexpr int16_t k_notification_dismiss_intent_px = 18;
static constexpr int16_t k_notification_dismiss_threshold_px = 72;
static constexpr uint32_t k_notification_dismiss_fly_ms = 160;
static constexpr uint32_t k_notification_dismiss_rebound_ms = 120;
static constexpr int16_t k_glass_y_start = 96;
static constexpr int16_t k_glass_stack_pitch = 15;
static constexpr int16_t k_notification_slide_pitch = 116;
static constexpr int16_t k_notification_clear_button_w = 104;
static constexpr int16_t k_notification_clear_button_h = 32;
static constexpr int16_t k_notification_clear_button_y = 46;
static constexpr int16_t k_notification_empty_panel_w = 164;
static constexpr int16_t k_notification_empty_panel_h = 52;
static constexpr int16_t k_notification_empty_panel_y = 176;
static constexpr uint32_t k_notification_empty_panel_bg = 0xffffff;
static constexpr lv_opa_t k_notification_empty_panel_opa = static_cast<lv_opa_t>(172);
static constexpr lv_opa_t k_notification_empty_panel_border_opa = static_cast<lv_opa_t>(34);
static constexpr int16_t k_overview_time_y = 26;
static constexpr int16_t k_overview_date_y = 49;
static constexpr int16_t k_overview_time_w = 260;
static constexpr int16_t k_overview_y_start = 78;
static constexpr int16_t k_overview_row_pitch = 58;
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

struct GlassCard {
    lv_obj_t* container = nullptr;
    lv_obj_t* dot = nullptr;
    lv_obj_t* text_box = nullptr;
    lv_obj_t* icon_bg = nullptr;
    lv_obj_t* icon = nullptr;
    lv_obj_t* time = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* body = nullptr;
    lv_obj_t* tag = nullptr;
    lv_obj_t* arrow = nullptr;
    lv_obj_t* battery_meter = nullptr;
    lv_obj_t* battery_segments[4] = {};
    lv_obj_t* pager = nullptr;
    lv_obj_t* pager_total = nullptr;
    uint8_t visible_index = 0xff;
};

struct OverviewRow {
    lv_obj_t* container = nullptr;
    lv_obj_t* icon_bg = nullptr;
    lv_obj_t* icon = nullptr;
    lv_obj_t* text_box = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* body = nullptr;
    lv_obj_t* detail = nullptr;
    lv_obj_t* arrow = nullptr;
};

enum class NotificationGestureMode {
    None,
    VerticalScroll,
    DismissCard,
    ClearAllPress,
};

enum class SettingsView {
    List,
    Brightness,
    Wifi,
    About,
};

struct SettingsRow {
    lv_obj_t* container = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* value = nullptr;
    xiaoxin_settings_item_t item = XIAOXIN_SETTINGS_ITEM_ABOUT;
};

static PaopaoGifBinary PaopaoGifAssetForState(paopao_pet_state_t state) {
    switch (state) {
        case PAOPAO_PET_STATE_IDLE:
            return {assets_images_idle_gif_start, assets_images_idle_gif_end};
        case PAOPAO_PET_STATE_WORKING:
            return {assets_images_working_gif_start, assets_images_working_gif_end};
        case PAOPAO_PET_STATE_SPEAKING:
            return {assets_images_speaking_fixed_gif_start, assets_images_speaking_fixed_gif_end};
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
        case PAOPAO_PET_STATE_HAPPY:
            return {assets_images_happy_gif_start, assets_images_happy_gif_end};
        case PAOPAO_PET_STATE_CRYING:
            return {assets_images_crying_gif_start, assets_images_crying_gif_end};
        case PAOPAO_PET_STATE_ANXIETY:
            return {assets_images_anxiety_gif_start, assets_images_anxiety_gif_end};
        case PAOPAO_PET_STATE_TIRED:
            return {assets_images_tired_gif_start, assets_images_tired_gif_end};
        case PAOPAO_PET_STATE_STAMP:
            return {assets_images_stamp_gif_start, assets_images_stamp_gif_end};
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
        case PAOPAO_PET_STATE_HAPPY:
            return 201;
        case PAOPAO_PET_STATE_CRYING:
            return 230;
        case PAOPAO_PET_STATE_ANXIETY:
            return 173;
        case PAOPAO_PET_STATE_TIRED:
            return 164;
        case PAOPAO_PET_STATE_STAMP:
            return 166;
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
        InitializeCardPagerLayer();
        RaiseOverlayObjects();
        lv_obj_invalidate(screen);

        const uint32_t now_ms = NowMs();
        paopao_pet_trigger_init(&trigger_, now_ms);
        paopao_pet_mood_init(&mood_, now_ms);
        xiaoxin_battery_state_init(&battery_context_, now_ms);
        battery_snapshot_ = xiaoxin_battery_state_snapshot(&battery_context_);
        xiaoxin_card_pager_init(&card_pager_, DISPLAY_HEIGHT);
        current_state_ = trigger_.displayed_state;
        PlayGifState(current_state_);

        if (render_task_ == nullptr) {
            xTaskCreatePinnedToCore(
                RenderTask,
                "paopao_pet",
                k_pet_render_task_stack_bytes,
                this,
                3,
                &render_task_,
                1
            );
        }
    }

    virtual void SetStatus(const char* status) override {
        if (status == nullptr) {
            return;
        }

        LcdDisplay::SetStatus(status);

        const bool status_error =
            StatusEquals(status, Lang::Strings::ERROR) ||
            Contains(status, "Error") || Contains(status, "error");

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
        } else if (status_error) {
            DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_VOICE_ERROR);
        } else if (IsBusyStatus(status)) {
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_CONNECTING);
        }
        {
            DisplayLockGuard lock(this);
            if (status_error) {
                AddVoiceFailureNotificationLocked(status);
            }
            RaiseOverlayObjects();
        }
    }

    virtual void SetEmotion(const char* emotion) override {
        const paopao_pet_trigger_event_t event = paopao_pet_trigger_for_emotion(emotion);
        if (event == PAOPAO_PET_TRIGGER_NONE) {
            return;
        }
        DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_SERVICE_EMOTION, event);
    }

    virtual void SetChatMessage(const char* role, const char* content) override {
        if (role == nullptr || content == nullptr) {
            return;
        }
        if (content[0] == '\0') {
            LcdDisplay::SetChatMessage(role, content);
            {
                DisplayLockGuard lock(this);
                if (system_bars_hidden_for_card_) {
                    bottom_bar_was_hidden_for_card_ = true;
                }
                RaiseOverlayObjects();
            }
            return;
        }

        if (std::strcmp(role, "user") == 0) {
            DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_CHAT_STARTED);
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_THINKING);
        } else if (std::strcmp(role, "assistant") == 0) {
            DispatchPetMoodEvent(PAOPAO_PET_MOOD_EVENT_ASSISTANT_REPLY);
            DispatchPetTrigger(PAOPAO_PET_TRIGGER_SPEAKING);
        }

        LcdDisplay::SetChatMessage(role, content);
        {
            DisplayLockGuard lock(this);
            if (system_bars_hidden_for_card_) {
                bottom_bar_was_hidden_for_card_ = IsHidden(bottom_bar_);
            }
            RaiseOverlayObjects();
        }
    }

    virtual void ClearChatMessages() override {
        LcdDisplay::ClearChatMessages();
        {
            DisplayLockGuard lock(this);
            if (system_bars_hidden_for_card_) {
                bottom_bar_was_hidden_for_card_ = true;
            }
            RaiseOverlayObjects();
        }
    }

    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override {
        LcdDisplay::ShowNotification(notification, duration_ms);
        {
            DisplayLockGuard lock(this);
            RaiseOverlayObjects();
        }
    }

    virtual void UpdateStatusBar(bool update_all = false) override {
        LcdDisplay::UpdateStatusBar(update_all);
        {
            DisplayLockGuard lock(this);
            RefreshBatterySnapshotLocked();
            HideLegacyLowBatteryPopupLocked();
            ApplySystemOverlayNetworkStyle();
            ApplyBatteryOverlayLevel();
            SyncLowBatteryNotificationLocked();
            SyncPetMoodDeviceStateLocked();
            RefreshOverviewPageIfVisible();
            RaiseOverlayObjects();
        }
    }

    virtual void SetPowerSaveMode(bool on) override {
        LvglDisplay::SetPowerSaveMode(on);
    }

    void DispatchPetTrigger(paopao_pet_trigger_event_t event) {
        DisplayLockGuard lock(this);
        DispatchPetTriggerLocked(event, NowMs());
    }

    void DispatchPetMoodEvent(
        paopao_pet_mood_event_t event,
        paopao_pet_trigger_event_t service_trigger = PAOPAO_PET_TRIGGER_NONE
    ) {
        DisplayLockGuard lock(this);
        DispatchPetMoodEventLocked(event, service_trigger, NowMs());
    }

    void DispatchLocalPetTrigger(
        paopao_pet_trigger_event_t trigger_event,
        paopao_pet_mood_event_t mood_event
    ) {
        DisplayLockGuard lock(this);
        DispatchLocalPetTriggerLocked(trigger_event, mood_event, NowMs());
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

    bool IsSettingsOpen() {
        DisplayLockGuard lock(this);
        return settings_open_;
    }

    void OpenSettingsOverlay() {
        DisplayLockGuard lock(this);
        if (settings_open_) {
            return;
        }
        EnsureSettingsOverlayLocked();
        settings_view_ = SettingsView::List;
        settings_open_ = true;
        RenderSettingsListLocked();
        lv_obj_remove_flag(settings_layer_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(settings_layer_);
        RaiseOverlayObjects();
    }

    void CloseSettingsOverlay() {
        DisplayLockGuard lock(this);
        if (!settings_open_) {
            return;
        }
        settings_open_ = false;
        settings_view_ = SettingsView::List;
        AddFlagIfCreated(settings_layer_, LV_OBJ_FLAG_HIDDEN);
        RaiseOverlayObjects();
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
    lv_obj_t* card_layer_ = nullptr;
    lv_obj_t* system_overlay_ = nullptr;
    lv_obj_t* battery_overlay_ = nullptr;
    lv_obj_t* battery_overlay_box_ = nullptr;
    lv_obj_t* battery_overlay_fill_ = nullptr;
    lv_obj_t* battery_overlay_cap_ = nullptr;
    lv_obj_t* card_title_label_ = nullptr;
    lv_obj_t* overview_time_label_ = nullptr;
    lv_obj_t* overview_date_label_ = nullptr;
    lv_obj_t* pull_indicator_ = nullptr;
    lv_obj_t* home_indicator_ = nullptr;
    lv_obj_t* notification_clear_button_ = nullptr;
    lv_obj_t* notification_clear_label_ = nullptr;
    lv_obj_t* notification_empty_panel_ = nullptr;
    lv_obj_t* notification_empty_label_ = nullptr;
    lv_obj_t* notification_indicator_dots_[k_notification_indicator_dot_count] = {};
    lv_obj_t* settings_layer_ = nullptr;
    lv_obj_t* settings_panel_ = nullptr;
    lv_obj_t* settings_title_label_ = nullptr;
    lv_obj_t* settings_hint_label_ = nullptr;
    SettingsRow settings_rows_[k_settings_item_max_count];
    xiaoxin_settings_item_t settings_items_[k_settings_item_max_count] = {};
    uint8_t settings_item_count_ = 0;
    SettingsView settings_view_ = SettingsView::List;
    bool settings_open_ = false;
    GlassCard glass_cards_[k_card_glass_count];
    OverviewRow overview_rows_[k_overview_row_count];
    lv_obj_t* overview_separators_[k_overview_sep_count] = {};
    xiaoxin_card_page_t rendered_card_page_ = XIAOXIN_CARD_PAGE_HOME;
    bool rendered_card_prepare_entry_ = false;
    bool card_page_rendered_ = false;
    lv_img_dsc_t pet_frame_dsc_ = {};
    uint16_t* pet_frame_buffer_ = nullptr;
    lv_img_dsc_t gif_source_dsc_ = {};
    std::unique_ptr<LvglGif> pet_gif_controller_ = nullptr;
    TouchReader* touch_ = nullptr;
    xiaoxin_card_pager_t card_pager_ = {};
    xiaoxin_overview_snapshot_t overview_snapshot_ = {};
    paopao_pet_trigger_context_t trigger_;
    paopao_pet_mood_context_t mood_ = {};
    xiaoxin_battery_context_t battery_context_ = {};
    xiaoxin_battery_snapshot_t battery_snapshot_ = {};
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
    uint32_t ui_perf_last_log_ms_ = 0;
    uint32_t ui_perf_drag_calls_ = 0;
    uint32_t ui_perf_drag_total_us_ = 0;
    uint32_t ui_perf_drag_max_us_ = 0;
    uint32_t ui_perf_pet_copy_calls_ = 0;
    uint32_t ui_perf_pet_copy_total_us_ = 0;
    uint32_t ui_perf_pet_copy_max_us_ = 0;
    uint32_t ui_perf_touch_loop_calls_ = 0;
    uint32_t ui_perf_touch_loop_total_us_ = 0;
    uint32_t ui_perf_touch_loop_max_us_ = 0;
    bool pet_animation_paused_for_card_ = false;
    bool system_bars_hidden_for_card_ = false;
    bool top_bar_was_hidden_for_card_ = false;
    bool status_bar_was_hidden_for_card_ = false;
    bool bottom_bar_was_hidden_for_card_ = false;
    int16_t notification_scroll_y_ = 0;
    int16_t notification_drag_start_scroll_y_ = 0;
    bool notification_drag_visual_owns_cards_ = false;
    NotificationGestureMode notification_gesture_mode_ = NotificationGestureMode::None;
    int8_t notification_pressed_slot_ = -1;
    uint8_t notification_pressed_visible_index_ = 0xff;
    int16_t notification_card_drag_x_ = 0;
    bool notification_dismiss_animating_ = false;
    uint8_t notification_animating_visible_index_ = 0xff;
    bool low_battery_notification_active_ = false;
    int last_low_battery_notification_level_ = -1;
    xiaoxin_battery_state_t last_low_battery_notification_state_ =
        XIAOXIN_BATTERY_STATE_UNKNOWN;
    bool network_notification_active_ = false;
    xiaoxin_system_overlay_network_state_t last_network_notification_state_ =
        XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED;

    static uint32_t NowMs() {
        return (uint32_t)(esp_timer_get_time() / 1000ULL);
    }

    static bool Contains(const char* text, const char* needle) {
        return text != nullptr && needle != nullptr && std::strstr(text, needle) != nullptr;
    }

    static bool StatusEquals(const char* status, const char* expected) {
        return status != nullptr && expected != nullptr && std::strcmp(status, expected) == 0;
    }

    void RefreshNotificationPageIfVisibleLocked() {
        if (xiaoxin_card_pager_current_page(&card_pager_) != XIAOXIN_CARD_PAGE_NOTIFICATIONS) {
            return;
        }
        RenderNotificationPageAfterDataChange();
    }

    void UpsertNotificationEventLocked(const xiaoxin_notification_event_t& event) {
        if (xiaoxin_card_pager_notification_upsert_event(&card_pager_, &event)) {
            notification_scroll_y_ = ClampNotificationScrollY(
                notification_scroll_y_,
                xiaoxin_card_pager_notification_count(&card_pager_)
            );
            RefreshNotificationPageIfVisibleLocked();
        }
    }

    void RemoveNotificationEventLocked(xiaoxin_notification_event_type_t type) {
        if (xiaoxin_card_pager_notification_remove_event(&card_pager_, type)) {
            notification_scroll_y_ = ClampNotificationScrollY(
                notification_scroll_y_,
                xiaoxin_card_pager_notification_count(&card_pager_)
            );
            RefreshNotificationPageIfVisibleLocked();
        }
    }

    xiaoxin_battery_load_t CurrentBatteryLoad() const {
        const auto state = Application::GetInstance().GetDeviceState();
        switch (state) {
            case kDeviceStateListening:
            case kDeviceStateSpeaking:
                return XIAOXIN_BATTERY_LOAD_VOICE_ACTIVE;
            default:
                return XIAOXIN_BATTERY_LOAD_IDLE;
        }
    }

    void RefreshBatterySnapshotLocked() {
        int level = 0;
        bool charging = false;
        bool discharging = true;
        const bool sample_valid = Board::GetInstance().GetBatteryLevel(level, charging, discharging);
        const int voltage_mv = sample_valid ? BoardBatteryVoltageMv() : 0;
        battery_snapshot_ = xiaoxin_battery_state_update(
            &battery_context_,
            voltage_mv,
            sample_valid,
            CurrentBatteryLoad(),
            NowMs()
        );
    }

    void SyncLowBatteryNotificationLocked() {
        const bool battery_powered =
            battery_snapshot_.power_source == XIAOXIN_BATTERY_POWER_BATTERY;
        const bool low =
            battery_powered &&
            (battery_snapshot_.state == XIAOXIN_BATTERY_STATE_LOW ||
             battery_snapshot_.state == XIAOXIN_BATTERY_STATE_CRITICAL);
        if (!low) {
            if (low_battery_notification_active_) {
                RemoveNotificationEventLocked(XIAOXIN_NOTIFICATION_EVENT_LOW_BATTERY);
            }
            low_battery_notification_active_ = false;
            last_low_battery_notification_level_ = battery_snapshot_.estimated_percent;
            last_low_battery_notification_state_ = battery_snapshot_.state;
            return;
        }

        const char* body = battery_snapshot_.state == XIAOXIN_BATTERY_STATE_CRITICAL
            ? "电量很低，请尽快充电"
            : "电量偏低，请尽快充电";

        if (low_battery_notification_active_ &&
            last_low_battery_notification_level_ == battery_snapshot_.estimated_percent &&
            last_low_battery_notification_state_ == battery_snapshot_.state) {
            return;
        }

        const xiaoxin_notification_event_t event = {
            .type = XIAOXIN_NOTIFICATION_EVENT_LOW_BATTERY,
            .title = nullptr,
            .body = body,
            .tag = nullptr,
            .priority = 0,
            .ttl_ms = 0,
        };
        UpsertNotificationEventLocked(event);
        low_battery_notification_active_ = true;
        last_low_battery_notification_level_ = battery_snapshot_.estimated_percent;
        last_low_battery_notification_state_ = battery_snapshot_.state;
    }

    void SyncNetworkNotificationLocked(xiaoxin_system_overlay_network_state_t state) {
        const bool disconnected = state != XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED;
        if (!disconnected) {
            if (network_notification_active_) {
                RemoveNotificationEventLocked(XIAOXIN_NOTIFICATION_EVENT_WIFI_DISCONNECTED);
            }
            network_notification_active_ = false;
            last_network_notification_state_ = state;
            return;
        }

        if (network_notification_active_ &&
            last_network_notification_state_ == state) {
            return;
        }

        const char* body = state == XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONFIGURING
            ? "正在配网或等待连接"
            : "WiFi 已断开，正在重新连接";
        const xiaoxin_notification_event_t event = {
            .type = XIAOXIN_NOTIFICATION_EVENT_WIFI_DISCONNECTED,
            .title = nullptr,
            .body = body,
            .tag = nullptr,
            .priority = 0,
            .ttl_ms = 0,
        };
        UpsertNotificationEventLocked(event);
        network_notification_active_ = true;
        last_network_notification_state_ = state;
    }

    void AddVoiceFailureNotificationLocked(const char* status) {
        const xiaoxin_notification_event_t event = {
            .type = XIAOXIN_NOTIFICATION_EVENT_VOICE_RECOGNITION_FAILED,
            .title = nullptr,
            .body = status != nullptr ? status : "没听清，请再说一次",
            .tag = nullptr,
            .priority = 0,
            .ttl_ms = 8000,
        };
        UpsertNotificationEventLocked(event);
    }

    static bool PointInObj(lv_obj_t* obj, uint16_t x, uint16_t y) {
        if (obj == nullptr || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
            return false;
        }
        lv_area_t coords;
        lv_obj_get_coords(obj, &coords);
        return x >= coords.x1 && x <= coords.x2 && y >= coords.y1 && y <= coords.y2;
    }

    xiaoxin_settings_caps_t SettingsCaps() const {
        const xiaoxin_settings_caps_t caps = {
            .has_audio_output = false,
            .has_vibration = false,
            .has_power_save_scheduler = false,
        };
        return caps;
    }

    void EnsureSettingsOverlayLocked() {
        if (settings_layer_ != nullptr) {
            return;
        }
        lv_obj_t* screen = lv_screen_active();
        settings_layer_ = lv_obj_create(screen);
        lv_obj_remove_style_all(settings_layer_);
        lv_obj_set_size(settings_layer_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_obj_set_style_bg_color(settings_layer_, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(settings_layer_, static_cast<lv_opa_t>(118), 0);
        lv_obj_clear_flag(settings_layer_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(settings_layer_, LV_OBJ_FLAG_HIDDEN);

        settings_panel_ = lv_obj_create(settings_layer_);
        lv_obj_remove_style_all(settings_panel_);
        lv_obj_set_size(settings_panel_, k_settings_panel_w, k_settings_panel_h);
        lv_obj_set_style_radius(settings_panel_, k_settings_panel_radius, 0);
        lv_obj_set_style_bg_color(settings_panel_, lv_color_hex(k_settings_panel_bg), 0);
        lv_obj_set_style_bg_opa(settings_panel_, static_cast<lv_opa_t>(220), 0);
        lv_obj_set_style_border_width(settings_panel_, 1, 0);
        lv_obj_set_style_border_color(settings_panel_, lv_color_hex(k_settings_panel_border), 0);
        lv_obj_set_style_border_opa(settings_panel_, LV_OPA_70, 0);
        lv_obj_clear_flag(settings_panel_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(settings_panel_, LV_ALIGN_CENTER, 0, 0);

        settings_title_label_ = lv_label_create(settings_panel_);
        lv_obj_set_style_text_color(settings_title_label_, lv_color_hex(k_settings_text_primary), 0);
        lv_obj_align(settings_title_label_, LV_ALIGN_TOP_MID, 0, k_settings_title_y);

        settings_hint_label_ = lv_label_create(settings_panel_);
        lv_obj_set_style_text_color(settings_hint_label_, lv_color_hex(k_settings_text_secondary), 0);
        lv_obj_align(settings_hint_label_, LV_ALIGN_BOTTOM_MID, 0, -18);

        for (uint8_t i = 0; i < k_settings_item_max_count; ++i) {
            SettingsRow& row = settings_rows_[i];
            row.container = lv_obj_create(settings_panel_);
            lv_obj_remove_style_all(row.container);
            lv_obj_set_size(row.container, k_settings_row_w, k_settings_row_h);
            lv_obj_set_style_radius(row.container, 14, 0);
            lv_obj_set_style_bg_color(row.container, lv_color_hex(0x1d3654), 0);
            lv_obj_set_style_bg_opa(row.container, static_cast<lv_opa_t>(122), 0);
            lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(row.container, LV_ALIGN_TOP_LEFT, k_settings_row_x, k_settings_row_y + i * k_settings_row_pitch);

            row.title = lv_label_create(row.container);
            lv_obj_set_style_text_color(row.title, lv_color_hex(k_settings_text_primary), 0);
            lv_obj_align(row.title, LV_ALIGN_LEFT_MID, 14, 0);

            row.value = lv_label_create(row.container);
            lv_obj_set_style_text_color(row.value, lv_color_hex(k_settings_text_secondary), 0);
            lv_obj_align(row.value, LV_ALIGN_RIGHT_MID, -14, 0);
        }
    }

    void HideSettingsRowsLocked() {
        for (uint8_t i = 0; i < k_settings_item_max_count; ++i) {
            AddFlagIfCreated(settings_rows_[i].container, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void RenderSettingsListLocked() {
        EnsureSettingsOverlayLocked();
        settings_view_ = SettingsView::List;
        lv_label_set_text(settings_title_label_, "璁剧疆");
        lv_label_set_text(settings_hint_label_, "BOOT 杩斿洖");
        settings_item_count_ = xiaoxin_settings_visible_items(SettingsCaps(), settings_items_, k_settings_item_max_count);
        HideSettingsRowsLocked();
        for (uint8_t i = 0; i < settings_item_count_; ++i) {
            SettingsRow& row = settings_rows_[i];
            row.item = settings_items_[i];
            lv_label_set_text(row.title, xiaoxin_settings_item_title(row.item));
            lv_label_set_text(row.value, "›");
            lv_obj_remove_flag(row.container, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void ApplySettingsBrightness(uint8_t brightness) {
        const uint8_t clamped = xiaoxin_settings_clamp_percent(brightness);
        Backlight* backlight = Board::GetInstance().GetBacklight();
        if (backlight != nullptr) {
            backlight->SetBrightness(clamped, true);
        }
    }

    void RequestSettingsWifiConfig() {
        CloseSettingsOverlay();
        static_cast<WifiBoard&>(Board::GetInstance()).EnterWifiConfigMode();
    }

    void RenderSettingsAboutPage() {
        settings_view_ = SettingsView::About;
        EnsureSettingsOverlayLocked();
        HideSettingsRowsLocked();
        const esp_app_desc_t* app_desc = esp_app_get_description();
        char text[160] = {};
        std::snprintf(
            text,
            sizeof(text),
            "鍥轰欢 %s\n%s\nWaveshare ESP32-S3 Touch LCD 1.46\n%s %s",
            app_desc != nullptr ? app_desc->version : "-",
            app_desc != nullptr ? app_desc->project_name : "ai_pet",
            app_desc != nullptr ? app_desc->date : "-",
            app_desc != nullptr ? app_desc->time : "-"
        );
        lv_label_set_text(settings_title_label_, "鍏充簬");
        lv_label_set_text(settings_hint_label_, text);
    }

    void OpenSettingsItemLocked(xiaoxin_settings_item_t item) {
        switch (item) {
            case XIAOXIN_SETTINGS_ITEM_BRIGHTNESS:
                settings_view_ = SettingsView::Brightness;
                lv_label_set_text(settings_title_label_, "浜害");
                lv_label_set_text(settings_hint_label_, "30  70  100");
                HideSettingsRowsLocked();
                break;
            case XIAOXIN_SETTINGS_ITEM_WIFI:
                settings_view_ = SettingsView::Wifi;
                lv_label_set_text(settings_title_label_, "Wi-Fi");
                lv_label_set_text(settings_hint_label_, "鐐瑰嚮閲嶆柊閰嶇綉");
                HideSettingsRowsLocked();
                break;
            case XIAOXIN_SETTINGS_ITEM_ABOUT:
                RenderSettingsAboutPage();
                break;
            default:
                RenderSettingsListLocked();
                break;
        }
    }

    void HandleSettingsTouch(uint16_t x, uint16_t y, bool pressed) {
        if (!pressed || touch_pressed_) {
            return;
        }
        if (settings_view_ != SettingsView::List) {
            RenderSettingsListLocked();
            return;
        }
        for (uint8_t i = 0; i < settings_item_count_; ++i) {
            if (PointInObj(settings_rows_[i].container, x, y)) {
                OpenSettingsItemLocked(settings_rows_[i].item);
                return;
            }
        }
    }

    int8_t NotificationCardSlotAtPoint(uint16_t x, uint16_t y) const {
        const uint8_t total = xiaoxin_card_pager_notification_count(&card_pager_);
        const uint8_t active_slot = NotificationIndexForScroll(notification_scroll_y_, total);

        if (active_slot < k_card_glass_count &&
            PointInObj(glass_cards_[active_slot].container, x, y)) {
            return (int8_t)active_slot;
        }

        for (int8_t slot = (int8_t)k_card_glass_count - 1; slot >= 0; --slot) {
            if ((uint8_t)slot == active_slot) {
                continue;
            }
            if (PointInObj(glass_cards_[slot].container, x, y)) {
                return slot;
            }
        }
        return -1;
    }

    bool NotificationClearButtonContains(uint16_t x, uint16_t y) const {
        return PointInObj(notification_clear_button_, x, y);
    }

    void AddUiPerfSample(uint32_t& calls, uint32_t& total_us, uint32_t& max_us, uint32_t elapsed_us) {
        if (!k_ui_perf_trace_enabled) {
            return;
        }
        calls++;
        total_us += elapsed_us;
        if (elapsed_us > max_us) {
            max_us = elapsed_us;
        }
    }

    void LogUiPerfSummary(uint32_t now_ms) {
        if (!k_ui_perf_trace_enabled) {
            return;
        }
        if (ui_perf_last_log_ms_ != 0 && now_ms - ui_perf_last_log_ms_ < k_ui_perf_log_interval_ms) {
            return;
        }
        ui_perf_last_log_ms_ = now_ms;

        const uint32_t drag_avg = ui_perf_drag_calls_ == 0 ? 0 : ui_perf_drag_total_us_ / ui_perf_drag_calls_;
        const uint32_t pet_avg = ui_perf_pet_copy_calls_ == 0 ? 0 : ui_perf_pet_copy_total_us_ / ui_perf_pet_copy_calls_;
        const uint32_t touch_avg = ui_perf_touch_loop_calls_ == 0 ? 0 : ui_perf_touch_loop_total_us_ / ui_perf_touch_loop_calls_;

        ESP_LOGI(
            TAG,
            "[UI-PERF] drag avg=%uus max=%uus calls=%u pet_copy avg=%uus max=%uus calls=%u touch_loop avg=%uus max=%uus calls=%u",
            (unsigned)drag_avg,
            (unsigned)ui_perf_drag_max_us_,
            (unsigned)ui_perf_drag_calls_,
            (unsigned)pet_avg,
            (unsigned)ui_perf_pet_copy_max_us_,
            (unsigned)ui_perf_pet_copy_calls_,
            (unsigned)touch_avg,
            (unsigned)ui_perf_touch_loop_max_us_,
            (unsigned)ui_perf_touch_loop_calls_
        );

        ui_perf_drag_calls_ = 0;
        ui_perf_drag_total_us_ = 0;
        ui_perf_drag_max_us_ = 0;
        ui_perf_pet_copy_calls_ = 0;
        ui_perf_pet_copy_total_us_ = 0;
        ui_perf_pet_copy_max_us_ = 0;
        ui_perf_touch_loop_calls_ = 0;
        ui_perf_touch_loop_total_us_ = 0;
        ui_perf_touch_loop_max_us_ = 0;
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
        if (IsCardLayerVisible()) {
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

    static bool IsHidden(lv_obj_t* obj) {
        return obj != nullptr && lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }

    static void RestoreHiddenFlag(lv_obj_t* obj, bool hidden) {
        if (obj == nullptr) {
            return;
        }
        if (hidden) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void HideLegacyLowBatteryPopupLocked() {
        if (low_battery_popup_ != nullptr) {
            lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    bool IsCardLayerVisible() const {
        return card_layer_ != nullptr && !lv_obj_has_flag(card_layer_, LV_OBJ_FLAG_HIDDEN);
    }

    void ApplyPetAnimationForCardPager() {
        if (pet_gif_controller_ == nullptr) {
            pet_animation_paused_for_card_ = false;
            return;
        }

        if (IsCardLayerVisible()) {
            if (pet_gif_controller_->IsPlaying()) {
                pet_gif_controller_->Pause();
            }
            pet_animation_paused_for_card_ = true;
            return;
        }

        if (pet_animation_paused_for_card_) {
            pet_gif_controller_->Resume();
            pet_animation_paused_for_card_ = false;
        }
    }

    void ApplySystemBarsForCardPager() {
        const bool card_visible = IsCardLayerVisible();
        if (card_visible) {
            return;
        }

        if (!system_bars_hidden_for_card_) {
            return;
        }

        RestoreHiddenFlag(top_bar_, top_bar_was_hidden_for_card_);
        RestoreHiddenFlag(status_bar_, status_bar_was_hidden_for_card_);
        RestoreHiddenFlag(bottom_bar_, bottom_bar_was_hidden_for_card_);
        system_bars_hidden_for_card_ = false;
    }

    void RaiseOverlayObjects() {
        HideLegacyLowBatteryPopupLocked();
        ApplySystemBarsForCardPager();
        if (IsCardLayerVisible()) {
            lv_obj_move_foreground(card_layer_);
            if (system_overlay_ != nullptr) {
                lv_obj_move_foreground(system_overlay_);
            }
            return;
        }

        if (top_bar_ != nullptr) {
            lv_obj_move_foreground(top_bar_);
        }
        if (status_bar_ != nullptr) {
            lv_obj_move_foreground(status_bar_);
        }
        if (bottom_bar_ != nullptr) {
            lv_obj_move_foreground(bottom_bar_);
        }
        if (system_overlay_ != nullptr) {
            lv_obj_move_foreground(system_overlay_);
        }
        if (settings_open_ && settings_layer_ != nullptr) {
            lv_obj_move_foreground(settings_layer_);
        }
    }

    void InitializeCardPagerLayer() {
        lv_obj_t* screen = lv_screen_active();

        if (network_label_ != nullptr) {
            lv_obj_add_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (battery_label_ != nullptr) {
            lv_obj_add_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);
        }

        auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
        const lv_font_t* icon_font = lvgl_theme != nullptr ? lvgl_theme->icon_font()->font() : nullptr;

        system_overlay_ = lv_obj_create(screen);
        lv_obj_remove_style_all(system_overlay_);
        lv_obj_set_size(system_overlay_, k_system_overlay_w, k_system_overlay_h);
        lv_obj_set_style_bg_opa(system_overlay_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(system_overlay_, 0, 0);
        lv_obj_set_style_layout(system_overlay_, LV_LAYOUT_NONE, 0);
        lv_obj_clear_flag(system_overlay_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(system_overlay_, LV_ALIGN_TOP_RIGHT, -k_system_overlay_right, k_system_overlay_top);

        network_label_ = lv_label_create(system_overlay_);
        lv_obj_set_width(network_label_, k_system_wifi_w);
        lv_obj_set_style_text_align(network_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(network_label_, lv_color_hex(k_battery_meter_fill), 0);
        lv_obj_set_style_text_opa(network_label_, XIAOXIN_SYSTEM_OVERLAY_ACTIVE_OPA, 0);
        if (icon_font != nullptr) {
            lv_obj_set_style_text_font(network_label_, icon_font, 0);
        }
        lv_label_set_text(network_label_, network_icon_ != nullptr ? network_icon_ : "");
        lv_obj_align(network_label_, LV_ALIGN_LEFT_MID, 0, 0);

        battery_overlay_ = lv_obj_create(system_overlay_);
        lv_obj_remove_style_all(battery_overlay_);
        lv_obj_set_size(battery_overlay_, k_system_battery_w + k_system_battery_tip_w + 6, k_system_battery_h + 4);
        lv_obj_set_style_bg_opa(battery_overlay_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(battery_overlay_, 0, 0);
        lv_obj_clear_flag(battery_overlay_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(battery_overlay_, LV_ALIGN_LEFT_MID, k_system_battery_x, 0);

        battery_overlay_box_ = lv_obj_create(battery_overlay_);
        lv_obj_remove_style_all(battery_overlay_box_);
        lv_obj_set_size(battery_overlay_box_, k_system_battery_w, k_system_battery_h);
        lv_obj_set_style_radius(battery_overlay_box_, 4, 0);
        lv_obj_set_style_bg_opa(battery_overlay_box_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(battery_overlay_box_, 1, 0);
        lv_obj_set_style_border_color(battery_overlay_box_, lv_color_hex(k_battery_meter_fill), 0);
        lv_obj_set_style_border_opa(battery_overlay_box_, LV_OPA_80, 0);
        lv_obj_align(battery_overlay_box_, LV_ALIGN_LEFT_MID, 0, 0);

        battery_overlay_fill_ = lv_obj_create(battery_overlay_box_);
        lv_obj_remove_style_all(battery_overlay_fill_);
        lv_obj_set_height(battery_overlay_fill_, k_system_battery_h - 4);
        lv_obj_set_style_radius(battery_overlay_fill_, 2, 0);
        lv_obj_set_style_bg_color(battery_overlay_fill_, lv_color_hex(k_battery_meter_fill), 0);
        lv_obj_set_style_bg_opa(battery_overlay_fill_, LV_OPA_COVER, 0);
        lv_obj_align(battery_overlay_fill_, LV_ALIGN_LEFT_MID, 2, 0);

        battery_overlay_cap_ = lv_obj_create(battery_overlay_);
        lv_obj_remove_style_all(battery_overlay_cap_);
        lv_obj_set_size(battery_overlay_cap_, k_system_battery_tip_w, 8);
        lv_obj_set_style_radius(battery_overlay_cap_, 1, 0);
        lv_obj_set_style_bg_color(battery_overlay_cap_, lv_color_hex(k_battery_meter_fill), 0);
        lv_obj_set_style_bg_opa(battery_overlay_cap_, LV_OPA_COVER, 0);
        lv_obj_align(battery_overlay_cap_, LV_ALIGN_RIGHT_MID, -1, 0);
        ApplySystemOverlayNetworkStyle();
        ApplyBatteryOverlayLevel();

        card_layer_ = lv_obj_create(screen);
        lv_obj_remove_style_all(card_layer_);
        lv_obj_set_size(card_layer_, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(card_layer_, lv_color_hex(k_card_layer_bg_color), 0);
        lv_obj_set_style_bg_opa(card_layer_, k_card_layer_bg_opa, 0);
        lv_obj_set_style_pad_all(card_layer_, 0, 0);
        lv_obj_set_scrollbar_mode(card_layer_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(card_layer_, LV_ALIGN_CENTER, 0, 0);

        pull_indicator_ = lv_obj_create(card_layer_);
        lv_obj_remove_style_all(pull_indicator_);
        lv_obj_set_size(pull_indicator_, 34, 3);
        lv_obj_set_style_bg_color(pull_indicator_, lv_color_hex(k_indicator_top), 0);
        lv_obj_set_style_bg_opa(pull_indicator_, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(pull_indicator_, 2, 0);
        lv_obj_align(pull_indicator_, LV_ALIGN_TOP_MID, 0, 24);

        home_indicator_ = lv_obj_create(card_layer_);
        lv_obj_remove_style_all(home_indicator_);
        lv_obj_set_size(home_indicator_, 56, 3);
        lv_obj_set_style_bg_color(home_indicator_, lv_color_hex(k_indicator_home), 0);
        lv_obj_set_style_bg_opa(home_indicator_, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(home_indicator_, 2, 0);
        lv_obj_align(home_indicator_, LV_ALIGN_BOTTOM_MID, 0, -8);

        card_title_label_ = lv_label_create(card_layer_);
        lv_obj_set_width(card_title_label_, 260);
        lv_obj_set_style_text_align(card_title_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(card_title_label_, lv_color_hex(k_page_title_color), 0);
        lv_obj_set_style_text_opa(card_title_label_, LV_OPA_COVER, 0);
        lv_label_set_text(card_title_label_, "");
        lv_obj_add_flag(card_title_label_, LV_OBJ_FLAG_HIDDEN);

        overview_time_label_ = lv_label_create(card_layer_);
        lv_obj_set_width(overview_time_label_, k_overview_time_w);
        lv_obj_set_style_text_align(overview_time_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(overview_time_label_, lv_color_hex(k_page_title_color), 0);
        lv_obj_set_style_text_opa(overview_time_label_, LV_OPA_COVER, 0);
        lv_label_set_text(overview_time_label_, "");
        lv_obj_align(overview_time_label_, LV_ALIGN_TOP_MID, 0, k_overview_time_y);
        lv_obj_add_flag(overview_time_label_, LV_OBJ_FLAG_HIDDEN);

        overview_date_label_ = lv_label_create(card_layer_);
        lv_obj_set_width(overview_date_label_, k_overview_time_w);
        lv_obj_set_style_text_align(overview_date_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(overview_date_label_, lv_color_hex(k_text_dimmed), 0);
        lv_obj_set_style_text_opa(overview_date_label_, LV_OPA_COVER, 0);
        lv_label_set_text(overview_date_label_, "");
        lv_obj_align(overview_date_label_, LV_ALIGN_TOP_MID, 0, k_overview_date_y);
        lv_obj_add_flag(overview_date_label_, LV_OBJ_FLAG_HIDDEN);

        notification_clear_button_ = lv_obj_create(card_layer_);
        lv_obj_remove_style_all(notification_clear_button_);
        lv_obj_set_size(notification_clear_button_, k_notification_clear_button_w, k_notification_clear_button_h);
        lv_obj_set_style_radius(notification_clear_button_, 16, 0);
        lv_obj_set_style_bg_color(notification_clear_button_, lv_color_hex(k_tag_bg), 0);
        lv_obj_set_style_bg_opa(notification_clear_button_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(notification_clear_button_, lv_color_hex(k_title_accent), 0);
        lv_obj_set_style_border_opa(notification_clear_button_, LV_OPA_70, 0);
        lv_obj_set_style_border_width(notification_clear_button_, 1, 0);
        lv_obj_align(notification_clear_button_, LV_ALIGN_TOP_MID, 0, k_notification_clear_button_y);
        lv_obj_add_flag(notification_clear_button_, LV_OBJ_FLAG_HIDDEN);

        notification_clear_label_ = lv_label_create(notification_clear_button_);
        lv_obj_set_style_text_color(notification_clear_label_, lv_color_hex(k_text_primary), 0);
        lv_label_set_text(notification_clear_label_, "全部清理");
        lv_obj_center(notification_clear_label_);

        notification_empty_panel_ = lv_obj_create(card_layer_);
        lv_obj_remove_style_all(notification_empty_panel_);
        lv_obj_set_size(notification_empty_panel_, k_notification_empty_panel_w, k_notification_empty_panel_h);
        lv_obj_set_style_radius(notification_empty_panel_, 20, 0);
        lv_obj_set_style_bg_color(notification_empty_panel_, lv_color_hex(k_notification_empty_panel_bg), 0);
        lv_obj_set_style_bg_opa(notification_empty_panel_, k_notification_empty_panel_opa, 0);
        lv_obj_set_style_border_color(notification_empty_panel_, lv_color_hex(k_page_title_color), 0);
        lv_obj_set_style_border_opa(notification_empty_panel_, k_notification_empty_panel_border_opa, 0);
        lv_obj_set_style_border_width(notification_empty_panel_, 1, 0);
        lv_obj_clear_flag(notification_empty_panel_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(notification_empty_panel_, LV_ALIGN_TOP_MID, 0, k_notification_empty_panel_y);
        lv_obj_add_flag(notification_empty_panel_, LV_OBJ_FLAG_HIDDEN);

        notification_empty_label_ = lv_label_create(notification_empty_panel_);
        lv_obj_set_width(notification_empty_label_, k_notification_empty_panel_w - 24);
        lv_obj_set_style_text_align(notification_empty_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(notification_empty_label_, lv_color_hex(k_page_title_color), 0);
        lv_obj_set_style_text_opa(notification_empty_label_, LV_OPA_COVER, 0);
        lv_label_set_text(notification_empty_label_, "暂无通知");
        lv_obj_center(notification_empty_label_);
        lv_obj_add_flag(notification_empty_label_, LV_OBJ_FLAG_HIDDEN);

        for (uint8_t i = 0; i < k_card_glass_count; ++i) {
            GlassCard& card = glass_cards_[i];

            card.container = lv_obj_create(card_layer_);
            lv_obj_remove_style_all(card.container);
            lv_obj_set_size(card.container, k_glass_width, k_glass_height);
            lv_obj_set_style_radius(card.container, k_glass_radius, 0);
            lv_obj_set_style_bg_color(card.container, lv_color_hex(k_card_bg_color), 0);
            lv_obj_set_style_bg_opa(card.container, k_glass_bg_opa[i], 0);
            lv_obj_set_style_border_color(card.container, lv_color_hex(k_card_border_color), 0);
            lv_obj_set_style_border_opa(card.container, k_glass_border_opa[i], 0);
            lv_obj_set_style_border_width(card.container, 1, 0);
            lv_obj_set_style_pad_top(card.container, k_glass_pad_v, 0);
            lv_obj_set_style_pad_bottom(card.container, k_glass_pad_v, 0);
            lv_obj_set_style_pad_left(card.container, k_glass_pad_h, 0);
            lv_obj_set_style_pad_right(card.container, k_glass_pad_h, 0);
            lv_obj_set_style_layout(card.container, LV_LAYOUT_NONE, 0);
            lv_obj_align(card.container, LV_ALIGN_TOP_MID, 0, k_glass_y_start + (int32_t)i * k_glass_stack_pitch);
            lv_obj_add_flag(card.container, LV_OBJ_FLAG_HIDDEN);

            card.icon_bg = lv_obj_create(card.container);
            lv_obj_remove_style_all(card.icon_bg);
            lv_obj_set_size(card.icon_bg, k_notification_icon_size, k_notification_icon_size);
            lv_obj_set_style_radius(card.icon_bg, 15, 0);
            lv_obj_set_style_bg_color(card.icon_bg, lv_color_hex(0x2a2a30), 0);
            lv_obj_set_style_bg_opa(card.icon_bg, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(card.icon_bg, lv_color_hex(0xff8b93), 0);
            lv_obj_set_style_border_opa(card.icon_bg, LV_OPA_50, 0);
            lv_obj_set_style_border_width(card.icon_bg, 1, 0);
            lv_obj_set_style_shadow_color(card.icon_bg, lv_color_hex(0xff5e5b), 0);
            lv_obj_set_style_shadow_width(card.icon_bg, 14, 0);
            lv_obj_set_style_shadow_opa(card.icon_bg, static_cast<lv_opa_t>(90), 0);
            lv_obj_align(card.icon_bg, LV_ALIGN_TOP_LEFT, 26, 22);

            card.icon = lv_label_create(card.icon_bg);
            lv_obj_set_style_text_color(card.icon, lv_color_hex(k_text_primary), 0);
            lv_obj_set_style_text_align(card.icon, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(card.icon, "!");
            lv_obj_center(card.icon);

            card.time = lv_label_create(card.container);
            lv_obj_set_width(card.time, 54);
            lv_obj_set_style_text_align(card.time, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_set_style_text_color(card.time, lv_color_hex(k_text_dimmed), 0);
            lv_label_set_long_mode(card.time, LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_text(card.time, "\xE7\x8E\xB0\xE5\x9C\xA8");
            lv_obj_align(card.time, LV_ALIGN_TOP_RIGHT, -28, 29);

            card.text_box = lv_obj_create(card.container);
            lv_obj_remove_style_all(card.text_box);
            lv_obj_set_size(card.text_box, k_glass_text_w, 52);
            lv_obj_set_flex_flow(card.text_box, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(card.text_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_row(card.text_box, 4, 0);
            lv_obj_set_pos(card.text_box, k_glass_text_x, k_glass_text_y);

            card.title = lv_label_create(card.text_box);
            lv_obj_set_width(card.title, k_glass_text_w);
            lv_obj_set_style_text_color(card.title, lv_color_hex(k_text_primary), 0);
            lv_label_set_long_mode(card.title, LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_text(card.title, "");

            card.body = lv_label_create(card.text_box);
            lv_obj_set_width(card.body, k_glass_text_w);
            lv_obj_set_style_text_color(card.body, lv_color_hex(0xd8d8de), 0);
            lv_label_set_long_mode(card.body, LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_text(card.body, "");

            if (i == 0) {
                for (uint8_t dot = 0; dot < k_notification_indicator_dot_count; ++dot) {
                    notification_indicator_dots_[dot] = lv_obj_create(card_layer_);
                    lv_obj_remove_style_all(notification_indicator_dots_[dot]);
                    lv_obj_set_size(
                        notification_indicator_dots_[dot],
                        k_notification_indicator_dot_size,
                        k_notification_indicator_dot_size
                    );
                    lv_obj_set_style_radius(notification_indicator_dots_[dot], LV_RADIUS_CIRCLE, 0);
                    lv_obj_set_style_bg_color(notification_indicator_dots_[dot], lv_color_hex(k_text_dimmed), 0);
                    lv_obj_set_style_bg_opa(notification_indicator_dots_[dot], LV_OPA_40, 0);
                    lv_obj_align(
                        notification_indicator_dots_[dot],
                        LV_ALIGN_BOTTOM_MID,
                        ((int16_t)dot - 1) * 12,
                        -35
                    );
                    lv_obj_add_flag(notification_indicator_dots_[dot], LV_OBJ_FLAG_HIDDEN);
                }
            }

        }

        for (uint8_t i = 0; i < k_overview_row_count; ++i) {
            OverviewRow& row = overview_rows_[i];

            row.container = lv_obj_create(card_layer_);
            lv_obj_remove_style_all(row.container);
            lv_obj_set_size(row.container, k_overview_row_w, k_overview_row_h);
            lv_obj_set_style_radius(row.container, 18, 0);
            lv_obj_set_style_bg_color(row.container, lv_color_hex(k_card_bg_color), 0);
            lv_obj_set_style_bg_opa(row.container, static_cast<lv_opa_t>(196), 0);
            lv_obj_set_style_border_color(row.container, lv_color_hex(k_card_border_color), 0);
            lv_obj_set_style_border_opa(row.container, LV_OPA_30, 0);
            lv_obj_set_style_border_width(row.container, 1, 0);
            lv_obj_set_style_shadow_color(row.container, lv_color_hex(k_card_shadow_color), 0);
            lv_obj_set_style_shadow_width(row.container, 10, 0);
            lv_obj_set_style_shadow_opa(row.container, LV_OPA_20, 0);
            lv_obj_set_style_shadow_offset_y(row.container, 3, 0);
            lv_obj_set_style_pad_top(row.container, 6, 0);
            lv_obj_set_style_pad_bottom(row.container, 6, 0);
            lv_obj_set_style_pad_left(row.container, 8, 0);
            lv_obj_set_style_pad_right(row.container, 8, 0);
            lv_obj_set_style_layout(row.container, LV_LAYOUT_NONE, 0);
            lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(row.container, LV_ALIGN_TOP_MID, 0, k_overview_y_start + (int32_t)i * k_overview_row_pitch);
            lv_obj_add_flag(row.container, LV_OBJ_FLAG_HIDDEN);

            row.icon_bg = lv_obj_create(row.container);
            lv_obj_remove_style_all(row.icon_bg);
            lv_obj_set_size(row.icon_bg, k_ov_icon_size, k_ov_icon_size);
            lv_obj_set_style_radius(row.icon_bg, k_ov_icon_radius, 0);
            lv_obj_set_style_bg_color(row.icon_bg, lv_color_hex(k_ov_icon_bg_weather), 0);
            lv_obj_set_style_bg_opa(row.icon_bg, k_ov_icon_bg_opa, 0);
            lv_obj_align(row.icon_bg, LV_ALIGN_LEFT_MID, k_overview_icon_x, 0);

            row.icon = lv_label_create(row.icon_bg);
            lv_obj_set_style_text_align(row.icon, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(row.icon, lv_color_hex(k_text_primary), 0);
            lv_obj_center(row.icon);
            lv_label_set_text(row.icon, "");

            row.text_box = lv_obj_create(row.container);
            lv_obj_remove_style_all(row.text_box);
            lv_obj_set_size(row.text_box, k_overview_text_w, 54);
            lv_obj_set_style_layout(row.text_box, LV_LAYOUT_NONE, 0);
            lv_obj_clear_flag(row.text_box, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(row.text_box, LV_ALIGN_LEFT_MID, k_overview_text_x, 0);

            row.title = lv_label_create(row.text_box);
            lv_obj_set_width(row.title, k_overview_text_w);
            lv_obj_set_style_text_color(row.title, lv_color_hex(k_text_primary), 0);
            lv_label_set_long_mode(row.title, LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_text(row.title, "");
            lv_obj_set_pos(row.title, 0, 0);

            row.body = lv_label_create(row.text_box);
            lv_obj_set_width(row.body, k_overview_text_w);
            lv_obj_set_style_text_color(row.body, lv_color_hex(k_title_accent), 0);
            lv_label_set_long_mode(row.body, LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_text(row.body, "");
            lv_obj_set_pos(row.body, 0, 17);

            row.detail = lv_label_create(row.text_box);
            lv_obj_set_width(row.detail, k_overview_text_w);
            lv_obj_set_style_text_color(row.detail, lv_color_hex(k_text_dimmed), 0);
            lv_label_set_long_mode(row.detail, LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_text(row.detail, "");
            lv_obj_set_pos(row.detail, 0, 34);

            row.arrow = lv_label_create(row.container);
            lv_obj_set_style_text_color(row.arrow, lv_color_hex(k_title_accent), 0);
            lv_label_set_text(row.arrow, LV_SYMBOL_RIGHT);
            lv_obj_align(row.arrow, LV_ALIGN_LEFT_MID, k_overview_arrow_x, 0);

            if (i < k_overview_sep_count) {
                overview_separators_[i] = lv_obj_create(card_layer_);
                lv_obj_remove_style_all(overview_separators_[i]);
                lv_obj_set_size(overview_separators_[i], k_overview_sep_w, 1);
                lv_obj_set_style_bg_color(overview_separators_[i], lv_color_hex(k_separator_color), 0);
                lv_obj_set_style_bg_opa(overview_separators_[i], LV_OPA_COVER, 0);
                lv_obj_align(
                    overview_separators_[i],
                    LV_ALIGN_TOP_MID,
                    0,
                    k_overview_y_start + (int32_t)k_overview_row_pitch * (i + 1) - 1
                );
                lv_obj_add_flag(overview_separators_[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        lv_obj_add_flag(card_layer_, LV_OBJ_FLAG_HIDDEN);
    }

    static void CardLayerSetY(void* obj, int32_t y) {
        lv_obj_set_y(static_cast<lv_obj_t*>(obj), y);
    }

    static int16_t ClampCardY(int32_t value, int16_t min_value, int16_t max_value) {
        return (int16_t)std::min<int32_t>(std::max<int32_t>(value, min_value), max_value);
    }

    static int16_t HiddenCardLayerY(xiaoxin_card_page_t page, int16_t screen_height) {
        if (page == XIAOXIN_CARD_PAGE_NOTIFICATIONS) {
            return (int16_t)-screen_height;
        }
        if (page == XIAOXIN_CARD_PAGE_OVERVIEW) {
            return screen_height;
        }
        return 0;
    }

    static int16_t DragCardLayerY(
        xiaoxin_card_page_t current,
        xiaoxin_card_page_t target,
        int16_t offset,
        int16_t screen_height,
        int16_t max_drag_px
    ) {
        const int32_t drag_limit = std::max<int16_t>(max_drag_px, 1);
        const int32_t abs_offset = std::min<int32_t>(std::abs(offset), drag_limit);
        const int32_t travel = (int32_t)screen_height;
        const int32_t revealed = (travel * abs_offset) / drag_limit;

        if (current == XIAOXIN_CARD_PAGE_HOME) {
            if (target == XIAOXIN_CARD_PAGE_NOTIFICATIONS) {
                return ClampCardY(-travel + revealed, -screen_height, 0);
            }
            if (target == XIAOXIN_CARD_PAGE_OVERVIEW) {
                return ClampCardY(travel - revealed, 0, screen_height);
            }
            return 0;
        }

        if (current == XIAOXIN_CARD_PAGE_NOTIFICATIONS) {
            return target == XIAOXIN_CARD_PAGE_HOME
                ? ClampCardY(-revealed, -screen_height, 0)
                : 0;
        }

        if (current == XIAOXIN_CARD_PAGE_OVERVIEW) {
            return target == XIAOXIN_CARD_PAGE_HOME
                ? ClampCardY(revealed, 0, screen_height)
                : 0;
        }

        return 0;
    }

    static uint32_t CardReleaseDurationMs(int16_t from_y, int16_t to_y, int16_t screen_height) {
        const int32_t distance = std::abs((int32_t)to_y - (int32_t)from_y);
        const int32_t denominator = std::max<int16_t>(screen_height, 1);
        const int32_t span = (int32_t)k_card_snap_max_anim_ms - (int32_t)k_card_snap_min_anim_ms;
        const int32_t scaled = (distance * span) / denominator;
        return (uint32_t)std::min<int32_t>(
            k_card_snap_max_anim_ms,
            std::max<int32_t>(k_card_snap_min_anim_ms, k_card_snap_min_anim_ms + scaled)
        );
    }

    static lv_opa_t DragCardLayerOpacity(
        xiaoxin_card_page_t current,
        int16_t offset,
        int16_t threshold_px
    ) {
        if (current != XIAOXIN_CARD_PAGE_HOME) {
            return LV_OPA_COVER;
        }

        const int32_t threshold = std::max<int16_t>(threshold_px, 1);
        const int32_t progress = std::min<int32_t>((std::abs(offset) * 255) / threshold, 255);
        return (lv_opa_t)std::min<int32_t>(70 + progress, 255);
    }

    static void AddFlagIfCreated(lv_obj_t* obj, lv_obj_flag_t flag) {
        if (obj != nullptr) {
            lv_obj_add_flag(obj, flag);
        }
    }

    static void RemoveFlagIfCreated(lv_obj_t* obj, lv_obj_flag_t flag) {
        if (obj != nullptr) {
            lv_obj_remove_flag(obj, flag);
        }
    }

    void UpdateNotificationIndicatorDots(uint8_t index, uint8_t total, bool prepare_entry_animation) {
        const uint8_t visible = std::min<uint8_t>(total, k_notification_indicator_dot_count);
        if (visible <= 1) {
            for (uint8_t i = 0; i < k_notification_indicator_dot_count; ++i) {
                AddFlagIfCreated(notification_indicator_dots_[i], LV_OBJ_FLAG_HIDDEN);
            }
            return;
        }

        const uint8_t active = std::min<uint8_t>(index, (uint8_t)(visible - 1));
        const int16_t gap = 12;
        const int16_t center_offset = (int16_t)((visible - 1) * gap / 2);
        for (uint8_t i = 0; i < k_notification_indicator_dot_count; ++i) {
            lv_obj_t* dot = notification_indicator_dots_[i];
            if (dot == nullptr) {
                continue;
            }
            if (i >= visible) {
                lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
                continue;
            }

            const bool selected = i == active;
            lv_obj_set_style_bg_color(dot, lv_color_hex(selected ? k_title_accent : k_text_dimmed), 0);
            lv_obj_set_style_bg_opa(dot, selected ? LV_OPA_COVER : LV_OPA_60, 0);
            lv_obj_set_style_shadow_color(dot, lv_color_hex(k_title_accent), 0);
            lv_obj_set_style_shadow_width(dot, selected ? 8 : 0, 0);
            lv_obj_set_style_shadow_opa(dot, selected ? LV_OPA_30 : LV_OPA_TRANSP, 0);
            lv_obj_set_style_opa(
                dot,
                prepare_entry_animation
                    ? static_cast<lv_opa_t>(LV_OPA_0)
                    : static_cast<lv_opa_t>(LV_OPA_COVER),
                0
            );
            lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, (int16_t)(i * gap) - center_offset, -35);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(dot);
        }
    }

    static uint32_t DotColorForPriority(uint32_t priority) {
        if (priority <= 1) {
            return k_dot_color_urgent;
        }
        if (priority == 2) {
            return k_dot_color_warning;
        }
        return k_dot_color_info;
    }

    static int16_t NotificationMinScrollY(uint8_t total) {
        if (total <= 1) {
            return 0;
        }
        return (int16_t)-((int16_t)(total - 1) * k_notification_slide_pitch);
    }

    static int16_t ClampNotificationScrollY(int16_t scroll_y, uint8_t total) {
        const int16_t min_scroll = NotificationMinScrollY(total);
        return std::max<int16_t>(min_scroll, std::min<int16_t>(0, scroll_y));
    }

    static int16_t NotificationScrollDisplayY(int16_t raw_scroll_y, uint8_t total) {
        const int16_t min_scroll = NotificationMinScrollY(total);
        if (raw_scroll_y > 0) {
            return (int16_t)(raw_scroll_y / 3);
        }
        if (raw_scroll_y < min_scroll) {
            return (int16_t)(min_scroll + (raw_scroll_y - min_scroll) / 3);
        }
        return raw_scroll_y;
    }

    static uint8_t NotificationIndexForScroll(int16_t scroll_y, uint8_t total) {
        if (total == 0) {
            return 0;
        }
        const int16_t clamped = ClampNotificationScrollY(scroll_y, total);
        const int16_t distance = (int16_t)-clamped;
        uint8_t index = (uint8_t)((distance + k_notification_slide_pitch / 2) / k_notification_slide_pitch);
        return std::min<uint8_t>(index, (uint8_t)(total - 1));
    }

    void PopulateNotificationCard(GlassCard& card, const xiaoxin_card_item_t* item) {
        if (card.container == nullptr || item == nullptr) {
            return;
        }

        lv_obj_set_size(card.container, k_glass_width, k_glass_height);
        lv_obj_set_style_radius(card.container, k_glass_radius, 0);
        lv_obj_set_style_border_color(card.container, lv_color_hex(k_card_border_color), 0);
        lv_obj_set_style_border_width(card.container, 1, 0);
        lv_obj_set_style_pad_top(card.container, k_glass_pad_v, 0);
        lv_obj_set_style_pad_bottom(card.container, k_glass_pad_v, 0);
        lv_obj_set_style_pad_left(card.container, k_glass_pad_h, 0);
        lv_obj_set_style_pad_right(card.container, k_glass_pad_h, 0);

        if (card.icon_bg != nullptr) {
            lv_obj_set_style_bg_color(card.icon_bg, lv_color_hex(DotColorForPriority(item->priority)), 0);
        }
        if (card.icon != nullptr) {
            lv_label_set_text(card.icon, "!");
        }
        if (card.time != nullptr) {
            lv_label_set_text(card.time, "\xE7\x8E\xB0\xE5\x9C\xA8");
        }
        if (card.title != nullptr) {
            lv_label_set_text(card.title, item->title);
        }
        if (card.body != nullptr) {
            lv_label_set_text(card.body, item->body);
        }

        lv_obj_remove_flag(card.container, LV_OBJ_FLAG_HIDDEN);
    }

    lv_opa_t NotificationCardOpacityForSlot(uint8_t slot, int16_t scroll_y) const {
        const int16_t y_offset = (int16_t)((int16_t)slot * k_notification_slide_pitch + scroll_y);
        const int16_t distance_from_center = std::min<int16_t>(
            (int16_t)std::abs(y_offset),
            (int16_t)(k_notification_slide_pitch * 2)
        );
        const int32_t opacity = LV_OPA_COVER - ((int32_t)distance_from_center * 90) / k_notification_slide_pitch;
        return (lv_opa_t)std::max<int32_t>(LV_OPA_60, std::min<int32_t>(LV_OPA_COVER, opacity));
    }

    void ApplyNotificationScrollVisual(
        int16_t scroll_y,
        bool prepare_entry_animation = false,
        bool lightweight_drag = false
    ) {
        const int64_t perf_start_us = k_ui_perf_trace_enabled ? esp_timer_get_time() : 0;
        const uint8_t total = xiaoxin_card_pager_notification_count(&card_pager_);
        const uint8_t active_index = NotificationIndexForScroll(scroll_y, total);

        if (lightweight_drag) {
            if (!notification_drag_visual_owns_cards_) {
                for (uint8_t slot = 0; slot < k_card_glass_count; ++slot) {
                    GlassCard& card = glass_cards_[slot];
                    if (card.container == nullptr || lv_obj_has_flag(card.container, LV_OBJ_FLAG_HIDDEN)) {
                        continue;
                    }
                    lv_anim_delete(card.container, nullptr);
                }
                notification_drag_visual_owns_cards_ = true;
            }
        } else {
            notification_drag_visual_owns_cards_ = false;
        }

        for (uint8_t slot = 0; slot < k_card_glass_count; ++slot) {
            GlassCard& card = glass_cards_[slot];
            if (card.container == nullptr || lv_obj_has_flag(card.container, LV_OBJ_FLAG_HIDDEN)) {
                continue;
            }

            const int16_t y_offset = (int16_t)((int16_t)slot * k_notification_slide_pitch + scroll_y);
            const bool near_center = slot == active_index;
            const lv_opa_t target_opa = prepare_entry_animation
                ? static_cast<lv_opa_t>(LV_OPA_0)
                : NotificationCardOpacityForSlot(slot, scroll_y);

            lv_obj_align(card.container, LV_ALIGN_TOP_MID, 0, k_glass_y_start + y_offset);
            lv_obj_set_style_opa(card.container, target_opa, 0);
            if (!lightweight_drag) {
                lv_obj_set_style_bg_opa(card.container, near_center ? static_cast<lv_opa_t>(174) : static_cast<lv_opa_t>(82), 0);
                lv_obj_set_style_border_opa(card.container, near_center ? static_cast<lv_opa_t>(44) : static_cast<lv_opa_t>(18), 0);
            }
        }

        if (!lightweight_drag) {
            UpdateNotificationIndicatorDots(active_index, total, prepare_entry_animation);
        }
        if (!lightweight_drag) {
            for (uint8_t slot = 0; slot < k_card_glass_count; ++slot) {
                if (slot != active_index && glass_cards_[slot].container != nullptr) {
                    lv_obj_move_foreground(glass_cards_[slot].container);
                }
            }
            if (active_index < k_card_glass_count &&
                glass_cards_[active_index].container != nullptr &&
                !lv_obj_has_flag(glass_cards_[active_index].container, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_move_foreground(glass_cards_[active_index].container);
            }
            for (uint8_t i = 0; i < k_notification_indicator_dot_count; ++i) {
                lv_obj_t* dot = notification_indicator_dots_[i];
                if (dot != nullptr && !lv_obj_has_flag(dot, LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_move_foreground(dot);
                }
            }
        }
        if (k_ui_perf_trace_enabled) {
            AddUiPerfSample(
                ui_perf_drag_calls_,
                ui_perf_drag_total_us_,
                ui_perf_drag_max_us_,
                (uint32_t)(esp_timer_get_time() - perf_start_us)
            );
        }
    }

    void RenderNotificationCards(const xiaoxin_card_item_t* /*items*/, uint8_t count, bool prepare_entry_animation) {
        notification_scroll_y_ = ClampNotificationScrollY(notification_scroll_y_, count);
        for (uint8_t slot = 0; slot < k_card_glass_count; ++slot) {
            GlassCard& card = glass_cards_[slot];
            if (card.container == nullptr) {
                continue;
            }

            const xiaoxin_card_item_t* item = xiaoxin_card_pager_notification_at(&card_pager_, slot);
            if (item == nullptr || slot >= count) {
                card.visible_index = 0xff;
                lv_obj_add_flag(card.container, LV_OBJ_FLAG_HIDDEN);
                continue;
            }

            card.visible_index = slot;
            PopulateNotificationCard(card, item);
        }

        const bool empty = xiaoxin_card_pager_notification_empty(&card_pager_);
        if (empty) {
            AddFlagIfCreated(notification_clear_button_, LV_OBJ_FLAG_HIDDEN);
            RemoveFlagIfCreated(notification_empty_panel_, LV_OBJ_FLAG_HIDDEN);
            RemoveFlagIfCreated(notification_empty_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            RemoveFlagIfCreated(notification_clear_button_, LV_OBJ_FLAG_HIDDEN);
            if (notification_clear_button_ != nullptr) {
                lv_obj_align(notification_clear_button_, LV_ALIGN_TOP_MID, 0, k_notification_clear_button_y);
                lv_obj_move_foreground(notification_clear_button_);
            }
            AddFlagIfCreated(notification_empty_panel_, LV_OBJ_FLAG_HIDDEN);
            AddFlagIfCreated(notification_empty_label_, LV_OBJ_FLAG_HIDDEN);
        }

        ApplyNotificationScrollVisual(notification_scroll_y_, prepare_entry_animation);
    }

    static void NotificationScrollSetY(void* obj, int32_t scroll_y) {
        auto* self = static_cast<PaopaoPetDisplay*>(obj);
        if (self != nullptr) {
            self->ApplyNotificationScrollVisual((int16_t)scroll_y, false, true);
        }
    }

    static void NotificationDismissSetX(void* obj, int32_t x) {
        lv_obj_set_x(static_cast<lv_obj_t*>(obj), (lv_coord_t)x);
    }

    static void NotificationDismissSetOpacity(void* obj, int32_t opa) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)opa, 0);
    }

    static void NotificationDismissAnimationCompleted(lv_anim_t* anim) {
        auto* self = static_cast<PaopaoPetDisplay*>(lv_anim_get_user_data(anim));
        if (self == nullptr) {
            return;
        }

        const uint8_t visible_index = self->notification_animating_visible_index_;
        self->notification_dismiss_animating_ = false;
        self->notification_animating_visible_index_ = 0xff;
        self->notification_card_drag_x_ = 0;
        self->notification_pressed_slot_ = -1;
        self->notification_pressed_visible_index_ = 0xff;

        if (visible_index != 0xff) {
            xiaoxin_card_pager_notification_dismiss(&self->card_pager_, visible_index);
        }
        self->notification_scroll_y_ = ClampNotificationScrollY(
            self->notification_scroll_y_,
            xiaoxin_card_pager_notification_count(&self->card_pager_)
        );
        self->RenderNotificationPageAfterDataChange();
    }

    static void NotificationDismissReboundCompleted(lv_anim_t* anim) {
        auto* self = static_cast<PaopaoPetDisplay*>(lv_anim_get_user_data(anim));
        if (self == nullptr) {
            return;
        }

        self->notification_dismiss_animating_ = false;
        self->notification_animating_visible_index_ = 0xff;
        self->notification_card_drag_x_ = 0;
        self->notification_pressed_slot_ = -1;
        self->notification_pressed_visible_index_ = 0xff;
        self->ApplyNotificationScrollVisual(self->notification_scroll_y_);
        self->RaiseOverlayObjects();
    }

    void AnimateNotificationDismiss(int8_t slot, uint8_t visible_index) {
        if (slot < 0 || slot >= (int8_t)k_card_glass_count) {
            return;
        }
        GlassCard& card = glass_cards_[slot];
        if (card.container == nullptr) {
            return;
        }

        notification_dismiss_animating_ = true;
        notification_animating_visible_index_ = visible_index;
        notification_drag_visual_owns_cards_ = false;

        lv_anim_delete(card.container, NotificationDismissSetX);
        lv_anim_delete(card.container, NotificationDismissSetOpacity);

        const int32_t start_x = notification_card_drag_x_;
        const int32_t start_opa = std::max<int32_t>(
            LV_OPA_30,
            LV_OPA_COVER + ((int32_t)notification_card_drag_x_ * LV_OPA_COVER) / DISPLAY_WIDTH
        );

        lv_anim_t x_anim;
        lv_anim_init(&x_anim);
        lv_anim_set_var(&x_anim, card.container);
        lv_anim_set_values(&x_anim, start_x, -DISPLAY_WIDTH);
        lv_anim_set_time(&x_anim, k_notification_dismiss_fly_ms);
        lv_anim_set_path_cb(&x_anim, lv_anim_path_ease_in);
        lv_anim_set_exec_cb(&x_anim, NotificationDismissSetX);
        lv_anim_set_completed_cb(&x_anim, NotificationDismissAnimationCompleted);
        lv_anim_set_user_data(&x_anim, this);
        lv_anim_start(&x_anim);

        lv_anim_t opa_anim;
        lv_anim_init(&opa_anim);
        lv_anim_set_var(&opa_anim, card.container);
        lv_anim_set_values(&opa_anim, start_opa, LV_OPA_0);
        lv_anim_set_time(&opa_anim, k_notification_dismiss_fly_ms);
        lv_anim_set_path_cb(&opa_anim, lv_anim_path_ease_in);
        lv_anim_set_exec_cb(&opa_anim, NotificationDismissSetOpacity);
        lv_anim_start(&opa_anim);
    }

    void AnimateNotificationDismissRebound(int8_t slot) {
        if (slot < 0 || slot >= (int8_t)k_card_glass_count) {
            return;
        }
        GlassCard& card = glass_cards_[slot];
        if (card.container == nullptr) {
            return;
        }

        notification_dismiss_animating_ = true;
        notification_animating_visible_index_ = 0xff;
        notification_drag_visual_owns_cards_ = false;

        lv_anim_delete(card.container, NotificationDismissSetX);
        lv_anim_delete(card.container, NotificationDismissSetOpacity);

        const int32_t start_x = notification_card_drag_x_;
        const int32_t start_opa = std::max<int32_t>(
            LV_OPA_30,
            LV_OPA_COVER + ((int32_t)notification_card_drag_x_ * LV_OPA_COVER) / DISPLAY_WIDTH
        );

        lv_anim_t x_anim;
        lv_anim_init(&x_anim);
        lv_anim_set_var(&x_anim, card.container);
        lv_anim_set_values(&x_anim, start_x, 0);
        lv_anim_set_time(&x_anim, k_notification_dismiss_rebound_ms);
        lv_anim_set_path_cb(&x_anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&x_anim, NotificationDismissSetX);
        lv_anim_set_completed_cb(&x_anim, NotificationDismissReboundCompleted);
        lv_anim_set_user_data(&x_anim, this);
        lv_anim_start(&x_anim);

        lv_anim_t opa_anim;
        lv_anim_init(&opa_anim);
        lv_anim_set_var(&opa_anim, card.container);
        lv_anim_set_values(&opa_anim, start_opa, NotificationCardOpacityForSlot((uint8_t)slot, notification_scroll_y_));
        lv_anim_set_time(&opa_anim, k_notification_dismiss_rebound_ms);
        lv_anim_set_path_cb(&opa_anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&opa_anim, NotificationDismissSetOpacity);
        lv_anim_start(&opa_anim);
    }

    static void NotificationScrollAnimationCompleted(lv_anim_t* anim) {
        auto* self = static_cast<PaopaoPetDisplay*>(lv_anim_get_user_data(anim));
        if (self == nullptr) {
            return;
        }
        self->ApplyNotificationScrollVisual(self->notification_scroll_y_);
        self->RaiseOverlayObjects();
    }

    void AnimateNotificationScroll(int16_t start_scroll_y, int16_t target_scroll_y) {
        notification_scroll_y_ = target_scroll_y;
        lv_anim_delete(this, NotificationScrollSetY);
        ApplyNotificationScrollVisual(start_scroll_y);

        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, this);
        lv_anim_set_values(&anim, start_scroll_y, target_scroll_y);
        lv_anim_set_time(&anim, k_notification_switch_anim_ms);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&anim, NotificationScrollSetY);
        lv_anim_set_completed_cb(&anim, NotificationScrollAnimationCompleted);
        lv_anim_set_user_data(&anim, this);
        lv_anim_start(&anim);
    }

    static void PopulateOverviewTime(xiaoxin_overview_state_t& state) {
        time_t now = 0;
        time(&now);

        struct tm timeinfo = {};
        if (now > 24 * 60 * 60 &&
            localtime_r(&now, &timeinfo) != nullptr &&
            timeinfo.tm_year >= 120) {
            state.time_valid = true;
            state.hour = timeinfo.tm_hour;
            state.minute = timeinfo.tm_min;
            state.month = timeinfo.tm_mon + 1;
            state.day = timeinfo.tm_mday;
            state.weekday = (uint8_t)timeinfo.tm_wday;
        }
    }

    xiaoxin_overview_state_t BuildOverviewState() {
        xiaoxin_overview_state_t state = {};
        PopulateOverviewTime(state);

        state.network_connected = SystemOverlayNetworkState() == XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED;
        state.battery_state = battery_snapshot_.state;
        state.battery_power_source = battery_snapshot_.power_source;
        state.battery_percent = battery_snapshot_.estimated_percent;
        state.battery_known = battery_snapshot_.state != XIAOXIN_BATTERY_STATE_UNKNOWN;

        state.weather_configured = false;
        state.weather_available = false;
        state.weather_summary = nullptr;
        state.weather_detail = nullptr;

        state.course_configured = false;
        state.course_available_today = false;
        state.course_title = nullptr;
        state.course_detail = nullptr;

        state.todo_configured = false;
        state.todo_count = 0;
        state.todo_detail = nullptr;

        return state;
    }

    xiaoxin_system_overlay_network_state_t SystemOverlayNetworkState() const {
        if (Application::GetInstance().GetDeviceState() == kDeviceStateWifiConfiguring) {
            return XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONFIGURING;
        }
        if (network_icon_ == nullptr || std::strcmp(network_icon_, FONT_AWESOME_WIFI_SLASH) == 0) {
            return XIAOXIN_SYSTEM_OVERLAY_NETWORK_DISCONNECTED;
        }
        return XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED;
    }

    void ApplySystemOverlayNetworkStyle() {
        if (network_label_ == nullptr) {
            return;
        }

        const auto network_state = SystemOverlayNetworkState();
        const auto style = xiaoxin_system_overlay_style(
            network_state,
            battery_snapshot_.state,
            battery_snapshot_.power_source
        );
        lv_obj_set_style_text_color(network_label_, lv_color_hex(style.network_color), 0);
        lv_obj_set_style_text_opa(network_label_, style.network_opa, 0);
        lv_label_set_text(
            network_label_,
            style.network_disconnected ? FONT_AWESOME_WIFI_SLASH : (network_icon_ != nullptr ? network_icon_ : "")
        );
        SyncNetworkNotificationLocked(network_state);
    }

    void SyncPetMoodDeviceStateLocked() {
        const uint32_t now_ms = NowMs();
        const bool battery_powered =
            battery_snapshot_.power_source == XIAOXIN_BATTERY_POWER_BATTERY;
        if (battery_powered &&
            (battery_snapshot_.low_edge || battery_snapshot_.critical_edge)) {
            DispatchPetMoodEventLocked(
                PAOPAO_PET_MOOD_EVENT_BATTERY_LOW,
                PAOPAO_PET_TRIGGER_NONE,
                now_ms
            );
        } else if (battery_powered && battery_snapshot_.recovered_edge) {
            DispatchPetMoodEventLocked(
                PAOPAO_PET_MOOD_EVENT_BATTERY_RECOVERED,
                PAOPAO_PET_TRIGGER_NONE,
                now_ms
            );
        }

        const bool wifi_connected =
            SystemOverlayNetworkState() == XIAOXIN_SYSTEM_OVERLAY_NETWORK_CONNECTED;
        if (wifi_connected != mood_.wifi_connected) {
            DispatchPetMoodEventLocked(
                wifi_connected
                    ? PAOPAO_PET_MOOD_EVENT_WIFI_CONNECTED
                    : PAOPAO_PET_MOOD_EVENT_WIFI_DISCONNECTED,
                PAOPAO_PET_TRIGGER_NONE,
                now_ms
            );
        }
    }

    void ApplyBatteryOverlayLevel() {
        if (battery_overlay_fill_ == nullptr || battery_overlay_box_ == nullptr) {
            return;
        }

        const int level = std::max(0, std::min(4, (int)battery_snapshot_.display_level));
        const auto style = xiaoxin_system_overlay_style(
            SystemOverlayNetworkState(),
            battery_snapshot_.state,
            battery_snapshot_.power_source
        );
        const int inner_w = k_system_battery_w - 4;
        const int fill_w =
            battery_snapshot_.power_source == XIAOXIN_BATTERY_POWER_UNKNOWN &&
            battery_snapshot_.display_level == 0
                ? 3
                : std::max(3, (inner_w * level) / 4);
        lv_obj_set_width(battery_overlay_fill_, fill_w);
        lv_obj_set_style_bg_color(
            battery_overlay_fill_,
            lv_color_hex(style.battery_color),
            0
        );
        lv_obj_set_style_border_color(
            battery_overlay_box_,
            lv_color_hex(style.battery_color),
            0
        );
        if (battery_overlay_cap_ != nullptr) {
            lv_obj_set_style_bg_color(
                battery_overlay_cap_,
                lv_color_hex(style.battery_color),
                0
            );
        }
    }

    static uint32_t OverviewIconBgColorForTag(const char* tag) {
        if (tag == nullptr) {
            return k_ov_icon_bg_weather;
        }
        if (std::strstr(tag, "课程") != nullptr) {
            return k_ov_icon_bg_course;
        }
        if (std::strstr(tag, "导航") != nullptr) {
            return k_ov_icon_bg_nav;
        }
        if (std::strstr(tag, "天气") != nullptr) {
            return k_ov_icon_bg_weather;
        }
        if (std::strstr(tag, "待办") != nullptr) {
            return k_ov_icon_bg_todo;
        }
        return k_ov_icon_bg_weather;
    }

    static const char* OverviewIconTextForTag(const char* tag) {
        if (tag == nullptr) {
            return "息";
        }
        if (std::strstr(tag, "课程") != nullptr) {
            return "课";
        }
        if (std::strstr(tag, "导航") != nullptr) {
            return "导";
        }
        if (std::strstr(tag, "天气") != nullptr) {
            return "云";
        }
        if (std::strstr(tag, "待办") != nullptr) {
            return "办";
        }
        return "息";
    }

    void ApplyCardEntryAnimation(xiaoxin_card_page_t page) {
        if (page == XIAOXIN_CARD_PAGE_NOTIFICATIONS) {
            for (uint8_t i = 0; i < k_card_glass_count; ++i) {
                GlassCard& card = glass_cards_[i];
                if (card.container == nullptr || lv_obj_has_flag(card.container, LV_OBJ_FLAG_HIDDEN)) {
                    continue;
                }

                lv_anim_t anim;
                lv_anim_init(&anim);
                lv_anim_set_var(&anim, card.container);
                lv_anim_set_values(&anim, LV_OPA_0, LV_OPA_COVER);
                lv_anim_set_time(&anim, k_entry_fade_ms);
                lv_anim_set_delay(&anim, (uint32_t)i * k_entry_stagger_ms);
                lv_anim_set_exec_cb(&anim, [](void* obj, int32_t opacity) {
                    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)opacity, 0);
                });
                lv_anim_start(&anim);
            }
            return;
        }

        if (page == XIAOXIN_CARD_PAGE_OVERVIEW) {
            for (uint8_t i = 0; i < k_overview_row_count; ++i) {
                OverviewRow& row = overview_rows_[i];
                if (row.container == nullptr || lv_obj_has_flag(row.container, LV_OBJ_FLAG_HIDDEN)) {
                    continue;
                }

                lv_anim_t anim;
                lv_anim_init(&anim);
                lv_anim_set_var(&anim, row.container);
                lv_anim_set_values(&anim, LV_OPA_0, LV_OPA_COVER);
                lv_anim_set_time(&anim, k_entry_fade_ms);
                lv_anim_set_delay(&anim, (uint32_t)i * k_entry_stagger_ms);
                lv_anim_set_exec_cb(&anim, [](void* obj, int32_t opacity) {
                    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)opacity, 0);
                });
                lv_anim_start(&anim);

                if (i < k_overview_sep_count &&
                    overview_separators_[i] != nullptr &&
                    !lv_obj_has_flag(overview_separators_[i], LV_OBJ_FLAG_HIDDEN)) {
                    lv_anim_t sep_anim;
                    lv_anim_init(&sep_anim);
                    lv_anim_set_var(&sep_anim, overview_separators_[i]);
                    lv_anim_set_values(&sep_anim, LV_OPA_0, k_separator_opa);
                    lv_anim_set_time(&sep_anim, k_entry_fade_ms);
                    lv_anim_set_delay(&sep_anim, (uint32_t)i * k_entry_stagger_ms);
                    lv_anim_set_exec_cb(&sep_anim, [](void* obj, int32_t opacity) {
                        lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)opacity, 0);
                    });
                    lv_anim_start(&sep_anim);
                }
            }
        }
    }

    static void CardLayerAnimationCompleted(lv_anim_t* anim) {
        auto* self = static_cast<PaopaoPetDisplay*>(lv_anim_get_user_data(anim));
        if (self == nullptr || self->card_layer_ == nullptr) {
            return;
        }
        lv_obj_set_y(self->card_layer_, 0);
        lv_obj_set_style_opa(self->card_layer_, LV_OPA_COVER, 0);
        if (xiaoxin_card_pager_current_page(&self->card_pager_) == XIAOXIN_CARD_PAGE_HOME) {
            lv_obj_add_flag(self->card_layer_, LV_OBJ_FLAG_HIDDEN);
        } else if (xiaoxin_card_pager_animation(&self->card_pager_) == XIAOXIN_CARD_ANIMATION_SNAP) {
            self->ApplyCardEntryAnimation(xiaoxin_card_pager_current_page(&self->card_pager_));
        }
        self->ApplyPetAnimationForCardPager();
        self->RaiseOverlayObjects();
    }

    static void CardLayerDragAnimationCompleted(lv_anim_t* anim) {
        auto* self = static_cast<PaopaoPetDisplay*>(lv_anim_get_user_data(anim));
        if (self == nullptr || self->card_layer_ == nullptr) {
            return;
        }
        lv_obj_set_style_opa(self->card_layer_, LV_OPA_COVER, 0);
        if (xiaoxin_card_pager_current_page(&self->card_pager_) == XIAOXIN_CARD_PAGE_HOME) {
            lv_obj_add_flag(self->card_layer_, LV_OBJ_FLAG_HIDDEN);
        }
        self->ApplyPetAnimationForCardPager();
        self->RaiseOverlayObjects();
    }

    void RenderCardPage(xiaoxin_card_page_t page, bool prepare_entry_animation = false) {
        if (card_layer_ == nullptr) {
            return;
        }

        notification_drag_visual_owns_cards_ = false;
        rendered_card_page_ = page;
        card_page_rendered_ = true;

        for (uint8_t i = 0; i < k_card_glass_count; ++i) {
            AddFlagIfCreated(glass_cards_[i].container, LV_OBJ_FLAG_HIDDEN);
            AddFlagIfCreated(glass_cards_[i].pager, LV_OBJ_FLAG_HIDDEN);
        }
        for (uint8_t i = 0; i < k_notification_indicator_dot_count; ++i) {
            AddFlagIfCreated(notification_indicator_dots_[i], LV_OBJ_FLAG_HIDDEN);
        }
        for (uint8_t i = 0; i < k_overview_row_count; ++i) {
            AddFlagIfCreated(overview_rows_[i].container, LV_OBJ_FLAG_HIDDEN);
        }
        for (uint8_t i = 0; i < k_overview_sep_count; ++i) {
            AddFlagIfCreated(overview_separators_[i], LV_OBJ_FLAG_HIDDEN);
        }
        AddFlagIfCreated(card_title_label_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(overview_time_label_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(overview_date_label_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(pull_indicator_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(home_indicator_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(notification_clear_button_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(notification_empty_panel_, LV_OBJ_FLAG_HIDDEN);
        AddFlagIfCreated(notification_empty_label_, LV_OBJ_FLAG_HIDDEN);

        if (page == XIAOXIN_CARD_PAGE_HOME) {
            lv_obj_add_flag(card_layer_, LV_OBJ_FLAG_HIDDEN);
            rendered_card_page_ = XIAOXIN_CARD_PAGE_HOME;
            rendered_card_prepare_entry_ = false;
            ApplyPetAnimationForCardPager();
            RaiseOverlayObjects();
            return;
        }

        lv_obj_remove_flag(card_layer_, LV_OBJ_FLAG_HIDDEN);
        ApplyPetAnimationForCardPager();
        ApplyBatteryOverlayLevel();

        if (page == XIAOXIN_CARD_PAGE_NOTIFICATIONS) {
            RemoveFlagIfCreated(pull_indicator_, LV_OBJ_FLAG_HIDDEN);
            RemoveFlagIfCreated(home_indicator_, LV_OBJ_FLAG_HIDDEN);
            if (home_indicator_ != nullptr) {
                lv_obj_align(home_indicator_, LV_ALIGN_BOTTOM_MID, 0, -8);
            }

            RenderNotificationCards(
                nullptr,
                xiaoxin_card_pager_notification_count(&card_pager_),
                prepare_entry_animation
            );
        } else if (page == XIAOXIN_CARD_PAGE_OVERVIEW) {
            xiaoxin_overview_state_t overview_state = BuildOverviewState();
            xiaoxin_overview_model_build(&overview_state, &overview_snapshot_);

            if (overview_time_label_ != nullptr) {
                lv_label_set_text(overview_time_label_, overview_snapshot_.time_text);
                lv_obj_align(overview_time_label_, LV_ALIGN_TOP_MID, 0, k_overview_time_y);
                lv_obj_remove_flag(overview_time_label_, LV_OBJ_FLAG_HIDDEN);
            }
            if (overview_date_label_ != nullptr) {
                lv_label_set_text(overview_date_label_, overview_snapshot_.date_text);
                lv_obj_align(overview_date_label_, LV_ALIGN_TOP_MID, 0, k_overview_date_y);
                lv_obj_remove_flag(overview_date_label_, LV_OBJ_FLAG_HIDDEN);
            }
            RemoveFlagIfCreated(home_indicator_, LV_OBJ_FLAG_HIDDEN);
            if (home_indicator_ != nullptr) {
                lv_obj_align(home_indicator_, LV_ALIGN_TOP_MID, 0, 8);
            }

            const xiaoxin_card_item_t* items = overview_snapshot_.items;
            const uint8_t count = overview_snapshot_.item_count;
            const uint8_t visible = std::min<uint8_t>(count, k_overview_row_count);
            for (uint8_t i = 0; i < visible && items != nullptr; ++i) {
                OverviewRow& row = overview_rows_[i];
                if (row.container == nullptr) {
                    continue;
                }
                if (row.icon_bg != nullptr) {
                    lv_obj_set_style_bg_color(row.icon_bg, lv_color_hex(OverviewIconBgColorForTag(items[i].tag)), 0);
                }
                if (row.icon != nullptr) {
                    lv_label_set_text(row.icon, OverviewIconTextForTag(items[i].tag));
                }

                if (row.title != nullptr) {
                    lv_label_set_text(row.title, items[i].title);
                }
                if (row.body != nullptr) {
                    lv_label_set_text(row.body, items[i].body != nullptr ? items[i].body : "");
                }
                if (row.detail != nullptr) {
                    lv_label_set_text(row.detail, items[i].detail != nullptr ? items[i].detail : "");
                }

                const uint32_t arrow_colors[k_overview_row_count] = {k_title_accent, 0x3d7ab8, 0x3d7ab8, 0x2d5a8a};
                if (row.arrow != nullptr) {
                    lv_obj_set_style_text_color(row.arrow, lv_color_hex(arrow_colors[i]), 0);
                }
                lv_obj_set_style_opa(
                    row.container,
                    prepare_entry_animation
                        ? static_cast<lv_opa_t>(LV_OPA_0)
                        : static_cast<lv_opa_t>(LV_OPA_COVER),
                    0
                );
                lv_obj_remove_flag(row.container, LV_OBJ_FLAG_HIDDEN);

                AddFlagIfCreated(
                    i < k_overview_sep_count ? overview_separators_[i] : nullptr,
                    LV_OBJ_FLAG_HIDDEN
                );
            }
        }

        RaiseOverlayObjects();
        rendered_card_page_ = page;
        rendered_card_prepare_entry_ = prepare_entry_animation;
    }

    void RefreshOverviewPageIfVisible() {
        if (rendered_card_page_ != XIAOXIN_CARD_PAGE_OVERVIEW ||
            card_layer_ == nullptr ||
            lv_obj_has_flag(card_layer_, LV_OBJ_FLAG_HIDDEN)) {
            return;
        }

        card_page_rendered_ = false;
        RenderCardPage(XIAOXIN_CARD_PAGE_OVERVIEW, false);
    }

    void EnsureCardPageRendered(xiaoxin_card_page_t page, bool prepare_entry_animation = false) {
        const bool visible_page_is_hidden =
            page != XIAOXIN_CARD_PAGE_HOME &&
            card_layer_ != nullptr &&
            lv_obj_has_flag(card_layer_, LV_OBJ_FLAG_HIDDEN);
        const bool home_page_is_visible =
            page == XIAOXIN_CARD_PAGE_HOME &&
            card_layer_ != nullptr &&
            !lv_obj_has_flag(card_layer_, LV_OBJ_FLAG_HIDDEN);
        if (card_page_rendered_ &&
            rendered_card_page_ == page &&
            rendered_card_prepare_entry_ == prepare_entry_animation &&
            !home_page_is_visible &&
            !visible_page_is_hidden) {
            return;
        }

        RenderCardPage(page, prepare_entry_animation);
    }

    void RenderNotificationPageAfterDataChange() {
        card_page_rendered_ = false;
        RenderCardPage(XIAOXIN_CARD_PAGE_NOTIFICATIONS, false);
    }

    void ApplyCardPagerVisual() {
        if (card_layer_ == nullptr) {
            return;
        }

        const xiaoxin_card_page_t current = xiaoxin_card_pager_current_page(&card_pager_);
        const xiaoxin_card_page_t visual_page = xiaoxin_card_pager_visual_page(&card_pager_);

        EnsureCardPageRendered(visual_page, false);
        if (visual_page == XIAOXIN_CARD_PAGE_HOME) {
            return;
        }

        const int16_t offset = xiaoxin_card_pager_offset_y(&card_pager_);
        const xiaoxin_card_page_t target = xiaoxin_card_pager_target_page(&card_pager_);
        lv_anim_delete(card_layer_, CardLayerSetY);
        lv_obj_set_y(card_layer_, DragCardLayerY(
            current,
            target,
            offset,
            card_pager_.screen_height,
            card_pager_.max_drag_px
        ));
        lv_obj_set_style_opa(
            card_layer_,
            xiaoxin_card_pager_is_dragging(&card_pager_)
                ? DragCardLayerOpacity(current, offset, card_pager_.threshold_px)
                : static_cast<lv_opa_t>(LV_OPA_COVER),
            0
        );
        ApplyPetAnimationForCardPager();
    }

    void AnimateCardPagerRelease(
        xiaoxin_card_page_t visual_page,
        int16_t from_y,
        int16_t to_y,
        bool from_drag
    ) {
        if (card_layer_ == nullptr) {
            return;
        }
        notification_drag_visual_owns_cards_ = false;
        if (visual_page != XIAOXIN_CARD_PAGE_NOTIFICATIONS || from_drag) {
            EnsureCardPageRendered(visual_page, false);
        }
        if (visual_page == XIAOXIN_CARD_PAGE_HOME) {
            return;
        }

        lv_anim_delete(card_layer_, CardLayerSetY);
        lv_obj_set_y(card_layer_, from_y);
        lv_obj_set_style_opa(card_layer_, LV_OPA_COVER, 0);

        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, card_layer_);
        lv_anim_set_values(&anim, from_y, to_y);
        lv_anim_set_time(
            &anim,
            (!from_drag && visual_page == XIAOXIN_CARD_PAGE_NOTIFICATIONS)
                ? k_notification_switch_anim_ms
                : CardReleaseDurationMs(from_y, to_y, card_pager_.screen_height)
        );
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&anim, CardLayerSetY);
        lv_anim_set_completed_cb(&anim, from_drag ? CardLayerDragAnimationCompleted : CardLayerAnimationCompleted);
        lv_anim_set_user_data(&anim, this);
        lv_anim_start(&anim);
    }

    void SetCardPagerPage(xiaoxin_card_page_t page) {
        notification_drag_visual_owns_cards_ = false;
        card_pager_.current_page = page;
        card_pager_.target_page = page;
        card_pager_.animation = XIAOXIN_CARD_ANIMATION_SNAP;
        card_pager_.offset_y = 0;
        card_pager_.pressed = false;
        card_pager_.dragging = false;
        ApplyCardPagerVisual();
        ApplyPetAnimationForCardPager();
    }

    void CopyPetFrameToScreen(const lv_img_dsc_t* frame, uint32_t image_scale) {
        if (pet_frame_buffer_ == nullptr || frame == nullptr || frame->data == nullptr) {
            return;
        }

        const int64_t perf_start_us = k_ui_perf_trace_enabled ? esp_timer_get_time() : 0;

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
        if (k_ui_perf_trace_enabled) {
            AddUiPerfSample(
                ui_perf_pet_copy_calls_,
                ui_perf_pet_copy_total_us_,
                ui_perf_pet_copy_max_us_,
                (uint32_t)(esp_timer_get_time() - perf_start_us)
            );
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
        ApplyPetAnimationForCardPager();
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

    bool ShouldDispatchMoodSuggestionLocked() const {
        switch (trigger_.base_state) {
            case PAOPAO_PET_STATE_FAILING:
            case PAOPAO_PET_STATE_SLEEPING:
            case PAOPAO_PET_STATE_WAITING:
            case PAOPAO_PET_STATE_THINKING:
            case PAOPAO_PET_STATE_SPEAKING:
                return false;
            default:
                return true;
        }
    }

    void DispatchPetMoodInputLocked(const paopao_pet_mood_input_t& input, uint32_t now_ms) {
        const paopao_pet_mood_suggestion_t suggestion =
            paopao_pet_mood_handle_event(&mood_, &input, now_ms);
        if (suggestion.has_trigger && ShouldDispatchMoodSuggestionLocked()) {
            paopao_pet_trigger_dispatch(&trigger_, suggestion.trigger, now_ms);
            ApplyPetStateIfChanged();
        }
    }

    void DispatchPetMoodEventLocked(
        paopao_pet_mood_event_t event,
        paopao_pet_trigger_event_t service_trigger,
        uint32_t now_ms
    ) {
        const paopao_pet_mood_input_t input = {
            .event = event,
            .service_trigger = service_trigger,
        };
        DispatchPetMoodInputLocked(input, now_ms);
    }

    void DispatchLocalPetTriggerLocked(
        paopao_pet_trigger_event_t trigger_event,
        paopao_pet_mood_event_t mood_event,
        uint32_t now_ms
    ) {
        DispatchPetMoodEventLocked(mood_event, PAOPAO_PET_TRIGGER_NONE, now_ms);
        paopao_pet_trigger_dispatch(&trigger_, trigger_event, now_ms);
        ApplyPetStateIfChanged();
    }

    void HandleTouchRelease(uint32_t now_ms) {
        const bool was_card_dragging = xiaoxin_card_pager_is_dragging(&card_pager_);
        const xiaoxin_card_page_t release_current_page = xiaoxin_card_pager_current_page(&card_pager_);
        const xiaoxin_card_page_t release_target_page = xiaoxin_card_pager_target_page(&card_pager_);
        const int16_t release_offset = xiaoxin_card_pager_offset_y(&card_pager_);
        const xiaoxin_card_page_t release_visual_page =
            release_target_page == XIAOXIN_CARD_PAGE_HOME
                ? release_current_page
                : release_target_page;
        const int16_t release_y = DragCardLayerY(
            release_current_page,
            release_target_page,
            release_offset,
            card_pager_.screen_height,
            card_pager_.max_drag_px
        );
        if (card_pager_.pressed) {
            xiaoxin_card_pager_release(&card_pager_);
        }

        if (release_current_page == XIAOXIN_CARD_PAGE_NOTIFICATIONS && !was_card_dragging) {
            const int16_t dx = (int16_t)touch_last_x_ - (int16_t)touch_start_x_;
            const int16_t dy = (int16_t)touch_last_y_ - (int16_t)touch_start_y_;
            const int16_t abs_dx = (int16_t)std::abs(dx);
            const int16_t abs_dy = (int16_t)std::abs(dy);

            if (notification_gesture_mode_ == NotificationGestureMode::DismissCard &&
                notification_pressed_slot_ >= 0) {
                if (dx <= -k_notification_dismiss_threshold_px) {
                    AnimateNotificationDismiss(notification_pressed_slot_, notification_pressed_visible_index_);
                } else {
                    AnimateNotificationDismissRebound(notification_pressed_slot_);
                }
                notification_gesture_mode_ = NotificationGestureMode::None;
                return;
            }

            if (notification_gesture_mode_ == NotificationGestureMode::ClearAllPress &&
                abs_dx < 8 && abs_dy < 8) {
                xiaoxin_card_pager_notification_clear_all(&card_pager_);
                notification_scroll_y_ = 0;
                RenderNotificationPageAfterDataChange();
                notification_gesture_mode_ = NotificationGestureMode::None;
                notification_pressed_slot_ = -1;
                notification_pressed_visible_index_ = 0xff;
                notification_card_drag_x_ = 0;
                return;
            }

            const uint8_t total = xiaoxin_card_pager_notification_count(&card_pager_);

            if (abs_dy >= k_touch_drag_min_px && abs_dy > abs_dx) {
                notification_gesture_mode_ = NotificationGestureMode::VerticalScroll;
                const int16_t raw_scroll = (int16_t)(notification_drag_start_scroll_y_ + dy);
                const int16_t display_scroll = NotificationScrollDisplayY(raw_scroll, total);
                const int16_t target_scroll = ClampNotificationScrollY(raw_scroll, total);
                card_pager_.notification_index = NotificationIndexForScroll(target_scroll, total);
                AnimateNotificationScroll(display_scroll, target_scroll);
                return;
            }
            if (abs_dy > 4 && abs_dy > abs_dx) {
                notification_gesture_mode_ = NotificationGestureMode::VerticalScroll;
                const int16_t raw_scroll = (int16_t)(notification_drag_start_scroll_y_ + dy);
                const int16_t display_scroll = NotificationScrollDisplayY(raw_scroll, total);
                const int16_t target_scroll = ClampNotificationScrollY(raw_scroll, total);
                card_pager_.notification_index = NotificationIndexForScroll(target_scroll, total);
                AnimateNotificationScroll(display_scroll, target_scroll);
                return;
            }
            notification_gesture_mode_ = NotificationGestureMode::None;
            notification_pressed_slot_ = -1;
            notification_pressed_visible_index_ = 0xff;
            notification_card_drag_x_ = 0;
            return;
        }

        if (was_card_dragging) {
            const int16_t target_y =
                xiaoxin_card_pager_current_page(&card_pager_) == XIAOXIN_CARD_PAGE_HOME
                    ? HiddenCardLayerY(release_visual_page, card_pager_.screen_height)
                    : 0;
            AnimateCardPagerRelease(release_visual_page, release_y, target_y, true);
            ESP_LOGI(TAG, "Card pager page=%s animation=%d",
                xiaoxin_card_page_name(xiaoxin_card_pager_current_page(&card_pager_)),
                (int)xiaoxin_card_pager_animation(&card_pager_));
            return;
        }

        const int16_t dx = (int16_t)touch_last_x_ - (int16_t)touch_start_x_;
        const int16_t abs_dx = (int16_t)std::abs(dx);
        const int16_t abs_dy = (int16_t)std::abs((int16_t)touch_last_y_ - (int16_t)touch_start_y_);
        if (!xiaoxin_card_pager_allows_pet_interaction(&card_pager_)) {
            if (now_ms - touch_start_ms_ >= k_touch_hold_ms) {
                SetCardPagerPage(XIAOXIN_CARD_PAGE_HOME);
            }
            return;
        }

        if (abs_dx >= k_touch_drag_min_px && abs_dx > abs_dy) {
            DispatchLocalPetTriggerLocked(
                dx < 0 ? PAOPAO_PET_TRIGGER_LOCAL_DRAG_LEFT : PAOPAO_PET_TRIGGER_LOCAL_DRAG_RIGHT,
                PAOPAO_PET_MOOD_EVENT_LOCAL_DRAG,
                now_ms
            );
        } else if (now_ms - touch_start_ms_ >= k_touch_hold_ms) {
            DispatchLocalPetTriggerLocked(
                PAOPAO_PET_TRIGGER_LOCAL_HOLD,
                PAOPAO_PET_MOOD_EVENT_LOCAL_HOLD,
                now_ms
            );
        } else {
            DispatchLocalPetTriggerLocked(
                PAOPAO_PET_TRIGGER_LOCAL_TAP,
                PAOPAO_PET_MOOD_EVENT_LOCAL_TAP,
                now_ms
            );
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

        if (settings_open_) {
            HandleSettingsTouch(x, y, pressed);
            touch_pressed_ = pressed;
            return;
        }

        if (pressed) {
            if (!touch_pressed_) {
                ESP_LOGI(TAG, "Touch point x=%u y=%u", x, y);
            }
            touch_last_x_ = x;
            touch_last_y_ = y;
            touch_last_active_ms_ = now_ms;
            if (!touch_pressed_) {
                touch_start_x_ = x;
                touch_start_y_ = y;
                touch_start_ms_ = now_ms;
                if (card_layer_ != nullptr) {
                    lv_anim_delete(card_layer_, CardLayerSetY);
                }
                notification_drag_start_scroll_y_ = notification_scroll_y_;
                notification_gesture_mode_ = NotificationGestureMode::None;
                notification_pressed_slot_ = -1;
                notification_pressed_visible_index_ = 0xff;
                notification_card_drag_x_ = 0;
                if (xiaoxin_card_pager_current_page(&card_pager_) == XIAOXIN_CARD_PAGE_NOTIFICATIONS) {
                    notification_pressed_slot_ = NotificationCardSlotAtPoint(x, y);
                    notification_pressed_visible_index_ =
                        notification_pressed_slot_ >= 0 ? glass_cards_[notification_pressed_slot_].visible_index : 0xff;
                    if (NotificationClearButtonContains(x, y)) {
                        notification_gesture_mode_ = NotificationGestureMode::ClearAllPress;
                    }
                }
                xiaoxin_card_pager_press(&card_pager_, (int16_t)x, (int16_t)y);
            } else {
                if (xiaoxin_card_pager_current_page(&card_pager_) == XIAOXIN_CARD_PAGE_NOTIFICATIONS) {
                    const int16_t dx = (int16_t)x - (int16_t)touch_start_x_;
                    const int16_t dy = (int16_t)y - (int16_t)touch_start_y_;
                    const int16_t abs_dx = (int16_t)std::abs(dx);
                    const int16_t abs_dy = (int16_t)std::abs(dy);
                    const uint8_t total = xiaoxin_card_pager_notification_count(&card_pager_);
                    const bool at_notification_bottom =
                        notification_scroll_y_ <= NotificationMinScrollY(total) + 1;

                    if (!notification_dismiss_animating_ &&
                        notification_gesture_mode_ == NotificationGestureMode::None &&
                        notification_pressed_slot_ >= 0 &&
                        notification_pressed_visible_index_ != 0xff &&
                        dx < 0 &&
                        abs_dx >= k_notification_dismiss_intent_px &&
                        ((int32_t)abs_dx * 4) > ((int32_t)abs_dy * 5)) {
                        notification_gesture_mode_ = NotificationGestureMode::DismissCard;
                    }

                    if (notification_gesture_mode_ == NotificationGestureMode::DismissCard) {
                        notification_card_drag_x_ = std::max<int16_t>((int16_t)-DISPLAY_WIDTH, dx);
                        lv_obj_set_x(glass_cards_[notification_pressed_slot_].container, notification_card_drag_x_);
                        const int32_t opa = LV_OPA_COVER + ((int32_t)notification_card_drag_x_ * LV_OPA_COVER) / DISPLAY_WIDTH;
                        lv_obj_set_style_opa(
                            glass_cards_[notification_pressed_slot_].container,
                            (lv_opa_t)std::max<int32_t>(LV_OPA_30, opa),
                            0
                        );
                        return;
                    }

                    if (abs_dy > abs_dx) {
                        notification_gesture_mode_ = NotificationGestureMode::VerticalScroll;
                        if (dy < 0 && at_notification_bottom) {
                            xiaoxin_card_pager_drag(&card_pager_, (int16_t)x, (int16_t)y);
                            if (xiaoxin_card_pager_is_dragging(&card_pager_)) {
                                ApplyCardPagerVisual();
                            }
                        } else {
                            const int16_t raw_scroll = (int16_t)(notification_drag_start_scroll_y_ + dy);
                            ApplyNotificationScrollVisual(NotificationScrollDisplayY(raw_scroll, total), false, true);
                        }
                        return;
                    }
                }

                xiaoxin_card_pager_drag(&card_pager_, (int16_t)x, (int16_t)y);
                if (xiaoxin_card_pager_is_dragging(&card_pager_)) {
                    ApplyCardPagerVisual();
                }
            }
        } else if (touch_pressed_) {
            HandleTouchRelease(now_ms);
            touch_last_active_ms_ = now_ms;
        }

        touch_pressed_ = pressed;
    }

    void RunRenderLoop() {
        while (true) {
            const int64_t perf_start_us = k_ui_perf_trace_enabled ? esp_timer_get_time() : 0;
            {
                DisplayLockGuard lock(this);
                const uint32_t now_ms = NowMs();
                PollTouch(now_ms);
                paopao_pet_trigger_tick(&trigger_, now_ms);
                ApplyPetStateIfChanged();
                LogUiPerfSummary(now_ms);
            }
            if (k_ui_perf_trace_enabled) {
                AddUiPerfSample(
                    ui_perf_touch_loop_calls_,
                    ui_perf_touch_loop_total_us_,
                    ui_perf_touch_loop_max_us_,
                    (uint32_t)(esp_timer_get_time() - perf_start_us)
                );
            }
            vTaskDelay(pdMS_TO_TICKS(k_touch_poll_ms));
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
    TaskHandle_t power_off_task_ = nullptr;
    xiaoxin_power_control_t power_control_ = {};
    adc_oneshot_unit_handle_t battery_adc_handle_ = nullptr;
    adc_cali_handle_t battery_adc_cali_handle_ = nullptr;
    bool battery_adc_initialized_ = false;
    bool battery_adc_available_ = false;
    int last_battery_voltage_mv_ = 0;
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

    void InitializeBatteryAdc() {
        if (battery_adc_initialized_) {
            return;
        }
        battery_adc_initialized_ = true;

        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
        };
        esp_err_t err = adc_oneshot_new_unit(&init_config, &battery_adc_handle_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Battery ADC unit init failed: %s", esp_err_to_name(err));
            return;
        }

        adc_oneshot_chan_cfg_t channel_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        err = adc_oneshot_config_channel(battery_adc_handle_, k_battery_adc_channel, &channel_config);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Battery ADC channel init failed: %s", esp_err_to_name(err));
            return;
        }

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        err = adc_cali_create_scheme_curve_fitting(&cali_config, &battery_adc_cali_handle_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Battery ADC calibration init failed: %s", esp_err_to_name(err));
            return;
        }

        battery_adc_available_ = true;
        ESP_LOGI(TAG, "Battery ADC initialized on GPIO8 / ADC1 channel 7");
    }

    int ReadBatteryVoltageMv() {
        InitializeBatteryAdc();
        if (!battery_adc_available_) {
            return 0;
        }

        int voltage_sum = 0;
        uint8_t sample_count = 0;
        for (uint8_t i = 0; i < k_battery_adc_samples; ++i) {
            int raw_value = 0;
            int pin_voltage_mv = 0;
            if (adc_oneshot_read(battery_adc_handle_, k_battery_adc_channel, &raw_value) != ESP_OK) {
                continue;
            }
            if (adc_cali_raw_to_voltage(battery_adc_cali_handle_, raw_value, &pin_voltage_mv) != ESP_OK) {
                continue;
            }
            voltage_sum += pin_voltage_mv * k_battery_voltage_divider;
            sample_count++;
        }

        return sample_count > 0 ? voltage_sum / sample_count : 0;
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
                        static_cast<PaopaoPetDisplay*>(display_)->DispatchLocalPetTrigger(PAOPAO_PET_TRIGGER_LOCAL_SHAKE, PAOPAO_PET_MOOD_EVENT_LOCAL_SHAKE);
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

    void WaitForPowerButtonReleaseAndSleep() {
        ESP_LOGI(TAG, "Waiting for PWR release before soft power-off sleep");
        while (!gpio_get_level(PWR_BUTTON_GPIO)) {
            vTaskDelay(pdMS_TO_TICKS(k_power_off_release_poll_ms));
        }

        vTaskDelay(pdMS_TO_TICKS(120));
        ESP_LOGI(TAG, "Entering deep sleep; PWR button will wake the board when USB is still powering it");
        esp_err_t err = esp_sleep_enable_ext0_wakeup(PWR_BUTTON_GPIO, 0);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "PWR deep-sleep wake setup failed: %s", esp_err_to_name(err));
            power_off_task_ = nullptr;
            vTaskDelete(nullptr);
            return;
        }
        esp_deep_sleep_start();
    }

    void RequestPowerOff() {
        if (xiaoxin_power_control_shutdown_requested(&power_control_)) {
            return;
        }

        xiaoxin_power_control_handle_long_press(&power_control_);
        GetBacklight()->SetBrightness(0);
        gpio_set_level(PWR_Control_PIN, xiaoxin_power_control_power_hold(&power_control_));
        ESP_LOGI(TAG, "PWR long press: power hold released");

        if (power_off_task_ == nullptr) {
            xTaskCreatePinnedToCore(
                PowerOffTask,
                "pwr_off",
                3072,
                this,
                4,
                &power_off_task_,
                1
            );
        }
    }

    static void PowerOffTask(void* arg) {
        static_cast<CustomBoard*>(arg)->WaitForPowerButtonReleaseAndSleep();
    }
 
    void InitializeButtonsCustom() {
        xiaoxin_power_control_init(&power_control_);
        gpio_reset_pin(BOOT_BUTTON_GPIO);                                     
        gpio_set_direction(BOOT_BUTTON_GPIO, GPIO_MODE_INPUT);   
        gpio_reset_pin(PWR_BUTTON_GPIO);                                     
        gpio_set_direction(PWR_BUTTON_GPIO, GPIO_MODE_INPUT);   
        gpio_reset_pin(PWR_Control_PIN);                                     
        gpio_set_direction(PWR_Control_PIN, GPIO_MODE_OUTPUT);     
        gpio_set_level(PWR_Control_PIN, xiaoxin_power_control_power_hold(&power_control_));
    }

    static xiaoxin_settings_runtime_state_t SettingsRuntimeState(DeviceState state) {
        switch (state) {
            case kDeviceStateStarting:
                return XIAOXIN_SETTINGS_RUNTIME_STARTING;
            case kDeviceStateWifiConfiguring:
                return XIAOXIN_SETTINGS_RUNTIME_WIFI_CONFIGURING;
            case kDeviceStateIdle:
                return XIAOXIN_SETTINGS_RUNTIME_IDLE;
            case kDeviceStateConnecting:
                return XIAOXIN_SETTINGS_RUNTIME_CONNECTING;
            case kDeviceStateListening:
                return XIAOXIN_SETTINGS_RUNTIME_LISTENING;
            case kDeviceStateSpeaking:
                return XIAOXIN_SETTINGS_RUNTIME_SPEAKING;
            case kDeviceStateUpgrading:
                return XIAOXIN_SETTINGS_RUNTIME_UPGRADING;
            case kDeviceStateActivating:
                return XIAOXIN_SETTINGS_RUNTIME_ACTIVATING;
            case kDeviceStateAudioTesting:
                return XIAOXIN_SETTINGS_RUNTIME_AUDIO_TESTING;
            case kDeviceStateFatalError:
                return XIAOXIN_SETTINGS_RUNTIME_FATAL_ERROR;
            default:
                return XIAOXIN_SETTINGS_RUNTIME_UNKNOWN;
        }
    }

    void OpenSettingsOverlayFromBootButton() {
        auto& app = Application::GetInstance();
        const DeviceState device_state = app.GetDeviceState();
        const xiaoxin_settings_runtime_state_t runtime = SettingsRuntimeState(device_state);
        if (runtime != XIAOXIN_SETTINGS_RUNTIME_IDLE || !xiaoxin_settings_can_open(runtime)) {
            if (app.GetDeviceState() == kDeviceStateConnecting ||
                app.GetDeviceState() == kDeviceStateListening ||
                app.GetDeviceState() == kDeviceStateSpeaking) {
                GetDisplay()->ShowNotification("璇峰厛缁撴潫瀵硅瘽", 1600);
            }
            return;
        }
        auto* display = static_cast<PaopaoPetDisplay*>(display_);
        if (display != nullptr) {
            display->OpenSettingsOverlay();
        }
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
            auto* display = static_cast<PaopaoPetDisplay*>(self->display_);
            if (display != nullptr && display->IsSettingsOpen()) {
                display->CloseSettingsOverlay();
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                self->EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        }, this);
        iot_button_register_cb(boot_btn, BUTTON_LONG_PRESS_START, nullptr, [](void* button_handle, void* usr_data) {
            auto self = static_cast<CustomBoard*>(usr_data);
            self->OpenSettingsOverlayFromBootButton();
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
            self->RequestPowerOff();
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

    static CustomBoard* Instance() {
        return instance_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_LEFT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_RIGHT); // I2S_STD_SLOT_LEFT / I2S_STD_SLOT_RIGHT / I2S_STD_SLOT_BOTH

        return &audio_codec;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        const int voltage_mv = ReadBatteryVoltageMv();
        if (voltage_mv <= 0) {
            last_battery_voltage_mv_ = 0;
            return false;
        }

        last_battery_voltage_mv_ = voltage_mv;
        level = xiaoxin_battery_percent_from_mv(voltage_mv);
        charging = false;
        discharging = true;
        ESP_LOGI(TAG, "Battery voltage=%dmV level=%d%%", voltage_mv, level);
        return true;
    }

    int LastBatteryVoltageMv() const {
        return last_battery_voltage_mv_;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

static int BoardBatteryVoltageMv() {
    CustomBoard* board = CustomBoard::Instance();
    return board != nullptr ? board->LastBatteryVoltageMv() : 0;
}

DECLARE_BOARD(CustomBoard);

CustomBoard* CustomBoard::instance_ = nullptr;
