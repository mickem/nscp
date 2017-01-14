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

#include <nsclient/logger/log_message_factory.hpp>

#include <nscapi/nscapi_protobuf.hpp>

#include <iostream>


void nsclient::logging::log_message_factory::log_fatal(std::string message) {
	std::cout << message << "\n";
}

std::string create_message(const std::string &module, ::Plugin::LogEntry::Entry::Level level, const char* file, const int line, const std::string &logMessage) {
	std::string str;
	try {
		::Plugin::LogEntry message;
		::Plugin::LogEntry::Entry *msg = message.add_entry();
		msg->set_sender(module);
		msg->set_level(level);
		msg->set_file(file);
		msg->set_line(line);
		msg->set_message(logMessage);
		return message.SerializeAsString();
	} catch (std::exception &e) {
		nsclient::logging::log_message_factory::log_fatal(std::string("Failed to generate message: ") + e.what());
	} catch (...) {
		nsclient::logging::log_message_factory::log_fatal("Failed to generate message: <UNKNOWN>");
	}
	return str;
}
std::string nsclient::logging::log_message_factory::create_critical(const std::string &module, const char* file, const int line, const std::string &message) {
	return create_message(module, ::Plugin::LogEntry_Entry_Level_LOG_CRITICAL, file, line, message);
}
std::string nsclient::logging::log_message_factory::create_error(const std::string &module, const char* file, const int line, const std::string &message) {
	return create_message(module, ::Plugin::LogEntry_Entry_Level_LOG_ERROR, file, line, message);
}
std::string nsclient::logging::log_message_factory::create_warning(const std::string &module, const char* file, const int line, const std::string &message) {
	return create_message(module, ::Plugin::LogEntry_Entry_Level_LOG_WARNING, file, line, message);
}
std::string nsclient::logging::log_message_factory::create_info(const std::string &module, const char* file, const int line, const std::string &message) {
	return create_message(module, ::Plugin::LogEntry_Entry_Level_LOG_INFO, file, line, message);
}
std::string nsclient::logging::log_message_factory::create_debug(const std::string &module, const char* file, const int line, const std::string &message) {
	return create_message(module, ::Plugin::LogEntry_Entry_Level_LOG_DEBUG, file, line, message);
}
std::string nsclient::logging::log_message_factory::create_trace(const std::string &module, const char* file, const int line, const std::string &message) {
	return create_message(module, ::Plugin::LogEntry_Entry_Level_LOG_TRACE, file, line, message);
}
