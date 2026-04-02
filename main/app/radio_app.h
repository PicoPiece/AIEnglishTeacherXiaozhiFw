#ifndef _RADIO_APP_H_
#define _RADIO_APP_H_

#include "app_base.h"
#include <lvgl.h>
#include <string>
#include <vector>

class LcdDisplay;
class RadioPlayer;
class AudioCodec;

class RadioApp : public AppBase {
public:
    RadioApp(RadioPlayer* player, AudioCodec* codec);
    ~RadioApp() override = default;

    const char* GetName() const override { return "Radio"; }
    const char* GetIcon() const override { return "FM"; }
    void OnEnter(LcdDisplay* display) override;
    void OnExit() override;
    void OnButtonClick() override;
    bool OnButtonDoubleClick() override;
    void OnVolumeUpClick() override;
    void OnVolumeDownClick() override;
    bool IsPlaying() override;

    void OnStationChanged(const std::string& name, int index, int total);
    void OnStatusChanged(const std::string& status);
    void OnPlaybackStopped();
    void OnStreamError(const std::string& error);

private:
    enum class Screen { kStationList, kNowPlaying };

    RadioPlayer* player_;
    AudioCodec* codec_;
    LcdDisplay* display_ = nullptr;
    lv_obj_t* ui_container_ = nullptr;
    lv_obj_t* status_label_ = nullptr;

    Screen current_screen_ = Screen::kStationList;
    int selection_ = 0;
    std::vector<lv_obj_t*> list_items_;

    void ShowStationList();
    void ShowNowPlaying();
    void UpdateSelection(int old_sel, int new_sel);
};

#endif // _RADIO_APP_H_
