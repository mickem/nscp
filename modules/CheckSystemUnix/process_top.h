// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The per-process sample behind the processes.top_cpu and processes.top_memory
// diagnostics tables (include/diagnostics/process_tables.hpp). The collector
// thread (realtime_thread.cpp) reads it once a second while
// [/settings/system/unix/diagnostics] processes is on, and not at all
// otherwise.
//
// The parsers and the CPU calculation here are platform-neutral and unit
// tested on captured file contents (process_top.cpp); read_processes() has one
// definition per platform: process_top_source_linux.cpp walks /proc,
// process_top_source_darwin.cpp has no reader yet and says so.

#include <diagnostics/process_tables.hpp>
#include <map>
#include <string>
#include <vector>

namespace process_top {

// One process as /proc/<pid>/stat and /proc/<pid>/statm describe it. No
// command line: the name is the kernel's `comm`.
struct process_times {
  std::string name;
  // starttime, in clock ticks since boot: a pid reused by a new process has a
  // different one, and must not be diffed against the old process.
  unsigned long long start = 0;
  // utime + stime, in clock ticks.
  unsigned long long ticks = 0;
  // Resident memory, and the part of it not shared with another process.
  unsigned long long working_set = 0;
  unsigned long long private_bytes = 0;
};
typedef std::map<long long, process_times> table;

// /proc/<pid>/stat: the name between the first '(' and the last ')' (a name
// may itself hold parentheses and blanks), then utime, stime and starttime
// (fields 14, 15 and 22). Numbers are read without the locale. False on a
// line that does not have them.
bool parse_stat(const std::string &line, process_times &out);

// /proc/<pid>/statm: size, resident and shared, in pages.
bool parse_statm(const std::string &line, unsigned long long page_size, process_times &out);

// Every process in `cur` as a table row. A process also in `prev` with the
// same start gets its CPU share over the interval: its ticks against
// `total_ticks`, the ticks every core spent (the /proc/stat aggregate) - so
// 100 is the whole machine. One that is new, or whose counters went
// backwards, gets 0.
std::vector<process_tables::process_row> rows(const table &prev, const table &cur, unsigned long long total_ticks);

// Every process on the host. Throws std::runtime_error when the platform
// cannot be read (or has no reader).
table read_processes();

}  // namespace process_top
