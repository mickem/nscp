#### About `check_dhcp_server`

`check_dhcp_server` reads the *DHCP Server* performance counter set, sampled
twice a second apart so the per-second rates carry real values: the DORA
exchange (`discovers`, `offers`, `requests`, `acks`), the refusals and
conflicts (`nacks`, `declines`), `informs`, `releases`, the packets received,
dropped as duplicates and expired in the queue, and the two queue lengths
(`active_queue`, `conflict_queue`). Every counter is written as perfdata, so
the check doubles as the source for a DHCP traffic graph.

Defaults: **WARNING** when `active_queue > 50 or declines > 0 or nacks > 10`,
**CRITICAL** when `active_queue > 200`.

- A **DHCPDECLINE** is a client reporting that the address it was offered is
  already in use on the network — an address conflict: a static address inside
  a scope's range, or a second DHCP server handing out the same range. Worth a
  look whenever it happens.
- A few **NAKs** are routine (a laptop asking for the address it had on another
  network); only a sustained rate points at a misconfigured scope or relay.
- The **active queue** is the overload signal: packets the server has not got
  to yet. `packets_expired` above 0 means it is already dropping them.

`-w`/`-c` bound `active_queue` and keep the `declines` and `nacks` conditions
of the default warning.

The rates are the average over that one-second sample, so a quiet server reads
0 most of the time and a short burst can be missed. For trends, graph the
perfdata; for alerting on scope exhaustion use `check_dhcp_scopes`.

**No-DHCP-server contract:** on a host without the DHCP Server role the counter
set does not exist and the check returns **UNKNOWN** with "DHCP Server
performance counters are not available - is the DHCP Server role installed on
this host?".
