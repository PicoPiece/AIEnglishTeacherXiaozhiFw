#include "music_player.h"
#include "audio_codec.h"

#include <esp_log.h>
#include <esp_random.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>

#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_audio_dec_default.h"
#include "esp_ae_rate_cvt.h"

#define TAG "MusicPlayer"

#define MP3_READ_BUF_SIZE   2048
#define PCM_OUT_BUF_SIZE    (4608 * 2)
#define PLAYBACK_STACK_SIZE (8 * 1024)

MusicPlayer::MusicPlayer(AudioCodec* codec)
    : codec_(codec) {
    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
}

MusicPlayer::~MusicPlayer() {
    Stop();
}

bool MusicPlayer::HasAudioExtension(const char* name) {
    size_t len = strlen(name);
    if (len < 4) return false;
    const char* dot = strrchr(name, '.');
    if (!dot) return false;
    return (strcasecmp(dot, ".mp3") == 0 ||
            strcasecmp(dot, ".wav") == 0 ||
            strcasecmp(dot, ".ogg") == 0 ||
            strcasecmp(dot, ".m4a") == 0);
}

static std::string FilenameFromPath(const std::string& path) {
    auto pos = path.rfind('/');
    std::string name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
    auto dot = name.rfind('.');
    if (dot != std::string::npos) name.resize(dot);
    return name;
}

void MusicPlayer::ScanMusicFiles(const char* base_path) {
    DIR* dir = opendir(base_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            std::string subdir = std::string(base_path) + "/" + entry->d_name;
            ScanMusicFiles(subdir.c_str());
        } else if (HasAudioExtension(entry->d_name)) {
            std::string filepath = std::string(base_path) + "/" + entry->d_name;
            playlist_.push_back(filepath);
        }
    }
    closedir(dir);
}

static void ScanMusicFilesRecursive(const char* base_path, std::vector<MusicFileInfo>& out) {
    DIR* dir = opendir(base_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            std::string subdir = std::string(base_path) + "/" + entry->d_name;
            ScanMusicFilesRecursive(subdir.c_str(), out);
        } else if (MusicPlayer::HasAudioExtension(entry->d_name)) {
            std::string filepath = std::string(base_path) + "/" + entry->d_name;
            struct stat st;
            long file_size = 0;
            if (stat(filepath.c_str(), &st) == 0) {
                file_size = st.st_size;
            }
            out.push_back({filepath, FilenameFromPath(filepath), file_size});
        }
    }
    closedir(dir);
}

std::vector<MusicFileInfo> MusicPlayer::ListMusicFiles(const char* base_path) {
    std::vector<MusicFileInfo> files;
    ScanMusicFilesRecursive(base_path, files);
    std::sort(files.begin(), files.end(), [](const MusicFileInfo& a, const MusicFileInfo& b) {
        return a.path < b.path;
    });
    return files;
}

std::string MusicPlayer::CurrentTrackName() const {
    if (playlist_.empty() || current_track_ < 0 || current_track_ >= (int)playlist_.size()) {
        return "";
    }
    return FilenameFromPath(playlist_[current_track_]);
}

void MusicPlayer::NotifyTrackInfo() {
    if (track_info_cb_) {
        track_info_cb_(CurrentTrackName(), current_track_ + 1, (int)playlist_.size());
    }
}

bool MusicPlayer::Start(const char* mount_point) {
    if (playing_) return true;

    playlist_.clear();
    ScanMusicFiles(mount_point);

    if (playlist_.empty()) {
        ESP_LOGW(TAG, "No music files found on SD card");
        return false;
    }

    std::sort(playlist_.begin(), playlist_.end());
    ESP_LOGI(TAG, "Found %d music files", (int)playlist_.size());
    for (auto& f : playlist_) {
        ESP_LOGI(TAG, "  %s", f.c_str());
    }

    if (current_track_ >= (int)playlist_.size()) {
        current_track_ = 0;
    }

    stop_requested_ = false;
    skip_requested_ = false;
    playing_ = true;

    xTaskCreate(PlaybackTaskEntry, "music_play", PLAYBACK_STACK_SIZE, this, 3, &task_handle_);
    return true;
}

