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
marker file and the time the newer kernel's image was written to disk (its
ctime; the mtime is the package's build time). When several signals are set the
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
- `kernel` — a newer kernel *of the running kernel's flavour* is installed than
  the one running, so the next boot changes kernel. Installed kernels are the
  images in `/boot/vmlinuz-<release>` and `/lib/modules/<release>/vmlinuz` (the
  RHEL 8+/Fedora layout); rescue images and `*.old` copies are ignored, and so
  is a `/lib/modules` directory without an image (a DKMS leftover). Releases
  are compared as versions, not as text (`6.1.0-18` is newer than `6.1.0-9`;
  `5.14.0-427.13.1.el9_4` newer than `5.14.0-427.el9`), and only within the
  running kernel's flavour (`-generic` vs `-lowlatency`, `-amd64` vs
  `-cloud-amd64`, RHEL's `+debug`), so a deliberately installed second flavour
  does not warn forever. This covers the RHEL family, which has no marker file.
  `running_kernel` and `newest_kernel` show what was compared.

In a container there is no kernel package for the host's running kernel (no
`/lib/modules/<uname -r>`), so `kernel` is never set there — a kernel package
installed in an image is not compared with the host's kernel — and a container
reports "No reboot pending" unless its image carries a marker file.

Not covered on purpose: `needs-restarting -r` (dnf/yum) and `needrestart` are
slow Python/Perl helpers that may not be installed; the kernel signal covers the
case that matters most without them. Libraries that were upgraded while still
mapped by running processes are a service-restart question, not a reboot one,
and too noisy to alert on.

##### macOS

Not supported yet: the check returns `UNKNOWN`.
