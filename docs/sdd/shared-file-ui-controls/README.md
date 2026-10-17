# Shared file UI controls

Each Controller owns one PreviewService independently of multi-selection. The
cursor and model changes refresh its target using DirectoryRoles::NativePathRole;
metadata changes increment the revision. Navigation clears the target before replacing
the model; provider generations reject stale results. stop() clears and shuts down
preview work on success, cancellation and external completion. Destruction retires the
service through shutdownFinished/deleteLater, preserving asynchronous worker ownership.

Breadcrumb labels and byte-exact ancestor identities remain separate. Typed locations
use existing validation, and Ctrl+L exposes the editor. The chooser consumes installed
Quick/Preview/PreviewQuick, shared detailed rows and FilePreview. The default window
is screen-bounded and wider; previews collapse below 900 logical pixels with an
accessible reveal toggle. Preview failures do not participate in acceptance validation.

Acceptance covers all portal operations, filters/choices, byte-exact URIs, superseded
preview results, decode cancellation, concurrent requests and asynchronous retirement.
Native parenting, keyboard and visual acceptance remain user-operated.

Verification results and compatible provider revisions are recorded in the umbrella
[initiative ledger](../../../../docs/initiatives/shared-file-ui-controls/TASKS.md).
