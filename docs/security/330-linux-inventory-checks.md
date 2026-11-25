---
title: "Linux inventory checks expose patch level and hardware identifiers"
fixed_in: next
severity: "Informational"
modules: [CheckSystem]
action: conditional
---
`check_pending_reboot` in `CheckSystem`, which was Windows-only, now returns
data on Linux. Anyone allowed to run checks on the agent (REST, NRPE, or any
other enabled transport) can now read what it reports:

- `check_pending_reboot`: the running kernel release, the newest installed
  kernel release and the package names in `/run/reboot-required.pkgs`, which
  together show the host's patch level.

The check needs no privileges: the files it reports from are world-readable,
and it runs no external process. When the agent runs as root it also reads
PID 1's environment to recognise a container; nothing from it is reported. A
message saying the kernel inventory could not be read names the path and the
error. The same check already exposes the equivalent data
on Windows.

**What to do:** nothing if only trusted monitoring servers can run checks.
Otherwise restrict who can reach this command (`allowed hosts`, REST roles).
