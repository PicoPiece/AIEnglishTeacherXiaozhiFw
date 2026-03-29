# EnglishTeacherAI — Product Guide

## Overview

EnglishTeacherAI is an AI-powered English learning device built on the AI-VOX3 board (ESP32-S3). It features voice conversation with an AI English teacher, an SD card music player, internet radio streaming (English stations), and a message inbox — all accessible through a multi-app menu system on a 240x240 LCD display.

**Server:** `https://aietadmin.picopiece.com`

---

## Hardware — AI-VOX3 Board

| Component | Chip / Spec | Interface |
|-----------|-------------|-----------|
| MCU | ESP32-S3R8 (16MB Flash, 8MB PSRAM) | — |
| Audio Codec | ES8311 | I2C + I2S |
| Speaker Amp | NS4150B (always on) | — |
| Display | ST7789 240x240 SPI LCD | SPI |
| LED | WS2812 RGB | GPIO 41 |
| Battery | Li-Po with FM5327 charger | ADC + GPIO |
| SD Card | microSD (SDMMC 1-bit) | SDMMC |

### GPIO Pinout

**Audio (ES8311 I2S)**

| Signal | GPIO |
|--------|------|
| MCLK | 11 |
| BCLK | 10 |
| WS (LRCK) | 8 |
| DOUT (MCU→Codec) | 7 |
| DIN (Codec→MCU) | 9 |

**Audio (ES8311 I2C)**

| Signal | GPIO |
|--------|------|
| SDA | 13 |
| SCL | 12 |

**Display (ST7789 SPI)**

| Signal | GPIO |
|--------|------|
| MOSI (SDA) | 21 |
| SCLK (SCL) | 17 |
| CS | 15 |
| DC | 14 |
| Backlight | 16 |

**Buttons**

| Button | GPIO | Position |
|--------|------|----------|
| BOOT | 0 | Main action button |
| VOL+ (Up) | 45 | Button B |
| VOL- (Down) | 46 | Button A |

**SD Card (SDMMC)**

| Signal | GPIO |
|--------|------|
| CMD | 38 |
| CLK | 39 |
| DAT0 | 40 |

**Other**

| Signal | GPIO |
|--------|------|
| WS2812 LED | 41 |
| Battery ADC | 18 |
| Charge Detect | 47 |

---

## Features

### 1. AI English Conversation (ChatApp)

- Voice conversation with an AI English teacher
- Press BOOT to start talking, AI auto-detects when you stop (VAD)
- Chat bubble UI (WeChat style) shows conversation history
- Scroll through conversation with VOL+/VOL- buttons
- Animated emoji on idle screen (breathing effect, 3/4 scale)
- Auto-idle: chat bubbles hide after 10 seconds of no interaction, emoji appears
- Conversation resumes on any interaction

### 2. Music Player (MusicApp)

- Browse and play MP3/WAV/OGG/M4A files from SD card
- Unified file browser showing folders `[FolderName]` and audio files
- Navigate into subfolders to any depth
1- Selecting a file builds a playlist from all audio files in the current folder
- Now Playing screen with track name, index, and play mode
- 4 play modes: Sequential, Repeat All (default), Repeat One, Shuffle
- Supports long filenames (FATFS LFN enabled)

### 3. Internet Radio (RadioApp)

- Stream English-language internet radio stations via HTTP MP3
- Pre-buffering (20KB) for smooth playback startup
- Pre-loaded stations optimized for Vietnam (low bitrate for less lag):
  - BBC World Service (56kbps — British English news/talk)
  - BBC WS East Asia (56kbps — optimized CDN for Asia)
  - NPR News (128kbps — US English news)
  - CNN Radio (96kbps — US news/business)
  - SomaFM Groove Salad (128kbps — ambient/chill background)
  - KEXP Seattle (128kbps — indie/alternative music)
- Station list UI with genre tags
- Now Playing screen with station name and status
- Auto-reconnect on stream interruption with configurable retry delay
- Long-press VOL+/VOL- to switch stations while streaming

### 4. Messages (MessagesApp)

- Receive messages from the server via MCP tools
- Message list with sender and content preview
- Detail view for full message content
- Unread badge count shown in app menu
- Supports up to 50 messages

### 5. Adaptive Display Brightness

- Auto-adjusts based on time of day (UTC+7, Ho Chi Minh):
  - Day (07:00–18:00): 25%
  - Evening (18:00–22:00): 15%
  - Night (22:00–07:00): 5%
- Default on boot: 20%

### 6. Battery Monitoring

