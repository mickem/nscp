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

#pragma once

#include <string>
#include <boost/program_options.hpp>
#include <nscapi/nscapi_protobuf.hpp>

namespace compat {
	void log_args(const Plugin::QueryRequestMessage::Request &request);
	void addAllNumeric(boost::program_options::options_description &desc, const std::string suffix = "");
	void addOldNumeric(boost::program_options::options_description &desc);
	void addShowAll(boost::program_options::options_description &desc);
	void do_matchFirstNumeric(const boost::program_options::variables_map &vm, const std::string key, std::string &target, const std::string var, const std::string bound, const std::string op);
	void matchFirstNumeric(const boost::program_options::variables_map &vm, const std::string upper, const std::string lower, std::string &warn, std::string &crit, const std::string suffix = "");
	void matchFirstOldNumeric(const boost::program_options::variables_map &vm, const std::string var, std::string &warn, std::string &crit);
	void matchShowAll(const boost::program_options::variables_map &vm, Plugin::QueryRequestMessage::Request &request, std::string prefix = "${status}: ");
	bool hasFirstNumeric(const boost::program_options::variables_map &vm, const std::string suffix);

	inline void inline_addarg(Plugin::QueryRequestMessage::Request &request, const std::string &str) {
		if (!str.empty())
			request.add_arguments(str);
	}
	inline void inline_addarg(Plugin::QueryRequestMessage::Request &request, const std::string &prefix, const std::string &str) {
		if (!str.empty())
			request.add_arguments(prefix + str);
	}
}