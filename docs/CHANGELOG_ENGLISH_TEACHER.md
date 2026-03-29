# English Teacher AI — Firmware Changelog

Branch: `hdang/enhance_dashboard`
Board: `english-teacher-ai` (AI-VOX3: ESP32-S3R8 + ES8311 + ST7789 240x240)

---

## v2.0 — Multi-App System & AI-VOX3 Board (Current)

### Hardware Migration: AI-VOX3 Board

Ported the entire firmware from the original prototype (ILI9341 160x128 + INMP441 + MAX98357A) to the AI-VOX3 production board.

| Component | Old | New |
|-----------|-----|-----|
| Display | ILI9341 160x128 | ST7789 240x240 |
| Audio Codec | INMP441 + MAX98357A | ES8311 + NS4150B |
| LED | Single GPIO LED | WS2812 RGB |
| SD Card | None | microSD (SDMMC) |
| Battery | None | Li-Po + FM5327 charger |
| Buttons | BOOT only | BOOT + VOL+ + VOL- |

**Files:** `config.h`, `english_teacher_ai_board.cc`

---

### Multi-App Menu System

Implemented a full application framework with menu-based navigation on the 240x240 LCD.

**Architecture:**
- `AppBase` — abstract interface for all applications (lifecycle, button handling)
- `AppManager` — manages app registration, LVGL menu UI, navigation, button routing
- 4 registered apps: ChatApp, MusicApp, RadioApp, MessagesApp

**Navigation:**
- Long-press BOOT (2s) from any app → return to menu
- VOL+/VOL- click in menu → navigate selection
- VOL+/VOL- long-press (0.8s) → volume up/down (±10%, works globally in any screen)
  - Uses manual `OnPressDown`/`OnPressUp` timing instead of iot_button `BUTTON_LONG_PRESS_START` for reliability on GPIO 45/46 strapping pins
- BOOT click in menu → enter selected app
- Long-press BOOT in menu → enter WiFi config mode
- Double-click BOOT in apps → back (within app sub-screens)

**Boot flow:** Device auto-enters ChatApp on startup for seamless UX. Menu is accessible via long-press.

**Files:** `app_base.h`, `app_manager.h/.cc`, `english_teacher_ai_board.cc`, `CMakeLists.txt`

---

### ChatApp — AI English Conversation

Wraps the existing `Application` singleton as an app within the multi-app system.

- `OnEnter()` calls `ShowChatUI()` to restore chat bubbles and emoji
- `OnExit()` calls `HideChatUI()` to hide chat elements when switching apps
- BOOT click delegates to `ToggleChatState()` (start/stop conversation)
- VOL+/VOL- scroll chat history

**Files:** `chat_app.h/.cc`, `lcd_display.h/.cc` (added `ShowChatUI`/`HideChatUI`)

---

### MusicApp — SD Card Music Player

Full-featured music player with folder browser and playback controls.

**Features:**
- Unified file browser showing folders `[Name]` and audio files in one view
- Navigate into subfolders to any depth
- Sort: folders first (alphabetical), then files (alphabetical)
- File extensions stripped in display, long filenames supported
- Now Playing screen with track name and index
- Supported formats: MP3, WAV, OGG, M4A

**Controls:**
- VOL+/VOL- to browse, BOOT click to enter folder or play file
- Double-click BOOT to go back to parent folder
- In Now Playing: VOL+ = prev track, VOL- = next track, BOOT = stop

**MusicPlayer engine enhancements:**
- Added `Pause()` / `Resume()` with FreeRTOS task integration
- Added `ListFolders()` / `ListFilesInFolder()` for MCP tool support
- Uses `stat()` fallback for `d_type` (FATFS compatibility)

**MCP Tools:**
- `self.list_sd_music` — list music files on SD card
- `self.play_sd_music` — play a specific file

**Files:** `music_app.h/.cc`, `music_player.h/.cc`

---

### RadioApp — Internet Radio Streaming

Full internet radio player streaming English-language stations via HTTP MP3.

**Features:**
- HTTP MP3 streaming with `esp_http_client`
- Pre-buffering (20KB) for smooth playback startup
- Auto-reconnect on stream interruption with configurable retry delay
- HTTP redirect support (up to 10 redirects)
- ICY protocol compatibility (Icecast/Shoutcast servers)
- 8 pre-loaded English radio stations (NPR, KEXP, SomaFM, 181FM, etc.)

**Streaming engine (`RadioPlayer`):**
- Dedicated FreeRTOS streaming task with 8KB stack
- 4KB read buffer, 5s HTTP read timeout, 10s connect timeout
- Pre-buffer phase fills 20KB before starting audio output
- Consecutive error tracking with auto-reconnect (200 error threshold)
- Station management: `AddStation()`, `NextStation()`, `PrevStation()`
- Thread-safe stop/switch via `std::atomic<bool>` flags