- Real-time battery level display via ADC
- Charge status detection (FM5327)

---

## User Guide — Button Controls

### Global Controls

| Action | What it does |
|--------|-------------|
| **BOOT click** | Primary action (depends on current app/screen) |
| **BOOT double-click** | Back / navigate up within an app |
| **BOOT long-press (2s)** | Return to app menu from any app |
| **VOL+ click** | Navigate up in lists / scroll up in chat |
| **VOL- click** | Navigate down in lists / scroll down in chat |
| **VOL+ long-press (0.8s)** | Volume up (+10%) — shows notification on screen |
| **VOL- long-press (0.8s)** | Volume down (-10%) — shows notification on screen |

### App Menu

The app menu is the central hub. Access it by long-pressing BOOT (2 seconds) from any app.

| Action | What it does |
|--------|-------------|
| VOL+/VOL- click | Move selection up/down |
| VOL+/VOL- long-press | Volume up/down |
| BOOT click | Enter selected app |
| BOOT long-press (2s) | Enter WiFi configuration mode |

### ChatApp (AI English Conversation)

| Action | What it does |
|--------|-------------|
| BOOT click | Start/stop talking to AI |
| VOL+ click | Scroll chat history up (older) |
| VOL- click | Scroll chat history down (newer) |
| VOL+/VOL- long-press | Volume up/down |
| BOOT double-click | *(no action)* |

### MusicApp (SD Card Player)

**File Browser:**

| Action | What it does |
|--------|-------------|
| VOL+/VOL- click | Move selection up/down |
| VOL+/VOL- long-press | Volume up/down |
| BOOT click on `[Folder]` | Open folder |
| BOOT click on file | Play selected song |
| BOOT double-click | Go back to parent folder |

**Now Playing:**

| Action | What it does |
|--------|-------------|
| VOL+ click | Previous track |
| VOL- click | Next track |
| VOL+/VOL- long-press | Volume up/down |
| BOOT click | Cycle play mode (Sequential → Repeat All → Repeat One → Shuffle) |
| BOOT double-click | Stop playback, return to browser |

### MessagesApp

**Message List:**

| Action | What it does |
|--------|-------------|
| VOL+/VOL- click | Move selection up/down |
| VOL+/VOL- long-press | Volume up/down |
| BOOT click | Open selected message |

**Message Detail:**

| Action | What it does |
|--------|-------------|
| VOL+/VOL- click | Scroll message content |
| VOL+/VOL- long-press | Volume up/down |
| BOOT double-click | Back to message list |

### RadioApp (Internet Radio)

**Station List:**

| Action | What it does |
|--------|-------------|
| VOL+/VOL- click | Move selection up/down |
| VOL+/VOL- long-press | Volume up/down |
| BOOT click | Start streaming selected station |

**Now Playing:**

| Action | What it does |
|--------|-------------|
| VOL+ click | Switch to previous station |
| VOL- click | Switch to next station |
| VOL+/VOL- long-press | Volume up/down |
| BOOT click | Stop streaming, return to station list |
| BOOT double-click | Stop streaming, return to station list |

---

## WiFi Configuration

### When does WiFi config mode activate?

- Automatically on first boot (no saved WiFi)
- Click BOOT during device startup ("Starting..." screen)
- Long-press BOOT (2s) while in the app menu

### Steps

1. Device creates WiFi hotspot: **Xiaozhi-XXXX** (open, no password)
2. Connect phone/laptop to this hotspot
3. Browser opens automatically, or navigate to `http://192.168.4.1`
4. On the WiFi config page:
   - Select your home WiFi network
   - Enter password
5. Device connects and contacts OTA server for activation

### Custom Server Setup

If using a self-hosted server instead of the default:

1. Enter WiFi config mode
2. Go to **Advanced** tab
3. Set **Custom OTA URL**: `https://aietadmin.picopiece.com/xiaozhi/ota/`
4. Save, then configure WiFi

---

## Device Activation

After connecting to WiFi, the device contacts the OTA server and displays an **activation code** on the LCD.

1. Open server web console: `https://aietadmin.picopiece.com`
2. Log in as admin
3. Navigate to device management
4. Enter the activation code shown on the device LCD
5. Assign the **English Teacher** agent to the device
6. Device connects via WebSocket and is ready to use

---

## SD Card Setup (for Music Player)

### Supported Formats

MP3, WAV, OGG, M4A

### Recommended Structure

