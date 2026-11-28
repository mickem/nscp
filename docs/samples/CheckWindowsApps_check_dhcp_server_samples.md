**Default check (warns on any decline, more than 10 NAKs a second or 50 queued packets; critical at 200 queued):**

```
check_dhcp_server
```

**Only alert on server overload, through the short thresholds (the decline and NAK conditions stay):**

```
check_dhcp_server w=20 c=100
```

**Alert when packets start expiring in the queue:**

```
check_dhcp_server "warning=packets_expired > 0" "critical=active_queue > 200"
```

**Ignore NAKs entirely (a network with many roaming clients):**

```
check_dhcp_server "warning=declines > 0 or active_queue > 50"
```
