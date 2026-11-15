// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <nscapi/protobuf/diagnostics.hpp>
#include <string>
#include <vector>

// Rendering the latest diagnostics snapshot for the `nscp test` prompt.
//
// The core answers a diagnostics query (NSAPIDiagnosticsQuery) with a
// DiagnosticsResponseMessage: whether diagnostics are collected at all, the
// snapshot's id and time, and the sections. `diagnostics` lists the sections;
// `diagnostics <name>` prints one as a table, a column per column and a line
// per row, so what a failed check was recorded with can be read at the prompt.
//
// A header of its own, like facts_renderer.hpp, because it is a pure function
// from the core's answer to the text on screen and is worth testing as one.
namespace client {
namespace diagnostics_detail {

inline std::string iso_time(const std::int64_t epoch) {
  if (epoch <= 0) return "";
  return boost::posix_time::to_iso_extended_string(boost::posix_time::from_time_t(static_cast<std::time_t>(epoch))) + "Z";
}

inline std::string format_double(const double value) {
  if (!std::isfinite(value)) return "";
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.2f", value);
  std::string text = buffer;
  // 12.50 -> 12.5, 3.00 -> 3: the trailing zeros only widen the column.
  while (!text.empty() && text.back() == '0') text.pop_back();
  if (!text.empty() && text.back() == '.') text.pop_back();
  return text;
}

inline std::string cell_text(const PB::Diagnostics::Cell &cell, const PB::Diagnostics::Column::Type type) {
  switch (cell.value_case()) {
    case PB::Diagnostics::Cell::kS: {
      // One line per row: a multi-line event message is shown up to its end
      // of line.
      std::string text = cell.s();
      const std::string::size_type eol = text.find_first_of("\r\n");
      if (eol != std::string::npos) text = text.substr(0, eol) + " ...";
      return text;
    }
    case PB::Diagnostics::Cell::kI:
      if (type == PB::Diagnostics::Column::TIMESTAMP) return iso_time(cell.i());
      return std::to_string(cell.i());
    case PB::Diagnostics::Cell::kF:
      return format_double(cell.f());
    case PB::Diagnostics::Cell::kB:
      return cell.b() ? "true" : "false";
    default:
      return "";
  }
}

// Columns padded to their widest cell, two blanks apart, nothing after the
// last one.
inline std::string render_rows(const std::vector<std::vector<std::string> > &rows, const std::string &indent) {
  std::vector<std::size_t> widths;
  for (const std::vector<std::string> &row : rows) {
    if (widths.size() < row.size()) widths.resize(row.size(), 0);
    for (std::size_t i = 0; i < row.size(); ++i) {
      if (row[i].size() > widths[i]) widths[i] = row[i].size();
    }
  }
  std::string out;
  for (const std::vector<std::string> &row : rows) {
    std::string line = indent;
    for (std::size_t i = 0; i < row.size(); ++i) {
      line += row[i];
      if (i + 1 < row.size()) line += std::string(widths[i] - row[i].size() + 2, ' ');
    }
    out += "\n" + line;
  }
  return out;
}

inline std::string render_section(const PB::Diagnostics::Section &section) {
  std::string out = section.name() + " (" + (section.source().empty() ? std::string("unknown source") : section.source()) + ", captured " +
                    iso_time(section.captured_at()) + ")";
  if (!section.description().empty()) out += "\n" + section.description();
  if (!section.error().empty()) out += "\nError: " + section.error();
  std::vector<std::vector<std::string> > rows;
  std::vector<std::string> header;
  for (const PB::Diagnostics::Column &c : section.columns()) {
    std::string name = c.name();
    for (char &ch : name) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (!c.unit().empty()) name += " (" + c.unit() + ")";
    header.push_back(name);
  }
  if (!header.empty()) rows.push_back(header);
  for (const PB::Diagnostics::Row &r : section.rows()) {
    std::vector<std::string> row;
    for (int i = 0; i < section.columns_size(); ++i) {
      row.push_back(i < r.cells_size() ? cell_text(r.cells(i), section.columns(i).type()) : std::string());
    }
    rows.push_back(row);
  }
  if (section.rows_size() == 0) {
    if (section.error().empty()) out += "\n(no rows)";
    return out;
  }
  out += render_rows(rows, "  ");
  if (section.truncated()) out += "\n(truncated: a row, cell or size cap cut this table)";
  return out;
}

}  // namespace diagnostics_detail

// `envelope` is what core_wrapper::get_diagnostics() returned for `section`
// (empty for the listing).
inline std::string render_diagnostics(const std::string &envelope, const std::string &section) {
  PB::Diagnostics::DiagnosticsResponseMessage message;
  if (envelope.empty() || !message.ParseFromString(envelope) || message.payload_size() == 0) {
    return "Diagnostics are not available from this core.";
  }
  const PB::Diagnostics::DiagnosticsResponseMessage::Response &payload = message.payload(0);
  if (!payload.enabled()) return "Diagnostics are disabled: set capture diagnostics = true in [/settings/core/failed checks].";
  if (payload.snapshot_id() == 0) return "No diagnostics collected yet: the first round runs with the next metrics round.";
  const PB::Diagnostics::DiagnosticsMessage &snapshot = payload.snapshot();
  if (!section.empty()) {
    if (snapshot.sections_size() == 0) return "Diagnostics section not found: " + section;
    return diagnostics_detail::render_section(snapshot.sections(0));
  }
  std::string out = "Snapshot " + std::to_string(payload.snapshot_id()) + ", collected " + diagnostics_detail::iso_time(payload.captured_at()) + ", at most " +
                    std::to_string(payload.max_rows()) + " rows per table:";
  std::vector<std::vector<std::string> > rows;
  rows.push_back({"NAME", "SOURCE", "ROWS", "CAPTURED", "STATUS"});
  for (const PB::Diagnostics::Section &s : snapshot.sections()) {
    std::string status = s.error().empty() ? (s.truncated() ? "truncated" : "ok") : "error: " + s.error();
    rows.push_back({s.name(), s.source(), std::to_string(s.rows_size()), diagnostics_detail::iso_time(s.captured_at()), status});
  }
  out += diagnostics_detail::render_rows(rows, "  ");
  for (const std::string &name : payload.excluded()) out += "\nLeft out by prune pattern: " + name;
  out += "\nShow one with: diagnostics <name>";
  return out;
}

}  // namespace client
