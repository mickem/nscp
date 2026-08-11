#### About `check_installed_software`

`check_installed_software` inventories installed packages via the system
package manager — `dpkg-query` on Debian/Ubuntu, `rpm -qa` on RHEL/Fedora/SUSE,
`pacman -Q` on Arch (detected in that order, dpkg first because Debian-family
hosts frequently carry an rpm binary too). It is the unix counterpart to the
Windows registry-based `check_installed_software`; the shared keywords (`name`,
`version`, `publisher`, `install_date`, `size`, `architecture`) carry the same
meaning on both platforms.

It answers three operator questions:

- **"Is unwanted or EOL software present?"** — `crit=name like 'openjdk-7'`
  (an empty match set is OK, so an absence probe is cheap).
- **"What was installed recently?"** — `warn=install_date > -7d` correlates
  incidents with fresh installs.
- **"What is installed at all?"** — a bare call is an OK inventory with the
  package count as perf data.

Keywords (one row per installed package):

| Keyword          | Description                                                                         |
|------------------|-------------------------------------------------------------------------------------|
| `name`           | Package name                                                                        |
| `version`        | Version string (rpm: `version-release`); comparisons are lexical, not version-aware |
| `publisher`      | Maintainer (dpkg, email stripped) / vendor (rpm); empty for pacman                  |
| `install_date`   | Install date; supports date expressions (`install_date > -30d`). Unset when unknown |
| `install_date_s` | Install date as `YYYY-MM-DD`; empty when unknown                                    |
| `size`           | Installed size in bytes; 0 when not recorded                                        |
| `architecture`   | Package architecture (`amd64`, `x86_64`, `noarch`, ...)                             |
| `manager`        | Package manager the entry came from (`dpkg`, `rpm`, `pacman`)                       |
| `status`         | Package state; always `installed` for listed packages                               |

There are no default thresholds (a bare call is an inventory), an empty match
set returns OK, and the matched package count is emitted as `count` perf data.
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
`publisher`, `size` and `install_date` stay unset there. `version` comparisons
are plain string comparisons, so pin patterns (e.g. `version like '1.2.'`)
rather than relying on numeric ordering across multi-digit components.
