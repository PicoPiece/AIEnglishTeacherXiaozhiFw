#include "music_app.h"
#include "audio/music_player.h"
#include "audio_codec.h"
#include "display/display.h"
#include "display/lcd_display.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <esp_log.h>

#define SD_PATH "/sdcard"

#define TAG "MusicApp"

namespace {

bool IsDirectory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

} // namespace

MusicApp::MusicApp(MusicPlayer* player, AudioCodec* codec)
    : player_(player), codec_(codec) {}

void MusicApp::OnEnter(LcdDisplay* display) {
    display_ = display;
    current_screen_ = Screen::kBrowse;
    current_path_ = SD_PATH;
    selection_ = 0;
    ShowBrowse();
    ESP_LOGI(TAG, "Entering Music app");
}

void MusicApp::OnExit() {
    DestroyUI();
    display_ = nullptr;
    ESP_LOGI(TAG, "Exiting Music app");
}

bool MusicApp::IsPlaying() {
    return player_ && player_->IsPlaying();
}

void MusicApp::OnTrackChanged(const std::string& name, int index, int total) {
    now_playing_name_ = name;
    now_playing_index_ = index;
    now_playing_total_ = total;
    if (display_ && current_screen_ == Screen::kNowPlaying && title_label_) {
        DisplayLockGuard lock(display_);
        char buf[128];
        snprintf(buf, sizeof(buf), "%s\n\n%d / %d", name.c_str(), index, total);
        lv_label_set_text(title_label_, buf);
    }
}

void MusicApp::OnPlaybackStopped() {
    if (display_ && current_screen_ == Screen::kNowPlaying) {
        ShowBrowse();
    }
}

void MusicApp::DestroyUI() {
    if (ui_container_ && display_) {
        DisplayLockGuard lock(display_);
        lv_obj_del(ui_container_);
    }
    ui_container_ = nullptr;
    title_label_ = nullptr;
    list_items_.clear();
}

