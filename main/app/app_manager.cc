#include "app_manager.h"
#include "display/lcd_display.h"
#include "display/display.h"

#include <cstdio>

#include <esp_log.h>

#define TAG "AppManager"

AppManager::AppManager(LcdDisplay* display) : display_(display) {
}

AppManager::~AppManager() {
}

void AppManager::RegisterApp(AppBase* app) {
    apps_.push_back(app);
    ESP_LOGI(TAG, "Registered app: %s", app->GetName());
}

void AppManager::ShowMenu() {
    if (active_app_) {
        active_app_->OnExit();
        active_app_ = nullptr;
    }
    CreateMenuUI();
    ESP_LOGI(TAG, "Menu shown with %d apps", static_cast<int>(apps_.size()));
}

void AppManager::ReturnToMenu() {
    if (active_app_) {
        active_app_->OnExit();
        active_app_ = nullptr;
    }
    CreateMenuUI();
    ESP_LOGI(TAG, "Returned to menu");
}

void AppManager::AutoEnterFirstApp() {
    if (apps_.empty()) return;
    active_app_ = apps_[0];
    ESP_LOGI(TAG, "Auto-entered app: %s (deferred OnEnter until display ready)", active_app_->GetName());
}

void AppManager::SelectApp(int index) {
    if (index < 0 || index >= static_cast<int>(apps_.size())) {
        return;
    }
    {
        DisplayLockGuard lock(display_);
        DestroyMenuUI();
    }
    active_app_ = apps_[index];
    active_app_->OnEnter(display_);
    ESP_LOGI(TAG, "Selected app: %s", active_app_->GetName());
}

void AppManager::CreateMenuUI() {
    DisplayLockGuard lock(display_);
    DestroyMenuUI();

    lv_obj_t* screen = lv_screen_active();

    menu_container_ = lv_obj_create(screen);
    lv_obj_set_size(menu_container_, 240, 200);
    lv_obj_align(menu_container_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(menu_container_, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(menu_container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(menu_container_, 0, 0);
    lv_obj_set_style_radius(menu_container_, 0, 0);
    lv_obj_set_style_pad_all(menu_container_, 8, 0);
    lv_obj_set_style_pad_row(menu_container_, 4, 0);
    lv_obj_set_flex_flow(menu_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(menu_container_, LV_SCROLLBAR_MODE_OFF);

    menu_items_.clear();
    menu_badges_.clear();

    if (!apps_.empty()) {
        if (menu_selection_ >= static_cast<int>(apps_.size())) {
            menu_selection_ = static_cast<int>(apps_.size()) - 1;
        }
        if (menu_selection_ < 0) {
            menu_selection_ = 0;
        }
    } else {
        menu_selection_ = 0;
    }

    for (int i = 0; i < static_cast<int>(apps_.size()); i++) {
        lv_obj_t* row = lv_obj_create(menu_container_);
        lv_obj_set_size(row, 220, 40);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_left(row, 12, 0);
        lv_obj_set_style_pad_right(row, 12, 0);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* icon_label = lv_label_create(row);
        lv_label_set_text(icon_label, apps_[i]->GetIcon());
        lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* name_label = lv_label_create(row);
        lv_label_set_text(name_label, apps_[i]->GetName());
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 30, 0);

        lv_obj_t* badge = lv_label_create(row);
        lv_label_set_text(badge, "");
        lv_obj_align(badge, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_text_color(badge, lv_color_hex(0xff6b6b), 0);
        int bc = apps_[i]->GetBadgeCount();
        if (bc > 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "(%d)", bc);
            lv_label_set_text(badge, buf);
        }

        menu_items_.push_back(row);
        menu_badges_.push_back(badge);
    }

    UpdateMenuHighlight();
}

void AppManager::DestroyMenuUI() {
    if (menu_container_) {
        lv_obj_del(menu_container_);
        menu_container_ = nullptr;
        menu_items_.clear();
        menu_badges_.clear();
    }
}

void AppManager::UpdateMenuHighlight() {
    for (int i = 0; i < static_cast<int>(menu_items_.size()); i++) {
        if (i == menu_selection_) {
            lv_obj_set_style_bg_color(menu_items_[i], lv_color_hex(0x16213e), 0);
            lv_obj_set_style_bg_opa(menu_items_[i], LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(menu_items_[i], lv_color_hex(0xe94560), 0);
        } else {
            lv_obj_set_style_bg_color(menu_items_[i], lv_color_hex(0x0f3460), 0);
            lv_obj_set_style_bg_opa(menu_items_[i], LV_OPA_80, 0);
            lv_obj_set_style_text_color(menu_items_[i], lv_color_hex(0xeaeaea), 0);
        }
    }
}

void AppManager::RefreshMenuBadges() {
    if (!menu_container_) {
        return;
    }
    DisplayLockGuard lock(display_);
    for (int i = 0; i < static_cast<int>(apps_.size()) && i < static_cast<int>(menu_badges_.size()); i++) {
        int bc = apps_[i]->GetBadgeCount();
        if (bc > 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "(%d)", bc);
            lv_label_set_text(menu_badges_[i], buf);
        } else {
            lv_label_set_text(menu_badges_[i], "");
        }
    }
}

void AppManager::OnButtonClick() {
    if (InMenu()) {
        SelectApp(menu_selection_);
    } else {
        active_app_->OnButtonClick();
    }
}

void AppManager::OnButtonDoubleClick() {
    if (!InMenu() && active_app_) {
        active_app_->OnButtonDoubleClick();
    }
}

void AppManager::OnButtonLongPress() {
    if (!InMenu() && active_app_) {
        ReturnToMenu();
    }
}

void AppManager::OnVolumeUpClick() {
    if (InMenu()) {
        if (menu_selection_ > 0) {
            menu_selection_--;
            DisplayLockGuard lock(display_);
            UpdateMenuHighlight();
        }
    } else {
        active_app_->OnVolumeUpClick();
    }
}

void AppManager::OnVolumeDownClick() {
    if (InMenu()) {
        if (menu_selection_ < static_cast<int>(apps_.size()) - 1) {
            menu_selection_++;
            DisplayLockGuard lock(display_);
            UpdateMenuHighlight();
        }
    } else {
        active_app_->OnVolumeDownClick();
    }
}

void AppManager::OnVolumeUpLongPress() {
    if (!InMenu() && active_app_) {
        active_app_->OnVolumeUpLongPress();
    }
}

void AppManager::OnVolumeDownLongPress() {
    if (!InMenu() && active_app_) {
        active_app_->OnVolumeDownLongPress();
    }
}
