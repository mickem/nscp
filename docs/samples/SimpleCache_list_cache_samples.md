**List the keys currently held:**

```
list_cache
UNKNOWN: nightly_backup
```

Note the status: **`list_cache` always returns UNKNOWN**, including on a
successful listing. Read the message, not the status, and do not wire this
command up as an alerting check.

**An empty cache:**

```
list_cache
UNKNOWN:
```

**Debugging a `check_cache` miss:**

This is what the command is for. When a lookup reports `Entry not found`, list
the keys and compare them with the one the reader is assembling — a mismatched
`primary index` expression, a host name in a different form, or an empty alias
on submission is usually obvious at a glance.

```
check_cache command=nightly_backup
UNKNOWN: Entry not found

list_cache
UNKNOWN: srv01-nightly_backup
```

Here the writer's `primary index` includes `${host}` while the reader asked for
the command name alone.
