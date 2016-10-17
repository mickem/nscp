/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <strEx.h>
#include "collectd_packet.hpp"

#include <parsers/expression/expression.hpp>

#include <boost/regex.hpp>

//unsigned int collectd::length::payload_length_ = 512;


std::list<collectd::collectd_builder::expanded_keys> collectd::collectd_builder::expand_keyword(const std::string &keyword, const std::string &value) {
	parsers::simple_expression::result_type expr;
	parsers::simple_expression::parse(keyword, expr);

	std::list<std::string> vars;
	BOOST_FOREACH(const parsers::simple_expression::entry &e, expr) {
		if (e.is_variable)
			vars.push_back(e.name);
	}
	std::list<expanded_keys> ret;
	if (vars.empty())
		ret.push_back(expanded_keys(keyword, value));
	BOOST_FOREACH(const std::string &e, vars) {
		std::pair<variables_map::const_iterator, variables_map::const_iterator> keyRange = variables.equal_range(e);
		variables_map::const_iterator cit;
		if (keyRange.first == keyRange.second) {
			//NSC_LOG_ERROR_EX("missing variable: " + e);
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
	BOOST_FOREACH(const metrics_map::value_type &e, metrics) {
		if (boost::regex_match(e.first, what, re, boost::match_extra)) {
			for (std::size_t i = 1; i < what.size(); ++i) {
				variables.insert(std::make_pair(key, what.str(i)));
			}
		}
	}

}


void collectd::collectd_builder::set_metric(const ::std::string& key, const std::string &value) {
	metrics[key] = value;
}
