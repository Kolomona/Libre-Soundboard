## Desktop Soundboard App: Tech Stack & Architecture

### Requirements
- Native Linux desktop app
- Grid-based sound containers with drag-and-drop (add/rearrange sounds)
	- Must support dragging files from file managers such as Thunar and Dolphin
	- Number keys 1-0 on the keyboard trigger play for the first 10 sounds in the first tab only
	- Rearrangement limited within a tab; copy/cut/paste (Ctrl+C/X/V) for moving sounds between tabs
- Each sound container: waveform, play button, volume knob
	- Alert icon if referenced file is missing; clicking can offer to locate/replace the file
- Multiple soundboards via tabs
- Maximum 32 simultaneous sounds
- Must work with JACK (jackd) for routing audio to DAWs (e.g., Ardour)
- Minimal latency, faithful playback of WAV/MP3 at any sample rate

### Recommended Tech Stack

- **UI Framework:**
	- Qt (C++) — native, performant, excellent drag-and-drop and custom widget support

- **Audio Backend:**
	- JACK API (C/C++) — direct integration for low-latency audio and routing

- **Audio File Decoding:**
	- libsndfile — for WAV and uncompressed formats
	- libmpg123 or FFmpeg/libav — for MP3 decoding
	- libsamplerate — for real-time sample rate conversion

- **Waveform Visualization:**
	- Custom Qt widgets or QCustomPlot

### Key Considerations
- Use libsamplerate to resample audio files to match JACK server sample rate
- Buffer audio efficiently and use JACK’s callback mechanism for real-time playback
- Expose each soundboard output as a separate JACK port for flexible DAW routing


### Project Decisions

- **Build System:** CMake — cross-platform, easy for agents to edit, widely supported
- **Testing:** Catch2 — header-only, simple syntax, ideal for personal/agent-driven projects
- **Documentation:** Doxygen-style comments for all classes and functions
- **Code Style:** clang-format for consistent, automated code formatting

### Example Project Structure

```
/src
	main.cpp
	MainWindow.h/cpp
	SoundContainer.h/cpp
	AudioEngine.h/cpp   // JACK integration
	WaveformWidget.h/cpp
	AudioFile.h/cpp     // Decoding, resampling
/resources
	icons/
	sounds/
```

---
This stack ensures native performance, JACK routing, support for multiple audio formats/sample rates, and minimal latency for professional audio workflows.

### Main Window

- Standard desktop window with a menu bar (File: Open, Save, Exit, Recent Soundboards; Edit: Undo, Redo, Preferences; Help: About, Documentation)
- Window should be resizable
- SoundContainers should be responsive and adapt to window size, similar to a modern web app grid layout
### Audio Playback

- Multiple sounds can play simultaneously (up to 32)
- Per-SoundContainer volume and playback controls
- Global stop/pause controls
- Master output volume control (not a separate JACK port)
### Persistence

- Save/load soundboard layouts and user settings manually (no autosave)
- Use JSON format for all persistence
### Error Handling & Feedback

- Log errors to file
- Show status bar messages for user feedback
- Critical errors (e.g., JACK not running) show dialogs
- Alert icon in SoundContainer if referenced file is missing
### Testing Approach

- Unit, integration, and UI/functional tests should be included
- Automated UI tests are preferred for agent-driven development
### Extensibility

- Modular audio engine for future plugin support (LV2 desirable, not required for MVP)
### Performance Constraints

- Limit of 32 maximum simultaneous sounds
### Additional Design Notes

- Number key shortcuts only apply to the first tab
- No additional keyboard shortcuts for now
- Undo/redo should cover all reasonable actions (add/remove/rearrange sounds, volume changes, etc.)
- Grid auto-adjusts columns/rows as the window resizes
- Waveform previews may be generated asynchronously
- Support additional formats (FLAC, OGG, etc.) if libraries are available

### Build & Run (Developer)

- The project uses modern CMake with a `build/bin` runtime output.
- To configure, build, and run locally:

```bash
mkdir -p build
cd build
cmake .. -DBUILD_TESTING=ON
cmake --build . -- -j$(nproc)
./bin/libresoundboard
```

- Run tests with CTest:

```bash
cd build
ctest --output-on-failure
```

- Formatting target (if `clang-format` is installed):

```bash
cmake --build . --target format
```

### Developer Infrastructure Notes

- Catch2 is integrated via CMake FetchContent (v2.13.10) for unit tests.
- Build artifacts are placed under `build/bin` (executable: `build/bin/libresoundboard`).
- A simple `format` target runs `clang-format` over source files when available.
- Packaging support is prepared via `CPack` (no generator selected by default).

If you want I can add AppImage/DEB packaging, CI scripts, or extend the test suite next.
