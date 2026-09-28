---
icon: "📊"
modules: [CheckSystem, CheckSystemUnix]
action: none
---
**`check_cpu` answers from the samples it has, instead of reporting an idle
machine for the first minutes after a start or a reload.** Nothing to do. The
collector's sample buffers start out full of empty slots, so a `5m` window (the
default) asked for two minutes after the agent started - or after any settings
reload, which replaces the collector - averaged two real minutes with three
empty ones, and a saturated host read as 40 % or less until the buffer had
filled; `warning=load>80` could not fire in that time. A window longer than
what has been sampled so far is now averaged over the samples there are.

Before the very first sample - the first second or so after a start or a
reload - the checks now answer UNKNOWN instead of reporting zeros:

* `check_cpu` on Windows and Linux says
  `No CPU data available yet (collector still initializing)`.
* On Linux, `check_memory` and `check_pagefile` do the same
  (`No memory data available yet ...`, `No pagefile/swap data available yet ...`).

The Linux checks already carried that message, but it could never be reached:
the "has data" test asked whether the buffer was non-empty, and the buffer is
created full, so it was always true. On Windows, a collector too busy to be
read within five seconds is reported as such
(`Failed to read CPU data: the collector is busy ...`) rather than as a fresh
start.
