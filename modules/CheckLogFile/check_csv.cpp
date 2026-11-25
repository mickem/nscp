// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_csv.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <files/safe_file.hpp>
#include <locale>
#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/csv/csv_parser.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <set>
#include <sstream>
#include <str/format.hpp>
#include <str/utf8.hpp>

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
  static const std::set<std::string> keywords = {"count", "total",   "ok_count",  "warn_count", "crit_count",   "problem_count",
                                                 "list",  "ok_list", "warn_list", "crit_list",  "problem_list", "detail_list",
                                                 "lines", "status",  "sep",       "row",        "field_count",  "values"};
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

// A decimal in the file's own notation ('.' separator, as RFC 4180 data and
// every spreadsheet export in the C locale write it) whatever the process
// locale is. The agent calls setlocale(LC_ALL, "") at startup, so strtod would
// read "3.75" as no value on a host whose locale writes "3,75".
boost::optional<double> parse_real(const std::string &raw) {
  const std::string value = boost::algorithm::trim_copy(raw);
  if (value.empty()) return boost::none;
  std::istringstream in(value);
  in.imbue(std::locale::classic());
  double d = 0;
  in >> d;
  // Anything left over ("3.75 kg", "3,75") or out of range is not a number.
  if (in.fail() || in.peek() != std::char_traits<char>::eof() || !std::isfinite(d)) return boost::none;
  return d;
}

