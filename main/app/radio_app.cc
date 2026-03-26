#include "radio_app.h"
#include "display/lcd_display.h"
#include "display/display.h"

#include <esp_log.h>

#define TAG "RadioApp"

RadioApp::RadioApp() {}

void RadioApp::OnEnter(LcdDisplay* display) {
    display_ = display;
    CreateUI();
    ESP_LOGI(TAG, "Entering Radio");
}

void RadioApp::OnExit() {
    DestroyUI();
    display_ = nullptr;
    ESP_LOGI(TAG, "Exiting Radio");
}

void RadioApp::CreateUI() {
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
    lv_label_set_text(icon, "FM");
    lv_obj_set_style_text_color(icon, lv_color_hex(0xe94560), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t* title = lv_label_create(ui_container_);
    lv_label_set_text(title, "Radio");
    lv_obj_set_style_text_color(title, lv_color_hex(0xeaeaea), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* subtitle = lv_label_create(ui_container_);
    lv_label_set_text(subtitle, "Coming Soon");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x888888), 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 25);
}

void RadioApp::DestroyUI() {
    if (ui_container_) {
        DisplayLockGuard lock(display_);
        lv_obj_del(ui_container_);
        ui_container_ = nullptr;
    }
}

void RadioApp::OnButtonClick() {}

void RadioApp::OnVolumeUpClick() {}

void RadioApp::OnVolumeDownClick() {}
