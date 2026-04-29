**Default check (uses the `total` bucket):**

```
check_connections
L        cli OK: OK: total/all: 226
L        cli  Performance data: 'total_all_close_wait'=0;0;0 'total_all_closing'=0;0;0 'total_all_established'=90;0;0 'total_all_fin_wait'=0;0;0 'total_all_last_ack'=0;0;0 'total_all_listen'=69;0;0 'total_all_syn_recv'=0;0;0 'total_all_syn_sent'=0;0;0 'total_all_time_wait'=6;0;0 'total_all_total'=226;0;0 'total_all_udp'=61;0;0
```

**Per-protocol breakdown (disable the default total filter):**

```
check_connections "filter=state = 'all'" "top-syntax=%(status): %(list)" "detail-syntax=%(protocol)/%(family)=%(count)"
L        cli OK: OK: tcp/ipv4=157, tcp6/ipv6=15, udp/ipv4=40, udp6/ipv6=21, total/any=233
L        cli  Performance data: 'tcp_all_close_wait'=0;0;0 'tcp_all_closing'=0;0;0 'tcp_all_established'=0;0;0 'tcp_all_fin_wait'=0;0;0 'tcp_all_last_ack'=0;0;0 'tcp_all_listen'=0;0;0 'tcp_all_syn_recv'=0;0;0 'tcp_all_syn_sent'=0;0;0 'tcp_all_time_wait'=0;0;0 'tcp_all_total'=0;0;0 'tcp_all_udp'=0;0;0 'tcp6_all_close_wait'=0;0;0 'tcp6_all_closing'=0;0;0 'tcp6_all_established'=0;0;0 'tcp6_all_fin_wait'=0;0;0 'tcp6_all_last_ack'=0;0;0 'tcp6_all_listen'=0;0;0 'tcp6_all_syn_recv'=0;0;0 'tcp6_all_syn_sent'=0;0;0 'tcp6_all_time_wait'=0;0;0 'tcp6_all_total'=0;0;0 'tcp6_all_udp'=0;0;0 'udp_all_close_wait'=0;0;0 'udp_all_closing'=0;0;0 'udp_all_established'=0;0;0 'udp_all_fin_wait'=0;0;0 'udp_all_last_ack'=0;0;0 'udp_all_listen'=0;0;0 'udp_all_syn_recv'=0;0;0 'udp_all_syn_sent'=0;0;0 'udp_all_time_wait'=0;0;0 'udp_all_total'=0;0;0 'udp_all_udp'=0;0;0 'udp6_all_close_wait'=0;0;0 'udp6_all_closing'=0;0;0 'udp6_all_established'=0;0;0 'udp6_all_fin_wait'=0;0;0 'udp6_all_last_ack'=0;0;0 'udp6_all_listen'=0;0;0 'udp6_all_syn_recv'=0;0;0 'udp6_all_syn_sent'=0;0;0 'udp6_all_time_wait'=0;0;0 'udp6_all_total'=0;0;0 'udp6_all_udp'=0;0;0 'total_all_close_wait'=1;0;0 'total_all_closing'=0;0;0 'total_all_established'=93;0;0 'total_all_fin_wait'=0;0;0 'total_all_last_ack'=0;0;0 'total_all_listen'=69;0;0 'total_all_syn_recv'=0;0;0 'total_all_syn_sent'=0;0;0 'total_all_time_wait'=9;0;0 'total_all_total'=233;0;0 'total_all_udp'=61;0;0
```

**Show only TCP states:**

```
check_connections "filter=protocol = 'tcp' and state != 'all'" "top-syntax=%(status): %(list)" "detail-syntax=%(state)=%(count)"
check_connections "filter=protocol = 'tcp' and state != 'all'" "top-syntax=%(status): %(list)" "detail-syntax=%(state)=%(count)"
L        cli OK: OK: ESTABLISHED=92, LISTEN=69, TIME_WAIT=9
```

**Warn/critical based on total connections:**

```
check_connections "warn=total > 500" "crit=total > 1000"
L        cli OK: OK: total/all: 231
```

**Warn when many sockets are stuck in TIME_WAIT:**

```
check_connections "filter=protocol = 'tcp' and state = 'TIME_WAIT'" "warn=count > 200" "crit=count > 1000"
L        cli OK: OK: tcp/TIME_WAIT: 14
```

**Alert on growing CLOSE_WAIT (often indicates leaks):**

```
check_connections "filter=state = 'CLOSE_WAIT'" "warn=count > 50" "crit=count > 200"
L        cli OK: No connection data
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_connections
OK: total/all: 231|'total_all_close_wait'=0;0;0 'total_all_closing'=0;0;0 'total_all_established'=85;0;0 'total_all_fin_wait'=0;0;0 'total_all_last_ack'=0;0;0 'total_all_listen'=69;0;0 'total_all_syn_recv'=0;0;0 'total_all_syn_sent'=1;0;0 'total_all_time_wait'=16;0;0 'total_all_total'=231;0;0 'total_all_udp'=60;0;0
```

