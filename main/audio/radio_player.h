#ifndef _RADIO_PLAYER_H_
#define _RADIO_PLAYER_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <string>
#include <vector>
#include <functional>

class AudioCodec;

struct RadioStation {
    std::string name;
    std::string url;
    std::string genre;
};

class RadioPlayer {
public:
    using StationChangeCallback = std::function<void(const std::string& name, int index, int total)>;
    using StopCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    RadioPlayer(AudioCodec* codec);
    ~RadioPlayer();

    void AddStation(const std::string& name, const std::string& url, const std::string& genre);
    int StationCount() const { return (int)stations_.size(); }
    const RadioStation& GetStation(int index) const { return stations_[index]; }
    const std::vector<RadioStation>& GetStations() const { return stations_; }

    bool Play(int station_index);
    void Stop();
    bool IsPlaying() const { return playing_; }
    int CurrentStationIndex() const { return current_station_; }
    std::string CurrentStationName() const;

    void NextStation();
    void PrevStation();

    void SetStationChangeCallback(StationChangeCallback cb) { station_change_cb_ = cb; }
    void SetStopCallback(StopCallback cb) { stop_cb_ = cb; }
    void SetErrorCallback(ErrorCallback cb) { error_cb_ = cb; }

private:
    void StreamTask();
    static void StreamTaskEntry(void* arg);
    void NotifyStationChange();

    AudioCodec* codec_;
    std::vector<RadioStation> stations_;
    std::atomic<int> current_station_{0};
    std::atomic<bool> playing_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> switch_requested_{false};
    TaskHandle_t task_handle_ = nullptr;
    StationChangeCallback station_change_cb_;
    StopCallback stop_cb_;
    ErrorCallback error_cb_;
};

#endif // _RADIO_PLAYER_H_
