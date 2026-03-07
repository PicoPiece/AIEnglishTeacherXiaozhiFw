# EnglishTeacherAI — Setup & Activation Guide

## Hardware

| Component | Chip | Interface |
|-----------|------|-----------|
| MCU | ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM) | — |
| Microphone | INMP441 | I2S |
| Speaker | MAX98357A | I2S |
| Display | ILI9341 128x160 | SPI |

### GPIO Pinout

**Microphone (INMP441)**

| Signal | GPIO |
|--------|------|
| WS | 4 |
| SCK | 5 |
| SD (DIN) | 6 |

**Speaker (MAX98357A)**

| Signal | GPIO |
|--------|------|
| DIN (DOUT) | 7 |
| BCLK | 15 |
| LRCK | 16 |

**Display (ILI9341 128x160)**

| Signal | GPIO |
|--------|------|
| MOSI | 41 |
| SCK | 42 |
| CS | 38 |
| DC | 1 |
| RST | 2 |
| BL (LED) | 17 |

**Other**

| Signal | GPIO |
|--------|------|
| BOOT button | 0 |
| LED | 48 |

> **Note:** GPIO 33-37 are reserved for Octal PSRAM on N16R8 modules. Do NOT use them for peripherals.

---

## Server Infrastructure

| Service | Address | Purpose |
|---------|---------|---------|
| Web Console | `http://192.168.1.48:8002` | Admin UI, Agent config, device management |
| WebSocket | `ws://192.168.1.48:8000/xiaozhi/v1/` | Real-time voice communication |
| OTA | `http://192.168.1.48:8002/xiaozhi/ota/` | Firmware update & device activation |
| Vision API | `http://192.168.1.48:8003/mcp/vision/explain` | Image analysis |
| MySQL | `192.168.1.48:3306` (Docker internal) | Database |
| Redis | `192.168.1.48:6379` (Docker internal) | Cache |

**Docker services:** `xiaozhi-esp32-server`, `xiaozhi-esp32-server-web`, `xiaozhi-esp32-server-db`, `xiaozhi-esp32-server-redis`

---

## Build & Flash Firmware

### Prerequisites

- ESP-IDF v5.5.2 at `C:\Users\kokon\esp\v5.5.2\esp-idf`
- ESP32-S3 connected via USB (CH343 on COM5)

### Build

```powershell
cd D:\7.Personal_Project\49.EnglishTeacherAI-xiaozhi
C:\Users\kokon\esp\v5.5.2\esp-idf\export.ps1
idf.py build
```

### Flash

```powershell
# Kill any monitor processes holding COM5 first
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match "COM5" -and $_.Name -eq "python.exe" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }

# Flash
idf.py -p COM5 flash
```

### Monitor

```powershell
$env:PYTHONIOENCODING = "utf-8"
idf.py -p COM5 monitor
```

---

## WiFi Configuration (First Boot / Server Change)

ESP32 enters WiFi config mode automatically on first boot or when no WiFi is saved.

### How to re-enter WiFi config mode

Press **RESET** button, then **immediately press BOOT** button while device is in "starting" state (within first 2 seconds).

### Steps

1. Connect phone to WiFi hotspot **AITeacher-XXXX** (open network, no password)
2. Browser opens automatically, or go to `http://192.168.4.1`
3. **Advanced** tab → set **Custom OTA URL**: `http://192.168.1.48:8002/xiaozhi/ota/`
4. **Save**
5. **Wi-Fi Config** tab → select home WiFi, enter password
6. ESP32 connects to WiFi and contacts OTA server for activation

---

## Device Activation

After connecting to WiFi, the ESP32 contacts the OTA server and displays an **activation code** on the LCD.

### Steps

1. Open web console: `http://192.168.1.48:8002`
2. Log in as admin (`picopiece`)
3. Navigate to **OTA Management** or **Agents** page
4. Find the pending device or enter the activation code shown on ESP32 LCD
5. Assign Agent **English Teacher** to the device
6. ESP32 automatically connects via WebSocket and is ready to use

### Switching from xiaozhi.me to self-hosted server

If the device was previously activated on xiaozhi.me:

1. Press RESET → immediately press BOOT to enter WiFi config mode
2. Go to **Advanced** → set Custom OTA URL to `http://192.168.1.48:8002/xiaozhi/ota/`
3. Save → re-connect to home WiFi
4. Device will re-activate against the self-hosted server
5. Complete activation on web console `http://192.168.1.48:8002`

---

## Agent Configuration

### English Teacher Agent

