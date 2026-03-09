# English Teacher AI - Firmware Changelog

Branch: `hdang/PicoAIEng`
Board: `english-teacher-ai` (ESP32-S3 + ILI9341 160x128 LCD)

---

## Summary of Changes

All changes are customizations for the English Teacher AI device, optimizing the user experience for a child learning English. The firmware is based on the `xiaozhi-esp32` platform with WeChat-style chat bubble UI.

---

## 1. Single-Turn Conversation Mode

**Commits:** `aaef1a5`, `3cf80da`

**Problem:** The original firmware auto-listened immediately after the AI finished speaking. This gave the child no time to read the response on screen or think before responding.

**Initial attempt:** Changed `GetDefaultListeningMode()` to `kListeningModeManualStop`. This broke the flow because the board uses toggle-click (`ToggleChatState`) rather than push-to-talk (`StartListening`/`StopListening`). In manual mode, the server waits for a `listen stop` message that never arrives, leaving the device stuck in listening state.

**Final fix (two changes in `application.cc`):**

1. **`GetDefaultListeningMode()`** returns `kListeningModeAutoStop` so the server uses VAD (Voice Activity Detection) to automatically detect when the child stops speaking.
2. **TTS stop handler** always transitions to `kDeviceStateIdle` after the AI finishes speaking, regardless of listening mode.

**Flow:**
```
Child presses button → Listening (VAD auto-detects silence)
    → Server ASR → LLM → TTS → Device speaking
    → AI finishes → Idle (screen shows response)
    → Child reads, thinks, presses button when ready
```

**Files:** `main/application.cc`

---

## 2. Disable Emoji Display

**Commit:** `580d769`

**Rationale:** The 160x128 LCD is small. Emoji/emotion icons take up valuable screen space that is better used for conversation text.

**Changes:**

| File | Change |
|------|--------|
| `main/display/lcd_display.cc` | `SetEmotion()` → no-op; `emoji_label_` and `emoji_image_` hidden at startup in `SetupUI()` |
| `main/display/lcd_display.cc` | `ClearChatMessages()` no longer re-shows emoji label |
| `main/application.cc` | Removed all `SetEmotion()` calls from state machine (Idle, Connecting, Listening), LLM response handler, `Alert()`, and `DismissAlert()` |

---

## 3. Preserve Chat Messages on Idle

**Commit:** `580d769`

**Rationale:** After the AI finishes speaking and the device returns to Idle, the child should be able to re-read the conversation. Previously, entering Idle state cleared all chat bubbles and showed "Standby".

**Changes in `HandleStateChangedEvent()` for `kDeviceStateIdle`:**

| Before | After |
|--------|-------|
| `display->SetStatus(STANDBY)` | *(removed)* |
| `display->ClearChatMessages()` | *(removed)* |
| `display->SetEmotion("neutral")` | *(removed)* |
| Audio processing disabled | Audio processing disabled *(unchanged)* |

The chat bubble history now persists across conversation turns. Messages are only removed when they exceed `MAX_MESSAGES` (20), at which point the oldest message is deleted.

---

## 4. Chat Scroll with Volume Buttons

**Commit:** `bbc8ce0`

**Rationale:** With chat messages preserved on the small 160x128 screen, the child needs a way to scroll back through the conversation.

**New API:**

| File | Addition |
|------|----------|
| `main/display/display.h` | `virtual void ScrollChatBy(int dy)` (base class, default no-op) |
| `main/display/lcd_display.h` | Override declaration |
| `main/display/lcd_display.cc` | Implementation using `lv_obj_scroll_by()` on `content_` (WeChat mode only) |

**Button remapping in `english_teacher_ai_board.cc`:**

| Button | Click | Long Press |
|--------|-------|------------|
| Volume Up (GPIO 40) | Scroll up 40px (older messages) | Volume +20 |
| Volume Down (GPIO 39) | Scroll down 40px (newer messages) | Volume -20 |

---

## Files Modified (vs `main` branch)

```
main/application.cc                                  (single-turn mode, no emoji, preserve chat)
main/boards/english-teacher-ai/english_teacher_ai_board.cc  (button scroll remapping)
main/display/display.h                                (ScrollChatBy API)
main/display/lcd_display.h                            (ScrollChatBy override)
main/display/lcd_display.cc                           (ScrollChatBy impl, no emoji, no re-show on clear)
```

---

## Hardware Reference

```
Boot Button    : GPIO 0   → Toggle chat (click to start/stop conversation)
Volume Up      : GPIO 40  → Click: scroll chat up / Long press: volume +20
Volume Down    : GPIO 39  → Click: scroll chat down / Long press: volume -20
Built-in LED   : GPIO 48
Display        : ILI9341 SPI, 160x128, light/dark theme
Audio In       : INMP441 (GPIO 4/5/6)
Audio Out      : MAX98357A (GPIO 7/15/16)
```
