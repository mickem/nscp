---
title: "Linux inventory checks expose patch level and hardware identifiers"
fixed_in: next
severity: "Informational"
modules: [CheckSystem]
action: conditional
---
Two checks in `CheckSystem` that were Windows-only, `check_pending_reboot` and
`check_hardware`, now return data on Linux. Anyone allowed to run checks on the
agent (REST, NRPE, or any other enabled transport) can now read what they
report:

- `check_pending_reboot`: the running kernel release, the newest installed
  kernel release and the package names in `/run/reboot-required.pkgs`, which
  together show the host's patch level. It needs no privileges: the files it
  reports from are world-readable, and it runs no external process. When the
  agent runs as root it also reads PID 1's environment to recognise a
  container; nothing from it is reported. A message saying the kernel
  inventory could not be read names the path and the error.
- `check_hardware`: vendor, model, chassis type, asset tag and installed
  memory, which any user can read. When the agent runs as root it also reports
  the system serial, the system UUID, the enclosure serial and the per-DIMM
  inventory, which the kernel keeps root-only.

The same checks already expose the equivalent data on Windows. An agent
running as an unprivileged user cannot read the root-only DMI files:
`check_hardware` then reports empty serials and UUID and an `unknown` module
count.

**What to do:** nothing if only trusted monitoring servers can run checks.
Otherwise restrict who can reach these commands (`allowed hosts`, REST roles).