```
/sdcard/
├── Pop/
│   ├── song1.mp3
│   └── song2.mp3
├── Classical/
│   ├── beethoven.mp3
│   └── mozart/
│       └── symphony.mp3
├── Kids Songs/
│   └── abc.mp3
└── standalone_track.mp3
```

The Music app browses the SD card from the root. Folders are shown as `[FolderName]` and can be nested to any depth. Audio files are listed below folders, sorted alphabetically. Long filenames are fully supported.

---

## Build & Flash (Developer)

### Prerequisites

- ESP-IDF v5.5.2
- ESP32-S3 connected via USB (COM3)

### Build

```powershell
cd D:\7.Personal_Project\49.EnglishTeacherAI-xiaozhi
idf.py build
```

### Flash

```powershell
idf.py -p COM3 flash
```

### Monitor Serial Output

```powershell
$env:PYTHONIOENCODING = "utf-8"
idf.py -p COM3 monitor
```

---

## Server Configuration

### Server Infrastructure

| Service | Address |
|---------|---------|
| Web Console / OTA | `https://aietadmin.picopiece.com` |

### English Teacher Agent

| Setting | Value |
|---------|-------|
| LLM | Gemini 2.5 Flash |
| LLM (backup) | DeepSeek Chat |
| TTS | EdgeTTS (`en-US-AriaNeural`) |
| ASR | FunASR (SenseVoiceSmall) |

### System Prompt

```
You are an English teacher for Vietnamese students. Your name is Teacher AI.

Rules:
- Always speak in English, use simple and clear sentences
- If the student speaks Vietnamese, gently remind them to try in English
- Correct grammar mistakes kindly, then give the right sentence
- After correcting, explain briefly why it was wrong
- Encourage the student after each attempt
- Keep responses short (2-3 sentences max) for voice conversation
- Adjust difficulty based on student's level
- Use common daily English topics: greetings, school, family, hobbies, food
- Never discuss topics unrelated to English learning
```

### MCP Tools (Server → Device)

| Tool | Parameters | Description |
|------|------------|-------------|
| `self.list_sd_music` | *(none)* | Returns compact text summary of all folders and songs on SD card |
| `self.play_sd_music` | `query` (required), `folder` (optional) | Search and play music by keyword. Builds playlist from matching scope |
| `self.push_message` | `sender`, `content` | Send a text message to the device Messages app |
| `self.list_messages` | *(none)* | List messages stored on the device |

**MCP Usage Notes:**
- `self.list_sd_music` returns a single compact response (fits within 64KB WebSocket limit)
- `self.play_sd_music` uses case-insensitive substring matching on song names
- When playing, all audio files in the search scope become the playlist (supports Next/Prev/Shuffle)

---

## Architecture

```
EnglishTeacherAiBoard (english_teacher_ai_board.cc)
├── AppManager (app_manager.cc)
│   ├── ChatApp        — AI English conversation (wraps Application singleton)
│   ├── MusicApp       — SD card music player with folder browser
│   ├── RadioApp       — Internet radio streaming (HTTP MP3)
│   └── MessagesApp    — Server message inbox
├── MusicPlayer (music_player.cc)
│   └── FreeRTOS playback task, audio decode, resampling
├── RadioPlayer (radio_player.cc)
│   └── HTTP MP3 streaming, auto-reconnect, station management
├── LcdDisplay (lcd_display.cc)
│   └── LVGL UI: chat bubbles, emoji, app UIs
└── Hardware
    ├── ES8311 Audio Codec (I2C + I2S)
    ├── ST7789 240x240 LCD (SPI)
    ├── WS2812 LED
    ├── 3 Buttons (BOOT, VOL+, VOL-)
    ├── SD Card (SDMMC)
    └── Battery Monitor (ADC)
```

### Key Source Files

| File | Purpose |
|------|---------|
| `main/boards/english-teacher-ai/config.h` | GPIO pinout and hardware constants |
| `main/boards/english-teacher-ai/english_teacher_ai_board.cc` | Board init, button routing, app manager setup |
| `main/app/app_base.h` | Abstract app interface |
| `main/app/app_manager.h/.cc` | Menu UI, app lifecycle, navigation |
| `main/app/chat_app.h/.cc` | AI chat wrapper |
| `main/app/music_app.h/.cc` | Music player with file browser |
| `main/app/radio_app.h/.cc` | Internet radio with station list |
| `main/app/messages_app.h/.cc` | Message inbox |
| `main/audio/music_player.h/.cc` | Audio playback engine |
| `main/audio/radio_player.h/.cc` | HTTP MP3 streaming engine |
| `main/display/lcd_display.h/.cc` | LVGL display driver and UI |
