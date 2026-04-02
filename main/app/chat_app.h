#ifndef _CHAT_APP_H_
#define _CHAT_APP_H_

#include "app_base.h"

class LcdDisplay;

class ChatApp : public AppBase {
public:
    ChatApp();
    ~ChatApp() override = default;

    const char* GetName() const override { return "AI Chat"; }
    const char* GetIcon() const override { return "AI"; }
    void OnEnter(LcdDisplay* display) override;
    void OnExit() override;
    void OnButtonClick() override;
    bool OnButtonDoubleClick() override;
    void OnVolumeUpClick() override;
    void OnVolumeDownClick() override;

private:
    LcdDisplay* display_ = nullptr;
};

#endif // _CHAT_APP_H_
