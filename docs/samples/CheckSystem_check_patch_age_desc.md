#### About `check_patch_age`

`check_patch_age` reports the **installed** side of Windows patching — the
counterpart to `check_os_updates`, which reports what is still *pending*. It
enumerates installed hotfixes from `Win32_QuickFixEngineering` and answers the
two questions operators actually ask:

- **"When was this box last patched?"** — via `age`, the number of days since
  the newest hotfix was installed.
- **"Is KB\<n\> installed?"** — via the `hotfix=` option (vulnerability-response
  patch verification), or by testing the `ids` list directly.

The default threshold is `crit=missing > 0`, which is inert unless you pass one
or more `hotfix=` options (a bare number is matched with an implicit `KB`
prefix, so `hotfix=5034441` == `hotfix=KB5034441`). Age alerting is opt-in via
`warn=age > N` / `crit=age > N`.

**Caveat:** `Win32_QuickFixEngineering` reports only servicing-stack /
Component-Based-Servicing hotfixes (the `KB` list), not every cumulative-update
component, and its `InstalledOn` field is frequently blank or locale-formatted.
The check parses the common `M/D/YYYY` and `YYYYMMDD` forms; hotfixes whose date
cannot be parsed are excluded from the `age` calculation (and `age` is `-1` only
when *no* hotfix has a parseable date). Treat `age` as "days since the newest
*dated* hotfix", not an exact patch SLA clock.
