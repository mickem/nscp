**Default check (queue depth + printer errors):**

```
check_printqueue
OK: All 6 printer(s) ok.
```

**Default check with a backed-up or errored queue:**

```
check_printqueue
CRITICAL: HP LaserJet: printing, 3 job(s)
```

**Alert on offline printers too (typical for a print server):**

```
check_printqueue "crit=error = 1 or offline = 1"
CRITICAL: HP LaserJet: offline, 0 job(s)
```

**Alert on a stuck queue — a job waiting more than 30 minutes:**

```
check_printqueue "warn=jobs > 10 or oldest_job_age > 30m"
WARNING: HP LaserJet: printing, 2 job(s)
```

**Check one specific printer:**

```
check_printqueue "filter=printer = 'HP LaserJet'" "crit=offline = 1 or error = 1"
OK: All 1 printer(s) ok.
```

**Custom output with full per-printer detail:**

```
check_printqueue "top-syntax=%(status): %(list)" "detail-syntax=%(printer): %(status)/%(error_state) jobs=%(jobs) oldest=%(oldest_job_age)s offline=%(offline)"
OK: HP LaserJet: idle/no_error jobs=0 oldest=-1s offline=0, Microsoft Print to PDF: idle/no_error jobs=0 oldest=-1s offline=0
```

**Over NRPE against a print server:**

```
check_nscp_client --host 192.168.56.103 --command check_printqueue --argument "crit=error = 1 or offline = 1"
OK: All 4 printer(s) ok.
```