bool MusicPlayer::PlayFile(const std::string& filepath) {
    Stop();

    playlist_.clear();
    playlist_.push_back(filepath);
    current_track_ = 0;

    stop_requested_ = false;
    skip_requested_ = false;
    playing_ = true;

    ESP_LOGI(TAG, "Playing file: %s", filepath.c_str());
    NotifyTrackInfo();

    xTaskCreate(PlaybackTaskEntry, "music_play", PLAYBACK_STACK_SIZE, this, 3, &task_handle_);
    return true;
}

void MusicPlayer::Stop() {
    if (!playing_) return;
    stop_requested_ = true;
    if (task_handle_) {
        int wait_ms = 0;
        while (playing_ && wait_ms < 2000) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_ms += 10;
        }
        task_handle_ = nullptr;
    }
}

void MusicPlayer::NextTrack() {
    if (!playing_ || playlist_.empty()) return;
    if (play_mode_ == PlayMode::kShuffle) {
        int n = (int)playlist_.size();
        if (n > 1) {
            int next;
            do { next = (int)(esp_random() % n); } while (next == current_track_);
            current_track_ = next;
        }
    } else {
        current_track_ = (current_track_ + 1) % (int)playlist_.size();
    }
    skip_requested_ = true;
}

void MusicPlayer::PrevTrack() {
    if (!playing_ || playlist_.empty()) return;
    current_track_ = (current_track_ - 1 + (int)playlist_.size()) % (int)playlist_.size();
    skip_requested_ = true;
}

void MusicPlayer::Pause() {
    paused_ = true;
}

void MusicPlayer::Resume() {
    paused_ = false;
}

void MusicPlayer::CyclePlayMode() {
    int m = (int)play_mode_ + 1;
    if (m >= (int)PlayMode::kCount) m = 0;
    play_mode_ = (PlayMode)m;
    ESP_LOGI(TAG, "Play mode: %s", PlayModeName(play_mode_));
}

int MusicPlayer::PickNextTrack() {
    if (playlist_.empty()) return -1;
    int n = (int)playlist_.size();

    switch (play_mode_) {
        case PlayMode::kSequential:
            if (current_track_ + 1 >= n) return -1;
            return current_track_ + 1;
        case PlayMode::kRepeatAll:
            return (current_track_ + 1) % n;
        case PlayMode::kRepeatOne:
            return current_track_;
        case PlayMode::kShuffle: {
            if (n <= 1) return 0;
            int next;
            do {
                next = (int)(esp_random() % n);
            } while (next == current_track_);
            return next;
        }
        default:
            return (current_track_ + 1) % n;
    }
}

bool MusicPlayer::StartPlaylist(const std::vector<std::string>& files, int start_index) {
    Stop();

    playlist_ = files;
    if (playlist_.empty()) return false;
    if (start_index < 0 || start_index >= (int)playlist_.size()) start_index = 0;
    current_track_ = start_index;

    stop_requested_ = false;
    skip_requested_ = false;
    playing_ = true;

    ESP_LOGI(TAG, "Starting playlist: %d files, track %d, mode %s",
             (int)playlist_.size(), start_index, PlayModeName(play_mode_));
    NotifyTrackInfo();

    xTaskCreate(PlaybackTaskEntry, "music_play", PLAYBACK_STACK_SIZE, this, 3, &task_handle_);
    return true;
}

std::vector<std::string> MusicPlayer::ListFolders(const char* base_path) {
    std::vector<std::string> folders;
    DIR* dir = opendir(base_path);
    if (!dir) return folders;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            folders.push_back(entry->d_name);
        }
    }
    closedir(dir);
    std::sort(folders.begin(), folders.end());
    return folders;
}

