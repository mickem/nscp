// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Kubernetes resource quantities ("500m", "1.5Gi", "2e3", "128974848") parsed
// to a plain number of base units: cores for CPU, bytes for memory. Header
// only and dependency free so node capacity and, later, metrics-server usage
// share one parser.
//
// The grammar (from the API conventions): a signed decimal number, optionally
// with a decimal exponent (e3 / E3), optionally followed by a suffix: binary
// (Ki Mi Gi Ti Pi Ei), decimal (n u m k M G T P E), or nothing. An "E"
// followed by digits is an exponent, not exa.

#include <cmath>
#include <cstdlib>
#include <string>

namespace kube_checks {

// Parse `text` into `value` (base units). False on malformed input.
inline bool parse_quantity(const std::string &text, double &value) {
  std::string s;
  for (const char c : text) {
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '"') s.push_back(c);
  }
  if (s.empty()) return false;
  std::size_t i = 0;
  bool negative = false;
  if (s[i] == '+' || s[i] == '-') {
    negative = s[i] == '-';
    ++i;
  }
  const std::size_t number_start = i;
  bool digits = false, dot = false;
  for (; i < s.size(); ++i) {
    if (s[i] >= '0' && s[i] <= '9') {
      digits = true;
    } else if (s[i] == '.' && !dot) {
      dot = true;
    } else {
      break;
    }
  }
  if (!digits) return false;
  const std::string mantissa = s.substr(number_start, i - number_start);
  double result = std::strtod(mantissa.c_str(), nullptr);

  // A decimal exponent: e/E followed by an optionally signed run of digits.
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    std::size_t j = i + 1;
    if (j < s.size() && (s[j] == '+' || s[j] == '-')) ++j;
    const std::size_t exp_start = j;
    while (j < s.size() && s[j] >= '0' && s[j] <= '9') ++j;
    if (j > exp_start) {
      if (j != s.size()) return false;  // an exponent takes no suffix
      const std::string exp = s.substr(i + 1, j - i - 1);
      result *= std::pow(10.0, std::strtod(exp.c_str(), nullptr));
      value = negative ? -result : result;
      return true;
    }
    // else: a lone "E" is the exa suffix, handled below
  }

  const std::string suffix = s.substr(i);
  double scale = 1.0;
  if (suffix.empty()) {
    scale = 1.0;
  } else if (suffix == "n") {
    scale = 1e-9;
  } else if (suffix == "u") {
    scale = 1e-6;
  } else if (suffix == "m") {
    scale = 1e-3;
  } else if (suffix == "k") {
    scale = 1e3;
  } else if (suffix == "M") {
    scale = 1e6;
  } else if (suffix == "G") {
    scale = 1e9;
  } else if (suffix == "T") {
    scale = 1e12;
  } else if (suffix == "P") {
    scale = 1e15;
  } else if (suffix == "E") {
    scale = 1e18;
  } else if (suffix == "Ki") {
    scale = 1024.0;
  } else if (suffix == "Mi") {
    scale = 1024.0 * 1024.0;
  } else if (suffix == "Gi") {
    scale = 1024.0 * 1024.0 * 1024.0;
  } else if (suffix == "Ti") {
    scale = 1024.0 * 1024.0 * 1024.0 * 1024.0;
  } else if (suffix == "Pi") {
    scale = 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
  } else if (suffix == "Ei") {
    scale = 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
  } else {
    return false;
  }
  result *= scale;
  value = negative ? -result : result;
  return true;
}

// A CPU quantity in millicores ("500m" = 500, "2" = 2000, "0.5" = 500);
// -1 when absent or malformed.
inline long long quantity_to_millicores(const std::string &text) {
  double value = 0;
  if (text.empty() || !parse_quantity(text, value)) return -1;
  return static_cast<long long>(std::llround(value * 1000.0));
}

// A memory (or storage) quantity in bytes ("128Mi" = 134217728); -1 when
// absent or malformed.
inline long long quantity_to_bytes(const std::string &text) {
  double value = 0;
  if (text.empty() || !parse_quantity(text, value)) return -1;
  return static_cast<long long>(std::llround(value));
}

// A count quantity ("110" pods); -1 when absent or malformed.
inline long long quantity_to_count(const std::string &text) { return quantity_to_bytes(text); }

}  // namespace kube_checks
