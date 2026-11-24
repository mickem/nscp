// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_csv.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <set>
#include <str/format.hpp>
#include <str/saturate.hpp>

namespace po = boost::program_options;

namespace check_csv_command {

namespace {

// Keywords a column may not be named after: the generic summary keywords the
// filter engine owns (see generic_summary::has_variable - a column called
// `count` would shadow it in filter= and lose to it in top-syntax) and the
// built-in per-row keywords below. Variables are looked up case-sensitively,
// so `Count` is a different keyword and allowed. The words of the expression
// grammar are matched without regard to case, so those are refused in any
// spelling.
bool is_reserved_name(const std::string &name) {
  static const std::set<std::string> keywords = {"count",     "total",     "ok_count",  "warn_count", "crit_count", "problem_count",
                                                 "list",      "ok_list",   "warn_list", "crit_list",  "problem_list", "detail_list",
                                                 "lines",     "status",    "sep",       "row",        "field_count",  "values"};
  static const std::set<std::string> grammar = {"and", "or", "not", "in", "like", "regexp", "eq", "ne", "lt", "le", "gt", "ge", "str", "where"};
  return keywords.count(name) > 0 || grammar.count(boost::algorithm::to_lower_copy(name)) > 0;
}

// The filter grammar's identifier: a letter, then letters, digits and '_'.
bool is_identifier(const std::string &name) {
  if (name.empty()) return false;
  if (!std::isalpha(static_cast<unsigned char>(name[0]))) return false;
  for (const char c : name)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
  return true;
}

bool check_name(const std::string &name, std::set<std::string> &seen, std::string &error) {
  if (!is_identifier(name)) {
    error = "Invalid column name '" + name + "': a column name must start with a letter and contain only letters, digits and '_'";
    return false;
  }
  if (is_reserved_name(name)) {
    error = "Invalid column name '" + name + "': it is a reserved keyword, pick another name";
    return false;
  }
  if (!seen.insert(name).second) {
    error = "Column '" + name + "' is declared twice";
    return false;
  }
  return true;
}

bool parse_type(std::string type, column_type &out) {
  // VARCHAR(64), DECIMAL(10,2): the size is irrelevant here.
  const std::string::size_type paren = type.find('(');
  if (paren != std::string::npos) type = type.substr(0, paren);
  boost::algorithm::trim(type);
  boost::algorithm::to_upper(type);
  if (type.empty() || type == "TEXT" || type == "VARCHAR" || type == "CHAR" || type == "STRING" || type == "NVARCHAR" || type == "CLOB") {
    out = column_type::text;
  } else if (type == "INTEGER" || type == "INT" || type == "BIGINT" || type == "SMALLINT" || type == "TINYINT") {
    out = column_type::integer;
  } else if (type == "REAL" || type == "FLOAT" || type == "DOUBLE" || type == "NUMERIC" || type == "DECIMAL") {
    out = column_type::real;
  } else {
    return false;
  }
  return true;
}

// Split the declaration on commas outside parentheses (DECIMAL(10,2)).
std::vector<std::string> split_declaration(const std::string &spec) {
  std::vector<std::string> out;
  std::string current;
  int depth = 0;
  for (const char c : spec) {
    if (c == '(') ++depth;
    if (c == ')' && depth > 0) --depth;
    if (c == ',' && depth == 0) {
      out.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }
  out.push_back(current);
  return out;
}

boost::optional<long long> parse_integer(const std::string &raw) {
  const std::string value = boost::algorithm::trim_copy(raw);
  if (value.empty()) return boost::none;
  errno = 0;
  char *end = nullptr;
  const long long parsed = std::strtoll(value.c_str(), &end, 10);
  if (errno == 0 && end != nullptr && *end == '\0') return parsed;
  // SQLite's INTEGER affinity also takes "3.0": a real with no fraction.
  end = nullptr;
  const double d = std::strtod(value.c_str(), &end);
  if (end != nullptr && *end == '\0' && std::isfinite(d) && d == std::floor(d) && std::fabs(d) < 9.2e18) return static_cast<long long>(d);
  return boost::none;
}

double parse_real(const std::string &raw) {
  const std::string value = boost::algorithm::trim_copy(raw);
  if (value.empty()) return std::numeric_limits<double>::quiet_NaN();
  char *end = nullptr;
  const double d = std::strtod(value.c_str(), &end);
  if (end == nullptr || *end != '\0') return std::numeric_limits<double>::quiet_NaN();
  return d;
}

void append_utf8(std::string &out, unsigned int cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

bool decode_utf16(const std::string &bytes, std::size_t offset, bool little_endian, std::string &out, std::string &error) {
  if ((bytes.size() - offset) % 2 != 0) {
    error = "The file is not valid UTF-16: it has an odd number of bytes";
    return false;
  }
  out.clear();
  out.reserve((bytes.size() - offset) / 2);
  auto unit_at = [&](std::size_t i) -> unsigned int {
    const unsigned int a = static_cast<unsigned char>(bytes[i]);
    const unsigned int b = static_cast<unsigned char>(bytes[i + 1]);
    return little_endian ? (a | (b << 8)) : ((a << 8) | b);
  };
  for (std::size_t i = offset; i < bytes.size(); i += 2) {
    unsigned int cp = unit_at(i);
    if (cp >= 0xD800 && cp <= 0xDBFF) {
      if (i + 3 < bytes.size()) {
        const unsigned int low = unit_at(i + 2);
        if (low >= 0xDC00 && low <= 0xDFFF) {
          cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
          i += 2;
          append_utf8(out, cp);
          continue;
        }
      }
      cp = 0xFFFD;  // unpaired high surrogate
    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
      cp = 0xFFFD;  // unpaired low surrogate
    }
    append_utf8(out, cp);
  }
  return true;
}

bool has_prefix(const std::string &bytes, const char *prefix, std::size_t len) { return bytes.size() >= len && bytes.compare(0, len, prefix, len) == 0; }

// "," / ";" / "\t" / "tab" / "|" ... -> the single character.
bool parse_char_option(const std::string &option, const std::string &value, bool allow_none, char &out, std::string &error) {
  const std::string lower = boost::algorithm::to_lower_copy(value);
  if (allow_none && (value.empty() || lower == "none")) {
    out = '\0';
    return true;
  }
  if (lower == "\\t" || lower == "tab") {
    out = '\t';
    return true;
  }
  if (lower == "semicolon") {
    out = ';';
    return true;
  }
  if (lower == "comma") {
    out = ',';
    return true;
  }
  if (lower == "pipe") {
    out = '|';
    return true;
  }
  if (value.size() != 1 || value[0] == '\r' || value[0] == '\n') {
    error = "Invalid " + option + ": '" + value + "' (expected a single character" + (allow_none ? ", or none" : "") + ")";
    return false;
  }
  out = value[0];
  return true;
}

// --- the filter ------------------------------------------------------------

typedef std::shared_ptr<const std::vector<column_spec>> columns_ptr;

// Owns everything it exposes: the row's cells are copied out of the parsed
// file, and the column list is shared (immutable) between the rows.
struct filter_obj {
  std::vector<std::string> fields;
  long long row = 0;
  columns_ptr columns;

  std::string cell(const std::size_t i) const { return i < fields.size() ? fields[i] : std::string(); }
  long long get_row() const { return row; }
  long long get_field_count() const { return static_cast<long long>(fields.size()); }
  std::string get_values() const {
    std::string out;
    if (!columns) return out;
    for (std::size_t i = 0; i < columns->size(); ++i) {
      // Space, not ", ": the rows themselves are joined with ", " in %(list).
      if (!out.empty()) out += " ";
      out += (*columns)[i].name + "=" + cell(i);
    }
    return out;
  }
  std::string show() const { return "row " + std::to_string(row); }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_int_var("row", parsers::where::type_int, [](auto obj) { return obj->get_row(); }, "Number of the data row (1 is the first row after the header)")
        .no_perf();
    registry_
        .add_int_var("field_count", parsers::where::type_int, [](auto obj) { return obj->get_field_count(); },
                     "Number of fields on this row; a row with fewer fields than declared columns has the missing ones empty")
        .no_perf();
    registry_.add_string_var("values", [](auto obj) { return obj->get_values(); }, "All declared columns of the row as name=value, separated by spaces (the default detail-syntax)");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

void register_columns(filter_type &filter, const std::vector<column_spec> &columns) {
  for (std::size_t i = 0; i < columns.size(); ++i) {
    const column_spec &col = columns[i];
    const std::string &name = col.name;
    switch (col.type) {
      case column_type::text:
        filter.context->registry_.add_string_var(name, [i](auto obj) { return obj->cell(i); }, "Column " + name + " (TEXT)");
        break;
      case column_type::integer:
        // An empty or non-numeric cell has no value: it renders as '' and
        // every numeric comparison on it is false, so `x > 5` cannot fire on
        // a blank cell; `x = ''` tests for one.
        filter.context->registry_.add_optional_int_var(name, [i](auto obj) { return parse_integer(obj->cell(i)); }, "",
                                                       "Column " + name + " (INTEGER)");
        break;
      case column_type::real: {
        auto var = std::make_shared<parsers::where::filter_variable<std::shared_ptr<filter_obj>>>(name, parsers::where::type_float,
                                                                                                    "Column " + name + " (REAL)");
        var->f_function = [i](std::shared_ptr<filter_obj> o, parsers::where::evaluation_context) { return parse_real(o->cell(i)); };
        var->i_function = [i](std::shared_ptr<filter_obj> o, parsers::where::evaluation_context) {
          return str::to_int64_saturating(parse_real(o->cell(i)));
        };
        var->s_function = [i](std::shared_ptr<filter_obj> o, parsers::where::evaluation_context) { return o->cell(i); };
        filter.context->registry_.add(var, false);
        break;
      }
    }
  }
}

}  // namespace

// --- parsing ---------------------------------------------------------------

bool parse_columns(const std::string &spec, std::vector<column_spec> &out, std::string &error) {
  out.clear();
  std::set<std::string> seen;
  for (std::string entry : split_declaration(spec)) {
    boost::algorithm::trim(entry);
    if (entry.empty()) {
      error = "Invalid columns: an empty column in '" + spec + "'";
      return false;
    }
    std::string name = entry, type;
    const std::string::size_type ws = entry.find_first_of(" \t");
    if (ws != std::string::npos) {
      name = entry.substr(0, ws);
      type = boost::algorithm::trim_copy(entry.substr(ws));
    }
    column_spec col;
    col.name = name;
    if (!parse_type(type, col.type)) {
      error = "Invalid type '" + type + "' for column '" + name + "' (expected TEXT, INTEGER or REAL)";
      return false;
    }
    if (!check_name(name, seen, error)) return false;
    out.push_back(col);
  }
  return true;
}

bool columns_from_header(const std::vector<std::string> &header, std::vector<column_spec> &out, std::string &error) {
  out.clear();
  std::set<std::string> seen;
  for (const std::string &raw : header) {
    column_spec col;
    col.name = boost::algorithm::trim_copy(raw);
    if (!check_name(col.name, seen, error)) {
      error += " (taken from the header row; name the columns with columns= instead)";
      return false;
    }
    out.push_back(col);
  }
  if (out.empty()) {
    error = "The header row is empty";
    return false;
  }
  return true;
}

bool decode_text(const std::string &bytes, const std::string &encoding_in, std::string &out, std::string &error) {
  std::string encoding = boost::algorithm::to_lower_copy(encoding_in);
  boost::algorithm::erase_all(encoding, "-");
  boost::algorithm::erase_all(encoding, "_");
  const bool bom8 = has_prefix(bytes, "\xEF\xBB\xBF", 3);
  const bool bom_le = has_prefix(bytes, "\xFF\xFE", 2);
  const bool bom_be = has_prefix(bytes, "\xFE\xFF", 2);

  if (encoding.empty() || encoding == "auto") {
    if (bom_le) return decode_utf16(bytes, 2, true, out, error);
    if (bom_be) return decode_utf16(bytes, 2, false, out, error);
    out = bom8 ? bytes.substr(3) : bytes;
    return true;
  }
  if (encoding == "utf8") {
    out = bom8 ? bytes.substr(3) : bytes;
    return true;
  }
  if (encoding == "utf16le" || encoding == "unicode") return decode_utf16(bytes, bom_le ? 2 : 0, true, out, error);
  if (encoding == "utf16be") return decode_utf16(bytes, bom_be ? 2 : 0, false, out, error);
  if (encoding == "latin1" || encoding == "iso88591") {
    out.clear();
    out.reserve(bytes.size());
    for (const char c : bytes) append_utf8(out, static_cast<unsigned char>(c));
    return true;
  }
  error = "Invalid encoding: '" + encoding_in + "' (expected auto, utf-8, utf-16le, utf-16be or latin1)";
  return false;
}

bool split_csv(const std::string &text, const char delimiter, const char quote, std::vector<csv_record> &out, std::string &error) {
  out.clear();
  csv_record record;
  std::string field;
  std::size_t line = 1;
  record.line = line;
  bool in_quotes = false;
  bool quoted_field = false;    // the current field opened with a quote
  bool record_has_data = false;  // anything (even an empty quoted field) on this record
  std::size_t quote_line = 0;

  auto end_field = [&]() {
    record.fields.push_back(field);
    field.clear();
    quoted_field = false;
  };
  auto end_record = [&]() {
    if (record_has_data) {
      end_field();
      out.push_back(record);
    }
    record.fields.clear();
    field.clear();
    quoted_field = false;
    record_has_data = false;
  };

  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (in_quotes) {
      if (c == quote) {
        if (i + 1 < text.size() && text[i + 1] == quote) {
          field += quote;  // "" inside a quoted field is one quote
          ++i;
        } else {
          in_quotes = false;
        }
      } else {
        if (c == '\n' || (c == '\r' && !(i + 1 < text.size() && text[i + 1] == '\n'))) ++line;
        field += c;
      }
      continue;
    }
    if (quote != '\0' && c == quote && field.empty() && !quoted_field) {
      in_quotes = true;
      quoted_field = true;
      record_has_data = true;
      quote_line = line;
      continue;
    }
    if (c == delimiter) {
      record_has_data = true;
      end_field();
      continue;
    }
    if (c == '\r' || c == '\n') {
      if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
      end_record();
      ++line;
      record.line = line;
      continue;
    }
    // Text after a closing quote ("a"b) is kept as written, as most readers do.
    field += c;
    record_has_data = true;
  }
  if (in_quotes) {
    error = "Unterminated quoted field starting on line " + std::to_string(quote_line);
    return false;
  }
  end_record();
  return true;
}

// --- the check -------------------------------------------------------------

void check_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const check::access::path_policy &access, const read_function &read) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::string source, columns_arg, delimiter_arg, quote_arg, encoding, max_size;
  bool header = false;
  int skip_lines = 0;
  std::uint64_t max_bytes = 0;

