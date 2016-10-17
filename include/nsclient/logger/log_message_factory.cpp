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
