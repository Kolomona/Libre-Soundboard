## Desktop Soundboard App: Tech Stack & Current Architecture

### Requirements (implemented / current behavior)
- Native Linux desktop app using Qt6 Widgets.
- Grid-based `SoundContainer` widgets with drag-and-drop to add files and to move/copy containers inside and between tabs.
	- Supports dragging files from file managers (Thunar, Dolphin, etc.) and internal drag between containers.
	- Number keys 1-0 trigger play for the first 10 slots in the first tab.
	- Rearrangement is per-tab; internal drag supports move (left-click drag) and copy (right-button drag).
- Each `SoundContainer` shows a waveform label, a Play button and a horizontal volume slider.
	- Waveform text is left-aligned; the slider expands to fill the container width.
	- Right-click context menu includes `Play` and, when populated, a `Clear` action that resets the slot to the default state.
- Multiple soundboards are exposed as tabs; each tab contains 32 slots (4×8 grid by default).
- The app targets a maximum of 32 simultaneous sounds (per design constraint).
- JACK (jackd) integration: app attempts to open a JACK client on startup; if JACK is absent the GUI still runs and audio features are disabled with a status-bar notice.
- Low-latency playback with sample-rate conversion using libsamplerate when needed.

### Recommended Tech Stack

- **UI Framework:**
- **UI Framework:**
	- Qt6 (C++) — native, performant, excellent drag-and-drop and custom widget support

- **Audio Backend:**
- **Audio Backend:**
	- JACK API (C/C++) — direct integration for low-latency audio and routing (AudioEngine)

- **Audio File Decoding:**
- **Audio File Decoding:**
	- libsndfile — for WAV and uncompressed formats
	- libmpg123 or FFmpeg/libav — for MP3 decoding
	- libsamplerate — for sample-rate conversion when file SR != JACK SR

- **Waveform Visualization:**
	- Custom Qt widgets or QCustomPlot

### Key Considerations (current notes)
- The app resamples audio to the JACK server rate using libsamplerate when required.
- AudioEngine uses JACK callbacks for real-time playback; successful JACK connection registers output ports.
- Each sound's playback is identified by its file path so voices can be stopped by id.


### Project Decisions

- **Build System:** CMake — cross-platform, easy for agents to edit, widely supported
- **Testing:** Catch2 — header-only, simple syntax, ideal for personal/agent-driven projects
- **Documentation:** Doxygen-style comments for all classes and functions
- **Code Style:** clang-format for consistent, automated code formatting
- **Build System:** CMake — cross-platform and used throughout the project.
- **Testing:** Catch2 is included via CMake FetchContent; tests live under `tests/` and can be run with CTest.
- **Packaging:** CPack support is present and can be used to create .deb or other installers; developer scripts/examples live in the repo.
- **Style/Docs:** Doxygen-style comments and `clang-format` target remain available.

### Project Structure (current)

```
`/src` contains the main application code including `MainWindow`, `SoundContainer`, `AudioEngine` (JACK client), `AudioFile` decoding/resampling, and utility widgets such as `WaveformWidget`.

`/resources` contains icons and example sound assets used in development.
```

---
This stack ensures native performance, JACK routing, support for multiple audio formats/sample rates, and minimal latency for professional audio workflows.

### Main Window (current behavior)

- Standard desktop window with a menu bar (File, Edit, Help). Edit contains `Undo`/`Redo` actions wired to the app undo/redo dispatcher.
- `performUndo()` / `performRedo()` are dispatchers that handle a variety of `Operation` types (Rename, Swap, CopyReplace, Clear, TabMove).
- Tab reordering is supported via a custom `CustomTabBar` that emits a `tabOrderChanged()` notification when tabs are reordered; `MainWindow` listens and records `TabMove` operations for undo/redo.
- The UI supports resizing and the sound grid auto-adjusts to the expected layout.
### Audio Playback

- Multiple sounds can play simultaneously (up to 32)
- Per-SoundContainer volume and playback controls
- Global stop/pause controls
- Master output volume control (not a separate JACK port)
### Audio Playback (current behavior)

- Multiple sounds can play simultaneously (design limit 32).
- Each `SoundContainer` has per-slot volume and play/stop control; the audio engine exposes voices by file id so they can be stopped individually.
- Global stop is available and status messages show playback/stop events.
### Persistence

- Save/load soundboard layouts and user settings manually (no autosave)
- Use JSON format for all persistence
### Persistence

- Layouts and per-slot state (file path + volume) are saved to a JSON file under the user's config location and restored on startup.
- Tab titles are also persisted and restored.
### Error Handling & Feedback

- Log errors to file
- Show status bar messages for user feedback
- Critical errors (e.g., JACK not running) show dialogs
- Alert icon in SoundContainer if referenced file is missing
### Debugging & Logging

- The app writes runtime debug lines to `/tmp/libresoundboard-debug.log` (also emits qDebug()) — these entries are used during development to trace tab moves, swaps, clears, and undo/redo flow.
- Status bar messages and occasional dialogs are used to notify users of critical failures (e.g., JACK not available).
- `SingleInstance` behavior uses a lock file and a UNIX socket (`/tmp/libresoundboard_instance.sock`) to prevent multiple primary instances.
### Testing Approach

- Unit, integration, and UI/functional tests should be included
- Automated UI tests are preferred for agent-driven development
### Testing

- Unit tests use Catch2 and are available in `tests/` (run with CTest). Integration/UI tests can be added; the current infra supports `BUILD_TESTING=ON` via CMake.
### Extensibility

- Modular audio engine for future plugin support (LV2 desirable, not required for MVP)
### Performance Constraints

- Limit of 32 maximum simultaneous sounds
### Additional Design Notes

- Number key shortcuts only apply to the first tab
- No additional keyboard shortcuts for now
- Undo/redo should cover all reasonable actions (add/remove/rearrange sounds, volume changes, etc.)
### Undo / Redo

- The app records user actions as `Operation` entries and supports undo/redo for:
	- `Rename` (tab rename)
	- `Swap` (swap two containers)
	- `CopyReplace` (copy/overwrite a destination slot)
	- `Clear` (clear a slot back to default)
	- `TabMove` (tab reorder)
- `performUndo()` and `performRedo()` dispatch to small per-operation helpers (e.g., `undoTabMove`, `undoSwap`) for clarity and robustness.
- Grid auto-adjusts columns/rows as the window resizes
- Waveform previews may be generated asynchronously
- Support additional formats (FLAC, OGG, etc.) if libraries are available

### Build & Run (Developer)

- The project uses modern CMake with a `build/bin` runtime output.
- To configure, build, and run locally:
### Build & Run (Developer)

- The project uses modern CMake and places the runtime executable at `build/bin/libresoundboard`.
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
### Packaging & Distribution Notes

- CPack support exists and can produce .deb packages (or other targets) from the CMake install rules. A `.deb` or AppImage is recommended for end users; Flatpak is also viable but requires audio sandboxing considerations for JACK/PipeWire.

If you want I can add AppImage / `.deb` packaging manifests, CI scripts, or extend the test suite next.

If you want I can add AppImage/DEB packaging, CI scripts, or extend the test suite next.
