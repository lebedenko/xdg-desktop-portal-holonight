# Local verification — 2026-10-09

This records automated verification of I-002, with a failed native user review explicitly
deferred to follow-up. It does not mark the initiative Integrated or authorize publication.

## Baselines and installed providers

Initial repository commit: `6cc4a0d9d560852bbdcef589aa04238548cf3275`. The repository-local
instructions and accepted SDD were committed, linked from the umbrella and marked Ready
before implementation. There was no prior backend repository or upstream baseline.

Host: GCC 16.2.1, Qt 6.12.0, C++23, Ninja, wayland-client 1.26.0. Minimum Qt remains 6.11.
The GuiPrivate dependency emits Qt's expected ABI-version notice; this adapter must be
rebuilt against the matching runtime. Complete build logs contain no compiler warnings.

Installed direct providers:

| Provider | Exact source revision | Artifact evidence |
|---|---|---|
| HolonightFileBrowser Core/Quick | cfc130e3cd0b0eb56a94f6b9435d433bc96cb78e | Clean published/pinned Files source; compatible I-001 provider-only artifacts installed from `/tmp/hn-file-browser-quick` into backend `build/providers` |
| HolonightSearch | 26067775e2eac3d9a779b3114dfbddc96834c7de | Installed Files dependency prefix and its provider-revisions.tsv |
| HolonightQt | 6c7ac33004702e166b8c152dcde918296be54286 | Same installed prefix, GCC toolchain and Qt 6.12.0; matches accepted compatibility reference |

All three source checkouts were verified clean at these revisions. The installed dependency
ledger also records Config d6a392b41991f70a004d58f7694c7b6115cb7280 and SystemServices
39472e6dcafc93acea218a234213c346be256586 used by the Qt provider. Consumer configuration
uses installed CMake packages and QML imports; it does not add provider source directories.

## Commands and results

Commands are normalized to this repository. `browser_prefix` is the absolute path to
`build/providers`; `shared_prefix` is the installed Files dependency prefix. `imports`
contains their installed `lib/qt6/qml` directories separated by semicolons for CMake.

```sh
cmake --install /tmp/hn-file-browser-quick --prefix "$browser_prefix"
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH="$browser_prefix;$shared_prefix" \
  -DQML_IMPORT_PATH="$imports"
cmake --build build/debug -j 4
ctest --test-dir build/debug --output-on-failure

cmake -S . -B build/acceptance -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH="$browser_prefix;$shared_prefix" \
  -DQML_IMPORT_PATH="$imports"
cmake --build build/acceptance -j 4
ctest --test-dir build/acceptance --output-on-failure
python3 scripts/check-cpp.py build/acceptance
python3 scripts/check-format.py
cmake --build build/acceptance --target all_qmllint
reuse lint
```

Focused Debug tests passed during development. A fresh Release acceptance build passed;
affected incremental builds and checks were repeated after corrections without rebuilding
unchanged providers. Final full CTest passes both entries:

- `portal-tests`: 25 QtTest passes, zero failures; one intentional skip for the activation
  case, which runs exclusively in the staged-install fixture. Totals include setup/cleanup.
- `portal-staged-activation`: three QtTest passes including setup/cleanup; installs to a
  temporary DESTDIR, checks descriptor/service/executable, uses the staged executable for
  real session-bus activation, exports a real QML chooser request, cancels it through Close,
  and exercises SIGTERM with another pending request returning response 2.

The suite covers exact backend introspection and reply signatures, nested filter/choice/name
wire types and preservation, export-before-presentation, duplicate handles, simultaneous
requests, owner-only Close, broker loss, cancellation during validation, handle reuse,
presentation failure and shutdown. Controller/filesystem cases cover all four file/folder
single/multiple selection modes, explicit selection independent of cursor movement,
selection retained through filters and cleared on navigation, current-folder search,
MIME content detection without extensions, stale folder/query/initial-location/validation
work, removed entries, fallback explanation, current_file precedence, non-UTF-8 folder and
filename bytes, indistinguishable display names, escaped/Unicode URIs, overwrite confirmation,
SaveFiles destination selection and deterministic collisions. Save tests assert that directory
entries and existing file contents remain unchanged and no target files are created.

Offscreen QML verification covers window creation, j/Space navigation without selection
changes, Ctrl+L and initial SaveFile filename focus. Full C++ lint passes all nine private/test
translation units, formatting passes, QML lint/type metadata passes, and REUSE passes.
Unix-bus socket creation and REUSE's multiprocessing socket required sandbox escalation;
both checks passed outside the sandbox. No desktop pointer or focus automation was used.

