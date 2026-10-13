# External dependency management — xdg-desktop-portal-holonight

Work package: EDM-016
Baseline: `7f09d6bcef64086bf3c7eb8c4eee05017a6f2dae`
Umbrella audit: `38146d8f474317f3f8d72699f60e6d16a71dd165`
Status: Done locally; implementation baseline `926de7ccff8146235a014c52a8da890dcc3b0919`
confirmed remotely and clean locally. Original audit baseline above is preserved.
Ready umbrella checkpoint: `26856ee`. Provider contract: pins from
`888ab1d758b7599b5870524c1dab890e5df0f7a0`.

## Requirements

- Configuration and normal build commands shall never acquire dependencies or prepare source providers.
- `task deps` shall explicitly prepare supplied local providers and check installed external prerequisites.
- Internal providers shall resolve only from approved prefixes; external packages shall resolve from an explicit
  external prefix or normal system locations.
- Enabled dependency-dependent features shall require their dependencies; minimal profiles shall disable them explicitly.
- Where production components use Qt, discovery shall require >=6.11; private Qt components shall match
  the selected public version exactly.
- Where tests use GTest, consumers shall require installed GTest >=1.14 CONFIG and any GMock targets they use.
- Existing public APIs, component switches, install-tree consumers and acceptance coverage shall be preserved.

## Design

Adopt canonical `tooling/modules/xdg-desktop-portal-holonight.json` and `.cmake`, then synchronize bundle 2.0.0 into this repository.
The copied metadata is owned by this standalone checkout. CMake performs authoritative discovery; shared probes
provide prerequisite diagnostics and record resolved versions/paths. Incompatible caches and local provider install
trees are archived before reuse. Package acquisition occurs only in explicit environment preparation. CI switches
build/runtime/release lanes together to qualified immutable images and runs acceptance offline after preparation.

Adopt the standalone bundle without copying provider implementation. Consume installed Files Core/Quick, Search and Qt packages; preserve isolated D-Bus acceptance and manual native acceptance.

## Implementation and verification tasks

- [x] Confirm the exact baseline and canonical handoff before implementation.
- [x] Adopt declarations, prefix helpers, explicit preparation and compatibility signatures.
- [x] Apply the repository-specific discovery changes above.
- [x] Update Task, CI preparation/entrypoints, Dockerfiles, release lanes and contributor instructions.
- [x] Run declaration/CMake/profile consistency checks and offline missing-package regressions.
- [x] Run narrow regressions, clean acceptance, installed-package consumers and required quality checks.
- [x] Inspect complete logs; record commands, versions, features, warnings and manual checks.
- [x] Make a repository-local commit only after local verification; publish only when authorized.

## Implementation record

Bundle 2.0.0 is copied locally with canonical declarations and explicit Files Quick
preparation. Config, Qt, Search, Files are installed in order; tests and Files app
are disabled. Public interfaces, C++23, Qt >=6.11 and version 0.1.0 are preserved.
CMake restricts all internal package discovery, requires GuiPrivate EXACT at public
Qt's version, validates wayland-client/protocols/scanner/XML and quotes generated
protocol commands with VERBATIM. pkg-config protocol paths are read verbatim to
avoid FindPkgConfig splitting spaces into list separators. The private QML module
declares its existing QtQml.Models type dependency for lint metadata.
Debug/test/Release presets and Task commands are
standalone. Old check scripts delegate to canonical strict tooling.

CI adapts Files' qualified launcher conventions to Portal's four lanes, omitting
Files-specific runtime/isolation work. Explicit preparation is the sole acquisition
operation; full preparation content and mode hashes reject stale/tampered inputs.
Snapshots preserve HEAD and the current tracked/untracked input inventory. All
acceptance inputs are read-only; caller UID/GID and private runtime directories
preserve the isolated bus and staged activation tests. Release verifies the exact
five-file production payload. The qualified build digest is
`37ca051c39d1688d6413c47d94b8c0f221152dac6981ec67d155f2c43ea17b40`;
REUSE digest is `85462a75c0f8efda09ddd190b92816b70e7662577c8427429e11e1b9f25a992e`.