| Setting | Value |
|---------|-------|
| Name | English Teacher |
| LLM (primary) | Gemini 2.5 Flash (free tier: 5 RPM, 20 RPD) |
| LLM (backup) | DeepSeek Chat (`deepseek-chat`, paid ~$0.27/1M tokens) |
| TTS | EdgeTTS (`en-US-AriaNeural`) — uses `TTS_EdgeTTS` model ID |
| ASR | FunASR (SenseVoiceSmall, local) |

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
- If asked about non-English topics, redirect: "Let's focus on English! How about we talk about..."
```

### Models Configuration (Web Console → Models)

> **IMPORTANT: Web console has TWO levels of config.**
> The "Model Name" field on the form is just a **display label** (stored in `model_name` DB column).
> The actual API parameters (`model_name`, `voice`, `api_key`, etc.) are inside **config_json** — look for separate fields below the display name in the edit form.
> If the config_json fields are wrong, the model will silently fail even though the display name looks correct.

**LLM — Gemini 2.5 Flash** (recommended)

| Field (config_json) | Value |
|---------------------|-------|
| type | `gemini` |
| model_name | `gemini-2.5-flash` |
| api_key | *(your Gemini API key from aistudio.google.com)* |
| http_proxy | *(leave empty)* |
| https_proxy | *(leave empty)* |

> **IMPORTANT:** `gemini-2.0-flash` has been deprecated by Google (quota = 0). Use `gemini-2.5-flash` or newer.
> Do NOT put API URLs in proxy fields. Proxy fields are for network proxies (SOCKS/HTTP) only.

**LLM — DeepSeek Chat** (backup)

| Field (config_json) | Value |
|---------------------|-------|
| type | `openai` |
| model_name | `deepseek-chat` |
| base_url | `https://api.deepseek.com/v1` |
| api_key | *(your DeepSeek API key from platform.deepseek.com)* |

**TTS — EdgeTTS English**

| Field (config_json) | Value |
|---------------------|-------|
| type | `edge` |
| voice | `en-US-AriaNeural` |
| output_dir | *(leave empty)* |

> **Common mistake:** If the `voice` field is empty, TTS will fail with `Invalid voice ''`. Must set a valid EdgeTTS voice name.

### Troubleshooting: Direct SQL Fix

If the web console doesn't save config_json fields properly, fix via MySQL directly:

```bash
# Fix EdgeTTS voice
docker exec xiaozhi-esp32-server-db mysql -uroot -p123456 \
  -e "USE xiaozhi_esp32_server; UPDATE ai_model_config SET config_json = JSON_SET(config_json, '$.voice', 'en-US-AriaNeural') WHERE id='163650e761a0b034658f0e4520419936';"

# Fix Gemini model_name
docker exec xiaozhi-esp32-server-db mysql -uroot -p123456 \
  -e "USE xiaozhi_esp32_server; UPDATE ai_model_config SET config_json = JSON_SET(config_json, '$.model_name', 'gemini-2.0-flash') WHERE id='9b1efdbbb5459b1a6e3b5d8f99924e11';"

# Clear incorrect proxy settings
docker exec xiaozhi-esp32-server-db mysql -uroot -p123456 \
  -e "USE xiaozhi_esp32_server; UPDATE ai_model_config SET config_json = JSON_SET(config_json, '$.https_proxy', '', '$.http_proxy', '') WHERE id='9b1efdbbb5459b1a6e3b5d8f99924e11';"

# Verify changes
docker exec xiaozhi-esp32-server-db mysql -uroot -p123456 \
  -e "USE xiaozhi_esp32_server; SELECT id, model_name, config_json FROM ai_model_config WHERE id='163650e761a0b034658f0e4520419936' OR id='9b1efdbbb5459b1a6e3b5d8f99924e11';"

# Restart server to apply
docker restart xiaozhi-esp32-server
```

---

## Button Usage

| Action | Behavior |
|--------|----------|
| Click BOOT (after boot) | Toggle Standby ↔ Listening (talk to AI) |
| Click BOOT (during startup) | Enter WiFi config mode |
| RESET + BOOT | Re-enter WiFi config mode from any state |

---

## Server Management

### Docker commands (on Ubuntu server 192.168.1.48)

```bash
cd ~/xiaozhi-server

# Check all services
docker compose -f docker-compose_all.yml ps

# View Python server logs
docker logs xiaozhi-esp32-server --tail 50

# Restart all services
docker compose -f docker-compose_all.yml restart

# Restart specific service
docker compose -f docker-compose_all.yml restart xiaozhi-esp32-server
```

