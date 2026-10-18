# Unified backend (UPB-001)

One virtual desktop object owns FileChooser and Settings at the unified `holonight`
identity. Settings is a private projection handler with no service registration.
It consumes installed HolonightQt Appearance and ThemeCatalog packages. An
AppearanceReader initializes defaults and watches configuration; only changed
projected values produce Settings signals. Invalid updates retain provider state.

The backend registers the object before the service and rolls back only resources
it acquired. Shutdown completes chooser requests before releasing registration.
Existing chooser controllers, native paths and presentation remain unchanged.

Acceptance covers wire signatures, namespace filtering, errors, live atomic
replacement, invalid retention/recovery, activation in either order, registration
rollback, and the existing chooser suite. Packaging advertises exactly the two
implemented interfaces. See the umbrella unified-portal-backend initiative for
upgrade, broker, manual acceptance and publication obligations.
