// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// How much the system is waiting, published as system.pressure: the kernel's
// pressure stall information (/proc/pressure/{cpu,memory,io}, Linux 4.20+ with
// PSI enabled) and the major page faults since boot (/proc/vmstat).
//
// The parsers and the metric builder here are platform-neutral, so the unit
// tests drive them with captured file contents; read_pressure() has one
// definition per platform (pressure_source_linux.cpp reads procfs,
// pressure_source_darwin.cpp has nothing to read). A source that is not there
// - an older kernel, PSI disabled, a container that hides it - is left out of
// the sample, and so out of the metrics: never published as 0, which would
// read as "nothing is stalling".

#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <vector>

namespace pressure {

// One line of a PSI file: the share of wall time (percent) in which tasks
// were stalled on the resource, averaged over 10, 60 and 300 seconds, and the
// total stall time since boot in microseconds.
struct stall {
  double avg10 = 0;
  double avg60 = 0;
  double avg300 = 0;
  unsigned long long total_us = 0;
};

// One resource's file. `some` is time at least one task was stalled; `full`
// time every non-idle task was. The cpu file has no `full` line before Linux
// 5.13.
struct resource {
  std::string name;
  bool has_some = false;
  stall some;
  bool has_full = false;
  stall full;
};

struct sample {
  std::vector<resource> resources;
  bool has_major_faults = false;
  unsigned long long major_faults = 0;
};

// Parse a /proc/pressure/<resource> file into `out` (whose name the caller
// sets). False when neither line parsed. Numbers are read in the classic
// locale whatever the process runs under.
bool parse_psi(const std::string &content, resource &out);

// The value of one `<key> <value>` line of /proc/vmstat. False when the key is
// not there.
bool parse_vmstat_value(const std::string &content, const std::string &key, unsigned long long &out);

// Add system.pressure to the `system` bundle. Nothing at all when the sample
// is empty.
void build_pressure_metrics(PB::Metrics::MetricsBundle *system, const sample &value);

// The live sample. Defined per platform.
sample read_pressure();

}  // namespace pressure
