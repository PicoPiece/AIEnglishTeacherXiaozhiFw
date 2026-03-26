#ifndef _MESSAGES_APP_H_
#define _MESSAGES_APP_H_

#include "app_base.h"
#include <lvgl.h>
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

class LcdDisplay;

struct Message {
    std::string sender;
    std::string content;
    int64_t timestamp;
    bool read;
};

class MessagesApp : public AppBase {
public:
    static constexpr int MAX_MESSAGES = 50;

    MessagesApp();
    ~MessagesApp() override = default;

    const char* GetName() const override { return "Messages"; }
    const char* GetIcon() const override { return "Msg"; }
    void OnEnter(LcdDisplay* display) override;
    void OnExit() override;
    void OnButtonClick() override;
    void OnButtonDoubleClick() override;
    void OnVolumeUpClick() override;
    void OnVolumeDownClick() override;
    int GetBadgeCount() override;

    void PushMessage(const std::string& sender, const std::string& content);
    std::vector<Message> GetMessagesCopy();
    std::vector<Message>& GetMessages() { return messages_; }
    std::mutex& GetMessagesMutex() { return messages_mutex_; }

private:
    void CreateUI();
    void DestroyUI();
    void UpdateHighlight();
    void ShowMessageDetail(int index);

    LcdDisplay* display_ = nullptr;
    std::vector<Message> messages_;
    std::mutex messages_mutex_;
    int unread_count_ = 0;
    int selection_ = 0;
    bool detail_view_ = false;

    lv_obj_t* ui_container_ = nullptr;
    std::vector<lv_obj_t*> list_items_;
};

#endif // _MESSAGES_APP_H_
