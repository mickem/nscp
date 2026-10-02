---
title: "Undefined-behaviour sweep: filter arithmetic, collector and status races, process memory, log truncation and the HTTP settings cache"
fixed_in: next
severity: "Low–Medium"
modules: [filters, core, CheckSystem, CheckSystemUnix, WEBServer]
action: none
---
Ten more findings from the second undefined-behaviour sweep were fixed. Only
the first two take their input from outside the host: a filter expression
sent by a caller that is allowed to pass arguments.

- `convert()` in a filter multiplied its operand by the unit, and added a
  time offset to the current time, without checking for overflow:
  `size > convert('9000000', 't')` or `written > convert(1.5e17, 'w')`
  computed a signed overflow, which is undefined, or rounded a fractional
  value to an unspecified integer. A value that does not fit is now reported
  as an evaluation error on the check instead.
- Negating an integer (`not` / `neg()`) of the smallest 64-bit value, or a
  date far enough from now, overflowed the same way. That is now reported as
  an evaluation error too.
- The reference time that date keywords (`age`, `written`, certificate
  expiry, …) are measured against was a plain global, set by every check and
  read by checks running at the same time on other threads. It is now
  atomic.
- On Windows, a performance counter using the default `static` collection
  strategy stored each new sample under a shared (read) lock, so a check
  reading the same counter could run at the same moment and, on 32-bit
  builds, see half-written values. The store now takes an exclusive lock.
- On Linux, `check_process` read the memory fields of a process that exited
  between opening and reading `/proc/<pid>/statm` without initialising them,
  and reported whatever was on the stack as its memory use. The fields now
  start at zero and a failed read is reported as an error on that process.
- With the settings loaded over `http(s)://`, a cache folder path naming an
  existing file threw an exception no handler catches, so the agent
  terminated instead of reporting `Cache path not found`. It is now reported
  like any other settings error.
- The log file truncation that keeps `nsclient.log` under `max size`
  computed 70% of the limit through a 32-bit `int`, which is undefined for a
  `max size` of about 2.9 GiB or more, and held the kept part in memory. It
  is now computed in the file-size type, and the kept part is moved to the
  front of the file through a fixed buffer.
- On Linux, the stop flag of the CheckSystemUnix collector thread was a plain
  `bool` shared between the module and the collector thread; it is now
  atomic.
- The legacy web API stored the agent status for `/core/isalive` under a
  shared (read) lock, so a `/core/reload` could assign the string while
  `/core/isalive` copied it. The store now takes an exclusive lock.
- When a module's version call failed, the version string was formatted from
  uninitialised integers. It now reads `0.0.0`.

None of these is known to have been exploited.

**What to do:** nothing beyond upgrading.