void MusicApp::ShowBrowse() {
    DestroyUI();
    current_screen_ = Screen::kBrowse;
    selection_ = 0;

    entry_names_.clear();
    entry_paths_.clear();
    entry_is_dir_.clear();

    DIR* dir = opendir(current_path_.c_str());
    if (dir) {
        std::vector<std::string> dirs, dir_paths;
        std::vector<std::string> files, file_paths;

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            std::string full_path = current_path_ + "/" + entry->d_name;

            bool is_dir = false;
            if (entry->d_type == DT_DIR) {
                is_dir = true;
            } else if (entry->d_type == DT_UNKNOWN) {
                is_dir = IsDirectory(full_path.c_str());
            }

            if (is_dir) {
                dirs.push_back(entry->d_name);
                dir_paths.push_back(full_path);
            } else if (MusicPlayer::HasAudioExtension(entry->d_name)) {
                files.push_back(entry->d_name);
                file_paths.push_back(full_path);
            }
        }
        closedir(dir);

        // Sort dirs and files while keeping name/path paired
        std::vector<std::pair<std::string, std::string>> dir_pairs, file_pairs;
        for (size_t i = 0; i < dirs.size(); i++) {
            dir_pairs.push_back({dirs[i], dir_paths[i]});
        }
        for (size_t i = 0; i < files.size(); i++) {
            file_pairs.push_back({files[i], file_paths[i]});
        }
        std::sort(dir_pairs.begin(), dir_pairs.end());
        std::sort(file_pairs.begin(), file_pairs.end());

        // Folders first, then files
        for (auto& p : dir_pairs) {
            entry_names_.push_back(p.first);
            entry_paths_.push_back(p.second);
            entry_is_dir_.push_back(true);
        }
        for (auto& p : file_pairs) {
            entry_names_.push_back(p.first);
            entry_paths_.push_back(p.second);
            entry_is_dir_.push_back(false);
        }
    }

    ESP_LOGI(TAG, "Browse '%s': %d entries", current_path_.c_str(), (int)entry_names_.size());

    // Derive a short title from the path
    std::string title_str;
    if (current_path_ == SD_PATH) {
        title_str = "SD Card";
    } else {
        auto slash = current_path_.rfind('/');
        title_str = (slash != std::string::npos) ? current_path_.substr(slash + 1) : current_path_;
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
    snprintf(title, sizeof(title), "%s (%d)", title_str.c_str(), (int)entry_names_.size());
    lv_label_set_text(header, title);
    lv_obj_set_style_text_color(header, lv_color_hex(0xe94560), 0);
    lv_obj_set_width(header, 230);
    lv_obj_set_style_text_align(header, LV_TEXT_ALIGN_CENTER, 0);

    list_items_.clear();
    if (entry_names_.empty()) {
        lv_obj_t* empty = lv_label_create(ui_container_);
        lv_label_set_text(empty, "Empty");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        lv_obj_set_width(empty, 230);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    } else {
        for (int i = 0; i < (int)entry_names_.size(); i++) {
            lv_obj_t* row = lv_obj_create(ui_container_);
            lv_obj_set_size(row, 228, 32);
            lv_obj_set_style_radius(row, 6, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_pad_left(row, 8, 0);
            lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

            lv_obj_t* label = lv_label_create(row);
            std::string display_name;
            if (entry_is_dir_[i]) {
                display_name = "[" + entry_names_[i] + "]";
            } else {
                display_name = entry_names_[i];
                auto dot = display_name.rfind('.');
                if (dot != std::string::npos) {
                    display_name.resize(dot);
                }
            }
            lv_label_set_text(label, display_name.c_str());
            lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(label, 210);
            lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

            list_items_.push_back(row);
        }
    }

    UpdateHighlight();
}

void MusicApp::ShowNowPlaying() {
    DestroyUI();
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
    lv_label_set_text(icon, "Now Playing");
    lv_obj_set_style_text_color(icon, lv_color_hex(0xe94560), 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);

    title_label_ = lv_label_create(ui_container_);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s\n\n%d / %d", now_playing_name_.c_str(), now_playing_index_, now_playing_total_);
    lv_label_set_text(title_label_, buf);
    lv_obj_set_width(title_label_, 220);
    lv_label_set_long_mode(title_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(title_label_, lv_color_hex(0xeaeaea), 0);
    lv_obj_set_style_text_align(title_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label_, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* hint = lv_label_create(ui_container_);
    lv_label_set_text(hint, "VOL: Prev/Next  x2: Back");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void MusicApp::UpdateHighlight() {
    for (int i = 0; i < (int)list_items_.size(); i++) {
        if (i == selection_) {
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

void MusicApp::OnButtonClick() {
    if (current_screen_ == Screen::kBrowse) {
        if (entry_names_.empty() || selection_ >= (int)entry_names_.size()) return;
        if (entry_is_dir_[selection_]) {
            current_path_ = entry_paths_[selection_];
            ShowBrowse();
        } else {
            if (player_ && codec_) {
                codec_->EnableOutput(true);
                player_->PlayFile(entry_paths_[selection_]);
                now_playing_name_ = entry_names_[selection_];
                auto dot = now_playing_name_.rfind('.');
                if (dot != std::string::npos) {
                    now_playing_name_.resize(dot);
                }
                now_playing_index_ = 1;
                now_playing_total_ = 1;
                ShowNowPlaying();
            }
        }
    } else if (current_screen_ == Screen::kNowPlaying) {
        if (player_ && player_->IsPlaying()) {
            player_->Stop();
        }
        ShowBrowse();
    }
}

void MusicApp::OnButtonDoubleClick() {
    if (current_screen_ == Screen::kBrowse) {
        if (current_path_ != SD_PATH) {
            auto slash = current_path_.rfind('/');
            if (slash != std::string::npos && slash > 0) {
                current_path_ = current_path_.substr(0, slash);
            } else {
                current_path_ = SD_PATH;
            }
            ShowBrowse();
        }
    } else if (current_screen_ == Screen::kNowPlaying) {
        if (player_ && player_->IsPlaying()) {
            player_->Stop();
        }
        ShowBrowse();
    }
}

void MusicApp::OnVolumeUpClick() {
    if (current_screen_ == Screen::kBrowse) {
        if (selection_ > 0) {
            selection_--;
            DisplayLockGuard lock(display_);
            UpdateHighlight();
        }
    } else if (current_screen_ == Screen::kNowPlaying) {
        if (player_) player_->PrevTrack();
    }
}

void MusicApp::OnVolumeDownClick() {
    if (current_screen_ == Screen::kBrowse) {
        if (selection_ < (int)entry_names_.size() - 1) {
            selection_++;
            DisplayLockGuard lock(display_);
            UpdateHighlight();
        }
    } else if (current_screen_ == Screen::kNowPlaying) {
        if (player_) player_->NextTrack();
    }
}