### Database access

```bash
# SSH: ssh picopiece@192.168.1.48
docker exec -i xiaozhi-esp32-server-db mysql -uroot -p123456 xiaozhi_esp32_server
```

### Key database tables

| Table | Purpose |
|-------|---------|
| `sys_user` | Admin/user accounts |
| `sys_params` | Server parameters (websocket URL, OTA URL, secret) |
| `ai_model_config` | Model configurations (LLM, TTS, ASR, etc.) — `config_json` column holds actual API params |
| `ai_agent` | Agent definitions (system prompt, selected models) |
| `ai_agent_chat_history` | Chat history logs |
| `ai_device` | Registered devices |

### Debugging server issues

```bash
# View last 100 logs
docker logs --tail=100 xiaozhi-esp32-server

# Real-time log monitoring (press BOOT on ESP32 while watching)
docker logs -f xiaozhi-esp32-server

# Filter for errors
docker logs --tail=200 xiaozhi-esp32-server 2>&1 | grep -iE "error|exception|fail"

# Check model config in DB
docker exec xiaozhi-esp32-server-db mysql -uroot -p123456 \
  -e "USE xiaozhi_esp32_server; SELECT id, model_name, config_json FROM ai_model_config WHERE is_enabled=1;"
```

### Known issues & fixes

| Symptom | Root Cause | Fix |
|---------|-----------|-----|
| LCD shows "Speaking..." but no text | `sentence_start` event not sent — LLM or TTS config broken | Check server logs for LLM/TTS errors |
| `GenerativeModel.generate_content() got unexpected keyword argument 'timeout'` | Server code passes `timeout` directly; SDK expects `request_options` | Fix in container: `sed -i 's/timeout=self.timeout/request_options={"timeout": self.timeout}/' /opt/xiaozhi-esp32-server/core/providers/llm/gemini/gemini.py` then restart |
| `Edge TTS请求失败: Invalid voice ''` | TTS `voice` field empty in config_json | Set voice to `en-US-AriaNeural` via web console or SQL |
| `Gemini 代理设置失败: HTTP 和 HTTPS 代理都不可用` | API URL put in proxy field instead of leaving empty | Clear `http_proxy` and `https_proxy` fields |
| `unexpected model name format` / quota = 0 | `gemini-2.0-flash` deprecated by Google | Switch to `gemini-2.5-flash` |
| `402 Insufficient Balance` (DeepSeek) | DeepSeek account has no credits | Top up at platform.deepseek.com (~$2 minimum) |
| `LLM 的 API key 未设置,当前值为: 你的api_key` | Default placeholder API key not replaced | Edit model and set real API key |
| Chat history shows user messages but no AI responses | LLM/TTS failing silently, no response generated | Fix LLM + TTS config first |

---

## Git History

```
<new>    Update theme + docs: Gemini 2.5 Flash, DeepSeek backup, server debug guide
4edd5bc Rebrand to AITeacher: English UI, ILI9341 driver, updated GPIO pinout
4544756 Add custom English Teacher AI board (ILI9341 128x160 + INMP441 + MAX98357A)
05f1a03 add waveshre ESP32-Touch-LCD-3.5 (#1794)  ← upstream xiaozhi-esp32
```

---

## Server Code Patches (applied inside Docker container)

These patches are needed for xiaozhi-esp32-server v0.9.1 and may be fixed in future versions.

### Gemini timeout parameter fix
```bash
docker exec xiaozhi-esp32-server sed -i \
  's/timeout=self.timeout/request_options={"timeout": self.timeout}/' \
  /opt/xiaozhi-esp32-server/core/providers/llm/gemini/gemini.py
docker restart xiaozhi-esp32-server
```

> **Note:** These changes are lost if the container is recreated (`docker compose up --force-recreate`). Re-apply after recreating.

---

## Cost Estimation (for production deployment)

### Per 100 users (~50 conversations/day each)

| Component | Monthly Cost | Notes |
|-----------|-------------|-------|
| LLM (Gemini 2.5 Flash paid) | ~$8-15 | $0.15/1M input, $0.60/1M output |
| LLM (DeepSeek as backup) | ~$15-30 | $0.27/1M input, $1.10/1M output |
| TTS (EdgeTTS) | $0 | Free Microsoft service |
| ASR (FunASR local) | $0 | Runs on server CPU |
| Server (electricity) | ~$10-20 | Home Xeon 24/7 |
| **Total** | **~$20-50/month** | |
