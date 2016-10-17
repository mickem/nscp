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