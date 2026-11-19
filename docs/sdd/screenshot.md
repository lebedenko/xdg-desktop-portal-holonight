# Hyprland Screenshot and PickColor

Build with `-DHOLONIGHT_PORTAL_SCREENSHOT=ON` after installing
`HolonightScreenshot::Capture`. Set `CMAKE_PREFIX_PATH` to the provider’s
installation if it is outside the system search path. The option installs
the Screenshot descriptor entry and exports both methods on the existing
desktop object. The build defaults to leaving Screenshot routing available
to the desktop’s current backend.

The backend implements Screenshot v3 with Screen and Area (mask 5).
With no target, non-interactive Screenshot selects the desktop and
interactive Screenshot asks the user to choose Desktop or Area. Explicit
targets fix that choice. PickColor uses a frozen desktop and returns
three normalized sRGB channels. [Screenshot contract](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.impl.portal.Screenshot.html)

Only the unique D-Bus owner of `org.freedesktop.portal.Desktop` can request
capture. A verified broker permission check can authorize a non-interactive
desktop screenshot. The remaining requests use a Wayland-parented approval
window. A display synchronization callback orders acquisition after removal
of that window. The current owner is checked again before capture and before
returning a result. [Broker permission handling](https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/1.22.1/src/screenshot.c)

The backend exports one request at a time. Close and broker loss cancel it.
Shutdown, layout changes, acquisition errors, and busy requests return
failure. PNGs are written under private directories in `XDG_RUNTIME_DIR`.
Successful replies give the broker a file URI for document export and the
PNG stays available for the session. Failed delivery removes the PNG and
its directory. [Request contract](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.impl.portal.Request.html)

Run the focused private-bus screenshot tests and staged activation tests
with Screenshot enabled. For native acceptance, use the staged Hyprland
routing and manual checks in the umbrella integration note.
