// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ncpa_check.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <stdexcept>

namespace ncpa {

namespace {

// Python's round(x, 2) for the values NCPA puts through it. Half-away-from-zero
// rather than banker's rounding: the difference only shows on an exact .005,
// and matching printf (which NCPA's "%0.2f" also uses) keeps the rendered text
// and the JSON value from disagreeing on such a sample.
double round2(const double v) {
  if (!std::isfinite(v)) return v;
  return std::round(v * 100.0) / 100.0;
}

std::string format_double(const double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%0.2f", v);
  return {buf};
}

std::string format_int(const double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
  return {buf};
}

}  // namespace

value value::from_double(const double v) {
  value out;
  out.number = v;
  return out;
}

value value::from_int(const long long v) {
  value out;
  out.number = static_cast<double>(v);
  out.is_integer = true;
  return out;
}

value value::from_string(std::string s) {
  value out;
  out.is_string = true;
  out.text = std::move(s);
  return out;
}

std::string value::format() const {
  if (is_string) return text;
  if (is_integer) return format_int(number);
  return format_double(number);
}

bool is_within_range(const std::string &range, const double v) {
  if (range.empty()) return false;

  // The same six shapes agent/listener/nodes.py matches, in the same order -
  // the order matters, because "10" and ":10" would both match a laxer
  // pattern and they do not mean the same thing.
  static const std::string first = "(-?[0-9]+(?:\\.[0-9]+)?)";
  static const boost::regex re_plain("^" + first + "$");
  static const boost::regex re_above("^" + first + ":$");
  static const boost::regex re_upto("^:" + first + "$");
  static const boost::regex re_neg_inf("^~:" + first + "$");
  static const boost::regex re_between("^" + first + ":" + first + "$");
  static const boost::regex re_inside("^@" + first + ":" + first + "$");

  boost::smatch what;
  if (boost::regex_match(range, what, re_plain)) {
    const double f = std::stod(what.str(1));
    // NCPA alerts on a negative value for a bare threshold, however small the
    // threshold is. Reproduced rather than corrected: a check written against
    // the real agent relies on it.
    return v > f || v < 0;
  }
  if (boost::regex_match(range, what, re_above)) return v < std::stod(what.str(1));
  if (boost::regex_match(range, what, re_upto)) {
    const double f = std::stod(what.str(1));
    return v > f || v < 0;
  }
  if (boost::regex_match(range, what, re_neg_inf)) return v > std::stod(what.str(1));
  if (boost::regex_match(range, what, re_between)) {
    const double lo = std::stod(what.str(1));
    const double hi = std::stod(what.str(2));
    return v < lo || v > hi;
  }
  if (boost::regex_match(range, what, re_inside)) {
    const double lo = std::stod(what.str(1));
    const double hi = std::stod(what.str(2));
    return !(v < lo || v > hi);
  }
  throw std::runtime_error("Improper warning/critical format.");
}

void adjust_scale(values_type &vals, const std::string &units, std::string &unit) {
  if (units.empty()) return;
  // Only a byte-valued node scales. A percentage or a count keeps the number it
  // had, so `-u G` against cpu/percent changes nothing - which is what the real
  // agent does, and what makes `units` safe to send on every request.
  if (unit != "b" && unit != "B") return;

  std::string u = boost::to_upper_copy(units);
  double factor = 1.0;
  std::string suffix;
  if (u == "T") {
    factor = 1e12;
    suffix = "T";
  } else if (u == "G") {
    factor = 1e9;
    suffix = "G";
  } else if (u == "M") {
    factor = 1e6;
    suffix = "M";
  } else if (u == "K") {
    factor = 1e3;
    suffix = "k";
  } else if (u == "TI") {
    factor = 1099511627776.0;
    suffix = "Ti";
  } else if (u == "GI") {
    factor = 1073741824.0;
    suffix = "Gi";
  } else if (u == "MI") {
    factor = 1048576.0;
    suffix = "Mi";
  } else if (u == "KI") {
    factor = 1024.0;
    suffix = "Ki";
  } else if (u == "B") {
    // An explicit `-u B` does not scale, but it does force the values to whole
    // bytes (NCPA: `if units == "B": val = int(val)`).
    for (value &v : vals) {
      if (v.is_string) continue;
      v.number = std::trunc(round2(v.number));
      v.is_integer = true;
    }
    return;
  } else {
    return;
  }

  for (value &v : vals) {
    if (v.is_string) continue;
    v.number = round2(v.number / factor);
    v.is_integer = false;
  }
  unit = suffix + unit;
}

void aggregate_values(values_type &vals, const std::string &mode) {
  if (vals.empty()) return;
  // A string value has nothing to aggregate; NCPA would raise and fall through
  // to UNKNOWN, so leaving the list alone is the closer answer.
  for (const value &v : vals) {
    if (v.is_string) return;
  }
  const auto by_number = [](const value &a, const value &b) { return a.number < b.number; };
  if (mode == "max") {
    const value m = *std::max_element(vals.begin(), vals.end(), by_number);
    vals.assign(1, m);
  } else if (mode == "min") {
    const value m = *std::min_element(vals.begin(), vals.end(), by_number);
    vals.assign(1, m);
  } else if (mode == "sum") {
    double sum = 0;
    bool all_int = true;
    for (const value &v : vals) {
      sum += v.number;
      all_int = all_int && v.is_integer;
    }
    vals.assign(1, all_int ? value::from_int(static_cast<long long>(sum)) : value::from_double(sum));
  } else if (mode == "avg") {
    double sum = 0;
    for (const value &v : vals) sum += v.number;
    // round(sum/len, 2), and a float even when the inputs were whole numbers -
    // NCPA's round() of a division always yields one.
    vals.assign(1, value::from_double(round2(sum / static_cast<double>(vals.size()))));
  }
}

std::string capitalize(const std::string &s) {
  if (s.empty()) return s;
  std::string out = boost::to_lower_copy(s);
  out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
  return out;
}

std::string elapsed_time(const double seconds) {
  struct interval {
    const char *name;
    long long count;
  };
  static const interval intervals[] = {{"days", 86400}, {"hours", 3600}, {"minutes", 60}, {"seconds", 1}};
  long long rest = static_cast<long long>(seconds);
  std::string out;
  for (const interval &i : intervals) {
    const long long v = rest / i.count;
    if (v == 0) continue;
    rest -= v * i.count;
    std::string name = i.name;
    // "1 day", not "1 days".
    if (v == 1) name.erase(name.size() - 1);
    if (!out.empty()) out += " ";
    out += std::to_string(v) + " " + name;
  }
  return out;
}

check_result render_check(const values_type &vals, const std::string &unit, const request_options &opts, const render_options &ropts) {
  check_result result;

  std::string proper_name = opts.title.empty() ? ropts.node_name : opts.title;
  boost::replace_all(proper_name, "|", "/");
  if (ropts.capitalize_title) proper_name = capitalize(proper_name);

  bool is_warning = false;
  bool is_critical = false;
  try {
    for (const value &v : vals) {
      if (v.is_string) {
        // float(value) on a string raises in NCPA, which its run_check turns
        // into UNKNOWN with the exception text. Only reachable when the caller
        // put a threshold on a string node.
        if (!opts.warning.empty() || !opts.critical.empty()) {
          throw std::runtime_error("could not convert string to float: '" + v.text + "'");
        }
        continue;
      }
      if (!opts.warning.empty() && is_within_range(opts.warning, v.number)) is_warning = true;
      if (!opts.critical.empty() && is_within_range(opts.critical, v.number)) is_critical = true;
    }
  } catch (const std::exception &e) {
    result.returncode = 3;
    result.stdout_text = e.what();
    return result;
  }

  std::string values_for_info_line;
  for (const value &v : vals) {
    if (!values_for_info_line.empty()) values_for_info_line += ", ";
    values_for_info_line += v.format();
    // NCPA joins value and unit with a space even when the unit is empty,
    // then rstrips the finished line - so a unitless single value ends up
    // without a trailing space but a unitless list keeps one before each comma.
    values_for_info_line += " " + unit;
  }

  std::string info_prefix = "OK";
  if (is_warning) {
    result.returncode = 1;
    info_prefix = "WARNING";
  }
  if (is_critical) {
    result.returncode = 2;
    info_prefix = "CRITICAL";
  }

  std::string perfdata_label = opts.perfdata_label;
  if (perfdata_label.empty()) {
    perfdata_label = opts.title.empty() ? ropts.node_name : opts.title;
    boost::replace_all(perfdata_label, "=", "_");
    boost::replace_all(perfdata_label, "'", "\"");
  }
  // A long unit is dropped from the perfdata rather than emitted: Nagios only
  // knows a handful of UOMs and a word there breaks the parse.
  const std::string perf_unit = unit.size() > 3 ? std::string() : unit;

  std::string perfdata;
  for (std::size_t i = 0; i < vals.size(); ++i) {
    std::string perf = "=" + vals[i].format() + perf_unit + ";";
    // Thresholds only describe the primary value of a parent check, so a
    // secondary child (primary_total != 0) emits empty threshold fields.
    if (ropts.primary_total == 0) {
      perf += opts.warning + ";" + opts.critical + ";";
    } else {
      perf += ";;";
    }
    const std::string label = vals.size() == 1 ? perfdata_label : perfdata_label + "_" + std::to_string(i);
    if (!perfdata.empty()) perfdata += " ";
    perfdata += "'" + label + "'" + perf;
  }
  result.perfdata = perfdata;

  std::string custom_output = ropts.custom_output;
  // Two nodes whose raw number means nothing to a human. NCPA special-cases
  // them by name in get_nagios_return; the comment there calls it a hack and
  // promises to remove it in NCPA 3, but the text is what operators' alert
  // histories contain today.
  if (ropts.node_name == "uptime" && !vals.empty() && !vals[0].is_string) {
    custom_output = proper_name + " was " + elapsed_time(vals[0].number);
    values_for_info_line.clear();
  } else if (ropts.node_name == "status" && !vals.empty() && !vals[0].is_string) {
    std::string readable = "unknown";
    if (vals[0].number == 0) {
      readable = "up";
    } else if (vals[0].number == 2) {
      readable = "down";
    }
    custom_output = proper_name + " is " + readable;
    values_for_info_line.clear();
  }

  std::string out;
  if (ropts.secondary_data) {
    out = proper_name + ": " + values_for_info_line;
  } else {
    std::string lead = proper_name + " was";
    if (!custom_output.empty()) lead = custom_output;
    out = lead + " " + values_for_info_line;
  }
  boost::trim_right(out);

  if (ropts.use_prefix) out = info_prefix + ": " + out;
  if (ropts.primary) out += " {extra_data}";
  if (ropts.use_perfdata) out += " | " + perfdata;

  result.stdout_text = out;
  return result;
}

bool delta_store::deltaize(const std::string &key, values_type &vals) { return deltaize_at(key, vals, std::time(nullptr)); }

bool delta_store::deltaize_at(const std::string &key, values_type &vals, const std::time_t now) {
  const std::lock_guard<std::mutex> guard(mutex_);
  const auto it = entries_.find(key);
  if (it == entries_.end() || it->second.seen == 0) {
    entry &e = entries_[key];
    e.values = vals;
    e.seen = now;
    for (value &v : vals) v = value::from_int(0);
    return false;
  }

  entry &e = it->second;
  const double elapsed = static_cast<double>(now - e.seen);
  const values_type previous = e.values;
  e.values = vals;
  e.seen = now;

  if (elapsed <= 0) {
    // Two polls inside the same second. Dividing by zero would report an
    // infinite rate, so report nothing changed instead.
    for (value &v : vals) v = value::from_int(0);
    return true;
  }
  for (std::size_t i = 0; i < vals.size(); ++i) {
    if (i >= previous.size() || vals[i].is_string || previous[i].is_string) continue;
    vals[i] = value::from_double(round2(std::fabs((vals[i].number - previous[i].number) / elapsed)));
  }
  return true;
}

void delta_store::clear() {
  const std::lock_guard<std::mutex> guard(mutex_);
  entries_.clear();
}

}  // namespace ncpa