Fresh native Wayland review remains pending for ecosystem integration. Compositor,
broker, routing, Debian qualification and the existing native-probe shutdown
follow-up remain outside EDM-016. No publication or pinning is authorized here.

Verification is recorded below and in [VERIFICATION.json](VERIFICATION.json).

Canonical tidy cleanup adds 23 `[[nodiscard]]` getter annotations in backend.h and
controller.h, without runtime changes. Cleanup commit: `61d4ddf79e5d2c89a8c763dc845b9786a397bcae`.

## Local acceptance — 2026-10-14 (Europe/Kyiv)

Explicit preparation: `python3 scripts/ci/run.py --prepare --provider-source-root ..`;
final preparation at `build/ci/20261013T215146Z-byryex53` reused all four providers
after canonical source/toolchain/options compatibility validation. The artifact
manifest covers provider sources, installed/build payload, modes and symlink targets.
Its exact identity/hash, provider revisions/options, image IDs, tool versions and
implementation/log hashes are retained in VERIFICATION.json.

Clean offline acceptance `python3 scripts/ci/run.py` at
`build/ci/20261013T215358Z-z4iaztex` passed Debug tests, static/QML checks and REUSE.
Its Release build passed but the new payload checker incorrectly expected an
unquoted service Exec value. After correcting only that checker, clean affected
Release acceptance `python3 scripts/ci/run.py --lane release` at
`build/ci/20261013T215653Z-h4k16c54` passed. The implementation hash comparison
confirms that only scripts/ci/check-payload.py changed between these snapshots.
Three unaffected passing acceptance lanes are retained; no earlier failing result
is represented as success. Complete logs were reviewed.

Results: both CTest registrations pass (2/2). Main Qt suite: 49 passed, zero failed,
one preserved activation-only skip. Real staged D-Bus activation: three passed,
zero failed/skipped. All existing test cases and isolated buses are preserved.
Fifteen real-CMake dependency regressions and eleven launcher regressions pass.
Missing/incompatible Qt, private/public mismatch, missing Wayland/scanner/XML, tests
OFF without Qt Test, restricted internal lookup, separate external/provider prefixes
with spaces, actual generated protocol commands with spaces, compatible consumer
reuse/incompatible-cache archival, stale/tampered preparations and snapshot isolation
are covered. Ordinary configure/build/test prerequisite failures produce no provider
or acquisition trees. Successful offline builds consume prepared installed providers.

Release payload is exactly five production files with correct activation command.
Canonical formatting, strict tidy across all nine owned translation units, QML
format/import policy/generated metadata/lint and REUSE pass. Legacy check-cpp.py
was also verified against its explicit build database in the incremental offline
build. Canonical fixture suites pass (21 external + 11 tooling = 32). Declaration
equality, bundle drift, installer-profile consistency, Python/shell/workflow syntax
and whitespace checks pass.

Environment: qualified immutable Arch full image, Qt 6.11.2, GCC 16.2.1, CMake 4.4.3,
Clang/clang-format/clang-tidy 22.1.8, Ninja 1.13.2, Task 3.53.1, Python 3.14.7,
Wayland/scanner 1.26.0, protocols 1.49 and REUSE 6.2.0. Providers use Debug with
tests/demo/gallery disabled and Qt Wayland enabled. Files app is OFF, Quick ON,
tests OFF. Consumer tests use Debug; production staging uses Release/testing OFF.

Reviewed expected diagnostics: Qt's private-module ABI notice (exact matching
version enforced), optional Vulkan header discovery, and existing offscreen fixture
icon-provider warnings. No owned compiler/tidy diagnostic or QML lint warning remains.
The missing icon assets in isolated fixtures do not substitute for fresh native
visual acceptance, which remains pending.

Changed files: canonical tooling bundle/declarations, CMake discovery/QML dependency
metadata, presets and Taskfile, qualified CI launcher/lanes/workflow/payload and
regression scripts, legacy check adapters, editor defaults, contributor README and
this SDD/evidence. Only two headers change in the separate annotation cleanup.
Publication, umbrella pinning and hosted CI checks remain separate authorized work.

Documentation-only licensing closure passed at
`build/ci/20261013T220010Z-_84poim4` (63/63 files); its log hash is recorded in
VERIFICATION.json. No implementation changed after accepted hashes.
