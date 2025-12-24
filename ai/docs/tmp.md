AppImage release steps (agent-friendly, concise)

Variables (set per-run):
- APP: executable base name (e.g. libresoundboard)
- VERSION: semver tag (e.g. v1.2.3)
- BUILD_DIR: build output dir (e.g. build)
- APPDIR=AppDir
- OUT_DIR: output artifacts dir

High-level flow:
1. Trigger: run on annotated tag push or Release event.
2. Build: native build (CMake) -> produce `BUILD_DIR/bin/$APP` (Release).
3. Create AppDir layout:
   - mkdir -p $APPDIR/usr/bin $APPDIR/usr/share/applications $APPDIR/usr/share/icons
   - copy binary -> $APPDIR/usr/b/$APP
   - copy icons, .desktop -> $APPDIR/usr/share/{applications,icons}
   - copy runtime resources (icons, sounds) as needed.
4. Bundle Qt/runtime: use `linuxdeployqt` or `appimage-builder`:
   - linuxdeployqt: `linuxdeployqt $APPDIR/usr/bin/$APP -appimage` (collects Qt plugins/libs).
   - OR appimage-builder: supply recipe.yml that maps files and runs linuxdeployqt internally.
5. Produce AppImage:
   - If linuxdeployqt used with `-appimage`, it may call appimagetool and output `$APP-$VERSION.AppImage`.
   - Otherwise run `appimagetool $APPDIR $OUT_DIR/$APP-$VERSION.AppImage`.
6. Metadata + integrity:
   - Generate checksum: `sha256sum $OUT_DIR/$APP-$VERSION.AppImage > $OUT_DIR/$APP-$VERSION.sha256`
   - (Optional) GPG sign: `gpg --batch --yes --detach-sign -u $GPG_KEY --output $OUT_DIR/$APP-$VERSION.AppImage.sig $OUT_DIR/$APP-$VERSION.AppImage`
   - (Optional) generate AppImage update-info (AppImageUpdate) and embed if desired.
7. Publish artifacts:
   - Create or update GitHub Release for tag $VERSION.
   - Upload AppImage, .sha256, .sig as release assets (use `gh` CLI or CI action `actions/upload-release-asset`).
8. Smoke test (CI): run AppImage in minimal container (xvfb if UI required) to ensure startup.

Secrets / CI prerequisites:
- GITHUB_TOKEN for Release API (CI-managed)
- GPG private key (if signing) loaded as secret; import in CI and set `GPG_KEY`.
- Use official docker image or action containing linuxdeployqt/appimagetool or install them in CI.

CI considerations (concise):
- Run on ubuntu-latest (or dedicated runner) for x86_64 AppImage.
- For Qt apps prefer linuxdeployqt action/image that matches Qt6.
- Build matrix only if multi-arch (arm64) required; AppImage is per-arch.
- Keep AppDir assembly declarative (appimage-builder recipe) where possible.
## Decisions Made

- Trigger: GitHub `release` event (maintainer publishes Release).
   - Explanation: CI builds on tag push but does not publish assets. The release event is the explicit, maintainer-controlled publish gate to avoid accidental releases.
   - Recommended: keep Release event as chosen — reduces accidental publishes and fits local signing workflow.

- Build location: CI runner (GitHub Actions `ubuntu-latest`).
   - Explanation: use hosted CI to produce reproducible builds and store artifacts for maintainer consumption.
   - Recommended: CI builds on tag push and stores unsigned artifacts as job artifacts.

- Architecture: x86_64 only.
   - Explanation: produce a single x86_64 AppImage. Multi-arch increases complexity and runner requirements.
   - Recommended: x86_64 for initial releases; add arm64 later if demand arises.

- Packaging tool: `linuxdeployqt -appimage` (use linuxdeployqt to create the final AppImage).
   - Explanation: linuxdeployqt collects Qt libs/plugins and can produce an AppImage directly.
   - Recommended: linuxdeployqt for Qt6 apps; it simplifies plugin inclusion.

- App binary path (CI): `bin/libresoundboard`.
   - Explanation: CI should produce/copy the final executable to this path inside the build output so AppDir assembly is simple.

- `.desktop` and icons (repo paths): `src/resources/app.desktop` and icons under `src/resources/icons/`.
   - Explanation: CI will copy these files into the AppDir's `usr/share/applications` and `usr/share/icons` respectively.

