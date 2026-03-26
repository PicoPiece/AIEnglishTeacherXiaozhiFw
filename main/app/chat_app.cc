#include "chat_app.h"
#include "application.h"
#include "board.h"
#include "display/lcd_display.h"
#include "display/display.h"

#include <esp_log.h>

#define TAG "ChatApp"

ChatApp::ChatApp() {}

void ChatApp::OnEnter(LcdDisplay* display) {
    display_ = display;
    ESP_LOGI(TAG, "Entering AI Chat");
    display_->ShowChatUI();
}

void ChatApp::OnExit() {
    ESP_LOGI(TAG, "Exiting AI Chat");
    if (display_) {
        display_->HideChatUI();
    }
    display_ = nullptr;
}

void ChatApp::OnButtonClick() {
    Application::GetInstance().ToggleChatState();
}

void ChatApp::OnButtonDoubleClick() {}

void ChatApp::OnVolumeUpClick() {
    auto* d = display_ ? display_ : static_cast<LcdDisplay*>(Board::GetInstance().GetDisplay());
    if (d) d->ScrollChatBy(40);
}

void ChatApp::OnVolumeDownClick() {
    auto* d = display_ ? display_ : static_cast<LcdDisplay*>(Board::GetInstance().GetDisplay());
    if (d) d->ScrollChatBy(-40);
}
