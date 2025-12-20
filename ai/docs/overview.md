## Desktop Soundboard App: Tech Stack & Current Architecture

### Summary (current behavior)
- Native Linux desktop app using Qt6 Widgets (C++).
- Grid-based `SoundContainer` widgets with drag-and-drop to add files and to move/copy containers inside and between tabs.
  - Supports dragging files from file managers (Thunar, Dolphin, etc.) and internal drag between containers.
  - Number keys `1`–`9` and `0` trigger play for the first 10 slots in the first tab.
  - Rearrangement is per-tab; internal drag supports move (left-click drag) and copy (right-button drag).
- Each `SoundContainer` shows a waveform label, a Play button and a horizontal volume slider.
  - Waveform text is left-aligned; the slider expands to fill the container width.
  - Right-click context menu includes `Play` and, when populated, a `Clear` action that resets the slot to the default state.
- Default UI: 4 tabs created at startup, each tab contains 32 slots (4×8 grid).
- Audio is driven by JACK when available; if JACK is not present the GUI still runs and audio is disabled with a status-bar notice.
- Sample-rate conversion uses `libsamplerate` when needed.

### Tech Stack (actual)

- UI Framework: Qt6 (C++) — native, performant, good drag-and-drop and custom widget support.
- Audio Backend: JACK (jackd) via the JACK C API; the project links to JACK when found by CMake.
- Audio File Decoding: `libsndfile` and `libmpg123` are used (CMake checks and links to these); additional format support (FLAC/OGG) can be added if libraries are provided.
- Sample-rate conversion: `libsamplerate` (CMake integration present).
- Waveform rendering: custom Qt widgets (`WaveformWidget`, `WaveformRenderer`) with asynchronous generation via `WaveformWorker`.

### Key Implementation Notes (matches code)

- The UI builds a fixed grid per tab: 4 rows × 8 columns (32 `SoundContainer`s per tab). The default startup creates 4 tabs.
- Number-key shortcuts: keys `1`–`9` map to the first 9 containers in the first tab; `0` maps to the 10th container.
- Persistence: layouts and per-slot state (file path, volume, optional backdrop color) are saved to a JSON file under the user's config location (example: `$XDG_CONFIG_HOME/libresoundboard/layout.json`) and restored on startup.
- Single-instance: the helper uses a lock file in the user's data directory (e.g., `$XDG_DATA_HOME/libresoundboard/libresoundboard.lock`) and a UNIX-domain socket at `/tmp/libresoundboard_instance.sock` to notify an existing instance.

### Debugging & Logging

- Debug logging is opt-in. If the environment variable `LIBRE_WAVEFORM_DEBUG_LOG_PATH` is set before startup, `DebugLog::install(path)` is called and Qt debug/info messages are written to the provided path.
- The `SingleInstance` helper writes instance-related logs to the repo/user data directory (e.g., `~/.local/share/libresoundboard/instance.log`) to aid launcher debugging.

### Testing / Developer Infrastructure

- Build system: CMake (project provides `src/CMakeLists.txt`).
- Unit tests: Catch2 is integrated via CMake (tests live under `tests/`) and can be run with CTest.
- Build output: executable placed at `build/bin/libresoundboard` by default when built from the project root.

### Build & Run (Developer)

```bash
mkdir -p build
cd build
cmake .. -DBUILD_TESTING=ON
cmake --build . -- -j$(nproc)
./bin/libresoundboard
```

Run tests with CTest:

```bash
cd build
ctest --output-on-failure
```

Formatting target (if `clang-format` is available):

```bash
cmake --build . --target format
```

### Packaging & Distribution Notes

- CPack support exists; `.deb`/AppImage/Flatpak packaging can be added but Flatpak requires audio sandboxing considerations for JACK/PipeWire.

If you want, I can apply this cleaned version in the repo (or make additional clarifications — e.g., document exact JSON layout fields, list required system packages for Debian/Ubuntu, or add CI packaging scripts).
