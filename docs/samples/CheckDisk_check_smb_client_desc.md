#### About `check_smb_client`

`check_smb_client` looks at SMB from the **client** side: one row per remote
share this host has open (`\\server\share`), read from the Windows
`SMB Client Shares` performance counter set (Windows 8 / Windows Server 2012
and later). It is the view an application server has of the file server or
Scale-Out File Server its data lives on. When a SQL Server database, a
Hyper-V VM or an IIS content directory sits on a share, a slow storage path
shows up here and nowhere else: the local disk counters `check_disk_io` reads
only cover local volumes.

The counter set is sampled twice, one second apart, so each run takes about a
second and the latencies and rates cover that second.

#### What the defaults alert on

| Condition                         | Status   |
|-----------------------------------|----------|
| `latency > 25` (ms)               | WARNING  |
| `credit_stalls > 0`               | WARNING  |
| `latency > 50` (ms)               | CRITICAL |
| no share open (the empty state)   | OK       |

- `latency` is the slower of `read_latency` and `write_latency`, so a
  read-mostly workload does not hide slow writes in the average
  (`total_latency`, the counter's own average over both, is there too).
- A **credit stall** is a request that had to wait because the server has not
  granted the client enough SMB credits for the load. A healthy share has
  none; a share that keeps stalling waits on the server whatever the network
  and the disks behind it do.
- `-w`/`-c` bound `latency`: `-w 10 -c 30` is `warning=latency > 10 or
  credit_stalls > 0`, `critical=latency > 30`.
- A share with no reads (or writes) in the sample reports a latency of 0, so
  an idle share never alerts on latency.

#### `IPC$` is left out

The default filter is `ipc = 0`. `IPC$` carries named pipes - RPC to a domain
controller, DFS referrals, the print spooler - and a pipe read waits until the
server has something to say, so its "latency" is not storage latency. Use
`filter=none` (or a filter of your own) to see it anyway.

#### Instance names

`server` is the name the client used to reach the share: a host name, an FQDN
or an address. A DFS path shows up under the server and share it resolved to,
not under the namespace. The performance data is labelled `server/share`.

#### Other platforms

The counter set is a Windows one. Elsewhere the check returns UNKNOWN
(`... is only available on Windows`) rather than an empty, healthy set.
