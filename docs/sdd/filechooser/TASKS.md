# Tasks

| ID | Requirements | Work | State |
|---|---|---|---|
| FC-T01 | FC-001,006 | Protocol, ownership, activation, lifecycle tests | Done |
| FC-T02 | FC-002,003 | Controller, native selection, filters and search | Done |
| FC-T03 | FC-004 | Asynchronous non-mutating destination validation | Done |
| FC-T04 | FC-005 | QML presentation and private Wayland import implementation | Done |
| FC-T05 | All | Clean automated acceptance, quality checks and local checkpoint | Done |
| FC-T06 | FC-002,003,005 | Describe and resolve native review issues individually; repeat manual checks | Done |
| FC-T07 | FC-002,003 | Fix review findings: single-file activation, single-folder clicks and hidden-file access; add regressions | Done |
| FC-T08 | FC-005 | Bound chooser dialog height; offscreen presentation regression and automated acceptance (native check remains FC-T06) | Done |
| FC-T09 | FC-002,003,005 | Follow picker mockup direction; clarify purpose, restore keyboard focus, improve folder Open and native probe timeout | Done |

See [VERIFICATION.md](VERIFICATION.md). The user reported that the manual recheck
passed on 2026-10-09, closing FC-T06 after the FC-T07 through FC-T09 fixes.
Umbrella I-002 status, publication and broader ecosystem acceptance are separate handoffs.
