#include "radio_player.h"
#include "audio_codec.h"

#include <esp_log.h>
#include <esp_crt_bundle.h>

#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_audio_dec_default.h"
#include "esp_ae_rate_cvt.h"

#define TAG "RadioPlayer"

#define READ_BUF_SIZE           4096
#define PCM_OUT_BUF_SIZE        (4608 * 2)
#define DECODER_STACK_SIZE      (16 * 1024)
#define READER_STACK_SIZE       (6 * 1024)
#define HTTP_CONNECT_TIMEOUT_MS 10000
#define HTTP_READ_TIMEOUT_MS    10000
#define RECONNECT_DELAY_MS      2000
#define MAX_RECONNECT_ATTEMPTS  10
#define RING_BUF_SIZE           (64 * 1024)
#define PREBUFFER_TARGET        (32 * 1024)

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
    reader_done_ = false;
    reader_error_ = false;
    playing_ = true;

    ESP_LOGI(TAG, "Playing station %d: %s", station_index, stations_[station_index].name.c_str());
    NotifyStationChange();

    xTaskCreate(DecoderTaskEntry, "radio_dec", DECODER_STACK_SIZE, this, 5, &decoder_task_);
    return true;
}

void RadioPlayer::Stop() {
    if (!playing_) return;
    stop_requested_ = true;
    if (decoder_task_) {
        const int stop_timeout = HTTP_CONNECT_TIMEOUT_MS + 5000;
        int wait_ms = 0;
        while (playing_ && wait_ms < stop_timeout) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_ms += 10;
        }
        if (playing_) {
            ESP_LOGW(TAG, "Stop timed out after %dms", wait_ms);
        }
        decoder_task_ = nullptr;
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

void RadioPlayer::DecoderTaskEntry(void* arg) {
    auto* self = static_cast<RadioPlayer*>(arg);
    self->DecoderTask();
    self->playing_ = false;
    self->decoder_task_ = nullptr;
    if (self->stop_cb_) {
        self->stop_cb_();
    }
    vTaskDelete(NULL);
}

void RadioPlayer::ReaderTaskEntry(void* arg) {
    static_cast<RadioPlayer*>(arg)->ReaderTask();
    vTaskDelete(NULL);
}

// Network reader: fills ring buffer from HTTP stream
void RadioPlayer::ReaderTask() {
    uint8_t* buf = (uint8_t*)malloc(READ_BUF_SIZE);
    if (!buf) {
        ESP_LOGE(TAG, "Reader: malloc failed");
        reader_error_ = true;
        reader_done_ = true;
        return;
    }

    int empty_count = 0;
    int error_count = 0;

    while (!stop_requested_ && !switch_requested_) {
        int bytes = esp_http_client_read(http_client_, (char*)buf, READ_BUF_SIZE);
        if (bytes < 0) {
            error_count++;
            ESP_LOGW(TAG, "Reader: read error #%d", error_count);
            if (error_count >= 3) {
                ESP_LOGE(TAG, "Reader: too many errors, giving up");
                reader_error_ = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        error_count = 0;

        if (bytes == 0) {
            empty_count++;
            if (empty_count > 100) {
                ESP_LOGW(TAG, "Reader: no data for 10s");
                reader_error_ = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        empty_count = 0;

        if (!xRingbufferSend(ring_buf_, buf, bytes, pdMS_TO_TICKS(2000))) {
            if (stop_requested_ || switch_requested_) break;
            ESP_LOGW(TAG, "Reader: ring buffer full");
        }
    }

    free(buf);
    reader_done_ = true;
    ESP_LOGI(TAG, "Reader task ended");
}

// Decoder: reads from ring buffer, decodes MP3, outputs PCM
void RadioPlayer::DecoderTask() {
    uint8_t* pcm_buf = (uint8_t*)malloc(PCM_OUT_BUF_SIZE);
    const int max_mono_samples = PCM_OUT_BUF_SIZE / sizeof(int16_t);
    const int max_resample_out = max_mono_samples * 2;
    int16_t* mono_buf = (int16_t*)malloc(max_mono_samples * sizeof(int16_t));
    int16_t* resample_buf = (int16_t*)malloc(max_resample_out * sizeof(int16_t));
    if (!pcm_buf || !mono_buf || !resample_buf) {
        ESP_LOGE(TAG, "Failed to allocate decode buffers");
        free(pcm_buf); free(mono_buf); free(resample_buf);
        return;
    }

    const int target_rate = codec_->output_sample_rate();
    int reconnect_attempts = 0;
    std::vector<int16_t> output_vec;
    output_vec.reserve(max_resample_out);

    while (!stop_requested_) {
        if (current_station_ < 0 || current_station_ >= (int)stations_.size()) break;

        const RadioStation& station = stations_[current_station_];
        ESP_LOGI(TAG, "Connecting to: %s [%s]", station.name.c_str(), station.url.c_str());
        NotifyStationChange();
        if (status_cb_) status_cb_("Connecting...");

        // Setup HTTP client
        esp_http_client_config_t http_cfg = {};
        http_cfg.url = station.url.c_str();
        http_cfg.timeout_ms = HTTP_CONNECT_TIMEOUT_MS;
        http_cfg.buffer_size = READ_BUF_SIZE;
        http_cfg.buffer_size_tx = 512;
        http_cfg.crt_bundle_attach = esp_crt_bundle_attach;
        http_cfg.max_redirection_count = 10;

        http_client_ = esp_http_client_init(&http_cfg);
        if (!http_client_) {
            ESP_LOGE(TAG, "Failed to init HTTP client");
            if (error_cb_) error_cb_("Connection failed");
            if (++reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) break;
            for (int d = 0; d < RECONNECT_DELAY_MS && !stop_requested_; d += 100)
                vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        esp_http_client_set_header(http_client_, "User-Agent", "Mozilla/5.0 (Linux; ESP32) AppleWebKit/537.36");
        esp_http_client_set_header(http_client_, "Accept", "*/*");
        esp_http_client_set_header(http_client_, "Connection", "keep-alive");
        esp_http_client_set_header(http_client_, "Icy-MetaData", "0");

        esp_err_t err = esp_http_client_open(http_client_, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open: %s", esp_err_to_name(err));
            esp_http_client_cleanup(http_client_);
            http_client_ = nullptr;
            if (error_cb_) error_cb_("Connection failed");
            if (++reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) break;
            for (int d = 0; d < RECONNECT_DELAY_MS && !stop_requested_; d += 100)
                vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int64_t content_len = esp_http_client_fetch_headers(http_client_);
        int status = esp_http_client_get_status_code(http_client_);
        ESP_LOGI(TAG, "HTTP %d, content_len=%d for %s", status, (int)content_len, station.name.c_str());

        esp_http_client_set_timeout_ms(http_client_, HTTP_READ_TIMEOUT_MS);

        if (status != 200 && status != 0) {
            ESP_LOGE(TAG, "HTTP %d for %s", status, station.name.c_str());
            esp_http_client_close(http_client_);
            esp_http_client_cleanup(http_client_);
            http_client_ = nullptr;
            if (error_cb_) error_cb_("HTTP error");
            if (++reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) break;
            for (int d = 0; d < RECONNECT_DELAY_MS && !stop_requested_; d += 100)
                vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        reconnect_attempts = 0;

        // Create ring buffer
        ring_buf_ = xRingbufferCreate(RING_BUF_SIZE, RINGBUF_TYPE_BYTEBUF);
        if (!ring_buf_) {
            ESP_LOGE(TAG, "Failed to create ring buffer");
            esp_http_client_close(http_client_);
            esp_http_client_cleanup(http_client_);
            http_client_ = nullptr;
            break;
        }

        // Start reader task (network I/O in separate thread)
        reader_done_ = false;
        reader_error_ = false;
        xTaskCreate(ReaderTaskEntry, "radio_net", READER_STACK_SIZE, this, 3, &reader_task_);

        // Wait for pre-buffer to fill
        ESP_LOGI(TAG, "Pre-buffering %s (target %dKB)...", station.name.c_str(), PREBUFFER_TARGET / 1024);
        if (status_cb_) status_cb_("Buffering...");
        while (!stop_requested_ && !switch_requested_ && !reader_error_) {
            UBaseType_t free_size = xRingbufferGetCurFreeSize(ring_buf_);
            int buffered = RING_BUF_SIZE - (int)free_size;
            if (buffered >= PREBUFFER_TARGET) break;
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        if (stop_requested_ || switch_requested_ || reader_error_) {
            goto cleanup_station;
        }

        {
            UBaseType_t free_size = xRingbufferGetCurFreeSize(ring_buf_);
            ESP_LOGI(TAG, "Pre-buffered %dKB, starting playback", (RING_BUF_SIZE - (int)free_size) / 1024);
            if (status_cb_) status_cb_("Streaming");
        }

        // Decode loop
        {
            esp_audio_simple_dec_cfg_t dec_cfg = {};
            dec_cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
            esp_audio_simple_dec_handle_t decoder = nullptr;
            esp_audio_err_t ret = esp_audio_simple_dec_open(&dec_cfg, &decoder);
            if (ret != ESP_AUDIO_ERR_OK || !decoder) {
                ESP_LOGE(TAG, "Failed to open decoder: %d", ret);
                goto cleanup_station;
            }

            esp_ae_rate_cvt_handle_t resampler = nullptr;
            bool info_ready = false;
            uint32_t src_rate = 0;
            uint8_t src_channels = 0;
            switch_requested_ = false;
            int underrun_count = 0;

            while (!stop_requested_ && !switch_requested_) {
                size_t item_size = 0;
                uint8_t* data = (uint8_t*)xRingbufferReceive(ring_buf_, &item_size, pdMS_TO_TICKS(100));

                if (!data) {
                    if (reader_done_ || reader_error_) {
                        ESP_LOGW(TAG, "Reader ended, buffer empty -> reconnect");
                        break;
                    }
                    underrun_count++;
                    if (underrun_count > 50) {
                        ESP_LOGW(TAG, "Buffer underrun for 5s, reconnecting...");
                        break;
                    }
                    continue;
                }
                underrun_count = 0;

                esp_audio_simple_dec_raw_t raw = {};
                raw.buffer = data;
                raw.len = (uint32_t)item_size;
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

                    if (src_channels >= 2) {
                        int mono_samples = sample_count / src_channels;
                        for (int i = 0; i < mono_samples; i++) {
                            int32_t sum = 0;
                            for (int ch = 0; ch < src_channels; ch++) {
                                sum += pcm16[i * src_channels + ch];
                            }
                            mono_buf[i] = (int16_t)(sum / src_channels);
                        }
                        pcm16 = mono_buf;
                        sample_count = mono_samples;
                    }

                    if (resampler && sample_count > 0) {
                        uint32_t max_out_samples = 0;
                        esp_ae_rate_cvt_get_max_out_sample_num(resampler, (uint32_t)sample_count, &max_out_samples);
                        if ((int)max_out_samples > max_resample_out) max_out_samples = (uint32_t)max_resample_out;
                        uint32_t actual_out = max_out_samples;
                        esp_ae_rate_cvt_process(resampler, (esp_ae_sample_t)pcm16,
                                               (uint32_t)sample_count,
                                               (esp_ae_sample_t)resample_buf, &actual_out);
                        output_vec.assign(resample_buf, resample_buf + actual_out);
                        codec_->OutputData(output_vec);
                    } else if (sample_count > 0) {
                        output_vec.assign(pcm16, pcm16 + sample_count);
                        codec_->OutputData(output_vec);
                    }
                }

                vRingbufferReturnItem(ring_buf_, data);
            }

            if (resampler) {
                esp_ae_rate_cvt_close(resampler);
            }
            esp_audio_simple_dec_close(decoder);
        }

cleanup_station:
        {
            bool user_requested_stop = stop_requested_.load();
            bool user_requested_switch = switch_requested_.load();

            // Signal reader to stop
            stop_requested_ = true;

            if (reader_task_) {
                int wait = 0;
                while (!reader_done_ && wait < 6000) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    wait += 50;
                }
                if (!reader_done_) {
                    ESP_LOGW(TAG, "Reader task did not exit in time");
                }
                reader_task_ = nullptr;
            }

            if (http_client_) {
                esp_http_client_close(http_client_);
                esp_http_client_cleanup(http_client_);
                http_client_ = nullptr;
            }

            if (ring_buf_) {
                vRingbufferDelete(ring_buf_);
                ring_buf_ = nullptr;
            }

            if (user_requested_stop) {
                // User pressed stop - exit completely
                break;
            } else if (user_requested_switch) {
                // User switched station - reset and loop
                stop_requested_ = false;
                switch_requested_ = false;
                reconnect_attempts = 0;
                ESP_LOGI(TAG, "Switching to station %d", current_station_.load());
            } else {
                // Stream error or ended - try reconnect
                stop_requested_ = false;
                switch_requested_ = false;
                reconnect_attempts++;
                if (reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) {
                    ESP_LOGE(TAG, "Max reconnect attempts reached, stopping");
                    if (error_cb_) error_cb_("Connection lost");
                    break;
                }
                ESP_LOGW(TAG, "Stream ended, reconnecting (%d/%d)...",
                         reconnect_attempts, MAX_RECONNECT_ATTEMPTS);
                for (int d = 0; d < RECONNECT_DELAY_MS && !stop_requested_; d += 100)
                    vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }

    free(pcm_buf);
    free(mono_buf);
    free(resample_buf);
    ESP_LOGI(TAG, "Radio stopped");
}
