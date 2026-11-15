// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <cstdint>
#include <nscapi/nscapi_diagnostics_helper.hpp>
#include <string>
#include <utility>
#include <vector>

// The two process diagnostics sections, `processes.top_cpu` and
// `processes.top_memory`, shared by CheckSystem (Windows) and CheckSystemUnix.
//
// The platform half is the collector: each module's 1 Hz thread already walks
// the process table (or, on Linux, does so while the section is enabled),
// turns two samples into a CPU share per process and hands the rows to rank(),
// which keeps the top ones. fetchDiagnostics then only copies the last ranked
// sample into the response with add_sections() - it never walks anything
// itself.
//
// No command lines, ever: they carry credentials far more often than check
// arguments do. A process is its name and pid.
namespace process_tables {

static constexpr const char *section_top_cpu = "processes.top_cpu";
static constexpr const char *section_top_memory = "processes.top_memory";

struct process_row {
  // The executable name (`acme-indexer.exe`, `postgres`), never a path or a
  // command line.
  std::string name;
  long long pid = 0;
  // Share of the whole machine's CPU capacity over the last sample interval,
  // all cores together: 100 is every core busy, so one process pegging one
  // core of four shows as 25.
  double cpu = 0.0;
  // Resident memory, and memory the process alone owns (Windows: private
  // bytes; Linux: resident pages not shared with another process).
  unsigned long long working_set = 0;
  unsigned long long private_bytes = 0;
};

// One ranked sample: the rows with the most CPU and the rows with the largest
// working set, each at most `keep` long. `captured_at` is 0 until the
// collector has two samples to compare.
struct sample {
  std::int64_t captured_at = 0;
  // How many processes the sample saw: a table shorter than this is a top list.
  std::size_t total = 0;
  std::vector<process_row> top_cpu;
  std::vector<process_row> top_memory;
};

namespace detail {
inline bool by_cpu(const process_row &a, const process_row &b) {
  if (a.cpu != b.cpu) return a.cpu > b.cpu;
  if (a.working_set != b.working_set) return a.working_set > b.working_set;
  return a.pid < b.pid;
}
inline bool by_memory(const process_row &a, const process_row &b) {
  if (a.working_set != b.working_set) return a.working_set > b.working_set;
  if (a.cpu != b.cpu) return a.cpu > b.cpu;
  return a.pid < b.pid;
}
inline std::vector<process_row> top(std::vector<process_row> rows, const std::size_t keep, bool (*order)(const process_row &, const process_row &)) {
  // (std::min) - windows.h defines a min macro.
  const std::size_t n = (std::min)(keep, rows.size());
  std::partial_sort(rows.begin(), rows.begin() + static_cast<std::ptrdiff_t>(n), rows.end(), order);
  rows.resize(n);
  return rows;
}
}  // namespace detail

// Rank every process of one sample. `keep` is how many of each the module
// holds on to: the most a section may carry, so the core's own (lower) row
// cap is what decides what a reader sees.
inline sample rank(const std::vector<process_row> &rows, const std::int64_t now, const std::size_t keep = nscapi::diagnostics::max_rows) {
  sample out;
  out.captured_at = now;
  out.total = rows.size();
  out.top_cpu = detail::top(rows, keep, &detail::by_cpu);
  out.top_memory = detail::top(rows, keep, &detail::by_memory);
  return out;
}

namespace detail {
inline void add_table(PB::Diagnostics::DiagnosticsMessage *out, const char *name, const char *description, const std::vector<process_row> &rows,
                      const std::int64_t captured_at, const std::size_t total) {
  using nscapi::diagnostics::type_float;
  using nscapi::diagnostics::type_int;
  auto t = nscapi::diagnostics::table(out, name).describe(description).captured_at(captured_at);
  t.column("name")
      .column("pid", type_int)
      .column("cpu", type_float, "percent", "Share of all cores over the last second")
      .column("working_set", type_int, "bytes", "Resident memory")
      .column("private_bytes", type_int, "bytes", "Memory not shared with other processes");
  for (const process_row &p : rows) t.row().add(p.name).add(p.pid).add(p.cpu).add(p.working_set).add(p.private_bytes);
  // There were more processes than the table holds: it is a top list.
  if (total > rows.size()) t.truncated();
}
}  // namespace detail

// Both sections from the last ranked sample; nothing before the first one.
inline void add_sections(PB::Diagnostics::DiagnosticsMessage *out, const sample &s) {
  if (s.captured_at == 0) return;
  detail::add_table(out, section_top_cpu, "The processes using the most CPU over the last second", s.top_cpu, s.captured_at, s.total);
  detail::add_table(out, section_top_memory, "The processes with the largest working set", s.top_memory, s.captured_at, s.total);
}

}  // namespace process_tables
