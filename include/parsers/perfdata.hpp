// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <str/utils_no_boost.hpp>
#include <str/xtos.hpp>
#include <string>
#include <vector>

namespace parsers {
namespace perfdata {

struct builder {
  virtual ~builder() = default;
  virtual void add(std::string alias) = 0;
  virtual void set_value(double value) = 0;
  virtual void set_warning(double value) = 0;
  virtual void set_critical(double value) = 0;
  virtual void set_minimum(double value) = 0;
  virtual void set_maximum(double value) = 0;
  virtual void set_unit(const std::string &value) = 0;

  // Threshold-range setters (Nagios range syntax like "4:5", ":10",
  // "@0:90", "~:10"). Defaulted to no-ops so existing builders compile
  // unchanged; the parser still calls set_warning / set_critical with the
  // lower bound for back-compat. Builders that need to preserve the full
  // syntax (notably the protobuf builder used by external-script paths)
  // override these to carry the string through. See GitHub issue #748.
  virtual void set_warning_range(const std::string & /*range*/) {}
  virtual void set_critical_range(const std::string & /*range*/) {}

  virtual void next() = 0;

  virtual void add_string(std::string alias, std::string value) = 0;
};

inline double trim_to_double(std::string s) {
  const auto pend = s.find_first_not_of("0123456789,.-");
  if (pend != std::string::npos) s = s.substr(0, pend);
  str::utils::replace(s, ",", ".");
  if (s.empty()) {
    return 0.0;
  }
  try {
    return str::stox<double>(s);
  } catch (...) {
    return 0.0;
  }
}

// True if the threshold field uses Nagios range syntax (anything beyond a
// simple number with optional UOM). The Nagios plugin development
// guidelines define ranges as:
//   value       single value, treated as 0..value
//   low:high    alert if outside [low, high]
//   :high       alert if value > high
//   low:        alert if value < low
//   @low:high   alert if value is inside [low, high] (inverted)
//   ~ marker    -infinity (e.g. ~:10 = alert when > 10)
// We don't try to parse the range here - we only detect that it IS a
// range so the parser preserves the original string. The numeric float
// (lower bound) is still set so consumers that only read the float don't
// regress.
inline bool is_threshold_range(const std::string &s) {
  if (s.empty()) return false;
  if (s[0] == '@' || s[0] == '~') return true;
  return s.find(':') != std::string::npos;
}

inline void parse(std::shared_ptr<builder> builder, const std::string &perff) {
  std::string perf = perff;
  // TODO: make this work with const!

  const std::string perf_separator = " ";
  const std::string perf_lable_enclosure = "'";
  const std::string perf_equal_sign = "=";
  const std::string perf_item_splitter = ";";
  const std::string perf_valid_number = "0123456789,.-";

  while (true) {
    if (perf.empty()) return;
    std::string::size_type p = 0;
    p = perf.find_first_not_of(perf_separator, p);
    if (p != 0) perf = perf.substr(p);
    if (perf[0] == perf_lable_enclosure[0]) {
      p = perf.find(perf_lable_enclosure[0], 1) + 1;
      if (p == std::string::npos) return;
    }
    p = perf.find(perf_separator, p);
    if (p == 0) return;
    std::string chunk;
    if (p == std::string::npos) {
      chunk = perf;
      perf = std::string();
    } else {
      chunk = perf.substr(0, p);
      p = perf.find_first_not_of(perf_separator, p);
      if (p == std::string::npos)
        perf = std::string();
      else
        perf = perf.substr(p);
    }
    std::vector<std::string> items;
    str::utils::split(items, chunk, perf_item_splitter);
    if (items.empty()) {
      builder->add_string("invalid", "invalid performance data");
      builder->next();
      break;
    }

    std::pair<std::string, std::string> fitem = str::utils::split2(items[0], perf_equal_sign);
    std::string alias = fitem.first;
    if (!alias.empty() && alias[0] == perf_lable_enclosure[0] && alias[alias.size() - 1] == perf_lable_enclosure[0]) alias = alias.substr(1, alias.size() - 2);

    if (alias.empty()) continue;

    std::string::size_type pstart = fitem.second.find_first_of(perf_valid_number);
    if (pstart == std::string::npos) {
      // Per the Nagios plugin guidelines, scripts may emit the literal "U" to indicate
      // an explicitly undefined value. Preserve it as a string value so that consumers
      // can distinguish it from a real 0 (issue #669). Accept "U" or "u" optionally
      // followed by a "%" suffix (mirroring the corresponding numeric value's unit).
      // Anything longer (e.g. "Unknown") is not the spec-defined undefined marker and
      // falls through to the legacy zero-value behaviour to avoid surprising existing
      // checks that emit non-numeric values for unrelated reasons.
      // See https://nagios-plugins.org/doc/guidelines.html#AEN200
      const std::string &v = fitem.second;
      const bool is_undef = (v == "U" || v == "u" || v == "U%" || v == "u%");
      if (is_undef) {
        builder->add_string(alias, "U");
        builder->next();
        continue;
      }
      builder->add(alias);
      builder->set_value(0);
      builder->next();
      continue;
    }
    builder->add(alias);
    if (pstart != 0) fitem.second = fitem.second.substr(pstart);
    std::string::size_type pend = fitem.second.find_first_not_of(perf_valid_number);
    if (pend == std::string::npos) {
      builder->set_value(trim_to_double(fitem.second));
    } else {
      builder->set_value(trim_to_double(fitem.second.substr(0, pend)));
      builder->set_unit(fitem.second.substr(pend));
    }
    // Warning / critical can be range syntax (e.g. "4:5"). Set the numeric
    // lower bound for back-compat AND, when the field is a range, forward
    // the original string so the formatter can round-trip it (issue #748).
    if (items.size() >= 2 && !items[1].empty()) {
      builder->set_warning(trim_to_double(items[1]));
      if (is_threshold_range(items[1])) builder->set_warning_range(items[1]);
    }
    if (items.size() >= 3 && !items[2].empty()) {
      builder->set_critical(trim_to_double(items[2]));
      if (is_threshold_range(items[2])) builder->set_critical_range(items[2]);
    }
    // min/max are single values per the Nagios spec - no range syntax.
    if (items.size() >= 4 && !items[3].empty()) builder->set_minimum(trim_to_double(items[3]));
    if (items.size() >= 5 && !items[4].empty()) builder->set_maximum(trim_to_double(items[4]));
    builder->next();
  }
}
}  // namespace perfdata
};  // namespace parsers