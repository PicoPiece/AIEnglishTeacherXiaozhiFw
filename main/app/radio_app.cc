#include "radio_app.h"
#include "audio/radio_player.h"
#include "audio_codec.h"
#include "display/display.h"
#include "display/lcd_display.h"
#include "board.h"

#include <esp_log.h>
#include <cstdio>

#define TAG "RadioApp"

RadioApp::RadioApp(RadioPlayer* player, AudioCodec* codec)
    : player_(player), codec_(codec) {}

void RadioApp::OnEnter(LcdDisplay* display) {
    display_ = display;
    current_screen_ = Screen::kStationList;
    selection_ = player_ ? player_->CurrentStationIndex() : 0;
    ShowStationList();
    ESP_LOGI(TAG, "Entering Radio");
}

void RadioApp::OnExit() {
    auto* saved_display = display_;
    display_ = nullptr;

    if (player_ && player_->IsPlaying()) {
        player_->Stop();
        Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
        ESP_LOGI(TAG, "WiFi power save restored on exit");
    }

    if (ui_container_ && saved_display) {
        DisplayLockGuard lock(saved_display);
        lv_obj_del(ui_container_);
    }
    ui_container_ = nullptr;
    status_label_ = nullptr;
    list_items_.clear();

    ESP_LOGI(TAG, "Exiting Radio");
}

bool RadioApp::IsPlaying() {
    return player_ && player_->IsPlaying();
}

void RadioApp::OnStationChanged(const std::string& name, int index, int total) {
    if (!display_) return;
    if (current_screen_ == Screen::kNowPlaying && status_label_) {
        DisplayLockGuard lock(display_);
        char buf[128];
        snprintf(buf, sizeof(buf), "%s\n(%d / %d)", name.c_str(), index + 1, total);
        lv_label_set_text(status_label_, buf);
    }
}

void RadioApp::OnStatusChanged(const std::string& status) {
    if (!display_) return;
    if (current_screen_ == Screen::kNowPlaying && status_label_) {
        DisplayLockGuard lock(display_);
        std::string name = player_->CurrentStationName();
        int idx = player_->CurrentStationIndex();
        int total = player_->StationCount();
        char buf[128];
        snprintf(buf, sizeof(buf), "%s\n(%d / %d)\n\n%s", name.c_str(), idx + 1, total, status.c_str());
        lv_label_set_text(status_label_, buf);
    }
}

void RadioApp::OnPlaybackStopped() {
    if (display_ && current_screen_ == Screen::kNowPlaying) {
        ShowStationList();
    }
}

void RadioApp::OnStreamError(const std::string& error) {
    if (!display_) return;
    if (current_screen_ == Screen::kNowPlaying && status_label_) {
        DisplayLockGuard lock(display_);
        std::string msg = player_->CurrentStationName() + "\n\n" + error + "...";
        lv_label_set_text(status_label_, msg.c_str());
    }
}

