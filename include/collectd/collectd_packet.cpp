/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include "collectd_packet.hpp"

#include <parsers/expression/expression.hpp>

#include <str/xtos.hpp>

#include <boost/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>


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
