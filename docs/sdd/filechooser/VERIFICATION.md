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
