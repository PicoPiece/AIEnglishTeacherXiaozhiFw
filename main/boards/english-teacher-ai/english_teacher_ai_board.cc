#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "adc_battery_monitor.h"
#include "audio/music_player.h"
#include "mcp_server.h"
#include "app/app_manager.h"
#include "app/chat_app.h"
#include "app/music_app.h"
#include "app/radio_app.h"
#include "app/messages_app.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <ctime>

#define TAG "EnglishTeacherAI"

class EnglishTeacherAiBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    LcdDisplay* display_ = nullptr;
    AdcBatteryMonitor* battery_monitor_ = nullptr;
    esp_timer_handle_t brightness_timer_ = nullptr;
    uint8_t last_auto_brightness_ = 0;
    MusicPlayer* music_player_ = nullptr;
    bool sd_card_mounted_ = false;
    AppManager* app_manager_ = nullptr;
    ChatApp* chat_app_ = nullptr;
    MusicApp* music_app_ = nullptr;
    MessagesApp* messages_app_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_SPI_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 20 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                     DISPLAY_SWAP_XY);
    }

    uint8_t GetBrightnessForCurrentTime() {
        time_t now = time(NULL);
        struct tm* tm = localtime(&now);
        if (tm->tm_year < 2025 - 1900) {
            return DEFAULT_BRIGHTNESS;
        }
        int hour = tm->tm_hour;
        if (hour >= 7 && hour < 18) return BRIGHTNESS_DAY;
        if (hour >= 18 && hour < 22) return BRIGHTNESS_EVENING;
        return BRIGHTNESS_NIGHT;
    }

    void UpdateAdaptiveBrightness() {
        uint8_t target = GetBrightnessForCurrentTime();
        if (target != last_auto_brightness_) {
            last_auto_brightness_ = target;
            GetBacklight()->SetBrightness(target);
            ESP_LOGI(TAG, "Adaptive brightness: %d%%", target);
        }
    }

    void InitializeAdaptiveBrightness() {
        last_auto_brightness_ = DEFAULT_BRIGHTNESS;
        GetBacklight()->SetBrightness(DEFAULT_BRIGHTNESS, true);

        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto self = static_cast<EnglishTeacherAiBoard*>(arg);
                self->UpdateAdaptiveBrightness();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "adaptive_brightness",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &brightness_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(brightness_timer_, 60 * 1000000));
    }

    void InitializeSdCard() {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.flags = SDMMC_HOST_FLAG_1BIT;
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 1;
        slot.clk = SD_CLK_PIN;
        slot.cmd = SD_CMD_PIN;
        slot.d0  = SD_D0_PIN;
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 0,
            .disk_status_check_enable = true,
        };
        sdmmc_card_t* card = nullptr;
        esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot, &mount_cfg, &card);
        if (ret == ESP_OK) {
            sdmmc_card_print_info(stdout, card);
            ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
            sd_card_mounted_ = true;
        } else {
            ESP_LOGW(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
        }
    }

    void InitializeMusicPlayer() {
        if (!sd_card_mounted_) return;
        music_player_ = new MusicPlayer(GetAudioCodec());
        RegisterMusicMcpTools();
    }

    void RegisterMusicMcpTools() {
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.list_sd_music",
            "List all music files on the SD card. Returns a JSON array of objects with path, name, and size fields.",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                auto files = MusicPlayer::ListMusicFiles(SD_MOUNT_POINT);
                cJSON* arr = cJSON_CreateArray();
                for (auto& f : files) {
                    cJSON* item = cJSON_CreateObject();
                    cJSON_AddStringToObject(item, "path", f.path.c_str());
                    cJSON_AddStringToObject(item, "name", f.name.c_str());
                    cJSON_AddNumberToObject(item, "size", f.size);
                    cJSON_AddItemToArray(arr, item);
                }
                return arr;
            });

        mcp.AddTool("self.play_sd_music",
            "Play a music file from the SD card. Stops any current playback first. "
            "Use self.list_sd_music to get available files.",
            PropertyList({
                Property("filepath", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                auto filepath = properties["filepath"].value<std::string>();
                if (!music_player_) {
                    return std::string("{\"status\":\"error\",\"message\":\"Music player not available\"}");
                }
                GetAudioCodec()->EnableOutput(true);
                bool ok = music_player_->PlayFile(filepath);
                if (!ok) {
                    return std::string("{\"status\":\"error\",\"message\":\"Failed to play file\"}");
                }
                std::string name = music_player_->CurrentTrackName();
                cJSON* result = cJSON_CreateObject();
                cJSON_AddStringToObject(result, "status", "playing");
                cJSON_AddStringToObject(result, "name", name.c_str());
                return result;
            });
    }

    void RegisterMessagesMcpTools() {
        if (!messages_app_) return;
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.push_message",
            "Push a text message to the device for the user to read.",
            PropertyList({
                Property("sender", kPropertyTypeString),
                Property("content", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                auto sender = properties["sender"].value<std::string>();
                auto content = properties["content"].value<std::string>();
                messages_app_->PushMessage(sender, content);
                if (app_manager_ && app_manager_->InMenu()) {
                    app_manager_->RefreshMenuBadges();
                }
                cJSON* result = cJSON_CreateObject();
                cJSON_AddStringToObject(result, "status", "delivered");
                return result;
            });

        mcp.AddTool("self.list_messages",
            "List messages currently stored on device.",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                auto& msgs = messages_app_->GetMessages();
                cJSON* arr = cJSON_CreateArray();
                for (auto& m : msgs) {
                    cJSON* item = cJSON_CreateObject();
                    cJSON_AddStringToObject(item, "sender", m.sender.c_str());
                    cJSON_AddStringToObject(item, "content", m.content.c_str());
                    cJSON_AddNumberToObject(item, "timestamp", (double)m.timestamp);
                    cJSON_AddBoolToObject(item, "read", m.read);
                    cJSON_AddItemToArray(arr, item);
                }
                return arr;
            });
    }

    void InitializeAppManager() {
        app_manager_ = new AppManager(display_);

        chat_app_ = new ChatApp();
        app_manager_->RegisterApp(chat_app_);

        if (music_player_) {
            music_app_ = new MusicApp(music_player_, GetAudioCodec());
            music_player_->SetTrackInfoCallback([this](const std::string& name, int idx, int total) {
                if (music_app_) {
                    music_app_->OnTrackChanged(name, idx, total);
                }
            });
            music_player_->SetStopCallback([this]() {
                if (music_app_) {
                    music_app_->OnPlaybackStopped();
                }
            });
            app_manager_->RegisterApp(music_app_);
        }

        app_manager_->RegisterApp(new RadioApp());

        messages_app_ = new MessagesApp();
        app_manager_->RegisterApp(messages_app_);
        RegisterMessagesMcpTools();

        // Auto-enter ChatApp on boot so existing UX is preserved.
        // Menu is accessible via long-press BOOT (2s) from inside any app.
        app_manager_->AutoEnterFirstApp();

        ESP_LOGI(TAG, "AppManager initialized with %d apps", (int)(music_player_ ? 4 : 3));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            auto state = app.GetDeviceState();
            if (state == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            if (state < kDeviceStateIdle) return;

            if (app_manager_) {
                app_manager_->OnButtonClick();
            }
        });

        boot_button_.OnDoubleClick([this]() {
            if (app_manager_) {
                app_manager_->OnButtonDoubleClick();
            }
        });

        boot_button_.OnLongPress([this]() {
            auto& app = Application::GetInstance();
            auto state = app.GetDeviceState();

            if (state == kDeviceStateStarting ||
                state == kDeviceStateWifiConfiguring) {
                EnterWifiConfigMode();
                return;
            }

            if (app_manager_ && !app_manager_->InMenu()) {
                app_manager_->OnButtonLongPress();
            } else {
                EnterWifiConfigMode();
            }
        });

        volume_up_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) return;

            if (app_manager_) {
                app_manager_->OnVolumeUpClick();
            }
        });

        volume_up_button_.OnLongPress([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 20;
            if (volume > 100) volume = 100;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) return;

            if (app_manager_) {
                app_manager_->OnVolumeDownClick();
            }
        });

        volume_down_button_.OnLongPress([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 20;
            if (volume < 0) volume = 0;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
    }

public:
    EnglishTeacherAiBoard() :
        boot_button_(BOOT_BUTTON_GPIO, false, 2000),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO)
    {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeSdCard();
        InitializeButtons();
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            InitializeAdaptiveBrightness();
        }
        battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_2, ADC_CHANNEL_7, 100000, 100000, CHARGE_DETECT_PIN);
        InitializeMusicPlayer();
        InitializeAppManager();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_, I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (battery_monitor_ == nullptr) return false;
        charging = battery_monitor_->IsCharging();
        discharging = battery_monitor_->IsDischarging();
        level = battery_monitor_->GetBatteryLevel();
        return true;
    }
};

DECLARE_BOARD(EnglishTeacherAiBoard);
