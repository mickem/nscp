**Check that an SSH server presents a valid banner:**

```
check_ssh host=github.com
OK: github.com:22 ok in 13ms
L        cli  Performance data: 'github.com_22_time'=13;1000;5000
```

**Non-standard SSH port:**

```
check_ssh host=192.168.56.10 port=2222
OK: 192.168.56.10:2222 ok in 2ms
```

**A port that is not speaking SSH is CRITICAL (`no_match`):**

```
check_ssh host=www.google.com port=443
CRITICAL: www.google.com:443 no_match in 12ms
```

**Tighter response-time thresholds:**

```
check_ssh host=192.168.56.10 "warn=time > 200" "crit=time > 1000 or result != 'ok'"
OK: 192.168.56.10:22 ok in 3ms
```

**Check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_ssh --argument "host=192.168.56.10"
OK: 192.168.56.10:22 ok in 2ms
```