**UI:**
- Station list with genre tags (scrollable)
- Now Playing screen with station name, genre, and stream status
- Volume control via VOL+/VOL- click

**Files:** `radio_app.h/.cc`, `radio_player.h/.cc`

---

### MessagesApp — Server Message Inbox

Receives and displays messages pushed from the server.

**Features:**
- Message list with sender, content preview, and timestamps
- Detail view with full message content (scrollable)
- Unread badge count shown in app menu
- Up to 50 messages stored (FIFO)
- Thread-safe message pushing from MCP callbacks

**MCP Tools:**
- `self.push_message` — push a message to the device
- `self.list_messages` — list stored messages

**Files:** `messages_app.h/.cc`

---

### Emoji Improvements

- Emoji scale reduced to 3/4 (512 → 384) for better proportions on 240x240
- Breathing animation range adjusted (345–420 scale)
- Emoji only shown in ChatApp (hidden in other apps)
- Auto-idle: chat bubbles hide after 10s of no interaction, emoji appears
- Interaction restores chat view

**Files:** `lcd_display.cc`

---

### Adaptive Display Brightness

- Default brightness: 20% (was 100%)
- Time-based auto-adjustment (UTC+7 Ho Chi Minh timezone):
  - Day (07:00–18:00): 25%
  - Evening (18:00–22:00): 15%
  - Night (22:00–07:00): 5%
- ESP timer checks every 60 seconds

**Files:** `english_teacher_ai_board.cc`, `config.h`

---

### SD Card Support

- SDMMC 1-bit mode on GPIO 38/39/40
- FAT32 with long filename support (`CONFIG_FATFS_LFN_HEAP`, max 255 chars)
- Mount point: `/sdcard`
- Used by MusicApp for browsing and playback

**Files:** `english_teacher_ai_board.cc`, `sdkconfig`

---

### WiFi Configuration via Long-Press

- Hold BOOT 2s while in app menu → enter WiFi config mode
- Also triggers during startup state
- WiFi config page displays in Vietnamese ("Cấu hình thành công!")

**Files:** `english_teacher_ai_board.cc`

---

## v1.0 — Original Prototype

### Single-Turn Conversation Mode

**Problem:** Original firmware auto-listened after AI spoke. No time for the child to read or think.

**Fix:** TTS stop handler transitions to `kDeviceStateIdle` after AI finishes speaking. Uses `kListeningModeAutoStop` (VAD) for natural silence detection.

**Flow:** Child presses button → Listening (VAD) → Server ASR → LLM → TTS → Idle → Child reads, thinks, presses again

**Files:** `application.cc`

---

### Chat Scroll with Volume Buttons

Added `ScrollChatBy()` API to `LcdDisplay`. VOL+ scrolls up (older), VOL- scrolls down (newer).

**Files:** `display.h`, `lcd_display.h/.cc`, `english_teacher_ai_board.cc`

---

### Preserve Chat Messages on Idle

Removed `ClearChatMessages()` from idle state handler. Chat bubbles persist across conversation turns. Messages only removed when exceeding `MAX_MESSAGES` (20).

**Files:** `application.cc`

---

## Files Modified (vs upstream `xiaozhi-esp32`)

```
main/boards/english-teacher-ai/config.h                    (NEW — board config)
main/boards/english-teacher-ai/english_teacher_ai_board.cc  (NEW — board implementation)
main/app/app_base.h                                         (NEW — app interface)
main/app/app_manager.h                                      (NEW — app manager header)
main/app/app_manager.cc                                     (NEW — app manager implementation)
main/app/chat_app.h                                         (NEW — chat app header)
main/app/chat_app.cc                                        (NEW — chat app implementation)
main/app/music_app.h                                        (NEW — music app header)
main/app/music_app.cc                                       (NEW — music app implementation)
main/app/radio_app.h                                        (NEW — radio app header)
main/app/radio_app.cc                                       (NEW — radio app implementation)
main/app/messages_app.h                                     (NEW — messages app header)
main/app/messages_app.cc                                    (NEW — messages app implementation)
main/audio/music_player.h                                   (NEW — music playback engine)
main/audio/music_player.cc                                  (NEW — music playback engine)
main/audio/radio_player.h                                   (NEW — HTTP MP3 streaming engine)
main/audio/radio_player.cc                                  (NEW — HTTP MP3 streaming engine)
main/display/lcd_display.cc                                 (MODIFIED — emoji, ShowChatUI/HideChatUI)
main/display/lcd_display.h                                  (MODIFIED — new method declarations)
main/display/display.h                                      (MODIFIED — ScrollChatBy API)
main/application.cc                                         (MODIFIED — single-turn, preserve chat)
main/CMakeLists.txt                                         (MODIFIED — new source files)
sdkconfig                                                   (MODIFIED — FATFS LFN, board selection)
```
