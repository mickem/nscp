#### About `check_registry_value`

`check_registry_value` inspects registry **values** — their type, contents and
size. Use [`check_registry_key`](#check_registry_key) when you care about the
key itself rather than what is in it.

At least one `key=` is required (repeatable). `value=` restricts the check to
specific value names; omit it, or pass `value=*`, to enumerate every value in
the key. The unnamed default value is reported as `(default)`. The default
critical threshold is `not exists`, so naming a value and running the check bare
is an existence probe.

##### Reading the value

Two keywords carry the contents, and picking the right one matters:

- **`string_value`** is the rendered form and works for every type. Use it for
  `REG_SZ`, `REG_EXPAND_SZ` and `REG_MULTI_SZ`, and for matching with `like`.
- **`int_value`** is the numeric value of a `REG_DWORD` or `REG_QWORD`, and is
  `0` for every other type. That zero is a trap: a threshold like
  `crit=int_value = 0` fires on any string value too, so pair it with a `type=`
  guard when the key might hold something unexpected.

`type` compares against the registry type names (`REG_SZ`, `REG_DWORD`, …), and
`size` is the raw byte size of the data.

This is the check for verifying that a policy or product setting actually holds
the value it is supposed to:

```
check_registry_value key=HKLM\SYSTEM\CurrentControlSet\Control\Lsa value=RunAsPPL "crit=int_value != 1"
check_registry_value key=HKLM\SOFTWARE\MyApp value=LogLevel "crit=string_value != 'INFO'"
```

##### Enumeration, views and remote hosts

`recursive=true` walks values in sub-keys as well, bounded by `max-depth=`
(unlimited by default under `recursive`); `exclude=` drops value names during
enumeration. `view=` selects the 32-bit or 64-bit registry view — the usual
cause of a value that is "missing" from the check but visible in regedit — and
`computer=` reads a remote machine's registry, which needs the Remote Registry
service and rights on the target.

`empty-state` is `unknown`, so an enumeration that matches nothing reports
UNKNOWN rather than OK.
