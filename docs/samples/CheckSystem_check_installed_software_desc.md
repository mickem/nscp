#### About `check_installed_software`

`check_installed_software` inventories installed software from the registry
Uninstall hives — `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall` in
**both** the 64-bit and 32-bit (Wow6432Node) views, plus every loaded per-user
hive under `HKEY_USERS` (which covers per-user installs such as VS Code,
JetBrains IDEs and Electron apps, regardless of the account the service runs
as).

It answers three operator questions:

- **"Is unwanted or EOL software present?"** — `crit=name like 'BitTorrent'`,
  `crit=version < '8'` (an empty match set is OK, so an absence probe is cheap).
- **"What was installed recently?"** — `warn=install_date > -7d` correlates
  incidents with fresh installs.
- **"What is installed at all?"** — a bare call is an OK inventory with the
  package count as perf data.

The default filter is `system_component = 0`, matching what Add/Remove Programs
shows; pass `filter=none` to include runtime/driver components. There are no
default thresholds (a bare call is an inventory), an empty match set returns OK,
and the matched package count is emitted as `count` perf data.

**Caveats:** `InstallDate` is best-effort — many installers never write it (the
`install_date` expressions simply never match such entries), and MSI stamps it
on every repair/modify, not only the original install. `version` comparisons
are plain string comparisons, so pin patterns accordingly (e.g.
`version like '7.'`) rather than relying on numeric ordering across
multi-digit components. Legacy patch entries (children with `ParentKeyName`)
and entries without a `DisplayName` are skipped. On Linux the same command is
provided by the unix CheckSystem module, backed by dpkg/rpm/pacman, with the
shared keywords (`name`, `version`, `publisher`, `install_date`, `size`,
`architecture`) carrying the same meaning.
