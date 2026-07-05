// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "collectd_packet.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <parsers/expression/expression.hpp>
#include <str/xtos.hpp>

std::list<collectd::collectd_builder::expanded_keys> collectd::collectd_builder::expand_keyword(const std::string &keyword, const std::string &value) {
  parsers::simple_expression::result_type expr;
  parsers::simple_expression::parse(keyword, expr);

  std::list<std::string> vars;
  for (const parsers::simple_expression::entry &e : expr) {
    if (e.is_variable) vars.push_back(e.name);
  }
  std::list<expanded_keys> ret;
  if (vars.empty()) ret.push_back(expanded_keys(keyword, value));
  for (const std::string &e : vars) {
    std::pair<variables_map::const_iterator, variables_map::const_iterator> keyRange = variables.equal_range(e);
    variables_map::const_iterator cit;
    if (keyRange.first == keyRange.second) {
      // NSC_LOG_ERROR_EX("missing variable: " + e);
    }

    for (cit = keyRange.first; cit != keyRange.second; ++cit) {
      std::string tmp = "${" + e + "}";
      ret.push_back(expanded_keys(boost::replace_all_copy(keyword, tmp, cit->second), boost::replace_all_copy(value, tmp, cit->second)));
    }
  }
  return ret;
}

void collectd::collectd_builder::add_variable(std::string key, std::string value) {
  boost::regex re(value);

  boost::smatch what;
  for (const metrics_map::value_type &e : metrics) {
    if (boost::regex_match(e.first, what, re, boost::match_extra)) {
      for (std::size_t i = 1; i < what.size(); ++i) {
        variables.insert(std::make_pair(key, what.str(static_cast<int>(i))));
      }
    }
  }
}

void collectd::collectd_builder::set_metric(const ::std::string &key, const std::string &value) { metrics[key] = value; }
