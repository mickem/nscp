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
cannot be parsed are excluded from the `age` calculation. Treat `age` as "days
since the newest *dated* hotfix", not an exact patch SLA clock.

When *no* hotfix has a parseable date, `age` has no value: it renders as
`unknown`, emits no perfdata, and every numeric comparison on it is false. An
age threshold on its own therefore stays quiet on such a host. The `age_known`
flag (`1` when the age is known, `0` when it is not) is how you alert on it:

```
check_patch_age "warn=age > 40" "crit=age > 90 or age_known = 0"
```
