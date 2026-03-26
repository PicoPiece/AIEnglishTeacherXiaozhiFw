#ifndef _APP_BASE_H_
#define _APP_BASE_H_

class LcdDisplay;

class AppBase {
public:
    virtual ~AppBase() = default;
    virtual const char* GetName() const = 0;
    virtual const char* GetIcon() const = 0;
    virtual void OnEnter(LcdDisplay* display) = 0;
    virtual void OnExit() = 0;
    virtual void OnButtonClick() = 0;
    virtual void OnButtonDoubleClick() {}
    virtual void OnVolumeUpClick() = 0;
    virtual void OnVolumeDownClick() = 0;
    virtual void OnVolumeUpLongPress() {}
    virtual void OnVolumeDownLongPress() {}
    virtual void OnTick() {}
    virtual bool IsPlaying() { return false; }
    virtual int GetBadgeCount() { return 0; }
};

#endif // _APP_BASE_H_
