// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstdint>
#include <nscapi/nscapi_diagnostics_helper.hpp>
#include <nscapi/protobuf/failures.hpp>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "diagnostics_snapshot.hpp"
#include "failed_check_store.hpp"

namespace nsclient {
namespace core {

// The pure half of the core's diagnostics round (NSClientT::process_diagnostics):
// what turns the sections the producers handed over into the snapshot that is
// kept. The other half - asking the producers - is
// plugin_manager::collect_diagnostics.
//
//   every `diagnostics interval` (with the metrics round unless set apart)
//     -> collect_diagnostics()        each producer hands over what it holds
//     -> add_recent_failures()        the core's own section
//     -> assemble()                   prune, cap, redact
//     -> diagnostics_snapshot_store   the latest snapshot
//
// Nothing here runs because a check failed or a reader asked: a failure
// attaches the snapshot the last round left, at most one interval old.
namespace diagnostics_round {

static constexpr std::size_t default_max_rows = 10;
static constexpr const char *recent_failures_section = "failures.recent";

struct caps {
  // `diagnostics rows`, clamped to 1..nscapi::diagnostics::max_rows.
  std::size_t max_rows = default_max_rows;
  std::size_t max_cell_length = nscapi::diagnostics::max_cell_length;
  std::size_t max_sections = nscapi::diagnostics::max_sections;
  // `max context size`, shared with metrics snapshots: rows are dropped from
  // the longest sections until the snapshot fits.
  std::size_t max_size = failed_check_store::default_max_context_size;
  // `prune pattern`, lower-cased (metrics_prune::parse_patterns): a section
  // whose name contains one is left out.
  std::vector<std::string> prune_patterns;
};

inline std::size_t clamp_rows(const std::size_t rows) {
  if (rows < 1) return 1;
  if (rows > nscapi::diagnostics::max_rows) return nscapi::diagnostics::max_rows;
  return rows;
}

// The core's own section: the last failed checks, newest first, from the
// failed-check log (`entries` as failed_check_store::list returns them). In a
// failure's own context it holds the failures before the snapshot was taken -
// the run-up to it.
inline void add_recent_failures(PB::Diagnostics::DiagnosticsMessage &out, const failed_check_store::entry_list &entries, const std::size_t max_rows,
                                const std::int64_t now) {
  using nscapi::diagnostics::type_timestamp;
  auto t = nscapi::diagnostics::table(&out, recent_failures_section).describe("The last failed checks, newest first").captured_at(now);
  t.column("time", type_timestamp, "", "When the check finished")
      .column("command")
      .column("status")
      .column("subject", nscapi::diagnostics::type_string, "", "Who asked for the check")
      .column("message", nscapi::diagnostics::type_string, "", "The first line of the check's output");
  std::size_t added = 0;
  for (const PB::Failures::FailedCheck &entry : entries) {
    if (added >= max_rows) {
      t.truncated();
      break;
    }
    const PB::Commands::QueryResponseMessage::Response &r = entry.response();
    std::string message = r.lines_size() > 0 ? r.lines(0).message() : std::string();
    const std::string::size_type eol = message.find_first_of("\r\n");
    if (eol != std::string::npos) message = message.substr(0, eol);
    t.row().add_time(entry.timestamp()).add(r.command()).add(nscapi::failures::status_name(r.result())).add(entry.subject()).add(message);
    ++added;
  }
}

namespace detail {
inline std::string ascii_lower(std::string text) {
  for (char &c : text) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return text;
}

inline bool is_excluded(const std::string &name, const std::vector<std::string> &patterns) {
  if (patterns.empty()) return false;
  const std::string lower = ascii_lower(name);
  for (const std::string &pattern : patterns) {
    if (!pattern.empty() && lower.find(pattern) != std::string::npos) return true;
  }
  return false;
}

// Clip and redact one string; true when clipping cut it.
inline bool scrub(std::string &text, const std::size_t max_length) {
  bool cut = false;
  if (text.size() > max_length) {
    text = nscapi::diagnostics::clip(text, max_length);
    cut = true;
  }
  text = failed_check_store::redact_message(text);
  return cut;
}

// Hold one section to the row and cell caps, and run every string a producer
// controls through the failed-check log's redaction: an event message or a
// service's binary path can carry `password=...` as easily as check output.
inline void cap_section(PB::Diagnostics::Section &section, const caps &limits) {
  if (static_cast<std::size_t>(section.rows_size()) > limits.max_rows) {
    section.mutable_rows()->DeleteSubrange(static_cast<int>(limits.max_rows), section.rows_size() - static_cast<int>(limits.max_rows));
    section.set_truncated(true);
  }
  const int width = section.columns_size();
  for (PB::Diagnostics::Row &row : *section.mutable_rows()) {
    // A producer that did not use the builder can hand over a ragged row.
    if (row.cells_size() > width) {
      row.mutable_cells()->DeleteSubrange(width, row.cells_size() - width);
      section.set_truncated(true);
    }
    while (row.cells_size() < width) row.add_cells();
    for (PB::Diagnostics::Cell &cell : *row.mutable_cells()) {
      if (cell.value_case() != PB::Diagnostics::Cell::kS) continue;
      std::string text = cell.s();
      if (scrub(text, limits.max_cell_length)) section.set_truncated(true);
      cell.set_s(text);
    }
  }
  std::string error = section.error();
  scrub(error, limits.max_cell_length);
  section.set_error(error);
  std::string description = section.description();
  scrub(description, limits.max_cell_length);
  section.set_description(description);
}

// Drop the last row of the longest section until the message fits `max_size`.
// False when there are no rows left to drop and it still does not fit.
inline bool fit(PB::Diagnostics::DiagnosticsMessage &message, const std::size_t max_size) {
  while (message.ByteSizeLong() > max_size) {
    PB::Diagnostics::Section *longest = nullptr;
    for (PB::Diagnostics::Section &s : *message.mutable_sections()) {
      if (s.rows_size() > 0 && (longest == nullptr || s.rows_size() > longest->rows_size())) longest = &s;
    }
    if (longest == nullptr) return false;
    longest->mutable_rows()->RemoveLast();
    longest->set_truncated(true);
  }
  return true;
}
}  // namespace detail

// Turn what was collected (the core's section first, then each producer's, in
// load order) into a snapshot: sections matching `prune pattern` left out, a
// name seen twice kept once, at most `max_sections` of them, each held to the
// row and cell caps and redacted, and the whole trimmed to `max_size`. The id
// and wire size are the store's to fill in.
inline diagnostics_snapshot assemble(PB::Diagnostics::DiagnosticsMessage collected, const caps &limits, const std::int64_t now) {
  diagnostics_snapshot out;
  out.captured_at = now;
  out.max_rows = limits.max_rows;
  std::set<std::string> seen;
  for (PB::Diagnostics::Section &section : *collected.mutable_sections()) {
    if (detail::is_excluded(section.name(), limits.prune_patterns)) {
      out.excluded.push_back(section.name());
      continue;
    }
    if (!seen.insert(section.name()).second || static_cast<std::size_t>(out.message.sections_size()) >= limits.max_sections) {
      ++out.dropped;
      continue;
    }
    detail::cap_section(section, limits);
    out.message.add_sections()->Swap(&section);
  }
  detail::fit(out.message, limits.max_size);
  return out;
}

}  // namespace diagnostics_round
}  // namespace core
}  // namespace nsclient
