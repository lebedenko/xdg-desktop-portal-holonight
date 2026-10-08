# HoloNight FileChooser portal

Standalone Qt Quick backend for `org.freedesktop.impl.portal.FileChooser`.
Implements OpenFile, SaveFile, SaveFiles and independently cancellable Request objects.
The executable does not implement Settings or change desktop portal routing.

Requires C++23, Qt >= 6.11 (including matching GuiPrivate development headers),
wayland-client, wayland-protocols, wayland-scanner and installed HoloNight packages:
HolonightFileBrowser Core/Quick, HolonightSearch and HolonightQt.
The accepted Files provider revision is `cfc130e3cd0b0eb56a94f6b9435d433bc96cb78e`.

## Build and verify

Install providers to a prefix, then configure using their installed CMake packages and QML modules:

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
`scripts/check-cpp.py` removes GCC-only flags from a temporary Clang database, leaving the
real compiler database intact. Full logs remain under the build's `tidy/` directory.

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

The test host shows a parent and exports it through xdg-foreign-v2. Interact manually to
check parenting, modality, parent disappearance, placement, cancellation and keyboard flow.
No pointer movement, focus changes or synthetic input are automated by this native host.
The offscreen regression suite exercises keyboard input in an isolated fixture.

Keyboard: j/k move the cursor, h goes up, l enters a folder, / focuses folder search,
Ctrl+L focuses location, Space toggles selection, Enter enters a cursor folder or accepts
files. The accept button selects folders, including the current folder if none are selected.
SaveFile focuses its filename. Multi-selection is explicit and survives filtering, while
navigation clears it. SaveFile requires overwrite confirmation; SaveFiles numbers collisions
in input order. Neither operation creates or reserves a destination.

No history, recursive search, previews, thumbnails, bookmarks, devices, X11 parenting or
additional portal interfaces. Compositor and real-broker document-access acceptance are
recorded separately from local automated verification in the [SDD](docs/sdd/filechooser/README.md).
