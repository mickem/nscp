/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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