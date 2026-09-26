---
icon: "🍎"
modules: [CheckSystem]
action: none
---
**`CheckSystem` runs on macOS.** Nothing to do on Linux or Windows. The macOS
package now carries all 21 of the module's Linux commands - `check_cpu`,
`check_memory`, `check_process`, `check_service`, `check_uptime`,
`check_network`, `check_load`, `check_os_version`, `check_os_updates`,
`check_installed_software`, `check_battery` and the rest - plus the real-time
filters, the system metrics on the dashboard and the host, network and
software fact sets. A Linux `nsclient.ini` works as it is: the section is still
`/settings/system/unix` and the keywords are the same.

The data comes from sysctl, the Mach host statistics, libproc, IOKit and
`launchctl` instead of procfs, and a few values have no Darwin counterpart.
Those are reported as absent - the keyword renders `unknown`, never satisfies
a threshold and emits no performance data - or the check returns UNKNOWN,
never a zero that was not measured:

* `check_process` cannot read the memory, fault and CPU counters or the
  command line of another user's process unless the agent runs as root, and
  macOS keeps no peak sizes. Those keywords are optional now on every platform;
  Linux fills them as before.
* `check_service` checks launchd jobs by label (`service=com.apple.logd`).
  An idle job launchd starts on demand is `start_type` `on-demand` and state
  `static`, so it passes the default thresholds. A new `has_metrics` keyword
  says whether `rss`, `vms`, `cpu` and `tasks` were measured.
* `check_installed_software` lists installer receipts, application bundles and
  Homebrew kegs; the `manager` keyword says which (`pkgutil`, `bundle`,
  `homebrew`).
* `check_os_updates` reads the list macOS cached at its last background check.
  The new `live=true` option asks Apple's update server instead, which takes 10
  to 60 seconds. The new `last_checked` keyword is when the list was refreshed.
* `check_temperature` and `check_cpu_frequency` are always UNKNOWN, and
  `check_kernel_stats` has only its `threads` row. `check_kernel_memory` has no
  slab, and reports the new `wired` and `compressed` keywords instead; on Linux
  those two are `unknown`.

The full list is in [Installing on
macOS](installing.md#what-is-not-in-the-macos-build-yet).

One fix reaches Linux too: on a machine with more than ten cores, `check_cpu`,
the real-time CPU filters and the per-core metrics reported the load of core 10
and up under the wrong core numbers. Each core now keeps its own number.
