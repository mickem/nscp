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

#include <nsclient/logger/log_level.hpp>

#include <boost/algorithm/string.hpp>


bool nsclient::logging::log_level::set(const std::string level) {
	std::string lc = boost::to_lower_copy(level);
	if (lc == "critical" || lc == "crit" || lc == "c") {
		current_level_ = critical;
		return true;
	} else if (lc == "error" || lc == "err" || lc == "e") {
		current_level_ = error;
		return true;
	} else if (lc == "warning" || lc == "warn" || lc == "w") {
		current_level_ = warning;
		return true;
	} else if (lc == "info" || lc == "log" || lc == "i") {
		current_level_ = info;
		return true;
	} else if (lc == "debug" || lc == "d") {
		current_level_ = debug;
		return true;
	} else if (lc == "trace" || lc == "t") {
		current_level_ = trace;
		return true;
	}
	return false;
}
std::string nsclient::logging::log_level::get() const {
	if (current_level_ == critical)
		return "critical";
	if (current_level_ == error)
		return "error";
	if (current_level_ == warning)
		return "warning";
	if (current_level_ == info)
		return "message";
	if (current_level_ == debug)
		return "debug";
	if (current_level_ == trace)
		return "trace";
	return "unknown";
}
