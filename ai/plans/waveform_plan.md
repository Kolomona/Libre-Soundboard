# waveform_plan

Purpose: implement resizable waveform thumbnails per `SoundContainer` with a cheap cached bitmap and a smooth playhead overlay. This plan is written for AI agents; steps are session-sized and executable independently.

Phases (each phase is session-sized; use checkboxes to track progress):

## Phase 1 — Worker infra
- [x] Add a worker abstraction for waveform jobs (enqueue, cancel, status).
- [x] Create a small threadpool or reuse existing app thread pool.
- [x] Define data types: `WaveformJob` (path, desired pixel width, DPR), `WaveformResult` (minmax arrays, duration, sample-rate, channels).
- [x] Provide queued signal from worker -> UI for completion and error.

For human Testing:
- Start the app; verify no UI regressions and that worker code compiles.
- Expect: no visual change. Logs should show worker initialization.

## Phase 2 — Pyramid generator
- [ ] Implement min/max bucket pyramid generator (configurable base bucket size, e.g., 256 samples).
- [ ] Produce multi-resolution levels (each level halves sample resolution by combining min/max pairs).
- [ ] Unit tests: small synthetic buffers -> expected min/max outputs.
- [ ] Expose API to request a specific pyramid level by desired pixel width.

For human Testing:
- Run unit tests for pyramid generator (ctest). Expect tests to pass.
- No UI required.

## Phase 3 — Bitmap renderer & cache interface
- [ ] Implement renderer that converts a pyramid level to a DPR-aware QImage/bitmap at requested width.
- [ ] Define cache key: hash(path + size + mtime + channels + samplerate + DPR + pixel-width).
- [ ] Implement atomic cache write (temp file + rename) and metadata JSON alongside images.
- [ ] On cache load: validate metadata; if invalid, delete and rebuild.

For human Testing:
- Load a sample audio file via the app; expect a cache file to appear in `QStandardPaths::CacheLocation` under `waveforms/`.
- If cache exists, app should load bitmap quickly on subsequent loads.

## Phase 4 — `SoundContainer` integration
- [ ] On `setFile()` enqueue a waveform job for current container width and DPR; return placeholder immediately.
- [ ] On `resizeEvent` or DPR change, enqueue a new render (cancel outstanding if needed).
- [ ] `paintEvent`: blit cached bitmap (if available); draw thin playhead overlay as a separate primitive. Do not re-render bitmap per frame.
- [ ] On `clearRequested` or slot replace: cancel job, clear cached display and update UI.

For human Testing:
- Drag an audio file into a slot. Expect placeholder then thumbnail appears within a few 100ms (depends on file length).
- Clearing slot removes thumbnail.

## Phase 5 — Playhead manager
- [ ] Implement centralized playhead manager with one timer (30 Hz default) that queries `AudioEngine` for current frame/time.
- [ ] Manager maps time -> pixel for each visible `SoundContainer` (use container duration/sample-count from `WaveformResult`).
- [ ] Manager calls `update()` only on containers whose playhead position changed.
- [ ] Expose a small thread-safe API in `AudioEngine` to read current frame/time (or post atomic updates from audio thread).

For human Testing:
- Play audio in a slot. Expect a smoothly moving thin vertical line across the waveform thumbnail.
- CPU usage should remain modest for a single playing sound.

## Phase 6 — Cache eviction & maintenance
- [ ] Implement cache directory scanning and LRU eviction by total bytes (configurable limit, default 200 MiB).
- [ ] Add TTL policy (e.g., remove entries older than 30 days) and a debug menu action to clear cache.
- [ ] Ensure concurrency safe: file locks or single-writer atomic rename pattern.

For human Testing:
- Populate cache by loading many files; ensure total cache size remains below limit and older items get evicted.
- Clear cache action removes cached files.

## Phase 7 — Tests & performance tuning
- [ ] Integration test: run app headless or with test harness to load 32 different files and render thumbnails.
- [ ] Measure CPU and memory; tune base bucket size and DPR sampling strategy for acceptable perf.
- [ ] Add regression tests for cancellation and cache validation.

For human Testing:
- Run the integration test; expect render durations and memory use reported. Adjust parameters if CPU > target.

## Phase 8 — Documentation & agent checklist
- [ ] Add concise design doc in `ai/docs/` describing data layout, cache key format, metadata layout, and worker lifecycle.
- [ ] Include a short agent checklist of session-sized tasks that reference these phases.

For human Testing:
- Verify `ai/docs` contains the design doc and that filenames and keys match the implementation.

---

Notes for AI agents:
- Keep sessions focused: implement one phase (or one subtask) per session. Each phase has small, verifiable outputs (compile, unit tests, cache file, visible thumbnail, moving playhead).
- Prefer worker-first changes: implement background computation and tests before touching UI paint paths.
- Use short commits per subtask and include the phase id in commit message (e.g., "phase-3: add cache writer").
- Do not assume global app state; use defensive checks and cancelation tokens for jobs.

End of plan.
