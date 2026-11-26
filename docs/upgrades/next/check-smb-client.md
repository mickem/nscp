---
icon: "🗄️"
modules: [CheckDisk]
action: none
---
**New check: `check_smb_client` (Windows).** The client side of SMB: one row
per remote share the host has open (`\\server\share`), read from the
`SMB Client Shares` performance counter set (Windows 8 / Windows Server 2012
or later). Application servers whose data lives on a file server or a
Scale-Out File Server can now be alerted on the storage path they actually
use, which no local disk check sees. Keywords cover `read_latency`,
`write_latency` and `total_latency` (milliseconds), `latency` (the slower of
read and write), `credit_stalls` (per second), `queue_length`,
`avg_queue_length`, request and byte rates, and `server`/`share`. The defaults
warn at `latency > 25` or any credit stall and go critical at `latency > 50`;
`-w`/`-c` bound `latency`. `IPC$` (named pipes) is left out by the default
filter, and a host with no share open is OK (`empty-state=critical` makes it an
alert). The check samples for one second and is marked experimental.
