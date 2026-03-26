#ifndef _APP_MANAGER_H_
#define _APP_MANAGER_H_

#include "app_base.h"
#include <lvgl.h>
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

    void OnButtonClick();
    void OnButtonDoubleClick();
    void OnButtonLongPress();
    void OnVolumeUpClick();
    void OnVolumeDownClick();
    void OnVolumeUpLongPress();
    void OnVolumeDownLongPress();

    void RefreshMenuBadges();

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
};

#endif // _APP_MANAGER_H_
