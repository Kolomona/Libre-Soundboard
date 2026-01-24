# LibreSoundboard Preferences Implementation Plan

## Problem Statement
LibreSoundboard has 10 key user-configurable preferences currently hardcoded or inaccessible, plus new keep-alive controls. Implement a VLC-style preferences dialog with tree-based navigation to allow persistent user configuration across 8 categories: Audio Engine, Grid & Layout, Waveform Cache, File Handling, Keyboard & Shortcuts, Startup, Debug, and Keep-Alive.

---

## Phase 1: Preferences Infrastructure & Dialog Scaffold
**Goals:** Create foundation for all preferences (settings persistence, dialog structure, tree widget)

**Tasks:**
- [x] Create `PreferencesManager` class to abstract QSettings and provide global access
- [x] Create `PreferencesDialog` with tree widget on left (empty categories), settings panel on right
- [x] Create page widget base class for individual preference pages
- [x] Wire Preferences menu action to open dialog
- [x] Ensure dialog has Cancel/Reset/Save buttons (non-functional reset for now)
- [x] Create category pages: AudioEngine, GridLayout, WaveformCache, FileHandling, KeyboardShortcuts, Startup, Debug, KeepAlive (all empty for now)
- [x] Verify CMakeLists.txt includes new files, build succeeds

**Notes:** Dialog should not apply settings on Cancel. Tree should show all categories but pages can be empty stubs.

**For human Testing:**
- Open Edit → Preferences
- Verify dialog opens with tree categories on left, empty panel on right
- Click each category in tree, verify panel switches but is empty
- Click Cancel, verify no settings are saved
- Verify build completes without errors

---

## Phase 2: Simple Scalar Preferences (Cache & Audio)
**Goals:** Implement Cache Size Limit, Cache TTL, Default Gain Level, Logging Level

**Tasks:**
- [x] Implement WaveformCache preferences page with spinboxes: Cache Size Limit (MB), Cache TTL (days)
- [x] Implement AudioEngine preferences page with spinbox: Default Gain Level (0.0-1.0 with step 0.1)
- [x] Implement Debug preferences page with combobox: Logging Level (Off, Error, Warning, Info, Debug)
- [x] PreferencesManager loads/saves these from QSettings on init and apply
- [x] Save button applies all preferences, persists to QSettings
- [x] MainWindow reads default gain from PreferencesManager when creating new SoundContainers
- [x] WaveformCache::evict() uses PreferencesManager values instead of hardcoded
- [x] DebugLog respects logging level setting
- [x] Build succeeds

**Notes:** Use QSpinBox for numeric, QComboBox for logging level. Reset button should reload from QSettings.

**For human Testing:**
- Open Preferences → Waveform Cache, set Cache Size to 100 MB, Cache TTL to 30 days
- Open Preferences → Audio Engine, set Default Gain to 0.5
- Open Preferences → Debug, set Logging to Info
- Click Save, close dialog
- Reopen Preferences, verify values persisted
- Create new SoundContainer via grid, verify initial volume matches setting
- Verify waveform cache eviction uses configured limits
- Verify build succeeds

---

## Phase 3: Keep-Alive Preferences
**Goals:** Add keep-alive settings page and wire to KeepAliveMonitor/MainWindow status

**Tasks:**
- [ ] Implement KeepAlive preferences page with: Enable toggle (default on), Silence timeout (seconds, default 60), Sensitivity threshold (dBFS slider/spinbox with “Any non-zero” option, default -60 dBFS)
- [ ] Add trigger target selector: default = Last tab, last loaded sound; allow Specific slot (tab + slot); do not implement Specific tab
- [ ] Add volume policy: use slot volume or override volume slider; include “Play test” button to audition with override volume
- [ ] Add auto-connect keep-alive input option (JACK input auto-connect)
- [ ] When keep-alive enabled, show status bar text "KeepAlive: Active"; hide when disabled
- [ ] PreferencesManager persists keep-alive settings; Save/Reset/Cancel wiring; build succeeds

**Notes:** Keep defaults matching current behavior: enabled, 60s, -60 dBFS, target last tab/last sound, no cooldown, slot volume.

**For human Testing:**
- Open Preferences → Keep-Alive, confirm defaults shown
- Toggle Enable off: status bar text disappears; on: shows "KeepAlive: Active"
- Set timeout to 30s, sensitivity to -50 dBFS, target = specific slot, set override volume via slider, click Play test to hear audition
- Click Save, close dialog, reopen, verify settings persisted
- With keep-alive enabled, leave input silent and wait for trigger; verify selected target plays at chosen volume
- Verify build succeeds

---

## Phase 4: File/Path Preferences (Directories)
**Goals:** Implement Cache Directory, Default Sound Directory

**Tasks:**
- [ ] Implement FileHandling preferences page with file picker button: Default Sound Directory
- [ ] Implement WaveformCache preferences page with file picker button: Cache Directory
- [ ] Add validatePath() to PreferencesManager (ensure directory exists, is writable)
- [ ] MainWindow file dialogs use PreferencesManager::defaultSoundDirectory() as initial dir
- [ ] WaveformCache uses PreferencesManager::cacheDirectory() instead of hardcoded path
- [ ] Reset button populates file paths with defaults from code
- [ ] Build succeeds

**Notes:** File pickers use QFileDialog::getExistingDirectory(). Show error if chosen path is invalid.

