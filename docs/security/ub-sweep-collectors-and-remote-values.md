---
title: "Undefined-behaviour sweep: process listing, remote registry, real-time log files and remote JSON values"
fixed_in: next
severity: "Low–Medium"
modules: [CheckSystem, CheckLogFile, CheckNet, NRPEServer, NSCAServer, NRPEClient, NSCAClient]
action: none
---
Five more findings from the second undefined-behaviour sweep were fixed. Only
one takes its input from outside the host: the reply of a server that
`check_http` is pointed at.

- `check_http json-path=` converted the extracted JSON number to an integer
  with a plain cast. A number beyond the 64-bit range (`1e300`, or an unsigned
  value above 2^63) made that conversion undefined, and in practice produced a
  large negative value. Such a number now saturates at the integer limit.
  The server (or anyone on the path when `verify=none`) chooses this value.
- On Windows, a host running 4096 or more processes made every process listing
  (`check_process`, the process collectors, real-time process filters) retry
  with a ten times larger buffer, over and over, until the allocation failed.
  The retry now compares against the buffer it actually passed, doubles the
  buffer and stops at a fixed limit.
- `check_registry_key` and `check_registry_value` with `computer=` closed the
  handle to the remote registry before reading it. Handle values are reused,
  so the reads either failed or went to another object the process had opened
  since. The handle now stays open while the key is read.
- Real-time log file monitoring on Linux read each inotify event's file name,
  but the events for a watched file have no name. Every write to a monitored
  log therefore ran a string scan into the next event, or past the data that
  was read.
- The NRPE and NSCA CRC32 table was filled in lazily by the first thread
  that needed it. Two connections arriving together on a freshly started agent
  could use a partly filled table and reject a valid packet. The same table is
  used by NRPEClient and NSCAClient for outbound packets. The table is now
  a compile-time constant.

None of these is known to have been exploited.

**What to do:** nothing beyond upgrading.
