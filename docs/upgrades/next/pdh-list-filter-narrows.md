---
icon: "🔧"
modules: [CheckSystem]
action: conditional
---
**The PDH counter browser now narrows on every filter, and ignores case.**
Nothing to do unless you have a script or a habit built on the counter browser
(`nscp sys -- --list …`, or `exec CheckSystem --list …` at the `nscp test`
prompt). Two things changed about which counters it lists:

* `--list`, `--filter` and `--counter` used to share one variable, so whichever
  came last won and the others were silently discarded: `--list SQL --filter
  Databases` listed everything matching `Databases`, including counters with no
  `SQL` in them at all. Every filter now has to match, so the same command
  lists what matches both — a **narrower** result than before. `--filter` is
  also repeatable now, so a listing can be cut down one word at a time.
* Matching is now **case insensitive**, where it used to be case sensitive.
  `--list disk` used to find nothing at all; it now finds the same counters as
  `--list Disk` — a **wider** result than before. PDH capitalises its own names
  inconsistently and a localised Windows spells them in another language, so
  having to guess the casing before getting any output was a trap rather than a
  feature. (The fold is ASCII: the non-ASCII part of a localised name still has
  to be typed as it is spelled.)

```
nscp sys -- --list disk --all --filter queue --filter avg.
\PhysicalDisk(_Total)\Avg. Disk Queue Length
\PhysicalDisk(_Total)\Avg. Disk Read Queue Length
\PhysicalDisk(_Total)\Avg. Disk Write Queue Length
```

Unchanged otherwise: a substring test against the whole
`\object(instance)\counter` path, and a `--filter` with no value is still
accepted and still filters nothing.
