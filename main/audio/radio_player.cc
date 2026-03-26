#include "radio_player.h"
#include "audio_codec.h"

#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>

#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_audio_dec_default.h"
#include "esp_ae_rate_cvt.h"

#define TAG "RadioPlayer"

#define STREAM_READ_BUF_SIZE  2048
#define PCM_OUT_BUF_SIZE      (4608 * 2)
#define STREAM_STACK_SIZE     (12 * 1024)
#define HTTP_TIMEOUT_MS       15000
#define RECONNECT_DELAY_MS    3000
#define MAX_RECONNECT_ATTEMPTS 5

RadioPlayer::RadioPlayer(AudioCodec* codec)
    : codec_(codec) {
    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
}

RadioPlayer::~RadioPlayer() {
    Stop();
}

void RadioPlayer::AddStation(const std::string& name, const std::string& url, const std::string& genre) {
    stations_.push_back({name, url, genre});
}

std::string RadioPlayer::CurrentStationName() const {
    if (stations_.empty() || current_station_ < 0 || current_station_ >= (int)stations_.size()) {
        return "";
    }
    return stations_[current_station_].name;
}

void RadioPlayer::NotifyStationChange() {
    if (station_change_cb_) {
        station_change_cb_(CurrentStationName(), current_station_, (int)stations_.size());
    }
}

bool RadioPlayer::Play(int station_index) {
    Stop();
    if (station_index < 0 || station_index >= (int)stations_.size()) return false;

    current_station_ = station_index;
    stop_requested_ = false;
    switch_requested_ = false;
    playing_ = true;

    ESP_LOGI(TAG, "Playing station %d: %s", station_index, stations_[station_index].name.c_str());
    NotifyStationChange();

    xTaskCreate(StreamTaskEntry, "radio_stream", STREAM_STACK_SIZE, this, 3, &task_handle_);
    return true;
}

void RadioPlayer::Stop() {
    if (!playing_) return;
    stop_requested_ = true;
    if (task_handle_) {
        int wait_ms = 0;
        while (playing_ && wait_ms < 5000) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_ms += 10;
        }
        task_handle_ = nullptr;
    }
}

void RadioPlayer::NextStation() {
    if (stations_.empty()) return;
    current_station_ = (current_station_ + 1) % (int)stations_.size();
    switch_requested_ = true;
}

void RadioPlayer::PrevStation() {
    if (stations_.empty()) return;
    current_station_ = (current_station_ - 1 + (int)stations_.size()) % (int)stations_.size();
    switch_requested_ = true;
}

void RadioPlayer::StreamTaskEntry(void* arg) {
    auto* self = static_cast<RadioPlayer*>(arg);
    self->StreamTask();
    self->playing_ = false;
    self->task_handle_ = nullptr;
    if (self->stop_cb_) {
        self->stop_cb_();
    }
    vTaskDelete(NULL);
}

void RadioPlayer::StreamTask() {
    uint8_t* read_buf = (uint8_t*)malloc(STREAM_READ_BUF_SIZE);
    uint8_t* pcm_buf = (uint8_t*)malloc(PCM_OUT_BUF_SIZE);
    if (!read_buf || !pcm_buf) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        free(read_buf);
        free(pcm_buf);
        return;
    }

    const int target_rate = codec_->output_sample_rate();

    while (!stop_requested_) {
        if (current_station_ < 0 || current_station_ >= (int)stations_.size()) break;

        const RadioStation& station = stations_[current_station_];
        ESP_LOGI(TAG, "Connecting to: %s [%s]", station.name.c_str(), station.url.c_str());
        NotifyStationChange();

        esp_http_client_config_t http_cfg = {};
        http_cfg.url = station.url.c_str();
        http_cfg.timeout_ms = HTTP_TIMEOUT_MS;
        http_cfg.buffer_size = STREAM_READ_BUF_SIZE;
        http_cfg.buffer_size_tx = 512;
        http_cfg.crt_bundle_attach = esp_crt_bundle_attach;

        esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
        if (!client) {
            ESP_LOGE(TAG, "Failed to init HTTP client");
            if (error_cb_) error_cb_("Connection failed");
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            continue;
        }

        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            if (error_cb_) error_cb_("Connection failed");
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            continue;
        }

        esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        if (status != 200) {
            ESP_LOGE(TAG, "HTTP %d for %s", status, station.name.c_str());
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            if (error_cb_) error_cb_("HTTP error");
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            continue;
        }

        ESP_LOGI(TAG, "Streaming: %s", station.name.c_str());

        esp_audio_simple_dec_cfg_t dec_cfg = {};
        dec_cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
        esp_audio_simple_dec_handle_t decoder = nullptr;
        esp_audio_err_t ret = esp_audio_simple_dec_open(&dec_cfg, &decoder);
        if (ret != ESP_AUDIO_ERR_OK || !decoder) {
            ESP_LOGE(TAG, "Failed to open decoder: %d", ret);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            break;
        }

        esp_ae_rate_cvt_handle_t resampler = nullptr;
        bool info_ready = false;
        uint32_t src_rate = 0;
        uint8_t src_channels = 0;
        switch_requested_ = false;
        int consecutive_errors = 0;

        while (!stop_requested_ && !switch_requested_) {
            int bytes_read = esp_http_client_read(client, (char*)read_buf, STREAM_READ_BUF_SIZE);
            if (bytes_read < 0) {
                ESP_LOGW(TAG, "Read error, reconnecting...");
                break;
            }
            if (bytes_read == 0) {
                vTaskDelay(pdMS_TO_TICKS(50));
                consecutive_errors++;
                if (consecutive_errors > 100) {
                    ESP_LOGW(TAG, "No data for 5s, reconnecting...");
                    break;
                }
                continue;
            }
            consecutive_errors = 0;

            esp_audio_simple_dec_raw_t raw = {};
            raw.buffer = read_buf;
            raw.len = (uint32_t)bytes_read;
            raw.eos = false;

            while (raw.len > 0 && !stop_requested_ && !switch_requested_) {
                esp_audio_simple_dec_out_t out = {};
                out.buffer = pcm_buf;
                out.len = PCM_OUT_BUF_SIZE;

                ret = esp_audio_simple_dec_process(decoder, &raw, &out);
                raw.buffer += raw.consumed;
                raw.len -= raw.consumed;

                if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) continue;
                if (ret != ESP_AUDIO_ERR_OK && ret != ESP_AUDIO_ERR_CONTINUE) break;
                if (out.decoded_size == 0) continue;

                if (!info_ready) {
                    esp_audio_simple_dec_info_t info = {};
                    if (esp_audio_simple_dec_get_info(decoder, &info) == ESP_AUDIO_ERR_OK) {
                        src_rate = info.sample_rate;
                        src_channels = info.channel;
                        info_ready = true;
                        ESP_LOGI(TAG, "Audio: %luHz %dch", (unsigned long)src_rate, src_channels);

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
        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        if (!stop_requested_ && !switch_requested_) {
            ESP_LOGW(TAG, "Stream ended, reconnecting in %dms...", RECONNECT_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
        }
        switch_requested_ = false;
    }

    free(read_buf);
    free(pcm_buf);
    ESP_LOGI(TAG, "Radio stopped");
}