Build, test and quality logs remain in `build/acceptance` and `/tmp/filechooser-acceptance-*`.

## Native review and remaining handoffs

The user was asked to run the native host and manually verify parenting/modality, keyboard
flow and cancellation. The user reported that there were many issues, without identifying
a compositor or concrete reproduction, and explicitly requested follow-up one issue at a
time after the current implementation work. Record this as **failed native review, deferred**;
it is neither a pass nor merely an unavailable check.

The manual host was subsequently corrected to use a rendered QQuickWindow parent and a
separate broker connection, and to fall back when export is unavailable. These corrections
have build/lint evidence but no successful native recheck. They do not establish that the
reported chooser issues have been resolved.

FC-T06 remains Planned and umbrella I-002 remains In Progress. Publication and registering
a published submodule pin require a separate handoff. I-003 routing has not started. Full
Hyprland/Sway/labwc review, real-broker integration and sandbox document-access acceptance
remain required in I-004 before the initiative can become Integrated. Save validation cannot
reserve destinations against subsequent filesystem changes.

## Review fixes — 2026-10-09 (FC-T07)

Fixed single-file activation returning an earlier explicit selection, single-folder clicks
not selecting OpenFile/SaveFiles destinations, and inaccessible hidden files. Single-selection
clicks and j/k movement now select the entry; multi-selection stays explicit. Show hidden files
and Ctrl+H expose the existing provider toggle. Native bytes remain the selection identity.

Regression tests failed before the fixes for activation, both folder operations and the missing
hidden-files property. Final coverage also checks repeated clicks, replacement of a prior folder
selection, keyboard movement after a single-file click, opening a revealed dotfile, the QML
hidden-files control/shortcut, and preservation of multi-selection. QML clicks are direct signal
invocations in the offscreen fixture; no desktop pointer or focus interaction was automated.

Reused the installed providers listed above: their source revisions and dependency ledger still
match, with GCC 16.2.1 and Qt 6.12.0. The Files artifact is the provider-only Core/Quick build;
Search and Qt use the existing compatible Debug provider artifacts. A fresh Release consumer
was configured in `build/review-acceptance` using the same prefix/import arguments recorded above.

Verification passed:

- Focused Debug build and `ctest --test-dir build/debug -R '^portal-tests$' --output-on-failure`.
- `cmake --build build/review-acceptance -j 4`: clean build, complete build log reviewed,
  no compiler warnings; configuration emits only the expected Qt GuiPrivate ABI notice.
- `ctest --test-dir build/review-acceptance --output-on-failure`: both targets pass;
  32 QtTest passes with the intentional activation-only skip, plus three staged-activation passes.
- `python3 scripts/check-cpp.py build/review-acceptance`: all nine translation units pass.
- `python3 scripts/check-format.py`, `cmake --build build/review-acceptance --target all_qmllint`,
  `reuse lint`, and `git diff --check` pass.

Logs remain under `build/review-acceptance` (`build.log`, `check-cpp.log`, `tidy/`, and
`Testing/Temporary/LastTest.log`). Isolated D-Bus and REUSE required permission to create local
sockets. FC-T06 manual compositor acceptance remains open; these automated fixes do not establish
a successful native recheck or authorize publication/integration.

## Bounded chooser height — 2026-10-09 (FC-T08)

Added Qt.Dialog before native window creation, with initial/maximum height
max(1, min(600, Screen.desktopAvailableHeight - 48)) in logical pixels. The
400-pixel minimum is clamped to the same limit. Width, private parenting and
request-controlled modality are unchanged; no public API changes.

The new offscreen OpenFile/SaveFile regression failed before the fix (Window
instead of Dialog). It checks initial width/height, maximum height, consistent
minimum constraints, visible action buttons and scrolling an 80-file listing at
the initial height. The existing 400-pixel minimum layout can push the footer
outside the window because of the places column; that separate layout limitation
is not resolved by this focused sizing fix. Extremely short screens likewise
retain consistent window constraints without guaranteeing all content fits.

Reused the previously recorded installed providers: Files, Qt and Search sources
remain clean at their recorded revisions, and the Files dependency revision ledger
matches. Toolchain remains GCC 16.2.1 / Qt 6.12.0. Configured a fresh Release consumer
in build/height-acceptance with the same prefix/import arguments as above.

Verification:

