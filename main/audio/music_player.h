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

enum class PlayMode {
    kSequential,
    kRepeatAll,
    kRepeatOne,
    kShuffle,
    kCount,
};

inline const char* PlayModeName(PlayMode mode) {
    switch (mode) {
        case PlayMode::kSequential: return "Sequential";
        case PlayMode::kRepeatAll:  return "Repeat All";
        case PlayMode::kRepeatOne:  return "Repeat One";
        case PlayMode::kShuffle:    return "Shuffle";
        default:                    return "Unknown";
    }
}

class MusicPlayer {
public:
    using TrackInfoCallback = std::function<void(const std::string& name, int index, int total)>;
    using StopCallback = std::function<void()>;

    MusicPlayer(AudioCodec* codec);
    ~MusicPlayer();

    bool Start(const char* mount_point = "/sdcard");
    bool StartPlaylist(const std::vector<std::string>& files, int start_index = 0);
    bool PlayFile(const std::string& filepath);
    void Stop();
    void NextTrack();
    void PrevTrack();
    void Pause();
    void Resume();
    bool IsPlaying() const { return playing_; }
    bool IsPaused() const { return paused_; }

    PlayMode GetPlayMode() const { return play_mode_; }
    void SetPlayMode(PlayMode mode) { play_mode_ = mode; }
    void CyclePlayMode();

    int TrackCount() const { return (int)playlist_.size(); }
    int CurrentTrackIndex() const { return current_track_; }
    std::string CurrentTrackName() const;

    static std::vector<MusicFileInfo> ListMusicFiles(const char* base_path);
    static std::vector<std::string> ListFolders(const char* base_path);
    static std::vector<MusicFileInfo> ListFilesInFolder(const char* folder_path);
    static bool HasAudioExtension(const char* name);

    void SetTrackInfoCallback(TrackInfoCallback cb) { track_info_cb_ = cb; }
    void SetStopCallback(StopCallback cb) { stop_cb_ = cb; }

private:
    void ScanMusicFiles(const char* base_path);
    int PickNextTrack();
    void PlaybackTask();
    static void PlaybackTaskEntry(void* arg);
    void NotifyTrackInfo();

    AudioCodec* codec_;
    std::vector<std::string> playlist_;
    int current_track_ = 0;
    PlayMode play_mode_ = PlayMode::kRepeatAll;
    volatile bool playing_ = false;
    volatile bool paused_ = false;
    volatile bool stop_requested_ = false;
    volatile bool skip_requested_ = false;
    TaskHandle_t task_handle_ = nullptr;
    TrackInfoCallback track_info_cb_;
    StopCallback stop_cb_;
};

#endif // _MUSIC_PLAYER_H_
