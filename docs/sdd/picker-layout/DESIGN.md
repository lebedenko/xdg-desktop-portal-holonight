# Picker panel layout

## Intent and references

Align all six portal picker presentations with `docs/mockups/file-picker.png`,
the HoloNight Dual File Dialogs mockup, and the current holonight-files panels.
Use adjoining regions, shared detailed rows, theme typography and palette surfaces.
Keep the installed browsing and preview providers and all native byte path handling.

## Decisions and interfaces

The private Controller exposes a constant `saveMany` flag to distinguish Save Files
from Select Folder, plus `canGoBack`, `canGoForward`, `goBack()` and `goForward()`
for toolbar history. There are no public protocol or provider changes.
Chooser.qml owns the presentation: a purpose header with mode icon and close action,
a scrollable Places sidebar (220 pixels, matching Files), browser toolbar and listing,
a preview region, and a stable footer. The toolbar wraps search beneath navigation
below 900 pixels. Save File fields sit beneath the listing; open-mode filters move
from the footer to that region on narrow windows.

One FilePreview instance binds to the existing request preview service. Its visual
parent moves between a 220-pixel dock and a right-edge drawer over the browser.
Resizing across the breakpoint closes the drawer. This avoids duplicate preview
consumers and leaves cursor, selection and preview target under controller ownership.
The file-type control similarly moves without resetting its selected filter.

Caller choices live in a bounded, scrollable popup; their values remain in the
controller. Long labels wrap. Validation text scrolls above the footer action row.
Escape closes options or the preview drawer before cancelling a request; overwrite
confirmation retains its own dismissal behavior. Existing keyboard shortcuts and
initial filename/list focus remain. Default and minimum screen-clamped geometry is
unchanged. Back, Forward and Up share the toolbar. Private per-request history stores native
path bytes, skips consecutive duplicate locations and truncates the forward branch
on new navigation. No creation, grid, thumbnail browsing, device or bookmark UI is added.

Both sidebars use Files’ 220-pixel width and surface palette role. Places uses the
same caption, spacing, icon size and extra-small list delegates as Files. The toolbar
spans from the Places boundary to the window edge, above the browser and preview.
Hidden files toggle only through Ctrl+H.

## Acceptance

Offscreen portal tests cover six headings, default/minimum dimensions, reachable
actions and scrolling at 1160, 800 and 600 pixels, filter placement, preview Escape
and resize transitions preserving cursor/selection, and persistent options with many
long labels. Existing tests retain keyboard, overwrite, cancellation, filters and
native path byte coverage. Run focused tests, full CTest, formatting, and `task check`
for fresh acceptance including QML lint, static checks and REUSE.

Native visual acceptance remains manual through `task demo`: inspect all six modes,
narrow windows, long options/validation, preview dismissal and parenting/modality.
Desktop pointer and focus interaction must not be automated.