- Focused Debug portal-tests passed.
- Clean Release build passed; complete configure/build logs reviewed, no compiler
  warnings, only the expected Qt GuiPrivate ABI notice. An incremental test rebuild
  followed the QtTest naming annotation and test cleanup.
- CTest in build/height-acceptance: both entries passed; 34 QtTest passes with the
  intentional activation-only skip, plus three staged-activation passes.
- C++ checks passed all nine translation units.
- Formatting, all_qmllint, REUSE and git diff --check passed.

Logs: /tmp/portal-height-{configure,build,build-final,debug,cpp}.log and
build/height-acceptance/Testing/Temporary/LastTest.log. D-Bus and REUSE required
sandbox escalation to create local sockets.

Read-only hyprctl clients -j found no open chooser. Requested manual parented and
unparented checks using build/height-acceptance/native-check.sh; native geometry,
parenting and modality are pending user review, not established by offscreen tests.
FC-T06 remains open. No publication or umbrella pin update was performed.

## Picker presentation and interaction — 2026-10-09 (FC-T09)

User native review: clicking the parent did not focus it and the chooser remained
above it. This supports modality in the tested parented configuration, not full
compositor acceptance. The user reported unclear file/folder intent, vague
"Select a location" feedback, keyboard navigation lost after clicking Open and a
row, an image://icon/undefined warning, and NoReply from the manual probe.

Used docs/mockups/file-picker.png as visual direction within the existing scope:
visible mode heading and request title, compact HnListDelegate Places rows with
provider icons and current-location highlighting, a scrollable bounded sidebar,
multiple-selection count and emphasized primary action. HnLabel supplies shared
typography. No changes to holonight-qt were required; its existing primitives are
appropriate. Preview, history, device browsing and additional mockup features
remain outside this focused follow-up.

Open on a cursor folder in file mode now navigates when no files are selected.
Folder-mode acceptance still returns selected folders. Empty selections explain
which type is required. Row/selection-control/Places clicks restore listing focus;
the primary action restores it in browsing mode. Guarded icon sources avoid
constructing image://icon/undefined while delegate roles change or disappear.
The native probe uses INT_MAX for the delayed D-Bus call rather than the default
short timeout, so manual interaction does not expire after approximately 25 seconds.

Offscreen regressions check folder-click/Open navigation, j/k after delegate focus,
keyboard recovery after empty multi-selection feedback, all five visible mode
headings, bounded Places width, long-list scrolling and visible action buttons at
both initial and minimum heights. The previous 400-pixel footer limitation is
resolved by replacing the vertically expanding Places buttons with a scrollable
sidebar; extremely short screens still cannot guarantee all controls fit. The
navigation test rejects image://icon/undefined warnings. No desktop input was
automated.

Reused the same clean provider revisions and compatible installed artifacts as
FC-T08. A fresh Release build in build/design-acceptance passed with no compiler
warnings after complete configure/build log review (expected GuiPrivate ABI notice
only). Focused Debug portal tests passed. Full Release CTest passed both entries:
39 QtTest passes with the intentional activation-only skip, plus three staged
activation passes. Formatting, all_qmllint, REUSE and git diff --check passed.
C++ checks passed all nine translation units, with the affected test unit rechecked
after adding an explicit null-window guard for the analyzer. Test-only lint corrections
received an incremental rebuild and affected checks.
Logs remain in /tmp/portal-design-{configure,build,build-final,debug,cpp}.log and
build/design-acceptance/Testing/Temporary/LastTest.log.

Manual recheck: bash build/design-acceptance/native-check.sh open, multi, directory,
and save. Verify the new layout, folder navigation, keyboard recovery and console
output on Hyprland. The reported native console failures have automated coverage
or a concrete probe timeout correction, but a successful native recheck remains
pending. FC-T06 remains open. No publication or umbrella changes were made.

## Manual recheck passed — 2026-10-09 (FC-T06 closure)

After the FC-T09 implementation and the request to review the updated native probe
in build/design-acceptance, the user reported: "manual checks passed".
Record this as user-reported successful manual acceptance of the current chooser
follow-up, superseding the earlier failed/deferred native review and pending recheck.
No additional per-mode results, measured compositor geometry or console logs were
provided; the detailed automated evidence remains in FC-T08 and FC-T09 above.

FC-T06 is Done. This closes the repository-local native review follow-up, not the
broader compositor matrix, real-broker/document-access or umbrella integration
acceptance. No publication, commit or umbrella pin/status change was performed.
This closure edits only SDD documentation; git diff --check passed. Existing build,
test and quality-check evidence is unchanged and was not rerun.


