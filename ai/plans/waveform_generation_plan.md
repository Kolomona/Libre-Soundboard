# waveform_generation_plan

Purpose: generate DPR-aware waveform thumbnails from real audio, cache results, and expose efficient rendering and playhead metadata for `SoundContainer` widgets. File is written for AI agents; tasks are small, deterministic, and session-sized.

Guidelines for agents:
- Work one checkbox at a time. Keep changes minimal and build after each phase.
- Prefer worker-first: produce arrays/pyramid data and tests before UI wiring.
- Use cancellation tokens for long reads and validate cache metadata on load.
- Keep example code minimal (signatures only) and focus on testable outputs.

Phases:

## Phase 1 — Decode stream & job infra
- [x] Add a streaming audio decoder task that yields PCM frames in chunks; support WAV/FLAC/MP3/OGG via libs (libsndfile/ffmpeg/mpg123) or platform decoders.
- [x] Define `WaveformJob { path, pixelWidth, dpr, requestId, cancelToken }` and `WaveformResult { levels: vector<Level>, duration, sampleRate, channels, totalSamples }`.
- [x] Implement worker enqueue/cancel/status API and a single-thread worker loop which emits completion/error signals to UI.
- [x] Unit test: synthetic PCM stream -> expected totalSamples/duration.

For human Testing:
- Build. Run unit tests that validate stream decoding and that cancellation returns quickly.
- Expect: decoder reads correct totalSamples for synthetic files and cancels within ~50ms on request.

## Phase 2 — Base bucket accumulation (single-pass)
- [ ] Implement base-level accumulation: compute per-bucket min/max for a target bucket size derived from `totalSamples / (pixelWidth * dpr)`.
- [ ] Make accumulation stream-friendly: only keep current bucket aggregates and append min/max pairs as finished.
- [ ] Expose progress updates and early completion when cancellation requested.
- [ ] Unit test: synthetic signals (constant, ramp, impulses) -> verify min/max arrays.
- [x] Implement base-level accumulation: compute per-bucket min/max for a target bucket size derived from `totalSamples / (pixelWidth * dpr)`.
- [x] Make accumulation stream-friendly: only keep current bucket aggregates and append min/max pairs as finished.
- [ ] Expose progress updates and early completion when cancellation requested.
- [x] Unit test: synthetic signals (constant, ramp, impulses) -> verify min/max arrays.

For human Testing:
- Run unit tests for base accumulation. Expect arrays length ≈ pixelWidth*dpr and min/max values matching synthetic input.

## Phase 3 — Pyramid builder (multi-resolution)
- [x] Build pyramid levels by combining base-level pairs: next.min = min(a.min,b.min), next.max = max(a.max,b.max).
- [x] Record samplesPerBucket for each level and stop when level bucket <= 1 sample or level count reaches ~ceil(log2(baseBucket)).
- [x] Unit test: small buffers -> check each level correctness.

For human Testing:
- Run pyramid unit tests. Expect deterministic level values and consistent samplesPerBucket metadata.

## Phase 4 — Bitmap renderer & cache format
- [ ] Define cache key and metadata JSON: path + size + mtime + sampleRate + channels + totalSamples + dpr + pixelWidth.
- [ ] Render a level to a DPR-aware `QImage` using min/max arrays: map sample amplitude -> image Y. Keep rendering deterministic and fast.
- [ ] Implement atomic cache write: write to temp file, fsync, rename, write metadata JSON.
- [ ] On load: validate metadata, reject and delete on mismatch.
- [ ] Unit test: render a short synthetic level and compare pixel values at key columns.

For human Testing:
- Build and run a short UI flow to generate cache for a test file. Expect a cache file + metadata in `QStandardPaths::CacheLocation/libresoundboard/waveforms` and quick load on subsequent runs.

## Phase 5 — Worker API integration with `SoundContainer`
- [ ] On `setFile()` enqueue a `WaveformJob` with current widget pixel width and DPR, return placeholder immediately.
- [ ] On `resizeEvent` or DPR change, enqueue new job (cancel outstanding for same `requestId`).
- [ ] On completion: set `m_wavePixmap`, set `m_hasWavePixmap=true`, emit ready signal and register container with Playhead manager (phase 6).
- [ ] Unit test: simulate job completion -> widget receives pixmap and tooltip/filename behavior persists.

For human Testing:
- Drag a small audio file into a slot. Expect placeholder text then a waveform thumbnail, no UI freeze. Resize the slot and expect a re-request and new thumbnail.

## Phase 6 — Playhead manager & authoritative timing
- [ ] Implement `PlayheadManager` with a single QTimer (configurable, default 30Hz). Manager holds weak refs to visible containers and per-container duration/totalSamples metadata.
- [ ] Add `AudioEngine::getPlaybackSnapshot(id)` (lock-free/atomic) that returns currentSamplesPlayed, sampleRate, totalSamples.
- [ ] Manager maps samples -> normalized position: pos = currentSamplesPlayed / totalSamples (use duration or totalSamples/sampleRate as fallback). Notify containers only when pos changes by > epsilon.
- [ ] Unit test: feed synthetic snapshots -> manager notifies only on change and respects cancellation/unregister.

For human Testing:
- Play audio. Expect a smooth thin vertical line across the waveform thumbnail driven by real playback (not wall clock). CPU cost should be modest for 1 active voice.

## Phase 7 — Cache eviction & maintenance
- [ ] Implement LRU eviction by total bytes with configurable soft-limit (default 200 MiB) and TTL (30 days).
- [ ] Add debug menu with "Clear waveform cache" and manual eviction trigger.
- [ ] Ensure atomicity: any writer must use temp file + rename; readers validate metadata before use.

For human Testing:
- Populate cache with many files; run eviction. Expect disk usage ≤ configured limit and older entries removed.

## Phase 8 — Tests, perf tuning, and docs
- [ ] Integration test: headless harness to load N files (e.g., 32) and render thumbnails; measure time and memory.
- [ ] Add regression tests for cancellation, metadata mismatch handling, and cache corruption recovery.
- [ ] Document data layout, cache key, metadata fields, and worker lifecycle in `ai/docs/waveform_plan_agent.md`.

For human Testing:
- Run the integration harness and confirm throughput and memory; ensure tests pass and README/doc is present.


Notes for agents:
- Each checkbox must be independently verifiable (build, unit test, or file present).
- Keep changes local to a phase; do not refactor unrelated modules in the same session.
- Keep example snippets to function signatures only when necessary. Example: `WaveformResult computeBuckets(Decoder&, int pixelWidth, float dpr, CancelToken);`
- Use atomic file writes for cache stability. Validate metadata before trusting cached bitmaps.
- Report `mtime` + `filesize` and, if available, a short hash (first MB) for cache validation.


Selecting work:
- Agents should pick the lowest unchecked phase and complete all its checkboxes in a single session.
- If a checkbox can't be completed (external deps, platform codec missing), fail early and document the blocker.

End of plan.
