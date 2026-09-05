---
icon: "🔧"
modules: [core]
action: conditional
---
**`nscp settings --show` now says so when `--key` is missing.** `--show
--path /some/path` without a `--key` used to print nothing and exit 0; it
now reports `Invalid command line please use --path and --key with show`
and exits non-zero. A bare `--show` still describes the active
settings store, and `--show --path … --key …` is unchanged. Scripts that
relied on the silent success need the missing `--key` added.
