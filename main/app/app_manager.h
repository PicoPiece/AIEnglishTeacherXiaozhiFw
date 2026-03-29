#ifndef _APP_MANAGER_H_
#define _APP_MANAGER_H_

#include "app_base.h"
#include <lvgl.h>
#include <esp_timer.h>
#include <vector>

class LcdDisplay;

class AppManager {
public:
    AppManager(LcdDisplay* display);
    ~AppManager();

    void RegisterApp(AppBase* app);
    void ShowMenu();
    void ReturnToMenu();
    void AutoEnterFirstApp();

    bool InMenu() const { return active_app_ == nullptr; }
    AppBase* GetActiveApp() { return active_app_; }
    int AppCount() const { return (int)apps_.size(); }

    void OnButtonClick();
    void OnButtonDoubleClick();
    void OnButtonLongPress();
    void OnVolumeUpClick();
    void OnVolumeDownClick();
    void OnVolumeUpLongPress();
    void OnVolumeDownLongPress();

    void RefreshMenuBadges();
    void CleanupForWifiConfig();
    void ShowVolumeNotification(int volume);

private:
    void SelectApp(int index);
    void CreateMenuUI();
    void DestroyMenuUI();
    void UpdateMenuHighlight();

    LcdDisplay* display_;
    std::vector<AppBase*> apps_;
    AppBase* active_app_ = nullptr;
    int menu_selection_ = 0;

    lv_obj_t* menu_container_ = nullptr;
    std::vector<lv_obj_t*> menu_items_;
    std::vector<lv_obj_t*> menu_badges_;

    lv_obj_t* volume_overlay_ = nullptr;
    lv_obj_t* volume_bar_ = nullptr;
    lv_obj_t* volume_label_ = nullptr;
    esp_timer_handle_t volume_timer_ = nullptr;
};

#endif // _APP_MANAGER_H_
