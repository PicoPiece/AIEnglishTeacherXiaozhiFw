#ifndef _MUSIC_PLAYER_H_
#define _MUSIC_PLAYER_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <vector>
#include <functional>

class AudioCodec;

struct MusicFileInfo {
    std::string path;
    std::string name;
    long size;
};

class MusicPlayer {
public:
    using TrackInfoCallback = std::function<void(const std::string& name, int index, int total)>;
    using StopCallback = std::function<void()>;

    MusicPlayer(AudioCodec* codec);
    ~MusicPlayer();

    bool Start(const char* mount_point = "/sdcard");
    bool PlayFile(const std::string& filepath);
    void Stop();
    void NextTrack();
    void PrevTrack();
    bool IsPlaying() const { return playing_; }

    int TrackCount() const { return (int)playlist_.size(); }
    int CurrentTrackIndex() const { return current_track_; }
    std::string CurrentTrackName() const;

    static std::vector<MusicFileInfo> ListMusicFiles(const char* base_path);
    static bool HasAudioExtension(const char* name);

    void SetTrackInfoCallback(TrackInfoCallback cb) { track_info_cb_ = cb; }
    void SetStopCallback(StopCallback cb) { stop_cb_ = cb; }

private:
    void ScanMusicFiles(const char* base_path);
    void PlaybackTask();
    static void PlaybackTaskEntry(void* arg);
    void NotifyTrackInfo();

    AudioCodec* codec_;
    std::vector<std::string> playlist_;
    int current_track_ = 0;
    volatile bool playing_ = false;
    volatile bool stop_requested_ = false;
    volatile bool skip_requested_ = false;
    TaskHandle_t task_handle_ = nullptr;
    TrackInfoCallback track_info_cb_;
    StopCallback stop_cb_;
};

#endif // _MUSIC_PLAYER_H_
