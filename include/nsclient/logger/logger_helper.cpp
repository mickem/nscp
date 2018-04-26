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

#include <nsclient/logger/logger_helper.hpp>

#include <str/format.hpp>
#include <str/utils.hpp>
#include <utf8.hpp>

#include <boost/date_time.hpp>

#include <iostream>

void nsclient::logging::logger_helper::log_fatal(std::string message) {

	std::cout << message << "\n";
	try {
		std::ofstream stream("nsclient.fatal", std::ios::out | std::ios::app | std::ios::ate);
		stream << message << "\n";
	} catch (...) {}
}

std::string nsclient::logging::logger_helper::render_log_level_short(::Plugin::LogEntry::Entry::Level l) {
	if (l == ::Plugin::LogEntry_Entry_Level_LOG_CRITICAL) {
		return "C";
	} else if (l == ::Plugin::LogEntry_Entry_Level_LOG_ERROR) {
		return "E";
	} else if (l == ::Plugin::LogEntry_Entry_Level_LOG_WARNING) {
		return "W";
	} else if (l == ::Plugin::LogEntry_Entry_Level_LOG_INFO) {
		return "L";
	} else if (l == ::Plugin::LogEntry_Entry_Level_LOG_DEBUG) {
		return "D";
	} else if (l == ::Plugin::LogEntry_Entry_Level_LOG_TRACE) {
		return "T";
	} else {
		return "?";
	}
}

std::string nsclient::logging::logger_helper::render_log_level_long(::Plugin::LogEntry::Entry::Level l) {
	if (l == ::Plugin::LogEntry_Entry_Level_LOG_CRITICAL) {
		return "critical";
	} else if (l == ::Plugin::LogEntry_Entry_Level_LOG_ERROR) {
		return "error";
	} else if (l == ::Plugin::LogEntry_Entry_Level_LOG_WARNING) {
		return "warning";
	} else if (l == ::Plugin::LogEntry_Entry_Level_LOG_INFO) {
		return "info";
	} else if (l == ::Plugin::LogEntry_Entry_Level_LOG_DEBUG) {
		return "debug";
	} else if (l == ::Plugin::LogEntry_Entry_Level_LOG_TRACE) {
		return "trace";
	} else {
		return "unknown";
	}
}

std::pair<bool, std::string> nsclient::logging::logger_helper::render_console_message(const bool oneline, const std::string &data) {
	std::stringstream ss;
	bool is_error = false;
	try {
		Plugin::LogEntry message;
		if (!message.ParseFromString(data)) {
			log_fatal("Failed to parse message: " + str::format::strip_ctrl_chars(data));
			return std::make_pair(true, "ERROR");
		}

		for (int i = 0; i < message.entry_size(); i++) {
			const ::Plugin::LogEntry::Entry &msg = message.entry(i);
			std::string tmp = msg.message();
			str::utils::replace(tmp, "\n", "\n    -    ");
			if (oneline) {
				ss << msg.file()
					<< "("
					<< msg.line()
					<< "): "
					<< render_log_level_long(msg.level())
					<< ": "
					<< tmp
					<< "\n";
			} else {
				if (i > 0)
					ss << " -- ";
				ss << str::format::lpad(render_log_level_short(msg.level()), 1)
					<< " " << str::format::rpad(msg.sender(), 10)
					<< " " + msg.message()
					<< "\n";
				if (msg.level() == ::Plugin::LogEntry_Entry_Level_LOG_ERROR) {
					ss << "                    "
						<< msg.file()
						<< ":"
						<< msg.line() << "\n";
				}
			}
		}
#ifdef WIN32
		return std::make_pair(is_error, utf8::to_encoding(ss.str(), ""));
#else
		return std::make_pair(is_error, ss.str());
#endif
	} catch (std::exception &e) {
		log_fatal("Failed to parse data from: " + str::format::strip_ctrl_chars(data) + ": " + e.what());
	} catch (...) {
		log_fatal("Failed to parse data from: " + str::format::strip_ctrl_chars(data));
	}
	return std::make_pair(true, "ERROR");
}

std::string nsclient::logging::logger_helper::get_formated_date(std::string format) {
	std::stringstream ss;
	boost::posix_time::time_facet *facet = new boost::posix_time::time_facet(format.c_str());
	ss.imbue(std::locale(std::cout.getloc(), facet));
	ss << boost::posix_time::second_clock::local_time();
	return ss.str();
}
