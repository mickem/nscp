**Pinging a single host:**

```
check_ping host=192.168.0.1
OK: All 1 hosts are ok|'192.168.0.1_loss'=0%;5;10 '192.168.0.1'=2ms;60;100
```

**Pinging multiple hosts (repeat `host=`) with a total bucket:**

```
check_ping host=1.1.1.1 host=8.8.8.8 host=google.com total
L        cli OK: All 4 hosts are ok
L        cli  Performance data: '1.1.1.1_loss'=0%;5;10 '1.1.1.1'=3ms;60;100 '8.8.8.8_loss'=0%;5;10 '8.8.8.8'=9ms;60;100 'google.com_loss'=0%;5;10 'google.com'=2ms;60;100 'total_loss'=0%;5;10 'total'=14ms;60;100
```

**Tighter thresholds with explicit count and timeout:**

```
check_ping host=8.8.8.8 count=4 timeout=300 "warn=time > 30 or loss > 0%" "crit=time > 80 or loss > 25%"
L        cli OK: All 1 hosts are ok
L        cli  Performance data: '8.8.8.8_loss'=0%;0;25 '8.8.8.8'=2ms;30;80
```

**Custom payload and per-host text output:**

```
check_ping host=1.1.1.1 host=8.8.8.8 payload="hello" "top-syntax=%(status): %(list)" "detail-syntax=%(host)=%(time)ms"
L        cli OK: 1.1.1.1=2ms, 8.8.8.8=2ms
L        cli  Performance data: '1.1.1.1_loss'=0%;5;10 '1.1.1.1'=2ms;60;100 '8.8.8.8_loss'=0%;5;10 '8.8.8.8'=2ms;60;100
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_ping --argument "host=192.168.56.1"
OK: All 1 hosts are ok|'192.168.56.1_loss'=0%;5;10 '192.168.56.1'=1ms;60;100
```
