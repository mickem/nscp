#### About `check_installed_software`

`check_installed_software` inventories installed software and answers three
operator questions:

- **"Is unwanted or EOL software present?"** — `crit=name like 'BitTorrent'`
  (an empty match set is OK, so an absence probe is cheap).
- **"What was installed recently?"** — `warn=install_date > -7d` correlates
  incidents with fresh installs.
- **"What is installed at all?"** — a bare call is an OK inventory with the
  package count as perf data.

Each installed product is one row. There are no default thresholds (a bare call
is an inventory), an empty match set returns OK, and the matched package count
is emitted as `count` perf data. The shared keywords (`name`, `version`,
`publisher`, `install_date`, `size`, `architecture`) carry the same meaning on
both platforms; `version` comparisons are plain string comparisons everywhere,
so pin patterns (e.g. `version like '7.'`) rather than relying on numeric
ordering across multi-digit components.

##### Windows

Reads the registry Uninstall hives —
`HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall` in **both** the
64-bit and 32-bit (Wow6432Node) views, plus every loaded per-user hive under
`HKEY_USERS` (which covers per-user installs such as VS Code, JetBrains IDEs
and Electron apps, regardless of the account the service runs as). The `hive`
keyword says which view a row came from.

The default filter is `system_component = 0`, matching what Add/Remove Programs
shows; pass `filter=none` to include runtime/driver components.

**Caveats:** `InstallDate` is best-effort — many installers never write it (the
`install_date` expressions simply never match such entries), and MSI stamps it
on every repair/modify, not only the original install. Legacy patch entries
(children with `ParentKeyName`) and entries without a `DisplayName` are skipped.

##### Linux

Reads the system package manager — `dpkg-query` on Debian/Ubuntu, `rpm -qa` on
RHEL/Fedora/SUSE, `pacman -Q` on Arch (detected in that order, dpkg first
because Debian-family hosts frequently carry an rpm binary too). The `manager`
keyword says which one was used.

Only packages whose dpkg state is exactly `installed` are listed: removed
(`not-installed`), `config-files` leftovers and broken (`half-installed`,
`unpacked`) packages are skipped, while held packages (`hold ok installed`)
are kept. If the package-manager query itself fails, the check returns UNKNOWN
rather than an empty "no installed software found" inventory, so a broken
package database can never read as a clean OK.

**Caveats:** install dates are exact on rpm (`INSTALLTIME`); dpkg does not
record them, so they are approximated from the mtime of the package's
`/var/lib/dpkg/info/<name>[:<arch>].list` file (rewritten on upgrade — treat as
"last installed/upgraded"). `pacman -Q` exposes only name and version, so
`publisher`, `size` and `install_date` stay unset there.