  filter_type filter;
  // No default thresholds: what is wrong is entirely the caller's to say
  // (`critical=count > 0` over a filter, or a threshold on one cell). A file
  // with no matching row is OK - the row-count mode reads "nothing matched" as
  // "nothing is wrong"; set empty-state for the single-value mode.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("%(status): %(problem_list)", "%(values)", "row_%(row)", "%(status): No rows matched the filter",
                           "%(status): %(count) rows matched");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("file", po::value<std::string>(&source),
      "The CSV file to read: a local path, or a UNC path (\\\\server\\share\\file.csv) on Windows. Held against 'file access' in "
      "[/settings/logfile], like check_logfile.")
    ("columns", po::value<std::string>(&columns_arg),
      "The columns, in file order, as an SQL column list: 'Hostname TEXT, WaitingUpdates INTEGER, Load REAL'. Each becomes a keyword of that name and "
      "type (TEXT when the type is left out). Fields beyond the declared columns are ignored. When omitted the header row names the columns, all TEXT "
      "(requires header=true).")
    ("header", po::value<bool>(&header)->implicit_value(true)->default_value(false),
      "The first row (after skip-lines) is a header, not data. Used for the column names when columns= is not given.")
    ("delimiter", po::value<std::string>(&delimiter_arg)->default_value(","),
      "The field separator: a single character, or tab, semicolon, comma or pipe.")
    ("quote", po::value<std::string>(&quote_arg)->default_value("\""),
      "The quote character; a doubled quote inside a quoted field is a literal quote. none disables quoting.")
    ("encoding", po::value<std::string>(&encoding)->default_value("auto"),
      "The text encoding: auto (UTF-8, or UTF-16LE/BE when the file starts with a byte-order mark, as Windows exports often do), utf-8, utf-16le, "
      "utf-16be or latin1.")
    ("skip-lines", po::value<int>(&skip_lines)->default_value(0),
      "Records to drop from the top before the header or the data, such as the '#TYPE' line Export-Csv writes on Windows PowerShell 5.")
    ("max-size", po::value<std::string>(&max_size)->default_value("16m"),
      "Refuse a file larger than this, in bytes optionally suffixed k/m/g. The whole file is read into memory.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (source.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No CSV specified (use file=<path>)");

