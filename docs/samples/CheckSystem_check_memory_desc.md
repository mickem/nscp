#### Kinds of memory

There are several different kinds of memory that a computer system uses to manage data and processes. 
Here are the main types:

* `physical` Memory (RAM): This is the actual, tangible memory chips installed in your computer.  It's often referred to as RAM (Random Access Memory).
* `committed` Memory: Committed memory refers to the amount of virtual memory that has been reserved by processes. 
  When a program requests memory from the operating system, that memory is "committed."
  This committed memory is guaranteed to be available to the process, meaning Windows has set aside enough resources (either physical RAM or space in the page file) to back that memory.
* `virtual` Memory: Virtual memory is an abstraction layer created by the operating system (Windows) to provide a larger, contiguous address space to each process than the physical RAM actually available.

#### Memory paging rate (`\Memory\Pages/sec`)

A sustained high hard-page-fault rate is one of the strongest signals of memory
pressure. NSClient++ collects `\Memory\Pages/sec` by default under the alias
`memory_pages_sec`, so you can alert on it directly with `check_pdh` without
declaring the counter yourself:

```
check_pdh "counter=memory_pages_sec" "warn=value > 1000" "crit=value > 5000"
```

##### macOS

`physical` is installed memory against the free pages. `cached` counts
file-backed and purgeable pages as free as well, the same two Activity Monitor
adds up as Cached Files. `swap` is the dynamic swap files, so its size is zero
until macOS has needed swap. On macOS the system metrics also carry
`system.mem.wired` and `system.mem.compressed`.

#### Right after the agent starts (Linux and macOS)

On Linux and macOS the figures come from the 1 Hz background collector rather
than from a direct read, so for the first second after the agent (or the module)
starts there is nothing to report yet:

```
check_memory
UNKNOWN: No memory data available yet (collector still initializing)
```

`warmup-state` picks the status reported during that window (`ok`, `warning`,
`critical` or `unknown`, the default); the message stays the same. A collector
that has tried to sample and failed - an unreadable `/proc` inside a locked-down
container, for instance - is not warming up: the check then answers UNKNOWN
*"No memory data available: the collector failed to sample it: ..."* whatever
`warmup-state` says. On Windows `check_memory` reads the system directly and has no
warm-up, so the option is not accepted there. See
[Collector-backed checks and warm-up](../../concepts/checks.md#8-collector-backed-checks-and-warm-up).
