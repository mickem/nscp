---
title: "Undefined-behaviour sweep: filter convert(), filter reference time, PDH counters, process memory and the HTTP settings cache"
fixed_in: next
severity: "Low–Medium"
modules: [filters, core, CheckSystem, CheckSystemUnix]
action: none
---
Five more findings from the second undefined-behaviour sweep were fixed. Only
the first takes its input from outside the host: a filter expression sent by a
caller that is allowed to pass arguments.

- `convert()` in a filter multiplied a string operand by its unit, and added
  a time offset to the current time, without checking for overflow:
  `size > convert('9000000', 't')` or
  `written > convert('9223372036854775807', 's')` computed a signed overflow,
  which is undefined. A value that does not fit is now reported as an
  evaluation error on the check instead.
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

None of these is known to have been exploited.

**What to do:** nothing beyond upgrading.
