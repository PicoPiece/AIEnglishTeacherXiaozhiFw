#ifndef _RADIO_APP_H_
#define _RADIO_APP_H_

#include "app_base.h"
#include <lvgl.h>

class LcdDisplay;

class RadioApp : public AppBase {
public:
    RadioApp();
    ~RadioApp() override = default;

    const char* GetName() const override { return "Radio"; }
    const char* GetIcon() const override { return "FM"; }
    void OnEnter(LcdDisplay* display) override;
    void OnExit() override;
    void OnButtonClick() override;
    void OnVolumeUpClick() override;
    void OnVolumeDownClick() override;

private:
    LcdDisplay* display_ = nullptr;
    lv_obj_t* ui_container_ = nullptr;

    void CreateUI();
    void DestroyUI();
};

#endif // _RADIO_APP_H_
