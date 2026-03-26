#include "messages_app.h"
#include "display/lcd_display.h"
#include "display/display.h"
#include <esp_log.h>
#include <cstdio>
#include <ctime>
#include <algorithm>

#define TAG "MessagesApp"

MessagesApp::MessagesApp() {}

int MessagesApp::GetBadgeCount() {
    return unread_count_;
}

std::vector<Message> MessagesApp::GetMessagesCopy() {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    return messages_;
}

void MessagesApp::PushMessage(const std::string& sender, const std::string& content) {
    {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        Message msg;
        msg.sender = sender;
        msg.content = content;
        msg.timestamp = (int64_t)time(NULL);
        msg.read = false;

        messages_.insert(messages_.begin(), msg);
        if ((int)messages_.size() > MAX_MESSAGES) {
            messages_.pop_back();
        }
        unread_count_++;
    }

    auto* d = display_;
    if (d && ui_container_) {
        CreateUI();
    }
}

void MessagesApp::OnEnter(LcdDisplay* display) {
    display_ = display;
    selection_ = 0;
    detail_view_ = false;

    // Mark all as read
    {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        for (auto& m : messages_) m.read = true;
        unread_count_ = 0;
    }

    CreateUI();
    ESP_LOGI(TAG, "Entering Messages");
}

void MessagesApp::OnExit() {
    auto* saved_display = display_;
    display_ = nullptr;

    if (ui_container_ && saved_display) {
        DisplayLockGuard lock(saved_display);
        lv_obj_del(ui_container_);
    }
    ui_container_ = nullptr;
    list_items_.clear();

    ESP_LOGI(TAG, "Exiting Messages");
}

void MessagesApp::DestroyUI() {
    if (ui_container_ && display_) {
        DisplayLockGuard lock(display_);
        lv_obj_del(ui_container_);
    }
    ui_container_ = nullptr;
    list_items_.clear();
}

void MessagesApp::CreateUI() {
    DestroyUI();
    detail_view_ = false;

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
    char title[32];
    snprintf(title, sizeof(title), "Messages (%d)", (int)messages_.size());
    lv_label_set_text(header, title);
    lv_obj_set_style_text_color(header, lv_color_hex(0xe94560), 0);
    lv_obj_set_width(header, 230);
    lv_obj_set_style_text_align(header, LV_TEXT_ALIGN_CENTER, 0);

    list_items_.clear();

    std::lock_guard<std::mutex> mlock(messages_mutex_);

    if (messages_.empty()) {
        lv_obj_t* empty = lv_label_create(ui_container_);
        lv_label_set_text(empty, "No messages");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        lv_obj_set_width(empty, 230);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    } else {
        for (int i = 0; i < (int)messages_.size(); i++) {
            lv_obj_t* row = lv_obj_create(ui_container_);
            lv_obj_set_size(row, 228, 36);
            lv_obj_set_style_radius(row, 6, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_pad_left(row, 8, 0);
            lv_obj_set_style_pad_top(row, 2, 0);
            lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

            lv_obj_t* sender = lv_label_create(row);
            lv_label_set_text(sender, messages_[i].sender.c_str());
            lv_obj_set_style_text_color(sender, lv_color_hex(0xe94560), 0);
            lv_obj_align(sender, LV_ALIGN_TOP_LEFT, 0, 0);

            lv_obj_t* preview = lv_label_create(row);
            // Show first line/30 chars of content
            std::string preview_text = messages_[i].content.substr(0, 30);
            if (messages_[i].content.length() > 30) preview_text += "...";
            lv_label_set_text(preview, preview_text.c_str());
            lv_label_set_long_mode(preview, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(preview, 200);
            lv_obj_set_style_text_color(preview, lv_color_hex(0xcccccc), 0);
            lv_obj_align(preview, LV_ALIGN_BOTTOM_LEFT, 0, 0);

            list_items_.push_back(row);
        }
    }

    UpdateHighlight();
}

void MessagesApp::ShowMessageDetail(int index) {
    std::string sender, content;
    int64_t timestamp;
    {
        std::lock_guard<std::mutex> mlock(messages_mutex_);
        if (index < 0 || index >= (int)messages_.size()) return;
        sender = messages_[index].sender;
        content = messages_[index].content;
        timestamp = messages_[index].timestamp;
    }

    DestroyUI();
    detail_view_ = true;

    if (!display_) return;
    DisplayLockGuard lock(display_);
    lv_obj_t* screen = lv_screen_active();

    ui_container_ = lv_obj_create(screen);
    lv_obj_set_size(ui_container_, 240, 200);
    lv_obj_align(ui_container_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ui_container_, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(ui_container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_container_, 0, 0);
    lv_obj_set_style_radius(ui_container_, 0, 0);
    lv_obj_set_style_pad_all(ui_container_, 8, 0);
    lv_obj_set_scrollbar_mode(ui_container_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(ui_container_, LV_DIR_VER);

    lv_obj_t* sender_label = lv_label_create(ui_container_);
    lv_label_set_text(sender_label, sender.c_str());
    lv_obj_set_style_text_color(sender_label, lv_color_hex(0xe94560), 0);
    lv_obj_align(sender_label, LV_ALIGN_TOP_LEFT, 0, 0);

    time_t ts = (time_t)timestamp;
    struct tm* tm_info = localtime(&ts);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%H:%M %d/%m", tm_info);

    lv_obj_t* time_label = lv_label_create(ui_container_);
    lv_label_set_text(time_label, time_buf);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x888888), 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t* content_label = lv_label_create(ui_container_);
    lv_label_set_text(content_label, content.c_str());
    lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(content_label, 220);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0xeaeaea), 0);
    lv_obj_align(content_label, LV_ALIGN_TOP_LEFT, 0, 25);
}

void MessagesApp::UpdateHighlight() {
    for (int i = 0; i < (int)list_items_.size(); i++) {
        if (i == selection_) {
            lv_obj_set_style_bg_color(list_items_[i], lv_color_hex(0x16213e), 0);
            lv_obj_set_style_bg_opa(list_items_[i], LV_OPA_COVER, 0);
            lv_obj_scroll_to_view(list_items_[i], LV_ANIM_ON);
        } else {
            lv_obj_set_style_bg_color(list_items_[i], lv_color_hex(0x0f3460), 0);
            lv_obj_set_style_bg_opa(list_items_[i], LV_OPA_80, 0);
        }
    }
}

void MessagesApp::OnButtonClick() {
    if (!detail_view_ && !messages_.empty() && selection_ < (int)messages_.size()) {
        ShowMessageDetail(selection_);
    }
}

void MessagesApp::OnButtonDoubleClick() {
    if (detail_view_) {
        CreateUI();
    }
}

void MessagesApp::OnVolumeUpClick() {
    if (detail_view_) {
        if (display_) {
            DisplayLockGuard lock(display_);
            if (ui_container_) lv_obj_scroll_by(ui_container_, 0, 30, LV_ANIM_ON);
        }
    } else {
        if (selection_ > 0) {
            selection_--;
            if (display_) {
                DisplayLockGuard lock(display_);
                UpdateHighlight();
            }
        }
    }
}

void MessagesApp::OnVolumeDownClick() {
    if (detail_view_) {
        if (display_) {
            DisplayLockGuard lock(display_);
            if (ui_container_) lv_obj_scroll_by(ui_container_, 0, -30, LV_ANIM_ON);
        }
    } else {
        if (selection_ < (int)messages_.size() - 1) {
            selection_++;
            if (display_) {
                DisplayLockGuard lock(display_);
                UpdateHighlight();
            }
        }
    }
}