std::vector<MusicFileInfo> MusicPlayer::ListFilesInFolder(const char* folder_path) {
    std::vector<MusicFileInfo> files;
    DIR* dir = opendir(folder_path);
    if (!dir) return files;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR && HasAudioExtension(entry->d_name)) {
            std::string filepath = std::string(folder_path) + "/" + entry->d_name;
            struct stat st;
            long file_size = 0;
            if (stat(filepath.c_str(), &st) == 0) {
                file_size = st.st_size;
            }
            files.push_back({filepath, FilenameFromPath(filepath), file_size});
        }
    }
    closedir(dir);
    std::sort(files.begin(), files.end(), [](const MusicFileInfo& a, const MusicFileInfo& b) {
        return a.path < b.path;
    });
    return files;
}

void MusicPlayer::PlaybackTaskEntry(void* arg) {
    auto* self = static_cast<MusicPlayer*>(arg);
    self->PlaybackTask();
    self->playing_ = false;
    self->task_handle_ = nullptr;
    if (self->stop_cb_) {
        self->stop_cb_();
    }
    vTaskDelete(NULL);
}

void MusicPlayer::PlaybackTask() {
    uint8_t* read_buf = (uint8_t*)malloc(MP3_READ_BUF_SIZE);
    uint8_t* pcm_buf = (uint8_t*)malloc(PCM_OUT_BUF_SIZE);
    if (!read_buf || !pcm_buf) {
        ESP_LOGE(TAG, "Failed to allocate decode buffers");
        free(read_buf);
        free(pcm_buf);
        return;
    }

    const int target_rate = codec_->output_sample_rate();

    while (!stop_requested_) {
        if (current_track_ < 0 || current_track_ >= (int)playlist_.size()) {
            current_track_ = 0;
        }

        const std::string& filepath = playlist_[current_track_];
        ESP_LOGI(TAG, "Playing [%d/%d]: %s", current_track_ + 1, (int)playlist_.size(), filepath.c_str());
        NotifyTrackInfo();

        FILE* fp = fopen(filepath.c_str(), "rb");
        if (!fp) {
            ESP_LOGE(TAG, "Cannot open %s", filepath.c_str());
            current_track_ = (current_track_ + 1) % (int)playlist_.size();
            continue;
        }

        esp_audio_simple_dec_type_t dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
        const char* dot = strrchr(filepath.c_str(), '.');
        if (dot) {
            if (strcasecmp(dot, ".wav") == 0) dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_WAV;
            else if (strcasecmp(dot, ".m4a") == 0) dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_M4A;
            else if (strcasecmp(dot, ".ogg") == 0) dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_NONE;
        }
        if (dec_type == ESP_AUDIO_SIMPLE_DEC_TYPE_NONE) {
            ESP_LOGW(TAG, "Unsupported format: %s, skipping", filepath.c_str());
            fclose(fp);
            if (!skip_requested_ && !stop_requested_) {
                current_track_ = (current_track_ + 1) % (int)playlist_.size();
            }
            continue;
        }
        esp_audio_simple_dec_cfg_t dec_cfg = {};
        dec_cfg.dec_type = dec_type;
        esp_audio_simple_dec_handle_t decoder = nullptr;
        esp_audio_err_t ret = esp_audio_simple_dec_open(&dec_cfg, &decoder);
        if (ret != ESP_AUDIO_ERR_OK || !decoder) {
            ESP_LOGE(TAG, "Failed to open decoder: %d", ret);
            fclose(fp);
            break;
        }

        esp_ae_rate_cvt_handle_t resampler = nullptr;
        bool info_ready = false;
        uint32_t src_rate = 0;
        uint8_t src_channels = 0;
        skip_requested_ = false;

        while (!stop_requested_ && !skip_requested_) {
            while (paused_ && !stop_requested_ && !skip_requested_) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            if (stop_requested_ || skip_requested_) break;

            size_t bytes_read = fread(read_buf, 1, MP3_READ_BUF_SIZE, fp);
            if (bytes_read == 0) break;

            esp_audio_simple_dec_raw_t raw = {};
            raw.buffer = read_buf;
            raw.len = (uint32_t)bytes_read;
            raw.eos = feof(fp) ? true : false;

            while (raw.len > 0 && !stop_requested_ && !skip_requested_) {
                esp_audio_simple_dec_out_t out = {};
                out.buffer = pcm_buf;
                out.len = PCM_OUT_BUF_SIZE;

                ret = esp_audio_simple_dec_process(decoder, &raw, &out);
                raw.buffer += raw.consumed;
                raw.len -= raw.consumed;

                if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                    ESP_LOGW(TAG, "PCM buffer too small, need %lu", (unsigned long)out.needed_size);
                    continue;
                }
                if (ret != ESP_AUDIO_ERR_OK && ret != ESP_AUDIO_ERR_CONTINUE) {
                    break;
                }
                if (out.decoded_size == 0) continue;

                if (!info_ready) {
                    esp_audio_simple_dec_info_t info = {};
                    if (esp_audio_simple_dec_get_info(decoder, &info) == ESP_AUDIO_ERR_OK) {
                        src_rate = info.sample_rate;
                        src_channels = info.channel;
                        info_ready = true;
                        ESP_LOGI(TAG, "Audio: %luHz %dch %dbps",
                                 (unsigned long)src_rate, src_channels, info.bits_per_sample);

                        if ((int)src_rate != target_rate) {
                            esp_ae_rate_cvt_cfg_t cvt_cfg = {};
                            cvt_cfg.src_rate = src_rate;
                            cvt_cfg.dest_rate = (uint32_t)target_rate;
                            cvt_cfg.channel = 1;
                            cvt_cfg.bits_per_sample = 16;
                            cvt_cfg.complexity = 2;
                            cvt_cfg.perf_type = ESP_AE_RATE_CVT_PERF_TYPE_SPEED;
                            esp_ae_rate_cvt_open(&cvt_cfg, &resampler);
                        }
                    }
                }

                int16_t* pcm16 = (int16_t*)pcm_buf;
                int sample_count = (int)(out.decoded_size / sizeof(int16_t));

                std::vector<int16_t> mono;
                if (src_channels >= 2) {
                    int mono_samples = sample_count / src_channels;
                    mono.resize(mono_samples);
                    for (int i = 0; i < mono_samples; i++) {
                        int32_t sum = 0;
                        for (int ch = 0; ch < src_channels; ch++) {
                            sum += pcm16[i * src_channels + ch];
                        }
                        mono[i] = (int16_t)(sum / src_channels);
                    }
                    pcm16 = mono.data();
                    sample_count = mono_samples;
                }

                if (resampler && sample_count > 0) {
                    uint32_t max_out = 0;
                    esp_ae_rate_cvt_get_max_out_sample_num(resampler, (uint32_t)sample_count, &max_out);
                    std::vector<int16_t> resampled(max_out);
                    uint32_t actual_out = max_out;
                    esp_ae_rate_cvt_process(resampler, (esp_ae_sample_t)pcm16,
                                           (uint32_t)sample_count,
                                           (esp_ae_sample_t)resampled.data(), &actual_out);
                    resampled.resize(actual_out);
                    codec_->OutputData(resampled);
                } else if (sample_count > 0) {
                    std::vector<int16_t> out_data(pcm16, pcm16 + sample_count);
                    codec_->OutputData(out_data);
                }
            }
        }

        if (resampler) {
            esp_ae_rate_cvt_close(resampler);
            resampler = nullptr;
        }
        esp_audio_simple_dec_close(decoder);
        fclose(fp);

        if (!skip_requested_ && !stop_requested_) {
            int next = PickNextTrack();
            if (next < 0) break;
            current_track_ = next;
        }
        skip_requested_ = false;
    }

    free(read_buf);
    free(pcm_buf);
    ESP_LOGI(TAG, "Playback stopped");
}
