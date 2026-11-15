// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstdint>
#include <ctime>
#include <nscapi/protobuf/diagnostics.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

// How a module builds a diagnostics section.
//
// A diagnostics section is a small table with a fixed shape - columns, then
// rows of cells in column order - that a module hands over from what it
// already holds (see `"diagnostics": "produce"` in module.json and
// fetchDiagnostics). The builder mirrors the metrics one:
//
//   auto t = table(response, "processes.top_cpu")
//                .describe("Processes using the most CPU over the last second");
//   t.column("name").column("pid", type_int).column("cpu", type_float, "percent")
//    .column("working_set", type_int, "bytes").column("private_bytes", type_int, "bytes");
//   for (const auto &p : top) t.row().add(p.name).add(p.pid).add(p.cpu).add(p.ws).add(p.priv);
//
// What it enforces, so a producer cannot emit a ragged or oversized table:
//
//  - every row has exactly as many cells as the section has columns: a short
//    row is padded with empty cells (rendered as null), and a cell past the
//    last column throws std::length_error - a bug in the producer, which then
//    costs its sections rather than reaching a reader as a shifted table;
//  - columns are declared before the first row (std::logic_error otherwise);
//  - a string cell is clipped at max_cell_length bytes, on a UTF-8 boundary,
//    with a `...` marker, and the section is marked truncated;
//  - at most max_rows rows are kept; the rows after that are dropped and the
//    section is marked truncated. The core applies the operator's own, lower,
//    row cap (`diagnostics rows`) on top, so a producer emits up to max_rows
//    and never needs to know the setting.
//
// `fetchDiagnostics` itself must be cheap: it copies what the module's own
// collector last produced, it does not query, walk or wait.
namespace nscapi {
namespace diagnostics {

// The most rows a section may carry, and the cap on `diagnostics rows`.
static constexpr std::size_t max_rows = 50;
// The longest a string cell may be, in bytes.
static constexpr std::size_t max_cell_length = 1024;
// The most sections one snapshot holds.
static constexpr std::size_t max_sections = 16;

enum column_type { type_string, type_int, type_float, type_timestamp, type_bool };

inline PB::Diagnostics::Column::Type to_pb(const column_type type) {
  switch (type) {
    case type_int:
      return PB::Diagnostics::Column::INT;
    case type_float:
      return PB::Diagnostics::Column::FLOAT;
    case type_timestamp:
      return PB::Diagnostics::Column::TIMESTAMP;
    case type_bool:
      return PB::Diagnostics::Column::BOOL;
    default:
      return PB::Diagnostics::Column::STRING;
  }
}

// Cut `text` to at most `length` bytes, ending in `...` when it was cut, and
// never inside a UTF-8 sequence: a cell is a protobuf string, which must stay
// valid UTF-8 or the whole snapshot fails to serialise.
inline std::string clip(const std::string &text, const std::size_t length) {
  if (text.size() <= length) return text;
  if (length <= 3) return text.substr(0, length);
  std::size_t end = length - 3;
  // Step back over continuation bytes (10xxxxxx) to the start of the sequence
  // the cut would split.
  while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) --end;
  return text.substr(0, end) + "...";
}

// One row, filled left to right. Returned by table_builder::row(); a row past
// max_rows is a sink (`row_` null) whose cells go nowhere.
class row_builder {
 public:
  row_builder(PB::Diagnostics::Section *section, PB::Diagnostics::Row *row) : section_(section), row_(row) {}

  row_builder &add(const std::string &value) {
    PB::Diagnostics::Cell *c = next();
    if (c == nullptr) return *this;
    if (value.size() > max_cell_length) {
      c->set_s(clip(value, max_cell_length));
      section_->set_truncated(true);
    } else {
      c->set_s(value);
    }
    return *this;
  }
  row_builder &add(const char *value) { return add(std::string(value == nullptr ? "" : value)); }
  row_builder &add(const bool value) {
    PB::Diagnostics::Cell *c = next();
    if (c != nullptr) c->set_b(value);
    return *this;
  }
  row_builder &add(const double value) {
    PB::Diagnostics::Cell *c = next();
    if (c != nullptr) c->set_f(value);
    return *this;
  }
  row_builder &add(const float value) { return add(static_cast<double>(value)); }
  // Any integer (pid, bytes, an exit code). Unsigned values above INT64_MAX
  // saturate rather than wrap to a negative number.
  template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, int>::type = 0>
  row_builder &add(const T value) {
    PB::Diagnostics::Cell *c = next();
    if (c == nullptr) return *this;
    if constexpr (std::is_unsigned<T>::value) {
      if (static_cast<unsigned long long>(value) > static_cast<unsigned long long>(INT64_MAX)) {
        c->set_i(INT64_MAX);
        return *this;
      }
    }
    c->set_i(static_cast<std::int64_t>(value));
    return *this;
  }
  // A TIMESTAMP cell: seconds since the unix epoch.
  row_builder &add_time(const std::int64_t epoch) {
    PB::Diagnostics::Cell *c = next();
    if (c != nullptr) c->set_i(epoch);
    return *this;
  }
  // A missing value, rendered as null.
  row_builder &add_null() {
    next();
    return *this;
  }

