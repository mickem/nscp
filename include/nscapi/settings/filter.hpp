// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <nscapi/dll_defines.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/format.hpp>
#include <str/number_format.hpp>
#ifdef WIN32
#pragma warning(disable : 4251)
#endif

#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>
#include <utility>

namespace nscapi {
namespace settings_filters {

struct NSCAPI_EXPORT filter_object {
  bool debug;
  bool escape_html;
  std::string syntax_top;
  std::string syntax_detail;
  std::string target;
  std::string syntax_ok;
  std::string syntax_empty;
  // What joins the items of %(list) and friends; accepts \n, \r, \t and \\.
  // Mirrors the `list-separator` option of a queried check (issue #1370).
  std::string list_separator;
  // How the numbers of the message are rendered, mirroring the `decimals`,
  // `byte-unit`, `decimal-separator` and `thousands-separator` options of a
  // queried check (issue #1428). -1 decimals and empty strings mean "unset",
  // so apply_parent can inherit them from the default template.
  int decimals;
  std::string byte_unit;
  std::string decimal_separator;
  std::string thousands_separator;

 private:
  std::string filter_string_;

 public:
  std::string filter_ok;
  std::string filter_warn;
  std::string filter_crit;

  std::string perf_data;
  std::string perf_config;
  NSCAPI::nagiosReturn severity;
  std::string command;
  boost::optional<boost::posix_time::time_duration> max_age;
  boost::optional<boost::posix_time::time_duration> silent_period;
  std::string target_id;
  std::string source_id;
  std::string timeout_msg;

  filter_object(std::string syntax_top, std::string syntax_detail, std::string target)
      : debug(false),
        escape_html(false),
        syntax_top(std::move(syntax_top)),
        syntax_detail(std::move(syntax_detail)),
        target(std::move(target)),
        decimals(-1),
        severity(-1) {}

  filter_object(const filter_object &other) = default;

  void set_filter_string(const char *filter_string) { filter_string_ = filter_string; }
  const char *filter_string() const { return filter_string_.c_str(); }

  std::string to_string() const {
    std::stringstream ss;
    ss << "{TODO}";
    return ss.str();
  }

  void set_severity(const std::string &severity_) { severity = plugin_helper::translateReturn(severity_); }

  boost::posix_time::time_duration parse_time(const std::string &time) {
    std::string::size_type p = time.find_first_of("sSmMhHdDwW");
    if (p == std::string::npos) return boost::posix_time::seconds(boost::lexical_cast<long>(time));
    const long value = boost::lexical_cast<long>(time.substr(0, p));
    if ((time[p] == 's') || (time[p] == 'S')) return boost::posix_time::seconds(value);
    if ((time[p] == 'm') || (time[p] == 'M')) return boost::posix_time::minutes(value);
    if ((time[p] == 'h') || (time[p] == 'H')) return boost::posix_time::hours(value);
    if ((time[p] == 'd') || (time[p] == 'D')) return boost::posix_time::hours(value * 24);
    if ((time[p] == 'w') || (time[p] == 'W')) return boost::posix_time::hours(value * 24 * 7);
    return boost::posix_time::seconds(value);
  }

  void set_max_age(const std::string &age) {
    if (age != "none" && age != "infinite" && age != "false" && age != "off") max_age = parse_time(age);
  }
  void set_silent_period(const std::string &age) {
    if (age != "none" && age != "infinite" && age != "false" && age != "off") silent_period = parse_time(age);
  }

  // The number format these keys describe; unset keys keep their defaults.
  // Unlike the query path this has no error channel, so a nonsensical decimals
  // is clamped rather than rejected - the point is only to keep a config typo
  // from handing render_fixed an unbounded width and crashing the check.
  str::number_format number_format() const {
    str::number_format fmt;
    fmt.decimals = decimals < -1 ? -1 : (decimals > str::max_decimals ? str::max_decimals : decimals);
    fmt.byte_unit = byte_unit;
    if (!decimal_separator.empty()) fmt.decimal_separator = decimal_separator;
    fmt.thousands_separator = thousands_separator;
    return fmt;
  }

  // Whether the `byte unit` key names a unit we know about. The query path
  // rejects an unknown one outright; a real-time filter is persistent
  // monitoring, so it warns and falls back to auto-scaling rather than
  // dropping the filter - but either way an invalid unit is no longer silent.
  // Returns an error message, or "" when the unit is empty or valid.
  std::string invalid_byte_unit() const {
    if (!byte_unit.empty() && str::format::byte_unit_index(byte_unit) < 0)
      return "Invalid byte unit: " + byte_unit + " (expected one of B, KB, MB, GB, TB, PB, EB)";
    return "";
  }

  void read_object(settings_helper::path_extension &path, bool is_default);
  void apply_parent(const filter_object &parent);
};
}  // namespace settings_filters
}  // namespace nscapi