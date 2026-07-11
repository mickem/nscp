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

