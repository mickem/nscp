---
title: "Undefined-behaviour audit: crash and memory-safety fixes across the agent"
fixed_in: next
severity: "Medium"
modules: [core, filters, CheckSystem, CheckHelpers, CheckTaskSched, CheckEventLog, CheckDisk, CheckLogFile, WEBServer, NSClientServer, CheckMKServer, PythonScript, LUAScript, NRPEServer, NSCAClient]
action: none
---
After #1499 (a filter that matched nothing corrupted the heap) the whole C++
tree was scanned for the same classes of undefined behaviour: dangling
references in filter records, container access on empty input, lifetime and
data races around reload and unload, arithmetic overflow, wire-format buffers
and Windows/POSIX API misuse. Roughly seventy findings were fixed, each in its
own commit. The ones an outside party could reach:

- **A wrapped HTTP response could spin the agent.** The chunked-transfer
  decoder used by the NRDP, fleet and download clients wrapped its offset on a
  chunk-size line of `ffffffffffffffff` and re-parsed the same line forever on
  64-bit Linux builds. Any HTTP server the agent talks to could send it.
- **An empty console command crashed the agent.** `POST /console/exec` with a
  command of a single space (or the same at the `nscp test` prompt) read
  `front()` of an empty argument list. Requires the `console.exec` grant.
- **`filter_perf sort=normal` over mixed numeric and non-numeric perfdata**
  (the Nagios `U` marker from any external script) used a comparator that is
  not a strict weak ordering, which lets `std::sort` write out of bounds.
- **Default-path memory errors in common checks.** On Windows, `check_cpu`
  judged every core against a dead stack slot, `check_pagefile` read a
  destroyed temporary, `check_tasksched` fetched keywords through a released
  COM object, `render_perf remove-perf=true` read cleared entries, and a
  `check_timeout` that timed out let its worker write into a returned stack
  frame. `${guid}` in an event log filter read a GUID as a string and ran off
  the render buffer.
- **Reload and unload races.** Four collectors were replaced on a settings
  reload without being stopped, the WEBServer CLI client was freed under the
  metrics task, an unloaded log-handling module was resurrected by the next
  log line, and several server and client modules rewrote their tables under
  live worker threads.
- **Arithmetic.** Time and byte unit multipliers, float-to-integer casts in
  threshold expressions, and a handful of counters now saturate or report an
  error instead of overflowing.

None of these is known to have been exploited, and none is known to be usable
beyond terminating or hanging the agent; they are writes to freed or
out-of-bounds memory and were treated accordingly.

**What to do:** nothing beyond upgrading. A few defaults changed as a
consequence; see the
[upgrade note](../setup/upgrading.md#unreleased).