## Parenting before initial placement — 2026-10-09 (FC-T10)

Baseline: af1e9a4806f1b74aaffe392588c81a76d1275b2f, initially clean.
The previous adapter showed immediately, then discovered/imported the parent on
its dispatch timer. The updated presenter connects preparation completion before
starting it, and keeps the chooser hidden until completion. The adapter uses two
private-queue wl_display_sync callbacks: registry discovery, then import validation.
Qt retains ownership of display reads; there are no blocking roundtrips or nested
interaction loops. A direct surfaceRoleCreated connection applies set_parent_of
synchronously before Qt's initial shell-role commit. Preparation is bounded to
one second, with ordinary presentation on unavailable/invalid parents or timeout.
Close destroys the adapter immediately before deferred window deletion, cancelling
callbacks, dispatch and the deadline. Late completion cannot show or parent a
cancelled/timed-out request. Parent invalidation after presentation retains the
protocol's ordinary unparented behavior.

The placement ordering follows [Qt's initWindow implementation](https://raw.githubusercontent.com/qt/qtbase/6.11/src/plugins/platforms/wayland/qwaylandwindow.cpp)
and the [xdg-foreign-v2 protocol](https://raw.githubusercontent.com/wayland-mirror/wayland-protocols/main/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml).
This fixes the code race; it does not conclusively attribute earlier screenshots.
No installed/public API, dependency or portal wire-format changes were made.

Focused tests cover delayed preparation and show-once completion, synchronous
parent application between simulated shell-role creation and initial commit,
invalid import fallback, missing/unsupported/malformed handles, unavailable
Wayland platform, timeout ignoring late successful completion, and cancellation
before preparation. A private adapter factory and fake role signal exercise the
real presenter/deadline/direct-connection lifecycle offscreen. They do not emulate
a compositor or verify the actual registry/import protocol exchange or placement.

Reused the recorded installed Core/Quick, Search and Qt artifacts, with their
existing provider-revisions.tsv, GCC 16.2.1 and Qt 6.12.0. Files remains clean at
cfc130e3cd0b0eb56a94f6b9435d433bc96cb78e and Search remains clean at
26067775e2eac3d9a779b3114dfbddc96834c7de. The clean Qt source checkout has advanced
to 918d14dd7c739569233a16fd469e0063b9316971; this acceptance uses the installed
6c7ac33004702e166b8c152dcde918296be54286 artifact recorded by the dependency
ledger, not that newer checkout. Provider sources were not rebuilt or changed.

Commands use the same installed prefix/import arguments recorded above:

- Focused Debug build and ctest --test-dir build/debug -R '^portal-tests$' --output-on-failure.
- Fresh Release configuration/build in build/parent-acceptance, followed by affected
  incremental rebuilds after test coverage and style corrections. Complete configure
  and build logs reviewed: no compiler warnings; expected GuiPrivate ABI notice only.
- ctest --test-dir build/parent-acceptance --output-on-failure: both entries pass;
  49 QtTest passes, zero failures, one existing activation-only skip, plus three
  staged-activation passes.
- python3 scripts/check-cpp.py build/parent-acceptance: all nine translation units pass.
- python3 scripts/check-format.py, cmake --build build/parent-acceptance --target all_qmllint,
  reuse lint and git diff --check pass.

D-Bus and REUSE checks required sandbox escalation for local socket creation.
Logs are /tmp/portal-parent-{configure,build,build-final,debug,debug-final,cpp-final}.log
and build/parent-acceptance/Testing/Temporary/LastTest.log.

FC-T11 is In Progress. User-run Hyprland inspection with
bash build/parent-acceptance/native-check.sh open reported that modal checks passed.
Non-modal/unparented pickers remained open after parent closure. The probe required
Ctrl+C after dismissal; probe shutdown remains an open follow-up outside this
publication handoff. Repeated Ctrl+O/Esc with freshly activated and already running
real backend instances also remains pending; the current isolated probe issues one
request. These results do not close the broader compositor or ecosystem matrix.
No desktop pointer/focus automation or system installation was performed for this
fix. Publication and umbrella pinning are tracked in the umbrella ledger.

Publication review reused the completed clean build, CTest and quality-check
evidence above. After recording the manual results, local SDD Markdown links and
git diff --check passed; no implementation changed or acceptance rebuild was needed.
