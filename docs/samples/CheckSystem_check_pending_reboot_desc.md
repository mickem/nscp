#### About `check_pending_reboot`

`check_pending_reboot` answers a question no single OS API does: **is this
machine waiting for a reboot, and why?** A pending reboot is signalled
independently by several subsystems, so the check reads each one and reports the
union. This is the reliable way to catch servers that have applied updates but
will not finish patching until they restart.

The aggregate keywords — `pending`, `signals`, `written`, `written_s`, `age`,
`reasons` and `message` — mean the same on Windows and Linux, so
`crit=pending = 1 and age > 7d` ports across platforms. The per-cause keywords
are each platform's own, because the causes are (see below).

The default threshold is `warn=pending = 1` (WARNING whenever a reboot is
pending, no critical). Override it to escalate, to alert only on specific causes
(e.g. `crit=servicing = 1` on Windows, `crit=kernel = 1` on Linux), or to
suppress the default with `warn=none`. The check always returns a single
aggregate row, so there is no empty state.

##### How long has the reboot been pending?

A reboot queued minutes ago by an update is expected; one still queued days
later is usually the actionable case. The `age` and `written` keywords expose
how long the reboot has been pending, e.g.
`crit=pending = 1 and age > 7d`. On Windows the time comes from the last-write
time of the Component Based Servicing / Windows Update registry key (each exists
only while its reboot is queued). On Linux it is the modification time of the
marker file and the time the first kernel newer than the running one was
written to disk (its image's ctime; the mtime is the package's build time). When several signals are set the
oldest one wins. Three caveats:

- On Windows only those two signals carry a timestamp. A reboot signalled
  solely by a pending file rename, computer rename or domain join reports
  `age`/`written` as `unknown`, which never trips a numeric threshold
  (test for it explicitly with `written = 'unknown'`).
- Threshold staleness with a duration on `age` (`age > 7d`) or a *relative*
  date on `written` (`written < -7d` — the signal appeared more than seven
  days ago). A quoted date string (`written < '2026-08-01 00:00:00'`) is
  **not** parsed as a date: it is compared as text against the raw timestamp
  and will not order correctly. The only quoted comparison with a defined
  meaning is the `written = 'unknown'` probe.
- The registry records the key's *last* write, not its creation: if servicing
  re-touches the key while the reboot is still queued, `age` restarts. The same
  holds for the Linux marker, which every package hook that asks for a reboot
  touches again. Treat it as "pending at least this long since the last signal
  update".

##### Windows

Signals: `servicing` (Component Based Servicing), `windows_update`,
`file_rename` (`PendingFileRenameOperations`), `computer_rename` and
`domain_join`. All are read from the 64-bit registry view, so a 32-bit agent
under WOW64 still reads the native keys.

##### Linux

Two signals, each a cheap file-system read (no package-manager or helper
process is run):

- `marker` — the distribution's reboot marker is present:
  `/run/reboot-required` on Debian and Ubuntu (written by the package hooks of
  the kernel, libc, dbus and friends; the packages that asked are listed in
  `/run/reboot-required.pkgs` and exposed as `packages`), or
  `/run/reboot-needed` on SUSE (written by zypper; what
  `zypper needs-rebooting` tests).
- `kernel` — the next boot changes kernel: a newer kernel *of the running
  kernel's product line* is installed, or the running kernel has been removed
  (pacman deletes its modules on every Arch upgrade; the reason then reads
  `running kernel ... is no longer installed`). This covers the RHEL family,
  which has no marker file. `running_kernel` and `newest_kernel` show what was
  compared.

**What counts as an installed kernel.** Only a release with both a modules
directory (`/lib/modules/<release>` or `/usr/lib/modules/<release>`) and an
image — `/lib/modules/<release>/vmlinuz` (RHEL 8+, Fedora, Arch) or
`/boot/vmlinuz-<release>` (every distribution). A stray copy in `/boot`
(`vmlinuz-<release>.orig`, `.old`, a rescue image) has no modules directory, and
a DKMS leftover directory has no image, so neither is a kernel. A kernel
installed with an image in `/boot` but no modules directory at all is not
seen; distribution kernels always ship one.

**Product lines.** Releases are compared as versions, not as text (`6.1.0-18`
is newer than `6.1.0-9`; `5.14.0-427.13.1.el9_4` newer than `5.14.0-427.el9`),
and only within the running kernel's product line, so a deliberately installed
second line does not warn forever while successive builds of one line do:

| Distribution | Separate lines |
|---|---|
| RHEL family, Fedora, Amazon Linux | stock, Oracle UEK (`.el9uek`), real-time (`.rt…` on EL8, `+rt` on EL9), ELRepo (`.elrepo`), `+debug`, `+64k`, each architecture. Minor-release dist tags (`el9` / `el9_5`, `fc40` / `fc41`) are one line |
| Debian, Ubuntu, SUSE, Raspberry Pi OS | the flavour after the ABI number: `-amd64` / `-cloud-amd64` / `-rt-amd64`, `-generic` / `-lowlatency` / `-generic-64k`, `-default` |
| Arch | `linux`, `linux-lts`, `linux-zen`, `linux-hardened` |

A release that cannot be placed in a line raises no kernel signal: a missed
update is preferred to a warning that never clears.

**Containers.** A container shares the host's kernel and does not own it.
When this root is a container's — `/run/systemd/container`,
`/run/.containerenv`, `/.dockerenv`, OpenVZ's `/proc/vz`, `container=` in PID
1's environment, or an overlay root filesystem — `kernel` is never set, even if
the image happens to carry a kernel package. A container reports "No reboot
pending" unless its image carries a marker file.

**What could not be read is not "clear".** When a modules directory exists but
cannot be listed, or a kernel image that would decide the answer cannot be
looked up (a `/boot` the agent may not read, or a `/boot` that is not mounted so
even the running kernel's image is missing), `kernel` is `unknown` and the
message says why. A `marker` that cannot be looked up is `unknown` the same way.
An `unknown` value never matches a numeric threshold; test for it with
`kernel = 'unknown'`. If no other signal says a reboot is pending, the check
then returns **UNKNOWN** ("Cannot tell whether a reboot is pending: ...")
instead of "No reboot pending"; pass `allow-unreadable` to report the readable
signals anyway. A signal that did fire is always reported. On the RHEL family,
Fedora and Arch the images are in the world-readable modules directories, so
an unprivileged agent never needs `/boot`; on Debian and Ubuntu, where the
image is only in `/boot`, `/boot` is world-readable by default.

**When it became pending.** For the kernel signal `written` is the install time
of the *first* kernel newer than the running one (the ctime of its image), so a
host that keeps installing updates without rebooting does not restart its
`age`.

Not covered on purpose: `needs-restarting -r` (dnf/yum) and `needrestart` are
slow Python/Perl helpers that may not be installed; the kernel signal covers the
case that matters most without them. Libraries that were upgraded while still
mapped by running processes are a service-restart question, not a reboot one,
and too noisy to alert on.

##### macOS

Not supported yet: the check returns `UNKNOWN`.
