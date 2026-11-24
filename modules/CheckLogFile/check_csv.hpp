// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <check/path_access_policy.hpp>
#include <cstdint>
#include <functional>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

// check_csv: read a local (or UNC) CSV file and run the filter engine
// over its data rows, one record per row, with the columns declared by the
// caller as keywords. Covers both of the usual "CSV values" modes without an
// SQL engine: threshold the number of matching rows (count), or the value of a
// single cell picked out by `filter=`.
namespace check_csv_command {

enum class column_type { text, integer, real };

struct column_spec {
  std::string name;
  column_type type = column_type::text;
};

// Parse a column declaration in the shape of an SQL column list:
// "Hostname TEXT, WaitingUpdates INTEGER, Load REAL". The type is optional
// (TEXT), case-insensitive, and SQLite's spellings are accepted
// (VARCHAR(64), BIGINT, DOUBLE, ...). Each name must be a filter identifier
// and must not collide with a reserved or built-in keyword. Returns false and
// sets `error` on anything it cannot use.
bool parse_columns(const std::string &spec, std::vector<column_spec> &out, std::string &error);

// Turn header cells into TEXT columns (used when header=true and no columns=
// was given). Fails, naming the cell, when a header is not a usable keyword.
bool columns_from_header(const std::vector<std::string> &header, std::vector<column_spec> &out, std::string &error);

// Decode raw file bytes into UTF-8. `encoding` is auto (BOM sniffing: UTF-8,
// UTF-16LE, UTF-16BE; UTF-8 without one), utf-8, utf-16le, utf-16be or
// latin1. A leading BOM is always dropped. Implemented here rather than with
// the platform converters so it behaves the same on every OS.
bool decode_text(const std::string &bytes, const std::string &encoding, std::string &out, std::string &error);

// One parsed record and the (1-based) line of the text it started on.
struct csv_record {
  std::vector<std::string> fields;
  std::size_t line = 0;
};

// Split text into records, RFC 4180 style: `delimiter` separates fields, a
// field wrapped in `quote` may contain the delimiter, line breaks and the
// quote itself (written twice). quote == '\0' disables quoting. CRLF, LF and
// CR all end a record; empty lines are skipped. A backslash is an ordinary
// character, so a Windows path in a cell survives as written. Fails on an
// unterminated quoted field.
bool split_csv(const std::string &text, char delimiter, char quote, std::vector<csv_record> &out, std::string &error);

// Read the raw bytes of `path` (already through the access gate). Returns
// false with `error` set on failure. Injected so the check can be unit tested
// without the file system.
typedef std::function<bool(const std::string &path, std::uint64_t max_bytes, std::string &bytes, std::string &error)> read_function;

// The local reader (csv_source_win.cpp: CreateFileW, which also takes a UNC
// path, with every share flag so a file another program has open can still be
// read; csv_source_unix.cpp: an ifstream). Refuses a file larger than
// max_bytes (0 = unlimited) before reading it.
bool read_local_file(const std::string &path, std::uint64_t max_bytes, std::string &bytes, std::string &error);

// Run the check with an injected reader.
void check_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const check::access::path_policy &access, const read_function &read);

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
           const check::access::path_policy &access);

}  // namespace check_csv_command
