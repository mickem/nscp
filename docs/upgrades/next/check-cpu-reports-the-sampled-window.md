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
what has been sampled so far is now averaged over the samples there are, and
before the first sample the check answers UNKNOWN with
`No CPU data available yet (collector still initializing)` on Windows too, as it
already did on Linux.
