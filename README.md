# LibreSoundboard

A native Linux desktop soundboard app with JACK audio routing, built with Qt (C++), supporting drag-and-drop, waveform display, and low-latency playback of WAV/MP3 files.

## Project Structure

- `src/` — Source code (main app, UI, audio engine, etc.)
- `resources/icons/` — App icons
- `resources/sounds/` — Example or bundled sounds
- `CMakeLists.txt` — Project build configuration
- `.clang-format` — Code style configuration

## Build Instructions

1. Install dependencies:
   - Qt6 (Widgets)
   - JACK
   - libsndfile
   - libmpg123
   - libsamplerate
   - CMake >= 3.16
2. Build:
   ```sh
   mkdir build && cd build
   cmake ..
   make
   ```
3. Run:
   ```sh
   ./libresoundboard
   ```

## Testing
- Catch2 will be used for unit tests (to be added in `src/tests/`).

## Code Style & Documentation
- clang-format for code style
- Doxygen-style comments for all classes/functions
