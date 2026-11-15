// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/json.hpp>
#include <cmath>
#include <cstdint>
#include <nscapi/protobuf/diagnostics.hpp>
#include <str/rfc3339.hpp>
#include <string>

// Diagnostics sections as JSON. Header-only and shared by everything that
// sends a section off the host - the REST API (/api/v2/diagnostics and a
// failure's context) and the fleet failure report - so a consumer reads them
// all with one parser:
//
//   "processes.top_cpu": {
//     "source": "CheckSystem",
//     "description": "Processes using the most CPU over the last second",
//     "captured_at": "2026-11-15T10:42:07Z",
//     "truncated": true,
//     "columns": [ { "name": "name", "type": "string" },
//                  { "name": "cpu", "type": "float", "unit": "percent" } ],
//     "rows": [ { "name": "acme-indexer.exe", "cpu": 87.5 } ]
//   }
//
// A row is an object keyed by column name, so a reader never counts
// positions. A TIMESTAMP cell and every `captured_at` is an ISO 8601 UTC
// string; a missing cell is null; `description` and `error` are only present
// when set, and `unit` and a column's `description` likewise.
namespace diagnostics_json {

inline boost::json::value cell(const PB::Diagnostics::Cell &cell, const PB::Diagnostics::Column::Type type) {
  switch (cell.value_case()) {
    case PB::Diagnostics::Cell::kS:
      return boost::json::value(cell.s());
    case PB::Diagnostics::Cell::kI:
      if (type == PB::Diagnostics::Column::TIMESTAMP) {
        if (cell.i() <= 0) return nullptr;
        return boost::json::value(str::format_rfc3339(cell.i()));
      }
      return boost::json::value(cell.i());
    case PB::Diagnostics::Cell::kF:
      // JSON has no NaN or infinity.
      if (!std::isfinite(cell.f())) return nullptr;
      return boost::json::value(cell.f());
    case PB::Diagnostics::Cell::kB:
      return boost::json::value(cell.b());
    default:
      return nullptr;
  }
}

inline boost::json::array columns(const PB::Diagnostics::Section &section) {
  boost::json::array out;
  for (const PB::Diagnostics::Column &c : section.columns()) {
    boost::json::object node;
    node["name"] = c.name();
    node["type"] = nscapi::diagnostics::column_type_name(c.type());
    if (!c.unit().empty()) node["unit"] = c.unit();
    if (!c.description().empty()) node["description"] = c.description();
    out.push_back(node);
  }
  return out;
}

// The section's header without its rows: what a listing shows.
inline boost::json::object summary(const PB::Diagnostics::Section &section) {
  boost::json::object node;
  node["source"] = section.source();
  if (!section.description().empty()) node["description"] = section.description();
  node["captured_at"] = str::format_rfc3339(section.captured_at());
  node["truncated"] = section.truncated();
  if (!section.error().empty()) node["error"] = section.error();
  return node;
}

// The whole section: the summary plus the columns and the rows.
inline boost::json::object section(const PB::Diagnostics::Section &section) {
  boost::json::object node = summary(section);
  node["columns"] = columns(section);
  boost::json::array rows;
  for (const PB::Diagnostics::Row &r : section.rows()) {
    boost::json::object row;
    for (int i = 0; i < section.columns_size(); ++i) {
      const PB::Diagnostics::Column &c = section.columns(i);
      row[c.name()] = i < r.cells_size() ? cell(r.cells(i), c.type()) : boost::json::value(nullptr);
    }
    rows.push_back(row);
  }
  node["rows"] = rows;
  return node;
}

// Every section of a snapshot, keyed by name.
inline boost::json::object sections(const PB::Diagnostics::DiagnosticsMessage &message) {
  boost::json::object out;
  for (const PB::Diagnostics::Section &s : message.sections()) out[s.name()] = section(s);
  return out;
}

}  // namespace diagnostics_json
