// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <vector>

namespace parsers {
struct simple_expression {
  struct entry {
    bool is_variable;
    std::string name;
    entry() : is_variable(false) {}
    entry(bool is_var, std::string n) : is_variable(is_var), name(std::move(n)) {}
    entry(bool is_var, std::vector<char> n) : is_variable(is_var), name(n.begin(), n.end()) {}
    entry(const entry &other) = default;
    entry(entry &&other) = default;
    entry &operator=(const entry &other) = default;
    entry &operator=(entry &&other) = default;
  };
  using result_type = std::vector<entry>;
  static bool parse(const std::string &str, result_type &v);
};
}  // namespace parsers