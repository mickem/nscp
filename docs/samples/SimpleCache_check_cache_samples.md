**Put something in the cache first:**

`check_cache` only reads; something has to submit a result on the `CACHE`
channel. Here `check_and_forward` runs a check and forwards its result:

```
check_and_forward command=check_ok "arguments=message=backup finished" channel=CACHE alias=nightly_backup
OK: Message submitted: CACHE
```

**Read it back by key:**

The submitted result comes back verbatim — the same status and message the
original check produced.

```
check_cache key=nightly_backup
OK: backup finished
```

**Or let the key be assembled from its parts:**

With the default `primary index` of `${alias-or-command}`, naming the command is
equivalent to naming the key.

```
check_cache command=nightly_backup
OK: backup finished
```

Mixing the two forms does not work: an explicit `key=` wins and `host=`,
`command=`, `channel=` and `alias=` are ignored.

**A miss:**

The defaults report UNKNOWN, which a monitoring server can distinguish from a
genuine OK — "nobody has reported" is not the same as "reported healthy".

```
check_cache key=nothing
UNKNOWN: Entry not found
```

**Change what a miss looks like:**

```
check_cache key=nothing "not-found-msg=No result submitted in this cycle" not-found-code=critical
CRITICAL: No result submitted in this cycle
```

**With no key at all it is a syntax error, not a miss:**

```
check_cache
UNKNOWN: No key specified	...
```

**The cache does not survive a restart:**

It is held in memory only, so a freshly started agent answers every lookup with
the not-found result until submissions start arriving again.

```
check_cache key=nightly_backup
UNKNOWN: Entry not found
```
