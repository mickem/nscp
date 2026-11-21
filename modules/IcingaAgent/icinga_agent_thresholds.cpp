// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_thresholds.hpp"

#include <cstdlib>

namespace icinga_agent {

namespace {

std::string trim(const std::string &s) {
  const std::size_t b = s.find_first_not_of(" \t");
  if (b == std::string::npos) return "";
  return s.substr(b, s.find_last_not_of(" \t") - b + 1);
}

// The plugins read a number with lexical_cast<double>: the whole text, or it
// is refused.
std::string number(const std::string &text, const std::string &whole) {
  const std::string s = trim(text);
  char *end = nullptr;
  if (!s.empty()) std::strtod(s.c_str(), &end);
  if (s.empty() || end == nullptr || *end != '\0' || s.find_first_not_of("0123456789.-+") != std::string::npos) {
    throw threshold_error("'" + whole + "' is not a threshold of the Icinga Windows plugins");
  }
  return s;
}

bool strip_percent(std::string &s) {
  if (s.empty() || s.back() != '%') return false;
  s.pop_back();
  return true;
}

std::string value(const std::string &n, const plugin_threshold &t, const std::string &suffix, const bool keep_percent) {
  if (t.percent) return keep_percent ? n + "%" : n;
  return n + suffix;
}

// Whether the alert is on the high side (a single value) or on the inside (a
// range): what the plain form means, flipped once by `!` and once more by a
// plugin that watches free space.
bool plain_reading(const plugin_threshold &t, const bool inverted) { return t.negated == inverted; }

}  // namespace

plugin_threshold parse_plugin_threshold(const std::string &text) {
  plugin_threshold t;
  std::string s = trim(text);
  if (s.empty()) throw threshold_error("An empty threshold");
  if (s.front() == '!') {
    t.negated = true;
    s = s.substr(1);
  }
  if (s.size() >= 2 && s.front() == '[' && s.back() == ']') {
    t.is_range = true;
    s = s.substr(1, s.size() - 2);
    // The plugins split on every '-', so a range holds no negative number.
    const std::size_t dash = s.find('-');
    if (dash == std::string::npos || s.find('-', dash + 1) != std::string::npos) {
      throw threshold_error("'" + text + "': a threshold range is [lower-upper]");
    }
    std::string lower = trim(s.substr(0, dash));
    std::string upper = trim(s.substr(dash + 1));
    // A percentage only when both ends are; with one, the plugin fails to
    // read the other as a number.
    const bool lower_percent = strip_percent(lower);
    const bool upper_percent = strip_percent(upper);
    if (lower_percent != upper_percent) throw threshold_error("'" + text + "': both ends of a range are percentages, or neither");
    t.percent = lower_percent;
    t.lower = number(lower, text);
    t.upper = number(upper, text);
    // Equal ends are a single value to the plugin (it tests lower == upper).
    if (std::strtod(t.lower.c_str(), nullptr) == std::strtod(t.upper.c_str(), nullptr)) t.is_range = false;
    return t;
  }
  t.percent = strip_percent(s);
  t.lower = t.upper = number(s, text);
  return t;
}

std::string to_range(const plugin_threshold &t, const bool inverted, const std::string &suffix, const bool keep_percent) {
  const std::string upper = value(t.upper, t, suffix, keep_percent);
  if (!t.is_range) return (plain_reading(t, inverted) ? "~:" : "@~:") + upper;
  const std::string lower = value(t.lower, t, suffix, keep_percent);
  return (plain_reading(t, inverted) ? "@" : "") + lower + ":" + upper;
}

std::string to_expression(const plugin_threshold &t, const bool inverted, const std::string &keyword, const std::string &suffix, const bool keep_percent) {
  const std::string upper = value(t.upper, t, suffix, keep_percent);
  if (!t.is_range) return keyword + (plain_reading(t, inverted) ? " > " : " <= ") + upper;
  const std::string lower = value(t.lower, t, suffix, keep_percent);
  if (plain_reading(t, inverted)) return keyword + " >= " + lower + " and " + keyword + " <= " + upper;
  return keyword + " < " + lower + " or " + keyword + " > " + upper;
}

}  // namespace icinga_agent
