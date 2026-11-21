---
icon: "⏱️"
modules: [CheckSystem]
action: none
---
**`$INSTANCE$` counters now follow instances that come and go, and a departed
instance no longer slows the collector down.** Nothing to do. A counter such as
the default `disk_queue_length` (`\PhysicalDisk($INSTANCE$)\% Disk Time`) was
expanded once, when the collector added it to its query. On an RDS host with User Profile
Disks every sign-in attaches a `PhysicalDisk` and every sign-out detaches one:
each departed disk cost about a second on every tick of the 1 Hz collector,
for good (the `PDH collection took …ms, exceeding the 1-second cadence`
warning), and a disk attached later was never collected. The collector now
expands these counters again every minute, or after ten seconds once a counter
has had no data for three samples in a row. New instances are added, departed
ones are dropped along with their `pdh.<counter>.<instance>` metrics, and
instances that are still there keep their collected history (`rrd` counters). A counter without data costs
at most one shared one-second retry per tick, not one per counter, and no
retry at all once it had no data on the previous tick as well.
