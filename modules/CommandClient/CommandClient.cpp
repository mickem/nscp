/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "CommandClient.h"
#include <iostream>

#include <boost/make_shared.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

namespace sh = nscapi::settings_helper;

using namespace std;

void client_handler::output_message(const std::string &msg) {
	if (msg.find("\n") == std::string::npos) {
		NSC_LOG_MESSAGE(msg);
	} else {
		NSC_LOG_MESSAGE("Long message\n" + msg);
	}
}

void client_handler::log_debug(std::string module, std::string file, int line, std::string msg) const {
	if (get_core()->should_log(NSCAPI::log_level::debug)) {
		get_core()->log(NSCAPI::log_level::debug, file, line, msg);
	}
}

void client_handler::log_error(std::string module, std::string file, int line, std::string msg) const {
	if (get_core()->should_log(NSCAPI::log_level::debug)) {
		get_core()->log(NSCAPI::log_level::error, file, line, msg);
	}
}

bool CommandClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	client::cli_handler_ptr handler(new client_handler(get_core(), get_id()));
	client.reset(new client::cli_client(handler));
	return true;
}

bool CommandClient::unloadModule() {
	return true;
}

void CommandClient::handleLogMessage(const Plugin::LogEntry::Entry &message) {
	/*
	using namespace boost::posix_time;
	using namespace boost::gregorian;

	error_handler::log_entry entry;
	entry.line = message.line();
	entry.file = message.file();
	entry.message = message.message();
	entry.date = to_simple_string(second_clock::local_time());

	switch (message.level()) {
	case Plugin::LogEntry_Entry_Level_LOG_CRITICAL:
		entry.type = "critical";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_DEBUG:
		entry.type = "debug";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_ERROR:
		entry.type = "error";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_INFO:
		entry.type = "info";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_WARNING:
		entry.type = "warning";
		break;
	default:
		entry.type = "unknown";
	}
	log_data.add_message(message.level() == Plugin::LogEntry_Entry_Level_LOG_CRITICAL || message.level() == Plugin::LogEntry_Entry_Level_LOG_ERROR, entry);
	*/
}
bool is_running = false;

#ifdef WIN32
BOOL WINAPI consoleHandler(DWORD signal) {

	if (signal == CTRL_C_EVENT) {
		is_running = false;
		NSC_LOG_MESSAGE("Ctrl+c is not exit...");
	}
	return TRUE;
}
#endif


bool CommandClient::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {

#ifdef WIN32
	if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
		NSC_LOG_MESSAGE("Could not set control handler");
	}
#endif
	// 	if (core_->get_service_control().is_started())
	// 		info(__LINE__, "Service seems to be started (Sockets and such will probably not work)...");

	NSC_DEBUG_MSG("Enter command to execute, help for help or exit to exit...");
	is_running = true;
	while (is_running) {
		std::string s;
		std::getline(std::cin, s);
		if (s == "exit") {
			nscapi::protobuf::functions::set_response_good(*response, "Done");
			return true;
		}
		client->handle_command(s);
	}
	nscapi::protobuf::functions::set_response_good(*response, "Done");
}