**For human Testing:**
- Open Preferences → File Handling, click "Browse" for Default Sound Directory
- Navigate to a folder with audio files (e.g., ~/Music), click Select
- Open Preferences → Waveform Cache, click "Browse" for Cache Directory
- Navigate to a custom location (e.g., ~/cache/libre-soundboard), click Select
- Click Save, close dialog
- Reopen Preferences, verify paths are shown
- Close Preferences, open a file dialog from the app, verify it opens in configured directory
- Verify cache files write to configured cache directory
- Verify build succeeds

---

## Phase 5: Grid Dimensions Configuration
**Goals:** Allow users to customize rows/columns per tab

**Tasks:**
- [ ] Implement GridLayout preferences page with spinboxes: Rows (2-8), Columns (4-16)
- [ ] Add applySetting callback to PreferencesDialog that notifies MainWindow of grid changes
- [ ] MainWindow::onGridDimensionsChanged() reconstructs tabs with new grid layout
- [ ] Preserve sound data during reconstruction (create mapping of old → new positions)
- [ ] Default is 4 rows × 8 columns
- [ ] Reset uses default 4×8
- [ ] Build succeeds

**Notes:** Grid reconstruction is complex; use signal/slot between PreferencesDialog and MainWindow. Warn user that sound assignments may shift position during reconstruction.

**For human Testing:**
- Load some sounds into grid slots
- Open Preferences → Grid & Layout
- Change rows to 6, columns to 10
- Click Save
- Verify grid reconstructs with new dimensions
- Verify loaded sounds are preserved in grid (may be repositioned)
- Open file manager, load sound into a container
- Reopen Preferences, verify rows/columns show configured values
- Change back to 4×8, Save, verify grid returns to original
- Verify build succeeds

---

## Phase 6: JACK Connection Settings
**Goals:** Implement custom JACK client name, auto-connect to outputs

**Tasks:**
- [ ] Implement AudioEngine preferences page with text field: Custom JACK Client Name (default "libre-soundboard")
- [ ] Implement AudioEngine preferences page with checkbox: Auto-connect to system outputs
- [ ] AudioEngine uses PreferencesManager values in init() to configure JACK client
- [ ] PreferencesManager provides jackClientName(), jackAutoConnect()
- [ ] Restart audio engine on preference change (stop, shutdown, re-init with new settings)
- [ ] Build succeeds

**Notes:** JACK client name and auto-connect must be set before jack_client_open(). Changes require engine restart.

**For human Testing:**
- Verify JACK is running (command: `jack_lsp`)
- Open Preferences → Audio Engine
- Change JACK Client Name to "vibe-test"
- Check "Auto-connect to system outputs"
- Click Save
- Use `jack_lsp` to verify client name changed
- Use `jack_lsp vibe-test` to verify outputs are connected
- Restart app, verify settings persist and connections restored
- Verify build succeeds

---

## Phase 7: Keyboard Shortcuts (Slots & Global)
**Goals:** Allow per-slot hotkeys and system-wide shortcuts

**Tasks:**
- [ ] Create ShortcutsManager class to manage slot hotkeys and global shortcuts
- [ ] Implement KeyboardShortcuts preferences page with table: Slot # → QKeySequence editor
- [ ] Implement KeyboardShortcuts preferences page with table: Action (Play, Stop, etc) → QKeySequence editor
- [ ] PreferencesManager delegates shortcut storage/retrieval to ShortcutsManager
- [ ] MainWindow/SoundContainer register shortcuts on init, refresh on preference change
- [ ] Global shortcuts require platform-specific registration (use QHotkey or similar if available, else stub)
- [ ] Validate no duplicate shortcuts
- [ ] Build succeeds

**Notes:** Slot shortcuts trigger container play. Global shortcuts require external library or platform API. Consider initial scope: local shortcuts only (work when app has focus). Warn user about conflicts with system shortcuts.

**For human Testing:**
- Open Preferences → Keyboard & Shortcuts → Slot Shortcuts
- Assign hotkey (e.g., Ctrl+1) to slot 0, (e.g., Ctrl+2) to slot 1
- Click Save, close dialog
- Click on a sound container to focus it
- Press assigned hotkey, verify sound plays
- Verify no duplicate shortcuts are accepted
- Close app, reopen, verify hotkeys persist
- Verify build succeeds

---

## Phase 8: Startup Behavior
**Goals:** Allow restore-last-session or start-empty on app launch

**Tasks:**
- [ ] Implement Startup preferences page with radio buttons: "Restore last session" (default) or "Start with empty boards"
- [ ] MainWindow::onStartup() checks PreferencesManager::startupBehavior()
- [ ] If restore: load existing layout (current behavior)
- [ ] If start-empty: skip layout load, show empty grid
- [ ] Build succeeds

**Notes:** Startup behavior is read on MainWindow construction. No need to restart app to test.

**For human Testing:**
- Load sounds into several slots
- Open Preferences → Startup, select "Start with empty boards"
- Click Save, close dialog
- Close and restart app
- Verify all boards are empty (no sounds loaded)
- Open Preferences → Startup, select "Restore last session"
- Click Save, close dialog
- Close and restart app
- Verify previous sounds are restored in their slots
- Verify build succeeds

---

## Final Validation
- [ ] All 8 phases complete
- [ ] Full build succeeds (make clean && make)
- [ ] No compiler warnings related to preferences
- [ ] QSettings stores/retrieves all preferences correctly
- [ ] Dialog persists across sessions
- [ ] Reset button restores all defaults
- [ ] Cancel discards unsaved changes
