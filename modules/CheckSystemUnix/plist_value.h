// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// A property list as plain C++ values. The macOS sources several checks read
// (installer receipts, application bundles, the software-update cache) are
// property lists, XML or binary; plist_darwin.cpp turns one into this through
// CoreFoundation, and everything that maps it onto a check row works on this
// type, so it is unit-tested on every platform.

#include <map>
#include <string>
#include <vector>

namespace plist {

struct value {
  enum kind_type { none, string, integer, real, boolean, date, array, dict };
  kind_type kind = none;
  std::string str;
  long long integer_value = 0;  // integer; also the epoch seconds of a date
  double real_value = 0.0;
  bool bool_value = false;
  std::vector<value> items;
  std::map<std::string, value> members;

  static value make_string(const std::string &s) {
    value v;
    v.kind = string;
    v.str = s;
    return v;
  }
  static value make_integer(const long long i) {
    value v;
    v.kind = integer;
    v.integer_value = i;
    return v;
  }
  static value make_date(const long long epoch) {
    value v;
    v.kind = date;
    v.integer_value = epoch;
    return v;
  }
  static value make_bool(const bool b) {
    value v;
    v.kind = boolean;
    v.bool_value = b;
    return v;
  }

  bool empty() const { return kind == none; }

  // A dictionary member, or an empty value when this is not a dictionary or
  // has no such key.
  const value &operator[](const std::string &key) const {
    static const value missing;
    if (kind != dict) return missing;
    const auto it = members.find(key);
    return it == members.end() ? missing : it->second;
  }

  // The string, or `def` for anything else.
  std::string as_string(const std::string &def = "") const { return kind == string ? str : def; }
  // Epoch seconds of a date, or 0.
  long long as_date() const { return kind == date ? integer_value : 0; }
  long long as_integer(const long long def = 0) const {
    if (kind == integer) return integer_value;
    if (kind == real) return static_cast<long long>(real_value);
    return def;
  }
};

// Read a property list file (XML or binary). An empty value when the file is
// missing, unreadable or not a property list. Defined per platform: macOS
// parses through CoreFoundation; elsewhere there are no property lists to
// read and it always returns an empty value.
value read_file(const std::string &path);

}  // namespace plist
