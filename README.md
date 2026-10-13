# HoloNight FileChooser portal

Standalone Qt Quick backend for `org.freedesktop.impl.portal.FileChooser`.
Implements OpenFile, SaveFile, SaveFiles and independently cancellable Request objects.
The executable does not implement Settings or change desktop portal routing.

Requires C++23, Qt >= 6.11 (including matching GuiPrivate development headers),
wayland-client, wayland-protocols, wayland-scanner and installed HoloNight packages:
HolonightFileBrowser Core/Quick, HolonightSearch and HolonightQt.
The accepted Files provider revision is `7de2d9961a0feea28e41f80c4e834542faf1fefa`.

## Build and verify

Run `task deps PRESET=test` explicitly to prepare supplied sibling checkouts in order:
Config → Qt → Search → Files Core/Quick. Provider tests and the Files application are
disabled; Files Quick is explicitly enabled. Set `HOLONIGHT_CONFIG_SOURCE`,
`HOLONIGHT_QT_SOURCE`, `HOLONIGHT_SEARCH_SOURCE` and `HOLONIGHT_FILES_SOURCE` for other
local sources (including paths with spaces). No source download occurs.

`task configure`, `task build` and `task test` only validate installed prerequisites.
Debug/test presets enable tests; Release disables tests and installs to `/usr`.
Use `HOLONIGHT_DEPENDENCY_PREFIX` to consume existing internal providers without
preparing them, and `HOLONIGHT_EXTERNAL_PREFIX` for external packages. Internal
discovery is restricted to the approved internal prefixes, including Config as
Qt's transitive dependency. Incompatible caches are archived, never silently reused.

```sh
task deps PRESET=test
task test
task build PRESET=release
task format-check
task tidy
task lint PRESET=test
```

Direct CMake remains supported after providers have been installed:

```sh
cmake -S . -B build/acceptance -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON \
  -DCMAKE_PREFIX_PATH="$provider_prefix" -DQML_IMPORT_PATH="$provider_prefix/lib/qt6/qml"
cmake --build build/acceptance -j 4
ctest --test-dir build/acceptance --output-on-failure
python3 scripts/check-cpp.py build/acceptance
python3 scripts/check-format.py
cmake --build build/acceptance --target all_qmllint
reuse lint
```

`CMAKE_PREFIX_PATH` and CMake's `QML_IMPORT_PATH` accept semicolon-separated prefixes/import
directories. At runtime use a colon-separated `QML_IMPORT_PATH` if providers are outside
Qt's standard import path. Tests set the configured import paths automatically.
CTest uses isolated buses without desktop services and stages installation in a temporary
directory for real activation. It needs permission to bind private Unix sockets.

GuiPrivate is confined to the Wayland adapter; rebuild against the exact Qt runtime version.
`scripts/check-cpp.py BUILD` and `scripts/check-format.py` retain their entrypoints
and delegate to canonical strict checks. Tidy uses a filtered copy under
`.cache/tooling/clang`; the compiler database remains intact. QML checks include
formatting, qualified imports, generated metadata and `all_qmllint`.

## Qualified offline CI

Run `task ci:prepare` explicitly (or `python3 scripts/ci/run.py --prepare
--provider-source-root ..` to acquire exact revisions from local repositories).
Preparation pulls the qualified immutable Arch and REUSE images and builds the
pinned providers with tests/application disabled. `task ci` runs Debug tests,
Release installation, static/QML checks and licensing using `--pull=never
--network=none`, caller UID/GID, read-only snapshots and private runtime directories.
Acceptance never acquires images or providers. Preparation hashes cover source,
payload, modes, symlinks and environment evidence; stale/tampered inputs fail with
explicit preparation remediation. Compatible artifacts are reused; incompatible
preparation is archived. Complete logs and artifacts remain in `build/ci/`.
The accepted providers are pinned in `scripts/ci/run.py`; this repository is standalone.

## Installation

```sh
DESTDIR="$staging_root" cmake --install build/acceptance
```

Installs the executable under libexec, the session activation service under
`share/dbus-1/services`, and `holonight-filechooser.portal` under
`share/xdg-desktop-portal/portals`. Provider libraries/QML modules must be installed separately.
Select `holonight-filechooser` in the desktop's FileChooser preference when deploying;
HoloNight's coordinated routing change belongs to I-003.

## Manual native checks

```sh
bash build/acceptance/native-check.sh multi
bash build/acceptance/native-check.sh save
bash build/acceptance/native-check.sh directory
bash build/acceptance/native-check.sh open --nonmodal
bash build/acceptance/native-check.sh unparented
```

The test host shows a parent and exports it through xdg-foreign-v2. Its request waits
for a human response without the default short D-Bus timeout. Interact manually to
check parenting, modality, parent disappearance, placement, cancellation and keyboard flow.
No pointer movement, focus changes or synthetic input are automated by this native host.
The offscreen regression suite exercises keyboard input in an isolated fixture.

Keyboard: j/k move the cursor, h goes up, l enters a folder, / focuses folder search,
Ctrl+L focuses location, Space toggles selection, Enter enters a cursor folder or accepts
files. Ctrl+H or Show hidden files toggles dotfiles and hidden folders.
In single-selection mode, clicking an entry or moving with j/k selects it; activating a
file replaces any previous selection. The accept button uses the selected folder, including
for SaveFiles, or the current folder if none is selected.
In file mode, Open on an unselected folder browses into it. The visible heading
identifies the selection mode; Places uses compact shared delegates and scrolls.
SaveFile focuses its filename. Multi-selection is explicit and survives filtering, while
navigation clears it. SaveFile requires overwrite confirmation; SaveFiles numbers collisions
in input order. Neither operation creates or reserves a destination.

No history, recursive search, previews, thumbnails, bookmarks, devices, X11 parenting or
additional portal interfaces. Compositor and real-broker document-access acceptance are
recorded separately from local automated verification in the [SDD](docs/sdd/filechooser/README.md).