 private:
  PB::Diagnostics::Cell *next() {
    if (row_ == nullptr) return nullptr;
    if (row_->cells_size() >= section_->columns_size()) {
      throw std::length_error("diagnostics section " + section_->name() + ": a row has more cells than the " + std::to_string(section_->columns_size()) +
                              " columns declared");
    }
    return row_->add_cells();
  }

  PB::Diagnostics::Section *section_;
  PB::Diagnostics::Row *row_;
};

// One section. Declare it (description, columns), then add rows.
class table_builder {
 public:
  table_builder(PB::Diagnostics::DiagnosticsMessage *message, const std::string &name) : section_(message->add_sections()) {
    section_->set_name(name);
    section_->set_captured_at(static_cast<std::int64_t>(std::time(nullptr)));
  }
  table_builder(const table_builder &) = delete;
  table_builder &operator=(const table_builder &) = delete;
  table_builder(table_builder &&other) noexcept : section_(other.section_) { other.section_ = nullptr; }
  table_builder &operator=(table_builder &&) = delete;
  ~table_builder() { pad_last_row(); }

  // Each setter has an rvalue twin so a declaration can start from table()
  // and still be kept: `auto t = table(m, "x").describe("...");` moves the
  // builder into `t` instead of copying it.
  table_builder &describe(const std::string &text) & {
    section_->set_description(text);
    return *this;
  }
  table_builder &&describe(const std::string &text) && { return std::move(describe(text)); }

  table_builder &column(const std::string &name, const column_type type = type_string, const std::string &unit = "", const std::string &description = "") & {
    if (section_->rows_size() > 0) throw std::logic_error("diagnostics section " + section_->name() + ": column " + name + " declared after the first row");
    PB::Diagnostics::Column *c = section_->add_columns();
    c->set_name(name);
    c->set_type(to_pb(type));
    if (!unit.empty()) c->set_unit(unit);
    if (!description.empty()) c->set_description(description);
    return *this;
  }
  table_builder &&column(const std::string &name, const column_type type = type_string, const std::string &unit = "", const std::string &description = "") && {
    return std::move(column(name, type, unit, description));
  }

  // When the data was taken, if not now: a module that refreshes something
  // on a slow cadence hands over the time of its last refresh.
  table_builder &captured_at(const std::int64_t epoch) & {
    section_->set_captured_at(epoch);
    return *this;
  }
  table_builder &&captured_at(const std::int64_t epoch) && { return std::move(captured_at(epoch)); }

  // The producer cut the data itself (a walk that hit its time limit, a ring
  // that overflowed).
  table_builder &truncated(const bool value = true) & {
    section_->set_truncated(value);
    return *this;
  }
  table_builder &&truncated(const bool value = true) && { return std::move(truncated(value)); }

  // The section could not be collected; the reason is shown in its place.
  table_builder &error(const std::string &message) & {
    section_->set_error(message);
    return *this;
  }
  table_builder &&error(const std::string &message) && { return std::move(error(message)); }

  row_builder row() {
    if (section_->columns_size() == 0) throw std::logic_error("diagnostics section " + section_->name() + ": a row added before any column");
    pad_last_row();
    if (static_cast<std::size_t>(section_->rows_size()) >= max_rows) {
      section_->set_truncated(true);
      return row_builder(section_, nullptr);
    }
    return row_builder(section_, section_->add_rows());
  }

  std::size_t rows() const { return static_cast<std::size_t>(section_->rows_size()); }

 private:
  void pad_last_row() {
    if (section_ == nullptr || section_->rows_size() == 0) return;
    PB::Diagnostics::Row *last = section_->mutable_rows(section_->rows_size() - 1);
    while (last->cells_size() < section_->columns_size()) last->add_cells();
  }

  PB::Diagnostics::Section *section_;
};

inline table_builder table(PB::Diagnostics::DiagnosticsMessage *message, const std::string &name) { return table_builder(message, name); }

}  // namespace diagnostics
}  // namespace nscapi
