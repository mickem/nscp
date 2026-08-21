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
check_printqueue "top-syntax=%(status): %(list)" "detail-syntax=%(printer): %(printer_status)/%(error_state) jobs=%(jobs) oldest=%(oldest_job_age)s offline=%(offline)"
OK: HP LaserJet: idle/no_error jobs=0 oldest=-1s offline=0, Microsoft Print to PDF: idle/no_error jobs=0 oldest=-1s offline=0
```

**Over NRPE against a print server:**

```
check_nscp_client --host 192.168.56.103 --command check_printqueue --argument "crit=error = 1 or offline = 1"
OK: All 4 printer(s) ok.
```

**Show the device behind each queue (driver, port, sharing):**

```
check_printqueue warning=none critical=none "top-syntax=${list}" "detail-syntax=${printer} [drv=${driver}] [port=${port}] def=${default} shared=${shared} net=${network}"
Microsoft Print to PDF [drv=Microsoft Print To PDF] [port=PORTPROMPT:] def=0 shared=0 net=0, HP Color LaserJet Pro MFP 4302 [drv=Microsoft IPP Class Driver] [port=WSD-7f7ab05a-2fe9-4ca8-84cb-2f4b45e3bc9a] def=1 shared=0 net=0
```

**Alert when a queue moves to an unexpected driver or port:**

```
check_printqueue "filter=printer = 'HP LaserJet'" "crit=driver != 'HP Universal Printing PCL 6'"
CRITICAL: HP LaserJet: idle, 0 job(s)
```

**Only look at the shared queues on a print server:**

```
check_printqueue "filter=shared = 1" "crit=error = 1 or offline = 1"
OK: All 4 printer(s) ok.
```
