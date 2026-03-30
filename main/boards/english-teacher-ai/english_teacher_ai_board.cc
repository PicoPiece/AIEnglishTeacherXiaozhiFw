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
#include "audio/radio_player.h"
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
    RadioPlayer* radio_player_ = nullptr;
    bool sd_card_mounted_ = false;
    AppManager* app_manager_ = nullptr;
    esp_timer_handle_t vol_up_repeat_timer_ = nullptr;
    esp_timer_handle_t vol_down_repeat_timer_ = nullptr;
    bool vol_up_changed_ = false;
    bool vol_down_changed_ = false;
    ChatApp* chat_app_ = nullptr;
    MusicApp* music_app_ = nullptr;
    RadioApp* radio_app_ = nullptr;
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

    void InitializeRadioPlayer() {
        radio_player_ = new RadioPlayer(GetAudioCodec());

        // English radio stations optimized for Vietnam (low bitrate = less lag)
        radio_player_->AddStation("BBC World Service", "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service",           "News 56k");
        radio_player_->AddStation("BBC WS East Asia",  "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service_east_asia", "News 56k");
        radio_player_->AddStation("NPR News",          "http://npr-ice.streamguys1.com/live.mp3",                          "News 128k");
        radio_player_->AddStation("CNN Radio",          "https://tunein.cdnstream1.com/2868_96.mp3",                        "News 96k");
        radio_player_->AddStation("SomaFM Groove",     "http://ice1.somafm.com/groovesalad-128-mp3",                       "Chill 128k");
        radio_player_->AddStation("KEXP Seattle",      "http://live-mp3-128.kexp.org/kexp128.mp3",                         "Indie 128k");

        ESP_LOGI(TAG, "RadioPlayer initialized with %d stations", radio_player_->StationCount());
    }

    void RegisterMusicMcpTools() {
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.list_sd_music",
            "List all music on the SD card. Returns a compact text summary of folders and songs. "
            "Use the song names with self.play_sd_music to play.",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string summary;
                auto folders = MusicPlayer::ListFolders(SD_MOUNT_POINT);
                auto root_files = MusicPlayer::ListFilesInFolder(SD_MOUNT_POINT);

                for (auto& folder_name : folders) {
                    std::string folder_path = std::string(SD_MOUNT_POINT) + "/" + folder_name;
                    auto files = MusicPlayer::ListFilesInFolder(folder_path.c_str());
                    summary += "[" + folder_name + "] " + std::to_string(files.size()) + " songs:";
                    for (auto& f : files) {
                        std::string name = f.name;
                        if (name.length() > 60) name = name.substr(0, 57) + "...";
                        summary += " " + name + ";";
                    }
                    summary += "\n";
                }
                if (!root_files.empty()) {
                    summary += "[root] " + std::to_string(root_files.size()) + " songs:";
                    for (auto& f : root_files) {
                        std::string name = f.name;
                        if (name.length() > 60) name = name.substr(0, 57) + "...";
                        summary += " " + name + ";";
                    }
                    summary += "\n";
                }
                if (summary.empty()) summary = "No music files found on SD card.";
                return summary;
            });

        mcp.AddTool("self.play_sd_music",
            "Search and play music from SD card. Only works when user is in Music app. "
            "If user is in AI Chat, tell them to switch to Music app first via long-press BOOT. "
            "Provide a keyword to search by song name. Optionally set 'folder' to limit search.",
            PropertyList({
                Property("query", kPropertyTypeString),
                Property("folder", kPropertyTypeString, std::string(""))
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                auto query = properties["query"].value<std::string>();
                auto folder = properties["folder"].value<std::string>();
                if (!music_player_) {
                    return std::string("{\"error\":\"Music player not available\"}");
                }

                if (app_manager_) {
                    auto* active = app_manager_->GetActiveApp();
                    if (active && active != music_app_) {
                        return std::string("{\"error\":\"Music can only play in Music app. "
                            "User should long-press BOOT to open menu, then select Music.\"}");
                    }
                }

                std::string search_path = SD_MOUNT_POINT;
                if (!folder.empty()) {
                    search_path += "/" + folder;
                }
                auto files = MusicPlayer::ListMusicFiles(search_path.c_str());
                if (files.empty()) {
                    return std::string("{\"error\":\"No music files found\"}");
                }

                std::string query_lower = query;
                for (auto& c : query_lower) c = tolower(c);

                std::vector<std::string> playlist;
                int match_idx = -1;
                for (auto& f : files) {
                    std::string name_lower = f.name;
                    for (auto& c : name_lower) c = tolower(c);
                    playlist.push_back(f.path);
                    if (match_idx < 0 && name_lower.find(query_lower) != std::string::npos) {
                        match_idx = (int)playlist.size() - 1;
                    }
                }

                if (match_idx < 0) {
                    return std::string("{\"error\":\"No song matching '") + query + "' found\"}";
                }

                if (radio_player_ && radio_player_->IsPlaying()) {
                    radio_player_->Stop();
                }
                GetAudioCodec()->EnableOutput(true);
                music_player_->StartPlaylist(playlist, match_idx);

                cJSON* result = cJSON_CreateObject();
                cJSON_AddStringToObject(result, "status", "playing");
                cJSON_AddStringToObject(result, "name", files[match_idx].name.c_str());
                cJSON_AddStringToObject(result, "folder", folder.empty() ? "all" : folder.c_str());
                cJSON_AddNumberToObject(result, "playlist_size", (double)playlist.size());
                cJSON_AddStringToObject(result, "mode", PlayModeName(music_player_->GetPlayMode()));
                return result;
            });

        mcp.AddTool("self.stop_sd_music",
            "Stop currently playing music from SD card.",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                if (!music_player_) {
                    return std::string("{\"error\":\"Music player not available\"}");
                }
                if (music_player_->IsPlaying()) {
                    music_player_->Stop();
                    return std::string("{\"status\":\"stopped\"}");
                }
                return std::string("{\"status\":\"not playing\"}");
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
                auto msgs = messages_app_->GetMessagesCopy();
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

        if (radio_player_) {
            radio_app_ = new RadioApp(radio_player_, GetAudioCodec());
            radio_player_->SetStationChangeCallback([this](const std::string& name, int idx, int total) {
                if (radio_app_) {
                    radio_app_->OnStationChanged(name, idx, total);
                }
            });
            radio_player_->SetStatusCallback([this](const std::string& status) {
                if (radio_app_) {
                    radio_app_->OnStatusChanged(status);
                }
            });
            radio_player_->SetStopCallback([this]() {
                if (radio_app_) {
                    radio_app_->OnPlaybackStopped();
                }
            });
            radio_player_->SetErrorCallback([this](const std::string& error) {
                if (radio_app_) {
                    radio_app_->OnStreamError(error);
                }
            });
            app_manager_->RegisterApp(radio_app_);
        }

        messages_app_ = new MessagesApp();
        app_manager_->RegisterApp(messages_app_);
        RegisterMessagesMcpTools();

        app_manager_->AutoEnterFirstApp();

        ESP_LOGI(TAG, "AppManager initialized with %d apps", app_manager_->AppCount());
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
            auto& app = Application::GetInstance();
            auto state = app.GetDeviceState();
            if (state < kDeviceStateIdle) return;

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
                if (app_manager_) {
                    app_manager_->CleanupForWifiConfig();
                }
                EnterWifiConfigMode();
            }
        });

        // GPIO 45 = physical LEFT button, GPIO 46 = physical RIGHT button
        // Click: app navigation (direction already correct)
        // Hold: volume (LEFT = decrease 5%/s, RIGHT = increase 5%/s)

        esp_timer_create_args_t vol_up_timer_args = {
            .callback = [](void* arg) {
                auto* self = (EnglishTeacherAiBoard*)arg;
                auto codec = self->GetAudioCodec();
                int volume = codec->output_volume() - 5;
                if (volume < 0) volume = 0;
                codec->SetOutputVolume(volume);
                ESP_LOGI(TAG, "VOL- (left hold) -> volume=%d", volume);
                if (self->app_manager_) self->app_manager_->ShowVolumeNotification(volume);
                self->vol_up_changed_ = true;
                esp_timer_start_once(self->vol_up_repeat_timer_, 500000);
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "vol_up_repeat",
            .skip_unhandled_events = true
        };
        esp_timer_create(&vol_up_timer_args, &vol_up_repeat_timer_);

        esp_timer_create_args_t vol_down_timer_args = {
            .callback = [](void* arg) {
                auto* self = (EnglishTeacherAiBoard*)arg;
                auto codec = self->GetAudioCodec();
                int volume = codec->output_volume() + 5;
                if (volume > 100) volume = 100;
                codec->SetOutputVolume(volume);
                ESP_LOGI(TAG, "VOL+ (right hold) -> volume=%d", volume);
                if (self->app_manager_) self->app_manager_->ShowVolumeNotification(volume);
                self->vol_down_changed_ = true;
                esp_timer_start_once(self->vol_down_repeat_timer_, 500000);
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "vol_down_repeat",
            .skip_unhandled_events = true
        };
        esp_timer_create(&vol_down_timer_args, &vol_down_repeat_timer_);

        volume_up_button_.OnPressDown([this]() {
            vol_up_changed_ = false;
            esp_timer_start_once(vol_up_repeat_timer_, 800000);
        });

        volume_up_button_.OnPressUp([this]() {
            esp_timer_stop(vol_up_repeat_timer_);
            if (!vol_up_changed_) {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() >= kDeviceStateIdle && app_manager_) {
                    app_manager_->OnVolumeUpClick();
                }
            }
        });

        volume_down_button_.OnPressDown([this]() {
            vol_down_changed_ = false;
            esp_timer_start_once(vol_down_repeat_timer_, 800000);
        });

        volume_down_button_.OnPressUp([this]() {
            esp_timer_stop(vol_down_repeat_timer_);
            if (!vol_down_changed_) {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() >= kDeviceStateIdle && app_manager_) {
                    app_manager_->OnVolumeDownClick();
                }
            }
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
        InitializeRadioPlayer();
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
