// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
// #define BOOST_SPIRIT_DEBUG  ///$$$ DEFINE THIS WHEN DEBUGGING $$$///

#include <string>
#include <vector>

namespace parsers {
struct perfconfig {
  struct perf_option {
    std::string key;
    std::string value;
  };
  struct perf_rule {
    std::string name;
    std::vector<perf_option> options;
  };
  typedef std::vector<perf_rule> result_type;
  static bool parse(const std::string& str, result_type& v);
};
}  // namespace parsers