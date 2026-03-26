#ifndef _MUSIC_APP_H_
#define _MUSIC_APP_H_

#include "app_base.h"
#include <lvgl.h>
#include <string>
#include <vector>

class LcdDisplay;
class MusicPlayer;
class AudioCodec;

class MusicApp : public AppBase {
public:
    enum class Screen {
        kBrowse,
        kNowPlaying,
    };

    MusicApp(MusicPlayer* player, AudioCodec* codec);
    ~MusicApp() override = default;

    const char* GetName() const override { return "Music"; }
    const char* GetIcon() const override { return "M"; }
    void OnEnter(LcdDisplay* display) override;
    void OnExit() override;
    void OnButtonClick() override;
    void OnButtonDoubleClick() override;
    void OnVolumeUpClick() override;
    void OnVolumeDownClick() override;
    bool IsPlaying() override;

    void OnTrackChanged(const std::string& name, int index, int total);
    void OnPlaybackStopped();

private:
    void ShowBrowse();
    void ShowNowPlaying();
    void DestroyUI();
    void UpdateHighlight();
    void UpdateModeLabel();

    MusicPlayer* player_;
    AudioCodec* codec_;
    LcdDisplay* display_ = nullptr;
    Screen current_screen_ = Screen::kBrowse;

    std::string current_path_;
    std::vector<std::string> entry_names_;
    std::vector<std::string> entry_paths_;
    std::vector<bool> entry_is_dir_;
    int selection_ = 0;

    lv_obj_t* ui_container_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* mode_label_ = nullptr;
    std::vector<lv_obj_t*> list_items_;

    std::string now_playing_name_;
    int now_playing_index_ = 0;
    int now_playing_total_ = 0;
};

#endif // _MUSIC_APP_H_
