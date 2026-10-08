# Standalone FileChooser — I-002

Status: Accepted

Implementation: local automated acceptance passed. Native review reported multiple issues;
the user requested that they be addressed individually after this implementation checkpoint.
Native acceptance remains open; no publication or ecosystem acceptance is implied.

## Requirements

FC-001: Implement backend OpenFile, SaveFile, SaveFiles and Request.Close with exact
D-Bus signatures, delayed replies and one independently owned interaction per handle.
Reply once with 0 for success, 1 for cancellation, 2 for failure. Export before presentation.
Reject duplicate handles, handle broker disappearance and shutdown without nested loops.

FC-002: Use installed HolonightFileBrowser Core/Quick, HolonightSearch and HolonightQt.
Compose standard places, location, passive directory listing, filter, choices and footer.
Directories stay navigable through filters. Preserve provider sorting and hidden policy.
Search only the current folder; discard stale worker results.

FC-003: Keep native bytes for identity and filesystem operations and percent-encoded local
URIs. Cursor movement must not alter multi-selection. Clear selection on navigation, retain
it through filtering. Directory selection defaults to current folder if selection is empty.
Support j/k, h/l, /, Ctrl+L, Enter, Space and visible selection controls.

FC-004: SaveFile focuses filename initially, prefers current_file then current_folder/name,
falls back to Home with explanation, validates asynchronously and confirms overwrites.
SaveFiles preserves input order and bytes, numbers collisions, rejects invalid names.
Validation never creates, truncates or reserves paths; later filesystem changes remain possible.

FC-005: Private xdg-foreign-v2 import uses Qt native Wayland surfaces and event-loop dispatch.
Missing, unsupported or destroyed parents leave an ordinary chooser. Honor modality.

FC-006: Install executable, D-Bus service and holonight-filechooser.portal. Bus identity is
org.freedesktop.impl.portal.desktop.holonight_filechooser. Shell Settings is unaffected.

## Design

Private wire structures and adapter decode nested filters/choices. Request ownership
retains the delayed call, caller identity and controller. A presenter interface permits
deterministic lifecycle tests without QML. A controller composes the shared reader and
sorting proxy with private filtering, native selection and asynchronous validation.
Workers retain snapshots and cancellation tokens, never UI objects. QML owns presentation.
GuiPrivate and generated Wayland bindings are isolated in the native parent adapter.

## Boundaries and acceptance

No history, recursive search, previews, thumbnails, bookmarks, devices, X11 or other interfaces.
I-003 owns routing; I-004 owns final compositor matrix and broker/document-access acceptance.
Local tests cover wire types, lifecycle, selection, byte paths and non-mutating save validation.
See TASKS.md and VERIFICATION.md for actual evidence and outstanding native checks.

Contracts: https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.impl.portal.FileChooser.html
and https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.impl.portal.Request.html.
