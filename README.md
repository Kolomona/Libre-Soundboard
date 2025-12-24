# LibreSoundboard

LibreSoundboard is a native Linux desktop soundboard application implemented in C++ using Qt6. It focuses on low-latency audio playback and flexible routing via JACK, with an intuitive grid-based UI for loading and triggering sounds.

**Note: This project has been entirely vibe coded by AI so it likely has a lot of bugs in it** 🙂

## Reasoning

This app was born out of my frustration for not being able to find a usable soundboard that works with JACK.

For you windows users there are tons of great soundboard apps for you to choose from. Therefore I will NOT be making this app cross platform.

This app is specifically made for me and my purposes. If it works for you then great, but it's really supposed to work for me 🙂

**What the project does**

- Provides a tabbed soundboard UI where each tab contains a grid of sound slots (`SoundContainer` widgets).
- Supports drag-and-drop from file managers and internal drag/copy between slots.
- Displays waveform previews (generated asynchronously) and per-slot volume and play/stop controls.
- Integrates with JACK for low-latency output; when JACK is unavailable the UI still runs and audio features are disabled with a status notice.
- Decodes WAV and MP3 (via `libsndfile` and `libmpg123`) and performs sample-rate conversion with `libsamplerate` when needed.

**Completed (implemented in this repository)**

- Qt6-based native desktop UI with tabbed boards and 4×8 grids (32 slots per tab).
- Drag-and-drop and internal move/copy semantics for `SoundContainer` widgets.
- Waveform generation and a waveform cache with eviction controls.
- JACK-based audio engine integration and a mixing/playback backend with per-voice gain control.
- Persistence of board layouts and per-slot state (JSON saved to the user's config directory).
- Single-instance handling using a lock file and UNIX-domain socket (`/tmp/libresoundboard_instance.sock`).
- Unit-test infrastructure using Catch2 (tests located under `tests/` and runnable via CTest).

**Planned / TODO**

- Add optional packaging manifests (Debian/`.deb`, AppImage, or a Flatpak recipe).
- Expand automated UI/integration tests.
- Add optional plugin support (LV2) and broader codec support (FLAC/OGG) if libraries are available.
- Provide CI workflows for building, testing, and packaging.

## Requirements

Minimum development requirements to build from source (Debian/Ubuntu example):

- `cmake` (>= 3.18)
- `build-essential` / a C++ compiler (g++/clang)
- `pkg-config`
- Qt6 development packages (Qt6 widgets, Qt6 network, etc.)
- `libjack-jackd2-dev` (optional at runtime — app runs without JACK but audio is disabled)
- `libsndfile1-dev`
- `libmpg123-dev`
- `libsamplerate0-dev`
- `clang-format` (optional, for formatting target)

On Debian/Ubuntu these can be installed with:

```bash
sudo apt update
sudo apt install cmake build-essential pkg-config qtbase6-dev libjack-jackd2-dev \
	libsndfile1-dev libmpg123-dev libsamplerate0-dev clang-format
```

## Build & Run (Developer)

From the project root:

```bash
mkdir -p build
cd build
cmake .. -DBUILD_TESTING=ON
cmake --build . -- -j$(nproc)
./bin/libresoundboard
```

Notes:

- If JACK is not running or development headers/libraries are not available at build time, the project will still build but audio features that rely on JACK will be disabled.
- The runtime executable is produced at `build/bin/libresoundboard` by default.

## Tests

Run unit tests from the `build` directory with:

```bash
ctest --output-on-failure
```

Catch2 is integrated by CMake via FetchContent; no system-wide Catch2 package is required.

## Debugging & Logging

- Debug logging to a file is opt-in. To enable Qt debug/info messages to a file, set the environment variable `LIBRE_WAVEFORM_DEBUG_LOG_PATH` to a writable path before launching the app. Example:

```bash
export LIBRE_WAVEFORM_DEBUG_LOG_PATH=/tmp/libresoundboard-debug.log
./build/bin/libresoundboard
```

- Single-instance and launcher logs are written to the user data directory (e.g., `~/.local/share/libresoundboard/instance.log`) to aid in diagnosing launcher-related issues.

## Where to look in the code

- UI and application entry: `src/MainWindow.*`, `src/main.cpp`
- Sound slot widget: `src/SoundContainer.*`
- Audio engine and playback: `src/AudioEngine.*`, `src/AudioEnginePlay.*`
- Waveform generation and cache: `src/WaveformWorker.*`, `src/WaveformRenderer.*`, `src/WaveformCache.*`
- Single instance helper: `src/SingleInstance.*`
- Debug logging: `src/DebugLog.*`