- Include runtime sounds/resources in AppImage: No — do not bundle sounds (keep AppImage smaller).
   - Explanation: sample sounds will not be embedded in the AppImage; they can be provided separately or downloaded by users.

- Artifact naming: `$APP-$VERSION+$COMMIT.AppImage` (include commit short, 7 chars).
   - Explanation: include commit for traceability while keeping version visible.
   - Recommended: use this format to link artifacts to exact commit builds.

- Integrity: publish SHA512 checksums.
   - Explanation: higher-strength checksum (SHA512) for artifact integrity.
   - Recommended: publish SHA512; also provide a single checksum asset.

- Signing: Local GPG detached signatures (`.asc`) created by maintainer.
   - Explanation: private key remains offline; maintainer creates detached signatures and uploads them to Release.
   - Recommended: local signing as chosen — secure for now.

- GPG key ID to reference in docs/release notes: `59386B953C5B1584`.
   - Explanation: list this key ID so users can verify the `.asc` signatures.

- CI artifact retention: 90 days.
   - Explanation: maintainers need time to retrieve artifacts before signing and uploading.

- CI artifact storage: GitHub Actions job artifacts (recommended).
   - Explanation: artifacts accessible to maintainers via the Actions UI or `gh` CLI.

- Source artifact: include a `source-$VERSION+$COMMIT.tar.gz` alongside the AppImage.
   - Explanation: source tarball aids reproducibility and audits.

- Smoke test: run `$APP.AppImage --version` / `--help` only (fast).
   - Explanation: quick startup test to detect gross failures; full UI tests optional later.

- Target distro for testing: Ubuntu 22.04.
   - Explanation: common LTS base for many users; validate on clean images.

- Qt plugins to verify included: `platforms`, `imageformats`, `iconengines`, `platformthemes`.
   - Explanation: missing Qt plugins are the most common runtime failure; linuxdeployqt will include many automatically but verify explicitly.

- Update mechanism: no auto-update (manual updates only for initial releases).
   - Explanation: skip embedding update-info for now to reduce infra complexity.

- Release hosting: GitHub Releases only.
   - Explanation: central and integrated with tags; other mirrors can be added later.

- Verification guidance: provide short commands to verify signature + checksum in Release notes (e.g. `gpg --verify`, `sha512sum -c`).

- Automation trust model: conservative (CI builds, maintainer downloads & local-signs, maintainer uploads signed assets).
   - Explanation: prioritize private-key security and human review for initial releases.

- Rollback: revoke release and publish fix (delete assets), then publish corrected release.
   - Explanation: simple and immediate mitigation for broken artifacts.

- Docs updates: manual updates (do not auto-update README/docs on release).
   - Explanation: keep docs changes explicit and reviewable.

- Future automation: no plan to adopt fully automated signing now (keep local-signing for the foreseeable future).
   - Explanation: preserves private key security; revisit later.

CI role (concise recap):
- CI builds on tag push, stores unsigned artifacts with 90-day retention. Maintainer downloads, verifies, signs locally, and uploads signed assets + checksum to the GitHub Release (publish event).

Security note: do NOT store the private signing key in CI. If future automation is desired, prefer sigstore/cosign (OIDC) or cloud KMS with short-lived credentials.
   - Explanation: simple and immediate mitigation for broken artifacts.
   - Recommended: revoke and re-release with fix.

- Notifications: repository watchers only (default).
   - Explanation: rely on GitHub notifications and manual channels as needed.
   - Recommended: default notification behavior.

- Docs updates: manual updates (do not auto-update README/docs on release).
   - Explanation: keep docs changes explicit and reviewable.
   - Recommended: update docs manually.

- Future automation: no plan to adopt fully automated signing now (keep local-signing forever).
   - Explanation: preserves private key security; revisit later.
   - Recommended: keep local signing; consider sigstore when/if automation is required.

CI role (concise recap):
- CI builds on tag push, stores unsigned artifacts with long retention. Maintainer downloads, verifies, signs locally, and uploads signed assets + checksum to the GitHub Release (publish event).

Security note: do NOT store the private signing key in CI. If future automation is desired, prefer sigstore/cosign (OIDC) or cloud KMS with short-lived credentials.




## Proccess (to be filled on once the Decisions Made section is complete)