void RadioApp::ShowStationList() {
    if (ui_container_ && display_) {
        DisplayLockGuard lock(display_);
        lv_obj_del(ui_container_);
    }
    ui_container_ = nullptr;
    status_label_ = nullptr;
    list_items_.clear();
    current_screen_ = Screen::kStationList;

    if (!player_ || player_->StationCount() == 0) {
        DisplayLockGuard lock(display_);
        lv_obj_t* screen = lv_screen_active();
        ui_container_ = lv_obj_create(screen);
        lv_obj_set_size(ui_container_, 240, 200);
        lv_obj_align(ui_container_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(ui_container_, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_bg_opa(ui_container_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(ui_container_, 0, 0);
        lv_obj_set_style_radius(ui_container_, 0, 0);

        lv_obj_t* msg = lv_label_create(ui_container_);
        lv_label_set_text(msg, "No stations");
        lv_obj_set_style_text_color(msg, lv_color_hex(0x888888), 0);
        lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    DisplayLockGuard lock(display_);
    lv_obj_t* screen = lv_screen_active();

    ui_container_ = lv_obj_create(screen);
    lv_obj_set_size(ui_container_, 240, 200);
    lv_obj_align(ui_container_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ui_container_, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(ui_container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_container_, 0, 0);
    lv_obj_set_style_radius(ui_container_, 0, 0);
    lv_obj_set_style_pad_all(ui_container_, 4, 0);
    lv_obj_set_style_pad_row(ui_container_, 2, 0);
    lv_obj_set_flex_flow(ui_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(ui_container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_container_, LV_DIR_VER);

    lv_obj_t* header = lv_label_create(ui_container_);
    char title[64];
    snprintf(title, sizeof(title), "Radio (%d)", player_->StationCount());
    lv_label_set_text(header, title);
    lv_obj_set_style_text_color(header, lv_color_hex(0xe94560), 0);
    lv_obj_set_width(header, 230);
    lv_obj_set_style_text_align(header, LV_TEXT_ALIGN_CENTER, 0);

    for (int i = 0; i < player_->StationCount(); i++) {
        const auto& st = player_->GetStation(i);

        lv_obj_t* row = lv_obj_create(ui_container_);
        lv_obj_set_size(row, 228, 32);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_left(row, 8, 0);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* label = lv_label_create(row);
        char entry_text[80];
        snprintf(entry_text, sizeof(entry_text), "%s [%s]", st.name.c_str(), st.genre.c_str());
        lv_label_set_text(label, entry_text);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(label, 210);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

        list_items_.push_back(row);
    }

    UpdateSelection(0, selection_);
}

void RadioApp::ShowNowPlaying() {
    if (ui_container_ && display_) {
        DisplayLockGuard lock(display_);
        lv_obj_del(ui_container_);
    }
    ui_container_ = nullptr;
    list_items_.clear();
    current_screen_ = Screen::kNowPlaying;

    DisplayLockGuard lock(display_);
    lv_obj_t* screen = lv_screen_active();

    ui_container_ = lv_obj_create(screen);
    lv_obj_set_size(ui_container_, 240, 200);
    lv_obj_align(ui_container_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ui_container_, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(ui_container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_container_, 0, 0);
    lv_obj_set_style_radius(ui_container_, 0, 0);
    lv_obj_set_scrollbar_mode(ui_container_, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* icon = lv_label_create(ui_container_);
    lv_label_set_text(icon, "FM Radio");
    lv_obj_set_style_text_color(icon, lv_color_hex(0xe94560), 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 8);

    status_label_ = lv_label_create(ui_container_);
    std::string station_name = player_->CurrentStationName();
    char buf[128];
    snprintf(buf, sizeof(buf), "%s\n\nConnecting...", station_name.c_str());
    lv_label_set_text(status_label_, buf);
    lv_obj_set_width(status_label_, 220);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0xeaeaea), 0);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* hint = lv_label_create(ui_container_);
    lv_label_set_text(hint, "BOOT:Stop  VOL:Prev/Next");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

void RadioApp::UpdateSelection(int old_sel, int new_sel) {
    for (int i = 0; i < (int)list_items_.size(); i++) {
        if (i == new_sel) {
            lv_obj_set_style_bg_color(list_items_[i], lv_color_hex(0x16213e), 0);
            lv_obj_set_style_bg_opa(list_items_[i], LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(list_items_[i], lv_color_hex(0xe94560), 0);
            lv_obj_scroll_to_view(list_items_[i], LV_ANIM_ON);
        } else {
            lv_obj_set_style_bg_color(list_items_[i], lv_color_hex(0x0f3460), 0);
            lv_obj_set_style_bg_opa(list_items_[i], LV_OPA_80, 0);
            lv_obj_set_style_text_color(list_items_[i], lv_color_hex(0xeaeaea), 0);
        }
    }
}

void RadioApp::OnButtonClick() {
    if (current_screen_ == Screen::kStationList) {
        if (!player_ || player_->StationCount() == 0) return;
        if (selection_ < 0 || selection_ >= player_->StationCount()) return;

        Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
        ESP_LOGI(TAG, "WiFi power save OFF for streaming");
        codec_->EnableOutput(true);
        ShowNowPlaying();
        player_->Play(selection_);
    } else if (current_screen_ == Screen::kNowPlaying) {
        current_screen_ = Screen::kStationList;
        if (player_ && player_->IsPlaying()) {
            player_->Stop();
        }
        Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
        ESP_LOGI(TAG, "WiFi power save restored");
        ShowStationList();
    }
}

bool RadioApp::OnButtonDoubleClick() {
    if (current_screen_ == Screen::kNowPlaying) {
        current_screen_ = Screen::kStationList;
        if (player_ && player_->IsPlaying()) {
            player_->Stop();
        }
        Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
        ESP_LOGI(TAG, "WiFi power save restored");
        ShowStationList();
        return true;
    }
    return false;
}

void RadioApp::OnVolumeUpClick() {
    if (current_screen_ == Screen::kStationList) {
        if (selection_ > 0) {
            int old = selection_;
            selection_--;
            if (display_) {
                DisplayLockGuard lock(display_);
                UpdateSelection(old, selection_);
            }
        }
    } else if (current_screen_ == Screen::kNowPlaying) {
        if (player_ && player_->IsPlaying()) {
            player_->PrevStation();
        }
    }
}

void RadioApp::OnVolumeDownClick() {
    if (current_screen_ == Screen::kStationList) {
        if (player_ && selection_ < player_->StationCount() - 1) {
            int old = selection_;
            selection_++;
            if (display_) {
                DisplayLockGuard lock(display_);
                UpdateSelection(old, selection_);
            }
        }
    } else if (current_screen_ == Screen::kNowPlaying) {
        if (player_ && player_->IsPlaying()) {
            player_->NextStation();
        }
    }
}
