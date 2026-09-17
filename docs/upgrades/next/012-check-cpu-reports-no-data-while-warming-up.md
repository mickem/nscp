---
icon: "📊"
modules: [CheckSystem, CheckSystemUnix]
action: conditional
---
**`check_cpu` no longer reports `0 %` while the collector is warming up.** Check
your alerting if it treats UNKNOWN as a failure.

The CPU collector's rolling buffers were allocated full of zero slots, and an
average was taken over the whole window whether or not anything had been
sampled into it. On a saturated host, `check_cpu` therefore answered
`total 5m: 0 % OK` for the first five minutes after a start - and, because a
settings reload builds a fresh collector, after every reload as well -
and a `warning=load>80` threshold could not fire in that window.

Two changes:

* An average is now taken over the samples that exist, not over the window that
  was asked for. Thirty seconds after a start, `time=5m` is the average of
  those thirty seconds.
* Before the first sample exists at all, `check_cpu` answers UNKNOWN with
  *"No CPU data available yet (collector still initializing)"*. The Unix build
  already had this message but could never reach it; the Windows build had no
  such guard.

The window is at most one poll interval on a normal start. A monitoring system
that pages on UNKNOWN may see one such result after an agent restart or a
configuration reload.
