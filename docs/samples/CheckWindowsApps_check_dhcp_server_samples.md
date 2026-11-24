**Default check:**

```
check_dhcp_server
OK: 1 discovers/s, 1 offers/s, 2 requests/s, 2 acks/s, 0 nacks/s, 0 declines/s, 0 queued|'dhcp_acks'=2 'dhcp_active_queue'=0;50;200 'dhcp_conflict_queue'=0 'dhcp_declines'=0;0 'dhcp_discovers'=1 ...
```

**An address conflict (a client declined the offered address):**

```
check_dhcp_server
WARNING: 1 discovers/s, 1 offers/s, 2 requests/s, 2 acks/s, 0 nacks/s, 2 declines/s, 0 queued|...
```

**Only alert on server overload, through the short thresholds:**

```
check_dhcp_server w=20 c=100
```

**Alert when packets start expiring in the queue:**

```
check_dhcp_server "warning=packets_expired > 0" "critical=active_queue > 200"
```

**On a host without the DHCP Server role:**

```
check_dhcp_server
DHCP Server performance counters are not available - is the DHCP Server role installed on this host? (...)
```
