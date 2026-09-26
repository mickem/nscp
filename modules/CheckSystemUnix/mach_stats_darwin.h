// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The Mach host statistics several Darwin readers share: the collector's
// memory sample, check_kernel_memory, check_swap_io, check_kernel_stats and
// check_load. Every call here works for an unprivileged account.

#include <mach/vm_statistics.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <string>
#include <vector>

namespace mach_stats {

// host_statistics64(HOST_VM_INFO64) plus the page size its counts are in (16
// KiB on Apple silicon). False with `error` set when either call fails.
bool read_vm_statistics(vm_statistics64_data_t &stats, unsigned long long &page_size, std::string &error);

// Threads alive on the host, from the default processor set's load info -
// the same number `top` prints as Threads. False with `error` set on failure.
bool read_thread_count(long long &threads, std::string &error);

// Mach absolute-time units to nanoseconds. libproc reports CPU time in these,
// and they are nanoseconds on Intel but 125/3 ns ticks on Apple silicon.
unsigned long long mach_ticks_to_ns(unsigned long long ticks);

// The kernel's process table entry for every process, or for one: pid,
// parent, real uid, BSD state, start time and the accounting name. This is
// what ps reads, and unlike libproc's per-process calls it is readable for
// every process whoever owns it.
std::vector<struct kinfo_proc> read_all_processes();
bool read_process(pid_t pid, struct kinfo_proc &out);

}  // namespace mach_stats
