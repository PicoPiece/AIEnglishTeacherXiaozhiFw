# English Teacher AI - Future Features Roadmap

## Priority 1: Parent Voice Messages & Personalization

### 1a. Parent Messages via Dashboard

Parents send messages to the child's device through the service dashboard.

**Text Messages**
- Type and send text messages from dashboard
- Device receives and reads aloud via TTS
- Use cases: quick reminders, homework notes, short encouragements
- Supports scheduled delivery

**Voice Messages**
- Parents record audio directly on dashboard/mobile app
- Device receives and plays back parent's real voice
- Use cases: birthday wishes, goodnight messages, emotional encouragement
- Playback history so child can re-listen favorite messages

**Why first**: Text for convenience, voice for emotion.
A TTS robot saying "happy birthday" cannot compare to Mom's actual voice.

### 1b. Personalized Emotional Companion

AI knows the child's name, parents' names, and family context.

- Configure child name, parent names (Ba/Me) via dashboard
- AI references parents naturally ("Ba Minh se rat vui khi nghe con hoc gioi")
- Motivational and comforting conversations
- Age-appropriate emotional support dialogues
- Storytelling mode with familiar family context

**Why together**: Both features make the device feel personal, not generic.

### Technical notes

- Firmware: Messages app already exists, extend to support audio playback
- Server: audio upload API + push via WebSocket/MQTT
- Dashboard: audio recorder widget + scheduling UI
- Device storage: cache recent voice messages on SD card

---

## Priority 2: Multi-Subject Learning Expansion

Expand beyond English to support other school subjects.

- Mathematics (mental math, problem solving)
- Science (fun facts, experiments explanation)
- Vietnamese language arts
- History and geography
- Configurable subject focus per child's grade level
- Subject-specific AI personas and teaching styles
- Menu on device to switch subjects

**Why second**: Multiplies the device's value with relatively low effort (mostly server-side prompt changes + device menu).

---

## Priority 3: Course Material & Knowledge Base Integration

Connect to NotebookLLM or custom knowledge bases uploaded by parents.

- Parents upload course materials, textbooks, study guides via dashboard
- AI uses uploaded content as context for teaching and Q&A
- Support PDF, text, audio materials
- Linked with NotebookLLM for structured learning paths
- Progress tracking and quiz generation from uploaded content

**Why third**: Highest educational value but requires complex RAG pipeline backend.

---

## Priority 4: USB File Transfer Support

Enable USB connectivity for direct file management.

- Copy music files (MP3) to SD card via USB
- Transfer course materials and documents
- Firmware update via USB
- No need for SD card reader — plug device directly into computer

**Why last**: Convenience feature. Removing SD card still works as a workaround.