  char delimiter = ',', quote = '"';
  std::string error;
  if (!parse_char_option("delimiter", delimiter_arg, false, delimiter, error)) return nscapi::protobuf::functions::set_response_bad(*response, error);
  if (!parse_char_option("quote", quote_arg, true, quote, error)) return nscapi::protobuf::functions::set_response_bad(*response, error);
  if (quote != '\0' && quote == delimiter) return nscapi::protobuf::functions::set_response_bad(*response, "The quote and the delimiter must differ");
  if (skip_lines < 0) return nscapi::protobuf::functions::set_response_bad(*response, "skip-lines must not be negative");
  try {
    const long long parsed = str::format::decode_byte_units(max_size);
    if (parsed < 0) return nscapi::protobuf::functions::set_response_bad(*response, "Invalid max-size: " + max_size + " (must not be negative)");
    max_bytes = static_cast<std::uint64_t>(parsed);
  } catch (const std::exception &) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid max-size: " + max_size + " (expected a byte count, optionally suffixed k/m/g)");
  }

  std::vector<column_spec> columns;
  if (!columns_arg.empty() && !parse_columns(columns_arg, columns, error)) return nscapi::protobuf::functions::set_response_bad(*response, error);
  if (columns_arg.empty() && !header) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No columns declared: give columns=<name TYPE, ...> or header=true");
  }

  // Hold the path against [/settings/logfile] 'file access' before anything
  // is opened: the columns return the file's contents, exactly as
  // check_logfile's do, so the two checks share one gate.
  {
    const check::access::decision decision = access.resolve(source);
    if (!decision.allowed) return nscapi::protobuf::functions::set_response_bad(*response, decision.error);
    source = decision.value;
  }

  std::string bytes;
  if (!read(source, max_bytes, bytes, error)) return nscapi::protobuf::functions::set_response_bad(*response, error);
  if (max_bytes > 0 && bytes.size() > max_bytes) {
    return nscapi::protobuf::functions::set_response_bad(*response, "CSV is larger than max-size (" + max_size + "): " + source);
  }

  std::string text;
  if (!decode_text(bytes, encoding, text, error)) return nscapi::protobuf::functions::set_response_bad(*response, error + ": " + source);
  bytes.clear();

  std::vector<csv_record> records;
  if (!split_csv(text, delimiter, quote, records, error)) return nscapi::protobuf::functions::set_response_bad(*response, error + ": " + source);
  text.clear();

  std::size_t first = std::min(records.size(), static_cast<std::size_t>(skip_lines));
  if (header) {
    if (first >= records.size()) return nscapi::protobuf::functions::set_response_bad(*response, "header=true but the CSV has no header row: " + source);
    if (columns.empty() && !columns_from_header(records[first].fields, columns, error)) {
      return nscapi::protobuf::functions::set_response_bad(*response, error);
    }
    ++first;
  }

  register_columns(filter, columns);
  if (!filter_helper.build_filter(filter)) return;

  const columns_ptr shared_columns = std::make_shared<const std::vector<column_spec>>(columns);
  long long row = 0;
  for (std::size_t i = first; i < records.size(); ++i) {
    const std::shared_ptr<filter_obj> record = std::make_shared<filter_obj>();
    record->fields = std::move(records[i].fields);
    record->row = ++row;
    record->columns = shared_columns;
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
           const check::access::path_policy &access) {
  check_with(request, response, access, &read_local_file);
}

}  // namespace check_csv_command
