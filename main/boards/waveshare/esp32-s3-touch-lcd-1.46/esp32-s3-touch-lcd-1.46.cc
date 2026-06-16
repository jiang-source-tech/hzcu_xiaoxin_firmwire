#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"

#include <esp_log.h>
#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_spd2010.h>
#include <esp_timer.h>
#include "esp_io_expander_tca9554.h"
#include <iot_button.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <cstring>

extern "C" {
#include "paopao_pet_renderer.h"
#include "paopao_pet_state.h"
}

#define TAG "waveshare_lcd_1_46"

// Draw the paopao pet frames directly so the on-device look matches paopao_ui.
static esp_lcd_panel_handle_t s_paopao_panel = nullptr;

extern "C" esp_err_t paopao_pet_board_draw_bitmap(
    int x_start,
    int y_start,
    int x_end,
    int y_end,
    const uint16_t* pixels
) {
    if (s_paopao_panel == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_lcd_panel_draw_bitmap(s_paopao_panel, x_start, y_start, x_end, y_end, pixels);
}

class PaopaoPetDisplay : public Display {
public:
    PaopaoPetDisplay() {
        width_ = DISPLAY_WIDTH;
        height_ = DISPLAY_HEIGHT;
        mutex_ = xSemaphoreCreateRecursiveMutex();
    }

    virtual void SetupUI() override {
        Display::SetupUI();
        DisplayLockGuard lock(this);
        paopao_pet_renderer_init();
        paopao_pet_play_state(current_state_);

        if (render_task_ == nullptr) {
            xTaskCreatePinnedToCore(RenderTask, "paopao_pet", 4096, this, 4, &render_task_, 1);
        }
    }

    virtual void SetStatus(const char* status) override {
        if (status == nullptr) {
            return;
        }

        if (std::strstr(status, "Listening") || std::strstr(status, "listening")) {
            SetPetState(PAOPAO_PET_STATE_WAITING);
        } else if (std::strstr(status, "Speaking") || std::strstr(status, "speaking")) {
            SetPetState(PAOPAO_PET_STATE_WORKING);
        } else if (std::strstr(status, "Thinking") || std::strstr(status, "thinking")) {
            SetPetState(PAOPAO_PET_STATE_THINKING);
        } else if (std::strstr(status, "Standby") || std::strstr(status, "standby")) {
            SetPetState(PAOPAO_PET_STATE_IDLE);
        }
    }

    virtual void SetEmotion(const char* emotion) override {
        if (emotion == nullptr) {
            return;
        }

        if (std::strstr(emotion, "happy") || std::strstr(emotion, "laugh") ||
            std::strstr(emotion, "loving") || std::strstr(emotion, "cool")) {
            SetPetState(PAOPAO_PET_STATE_DONE);
        } else if (std::strstr(emotion, "think")) {
            SetPetState(PAOPAO_PET_STATE_THINKING);
        } else if (std::strstr(emotion, "sleep")) {
            SetPetState(PAOPAO_PET_STATE_SLEEPING);
        } else if (std::strstr(emotion, "sad") || std::strstr(emotion, "angry") ||
                   std::strstr(emotion, "cry") || std::strstr(emotion, "shock")) {
            SetPetState(PAOPAO_PET_STATE_FAILING);
        } else if (std::strstr(emotion, "neutral") || std::strstr(emotion, "microchip")) {
            SetPetState(PAOPAO_PET_STATE_IDLE);
        }
    }

    virtual void SetChatMessage(const char* role, const char* content) override {}
    virtual void ClearChatMessages() override {}
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override {}
    virtual void UpdateStatusBar(bool update_all = false) override {}
    virtual void SetPowerSaveMode(bool on) override {}

private:
    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t render_task_ = nullptr;
    paopao_pet_state_t current_state_ = PAOPAO_PET_STATE_IDLE;

    void SetPetState(paopao_pet_state_t state) {
        DisplayLockGuard lock(this);
        current_state_ = state;
        paopao_pet_play_state(state);
    }

    void RunRenderLoop() {
        while (true) {
            {
                DisplayLockGuard lock(this);
                paopao_pet_renderer_tick((uint32_t)(esp_timer_get_time() / 1000ULL));
            }
            vTaskDelay(pdMS_TO_TICKS(8));
        }
    }

    static void RenderTask(void* arg) {
        static_cast<PaopaoPetDisplay*>(arg)->RunRenderLoop();
    }

    virtual bool Lock(int timeout_ms = 0) override {
        if (mutex_ == nullptr) {
            return true;
        }
        const TickType_t ticks = timeout_ms <= 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
        return xSemaphoreTakeRecursive(mutex_, ticks) == pdTRUE;
    }

    virtual void Unlock() override {
        if (mutex_ != nullptr) {
            xSemaphoreGiveRecursive(mutex_);
        }
    }
};

class CustomBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    esp_io_expander_handle_t io_expander = NULL;
    Display* display_;
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
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        s_paopao_panel = panel;
        display_ = new PaopaoPetDisplay();
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
            if (app.GetDeviceState() == kDeviceStateStarting) {
                self->EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        }, this);
        iot_button_register_cb(boot_btn, BUTTON_LONG_PRESS_START, nullptr, [](void* button_handle, void* usr_data) {
            // 长按无处理
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
        InitializeButtons();
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
