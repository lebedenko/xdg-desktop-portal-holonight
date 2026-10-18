# HoloNight portal backend

For current build, test, container acceptance and provider workflows, see [DEVELOPMENT.md](DEVELOPMENT.md).

Standalone Qt Quick backend for `org.freedesktop.impl.portal.FileChooser` and
`org.freedesktop.impl.portal.Settings`.
Implements OpenFile, SaveFile, SaveFiles and independently cancellable Request objects.
Settings reads and watches the canonical appearance document through installed
`Holonight::AppearanceReader`, without the shell. Only changed projected values
emit notifications; invalid updates retain the last valid appearance.

Requires C++23, Qt >= 6.11 (including matching GuiPrivate development headers),
wayland-client, wayland-protocols, wayland-scanner and installed HoloNight packages:
HolonightFileBrowser Core/Quick/Search and HolonightQt Appearance/ThemeCatalog. The browsing package now comes
from the independent `holonight-file-browser` repository. CI pins its published revision
in `scripts/ci/providers.json`; use `task check -- --siblings /path/to/umbrella` to test
current sibling sources explicitly.

## Build and verify

See [DEVELOPMENT.md](DEVELOPMENT.md) for current native and container commands.

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

See [DEVELOPMENT.md](DEVELOPMENT.md) for current native and container commands.

## Qualified offline CI

See [DEVELOPMENT.md](DEVELOPMENT.md) for current native and container commands.

## Installation

```sh
DESTDIR="$staging_root" cmake --install build/acceptance
```

Installs the executable under libexec, the session activation service under
`share/dbus-1/services`, and `holonight.portal` under
`share/xdg-desktop-portal/portals`. Provider libraries/QML modules must be installed separately.
Select `holonight;gtk` for both FileChooser and Settings. The activation name is
`org.freedesktop.impl.portal.desktop.holonight`, at `/org/freedesktop/portal/desktop`.
Upgrade shell and backend together: the shell must no longer own the descriptor or
publish Settings. Replace `holonight-filechooser` with `holonight` in personal portal
configuration manually; no compatibility alias is installed. The umbrella installer
transfers unchanged descriptor ownership and preserves modified legacy files.
See [the local design](docs/sdd/unified-backend/DESIGN.md).

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
