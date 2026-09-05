---
icon: "✅"
modules: [core, CheckDisk]
action: conditional
---
**No configuration migration required** (new `proxy` keys are opt-in). The
`check_files` fixes change a few corner cases: `max-depth=0` now scans the top
directory (#730); missing paths return UNKNOWN (#613); junction loops are not
double-counted (#605); empty results return OK instead of UNKNOWN (#717).
Review alerting that relied on the old corner-case behaviour.
