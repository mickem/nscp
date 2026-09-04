#### About `check_registry_key`

`check_registry_key` inspects registry **keys** — whether they exist, when they
were last written, and how many values and sub-keys they hold. Use
[`check_registry_value`](#check_registry_value) when you care about the contents
of a specific value instead.

At least one `key=` is required (repeatable), given as a full path including the
hive, e.g. `HKLM\Software\MyApp`. The default critical threshold is
`not exists`, so a bare call is an existence probe: name a key and the check
goes critical if it is missing.

##### Existence as a policy check

The `exists` keyword is the reason this check earns its place: a great deal of
Windows configuration is "this key is present" or "this key is absent". Both
directions are one expression:

```
check_registry_key key=HKLM\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate
check_registry_key key=HKLM\SOFTWARE\SomeVendor "crit=exists = 1"
```

Note that `empty-state` is `unknown`, so a check that matches nothing at all
reports UNKNOWN rather than a misleading OK.

##### Change detection

`written` is the key's last-write timestamp (epoch seconds, comparable as a
date) and `age` the seconds since. Together they turn the check into a
tamper/drift probe for keys that are supposed to be stable:

```
check_registry_key key=HKLM\SYSTEM\CurrentControlSet\Services\MyService "crit=age < 1d"
```

Be aware that Windows updates a key's last-write time for changes to its
*values* as well as its sub-keys, and that the timestamp is not maintained for
every hive with the same fidelity — treat it as a strong hint rather than an
audit record.

##### Enumeration, views and remote hosts

`recursive=true` walks the sub-keys below each starting key, bounded by
`max-depth=` (`-1`, the default under `recursive`, means unlimited). Without
`recursive` only the named key itself is examined. `exclude=` drops sub-keys by
name during enumeration.

`view=` selects the registry view on 64-bit Windows: `default`, `32`
(`KEY_WOW64_32KEY`) or `64` (`KEY_WOW64_64KEY`). This matters more often than it
looks — a 32-bit installer writes under `Wow6432Node`, and a check that does not
pin the view can report "missing" for a key that is plainly there in regedit.

`computer=` connects to a remote machine's registry, which requires the Remote
Registry service to be running there and appropriate rights; running the check
locally on the monitored host is both faster and easier to secure.
