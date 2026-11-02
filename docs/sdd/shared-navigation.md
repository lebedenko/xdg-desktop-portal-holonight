# Shared keyboard navigation

The chooser consumes HnNavigation only in active collection scopes. The application retains acceptance, cancellation, eligibility and single/multiple selection behavior. The cursor preserves native path identity across reorder. Profile-scoped shortcuts leave editors and modal dialogs alone. General visual polish, server-side rendering and metadata display corrections are deferred by user direction.

The interaction contract and installed-provider verification order are documented in the
umbrella `docs/shared-keyboard-navigation.md` and provider `holonight-qt/docs/navigation.md`.
Native focus and reachability acceptance remains manual. No deployment is part of this change.