boost::optional<long long> parse_integer(const std::string &raw) {
  const std::string value = boost::algorithm::trim_copy(raw);
  if (value.empty()) return boost::none;
  errno = 0;
  char *end = nullptr;
  const long long parsed = std::strtoll(value.c_str(), &end, 10);
  if (errno == 0 && end != nullptr && *end == '\0') return parsed;
  // SQLite's INTEGER affinity also takes "3.0": a real with no fraction.
  const boost::optional<double> d = parse_real(value);
  if (d && d.value() == std::floor(d.value()) && std::fabs(d.value()) < 9.2e18) return static_cast<long long>(d.value());
  return boost::none;
}

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
  if (value.size() != 1) {
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
    registry_
        .add_int_var(
            "row", parsers::where::type_int, [](auto obj) { return obj->get_row(); }, "Number of the data row (1 is the first row after the header)")
        .no_perf();
    registry_
        .add_int_var(
            "field_count", parsers::where::type_int, [](auto obj) { return obj->get_field_count(); },
            "Number of fields on this row; a row with fewer fields than declared columns has the missing ones empty")
        .no_perf();
    registry_.add_string_var(
        "values", [](auto obj) { return obj->get_values(); }, "All declared columns of the row as name=value, separated by spaces (the default detail-syntax)");
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
        filter.context->registry_.add_optional_int_var(name, [i](auto obj) { return parse_integer(obj->cell(i)); }, "", "Column " + name + " (INTEGER)");
        break;
      case column_type::real: {
        // Same "no value" contract as INTEGER, but compared as a decimal: an
        // integer threshold (`x > 3`) does not truncate 3.75, and perfdata
        // keeps the fraction. A present value renders as the cell's text.
        filter.context->registry_.add_optional_float_var(
            name, [i](auto obj) { return parse_real(obj->cell(i)); }, [i](auto obj) { return obj->cell(i); }, "", "Column " + name + " (REAL)");
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
  const utf8::byte_order_mark bom = utf8::detect_bom(bytes);
  // A mark that agrees with the requested encoding is dropped; any other
  // leading bytes are data.
  const auto without = [&](const utf8::byte_order_mark expected) { return bom == expected ? bytes.substr(utf8::bom_size(bom)) : bytes; };
  const auto utf16 = [&](const utf8::byte_order_mark expected, const bool little_endian) {
    if (utf8::from_utf16(without(expected), little_endian, out)) return true;
    error = "The file is not valid UTF-16: it has an odd number of bytes";
    return false;
  };

  if (encoding.empty() || encoding == "auto") {
    if (bom == utf8::byte_order_mark::utf16le) return utf16(bom, true);
    if (bom == utf8::byte_order_mark::utf16be) return utf16(bom, false);
    out = without(utf8::byte_order_mark::utf8);
    return true;
  }
  if (encoding == "utf8") {
    out = without(utf8::byte_order_mark::utf8);
    return true;
  }
  if (encoding == "utf16le" || encoding == "unicode") return utf16(utf8::byte_order_mark::utf16le, true);
  if (encoding == "utf16be") return utf16(utf8::byte_order_mark::utf16be, false);
  if (encoding == "latin1" || encoding == "iso88591") {
    out = utf8::from_latin1(bytes);
    return true;
  }
  error = "Invalid encoding: '" + encoding_in + "' (expected auto, utf-8, utf-16le, utf-16be or latin1)";
  return false;
}

// --- the size limit --------------------------------------------------------

bool parse_size(const std::string &value_in, std::uint64_t &bytes, std::string &error) {
  const std::string value = boost::algorithm::trim_copy(value_in);
  const std::string::size_type digits = value.find_first_not_of("0123456789");
  const std::string unit = digits == std::string::npos ? "" : boost::algorithm::trim_copy(value.substr(digits));
  long long factor = 1;
  if (value.empty() || digits == 0 || (!unit.empty() && !str::format::byte_unit_factor(unit, factor))) {
    error = "'" + value_in + "' is not a size (expected a byte count, optionally suffixed k, m or g)";
    return false;
  }
  try {
    const long long number = boost::lexical_cast<long long>(value.substr(0, digits));
    bytes = static_cast<std::uint64_t>(str::format::mul_checked(number, factor, "size"));
  } catch (const std::exception &) {
    error = "'" + value_in + "' is too large a size";
    return false;
  }
  return true;
}

bool read_local_file(const std::string &path, const std::uint64_t max_bytes, std::string &bytes, std::string &error) {
  switch (safe_file::read_all(path, max_bytes, bytes, error)) {
    case safe_file::read_status::ok:
      return true;
    case safe_file::read_status::too_large:
      error = "CSV is larger than max-size (" + std::to_string(max_bytes) + " bytes): " + path;
      return false;
    case safe_file::read_status::failed:
    default:
      return false;
  }
}

bool effective_max_bytes(const std::string &requested, const std::uint64_t ceiling, std::uint64_t &max_bytes, bool &capped, std::string &error) {
  std::uint64_t asked = 0;
  if (!parse_size(requested, asked, error)) {
    error = "Invalid max-size: " + error;
    return false;
  }
  // "No limit" from the caller is as much as the operator allows, and so is
  // anything above that.
  capped = ceiling > 0 && (asked == 0 || asked > ceiling);
  max_bytes = capped ? ceiling : asked;
  return true;
}

// --- the check -------------------------------------------------------------

void check_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const check::access::path_policy &access, const std::uint64_t max_bytes_ceiling, const read_function &read) {
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
      "Lines to drop from the top of the file before it is parsed, such as the '#TYPE' line Export-Csv writes on Windows PowerShell 5 or a "
      "report title. Every line counts, blank ones included, and nothing in a skipped line is interpreted: a quote there does not open a field.")
    ("max-size", po::value<std::string>(&max_size)->default_value("16m"),
      "Refuse a file larger than this, in bytes optionally suffixed k/m/g. The whole file is read into memory. A value above 'max csv size' in "
      "[/settings/logfile] (64m unless configured), or 0, means that setting's value.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (source.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No CSV specified (use file=<path>)");

  csv_parser::options format;
  std::string error;
  if (!parse_char_option("delimiter", delimiter_arg, false, format.delimiter, error)) return nscapi::protobuf::functions::set_response_bad(*response, error);
  if (!parse_char_option("quote", quote_arg, true, format.quote, error)) return nscapi::protobuf::functions::set_response_bad(*response, error);
  error = csv_parser::validate(format);
  if (!error.empty()) return nscapi::protobuf::functions::set_response_bad(*response, error);
  if (skip_lines < 0) return nscapi::protobuf::functions::set_response_bad(*response, "skip-lines must not be negative");
  format.skip_lines = static_cast<std::size_t>(skip_lines);
  bool capped = false;
  if (!effective_max_bytes(max_size, max_bytes_ceiling, max_bytes, capped, error)) return nscapi::protobuf::functions::set_response_bad(*response, error);
  // Where the limit that refused a file is the operator's rather than the
  // caller's, say so: raising max-size= would not help.
  const std::string too_large_hint = capped ? " (the limit is 'max csv size' in [/settings/logfile])" : "";

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
  if (!read(source, max_bytes, bytes, error)) {
    if (error.find("larger than max-size") != std::string::npos) error += too_large_hint;
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }
  if (max_bytes > 0 && bytes.size() > max_bytes) {
    return nscapi::protobuf::functions::set_response_bad(*response,
                                                         "CSV is larger than max-size (" + std::to_string(max_bytes) + " bytes): " + source + too_large_hint);
  }

  std::string text;
  if (!decode_text(bytes, encoding, text, error)) return nscapi::protobuf::functions::set_response_bad(*response, error + ": " + source);
  bytes.clear();

  std::vector<csv_parser::record> records;
  if (!csv_parser::parse(text, format, records, error)) return nscapi::protobuf::functions::set_response_bad(*response, error + ": " + source);
  text.clear();

  // skip-lines was applied by the parser, before anything was interpreted.
  std::size_t first = 0;
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
           const check::access::path_policy &access, const std::uint64_t max_bytes_ceiling) {
  check_with(request, response, access, max_bytes_ceiling, &read_local_file);
}

}  // namespace check_csv_command
