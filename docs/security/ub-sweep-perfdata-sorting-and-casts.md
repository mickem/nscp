---
title: "Undefined-behaviour sweep: perfdata parsing, filter_perf sorting, saturating casts and script bindings"
fixed_in: next
severity: "Low"
modules: [filters, CheckHelpers, CheckSystemUnix, CheckDocker, CheckKubernetes, PythonScript, LUAScript, CheckMKServer, CheckWMI, CheckSystem, CheckExternalScripts]
action: none
---
Ten more findings from the second undefined-behaviour sweep were fixed. All
are small and local; none is reachable by an unauthenticated remote peer, and
only two take their input from outside the host (a Docker daemon or Kubernetes
API server reply, and a WMI array value).

- The performance-data parser threw an unhandled `std::out_of_range` on
  input that was nothing but spaces, which a wrapped command or a script
  can produce. Such input is now parsed as empty.
- `filter_perf sort=normal|reverse` sorted with a comparator that was not a
  strict weak ordering once a non-numeric entry sat between two numeric ones,
  which makes `std::sort` undefined. Numeric entries now sort by value and
  every non-numeric entry sorts after them.
- A floating-point filter keyword read as an integer (`check_cpu` load
  averages, temperatures, …) was cast with a plain `static_cast`; a NaN or a
  value beyond the 64-bit range made that cast undefined. The value now
  saturates at the integer limit and is flagged as unsure, as `float_value`
  already did.
- `check_memory` and `check_pagefile` on Linux computed a percentage
  threshold as `total * number / 100` in 64-bit integers, which overflows
  (undefined) for a large caller-supplied number. The product is computed
  in floating point and saturated.
- `CheckDocker` and `CheckKubernetes` cast a floating-point JSON number from
  the daemon or API server straight to an integer. A NaN or a number beyond
  the 64-bit range now saturates instead.
- A Python script calling `query()` on the core had its request read, and
  its early-return tuple built, with the GIL released; both touch Python
  objects. They are now done before the GIL is dropped, as `submit()` does.
- A Lua script that called `obj:__gc()` on a Check_MK section, packet, line
  or metrics object itself, before the collector ran the metamethod again,
  deleted the wrapped C++ object twice. The slot is nulled after the first
  delete.
- On Windows, a WMI array whose `VT_BSTR` element was a null string was
  handed to `std::wstring` as a null pointer. It renders as an empty string.
- The registry settings backend read a `REG_DWORD` value through the byte
  buffer it had been queried into, an aliasing violation. The value is copied
  out, and a value shorter than a DWORD reads as 0.
- The Linux external-script runner wrote a dead global `early_timeout` from
  every worker thread. The variable is gone.

None of these is known to have been exploited.

**What to do:** nothing beyond upgrading.
