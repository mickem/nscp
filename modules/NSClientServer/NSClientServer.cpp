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

#include "NSClientServer.h"
#include "handler_impl.hpp"

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <socket/socket_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_common_options.hpp>

#include <str/utils.hpp>
#include <time.h>

#include <boost/assign.hpp>

namespace sh = nscapi::settings_helper;

NSClientServer::NSClientServer()
	: noPerfData_(false)
	, allowNasty_(false)
	, allowArgs_(false) {}
NSClientServer::~NSClientServer() {}

bool NSClientServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias("NSClient", alias, "server");

	settings.alias().add_path_to_settings()
		("NSCLIENT SERVER SECTION", "Section for NSClient (NSClientServer.dll) (check_nt) protocol options.")
		;

	settings.alias().add_key_to_settings()

		("performance data", sh::bool_fun_key(boost::bind(&NSClientServer::set_perf_data, this, _1), true),
			"PERFORMANCE DATA", "Send performance data back to Nagios (set this to 0 to remove all performance data).")

		;

	socket_helpers::settings_helper::add_port_server_opts(settings, info_, "12489");
	socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, false);
	socket_helpers::settings_helper::add_core_server_opts(settings, info_);

	settings.alias().add_parent("/settings/default").add_key_to_settings()

		("password", sh::string_fun_key(boost::bind(&NSClientServer::set_password, this, _1), ""),
			DEFAULT_PASSWORD_NAME, DEFAULT_PASSWORD_DESC)

		;

	settings.register_all();
	settings.notify();

#ifndef USE_SSL
	if (info_.use_ssl) {
		NSC_LOG_ERROR_STD(_T("SSL not available! (not compiled with openssl support)"));
	}
#endif
	NSC_LOG_ERROR_LISTS(info_.validate());

	std::list<std::string> errors;
	info_.allowed_hosts.refresh(errors);
	BOOST_FOREACH(const std::string &e, errors) {
		NSC_LOG_ERROR_STD(e);
	}
	NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());

	boost::asio::io_service io_service_;

	if (mode == NSCAPI::normalStart) {
		try {
#ifndef USE_SSL
			if (info_.use_ssl) {
				NSC_LOG_ERROR_STD(_T("SSL is not supported (not compiled with openssl)"));
				return false;
			}
#endif
			server_.reset(new check_nt::server::server(info_, this));
			if (!server_) {
				NSC_LOG_ERROR_STD("Failed to create server instance!");
				return false;
			}
			server_->start();
		} catch (std::exception &e) {
			NSC_LOG_ERROR_EXR("start", e);
			return false;
		} catch (...) {
			NSC_LOG_ERROR_EX("start");
			return false;
		}
	}
	return true;
}
bool NSClientServer::unloadModule() {
	try {
		if (server_) {
			server_->stop();
			server_.reset();
		}
	} catch (...) {
		NSC_LOG_ERROR_EX("unload");
		return false;
	}
	return true;
}

#define REQ_CLIENTVERSION	1	// Works fine!
#define REQ_CPULOAD			2	// Quirks
#define REQ_UPTIME			3	// Works fine!
#define REQ_USEDDISKSPACE	4	// Works fine!
#define REQ_SERVICESTATE	5	// Works fine!
#define REQ_PROCSTATE		6	// Works fine!
#define REQ_MEMUSE			7	// Works fine!
#define REQ_COUNTER			8	// Works fine!
#define REQ_FILEAGE			9	// Works fine! (i hope)
#define REQ_INSTANCES		10	// Works fine! (i hope)

bool NSClientServer::isPasswordOk(std::string remotePassword) {
	std::string localPassword = get_password();
	if (localPassword == remotePassword) {
		return true;
	}
	if ((remotePassword == "None") && (localPassword.empty())) {
		return true;
	}
	return false;
}

void split_to_list(std::list<std::string> &list, const std::string str, const std::string key) {
	BOOST_FOREACH(const std::string &s, str::utils::split_lst(str, std::string("&"))) {
		list.push_back(key + "=" + s);
	}
}

void log_bad_command(const std::string &cmd) {
	if (cmd == "check_cpu" || cmd == "check_uptime" || cmd == "check_memory") {
		NSC_LOG_ERROR(cmd + std::string(" failed to execute have you loaded CheckSystem? (CheckSystem=enabled under modules)"));
	} else {
		NSC_LOG_ERROR("Unknown command: " + cmd);
	}
}

inline std::string extract_perf_value(const ::Plugin::Common_PerformanceData &perf) {
	return nscapi::protobuf::functions::extract_perf_value_as_string(perf);
}
inline std::string extract_perf_total(const ::Plugin::Common_PerformanceData &perf) {
	return nscapi::protobuf::functions::extract_perf_maximum_as_string(perf);
}
inline long long extract_perf_value_i(const ::Plugin::Common_PerformanceData &perf) {
	return nscapi::protobuf::functions::extract_perf_value_as_int(perf);
}

std::string NSClientServer::list_instance(std::string counter) {
	std::list<std::string> exeresult;
	nscapi::core_helper ch(get_core(), get_id());
	ch.exec_simple_command("CheckSystem", "pdh", boost::assign::list_of(std::string("--list"))("--porcelain")("--counter")(counter)("--no-counters"), exeresult);
	std::string result;

	typedef boost::tokenizer< boost::escaped_list_separator<char>, std::string::const_iterator, std::string > Tokenizer;
	BOOST_FOREACH(const std::string &s, exeresult) {
		std::istringstream iss(s);
		std::string line;
		while (std::getline(iss, line, '\n')) {
			Tokenizer tok(line);
			Tokenizer::const_iterator cit = tok.begin();
			int i = 2;
			while ((i-- > 0) && (cit != tok.end()))
				++cit;
			if (i <= 1) {
				if (!result.empty())
					result += ",";
				result += *cit;
			} else {
				NSC_LOG_ERROR("Invalid line: " + line);
			}
		}
	}
	return result;
}

check_nt::packet NSClientServer::handle(check_nt::packet p) {
	std::string buffer = p.get_payload();

	std::string::size_type pos = buffer.find_first_of("\n\r");
	if (pos != std::string::npos) {
		std::string::size_type pos2 = buffer.find_first_not_of("\n\r", pos);
		if (pos2 != std::string::npos) {
			std::string rest = buffer.substr(pos2);
		}
		buffer = buffer.substr(0, pos);
	}

	str::utils::token pwd = str::utils::getToken(buffer, '&');
	if (!isPasswordOk(pwd.first)) {
		return check_nt::packet("ERROR: Invalid password.");
	}
	if (pwd.second.empty())
		return check_nt::packet("ERROR: No command specified.");
	str::utils::token cmd = str::utils::getToken(pwd.second, '&');
	if (cmd.first.empty())
		return check_nt::packet("ERROR: No command specified.");

	int c = boost::lexical_cast<int>(cmd.first.c_str());

	std::list<std::string> args;

	// prefix various commands
	switch (c) {
	case REQ_CPULOAD:
		cmd.first = "check_cpu";
		BOOST_FOREACH(const std::string &s, str::utils::split_lst(cmd.second, std::string("&"))) {
			args.push_back("time=" + s + "m");
		}
		break;
	case REQ_UPTIME:
		cmd.first = "check_uptime";
		args.push_back("warn=uptime<0");
		break;
	case REQ_USEDDISKSPACE:
		cmd.first = "check_drivesize";
		split_to_list(args, cmd.second, "drive");
		args.push_back("warn=free<0");
		args.push_back("crit=free<0");
		args.push_back("filter=type='fixed' and mounted = 1");
		args.push_back("perf-config=used(unit:B)free(unit:B)");
		break;
	case REQ_CLIENTVERSION:
		return check_nt::packet(get_core()->getApplicationName() + " " + get_core()->getApplicationVersionString());
	case REQ_SERVICESTATE:
		cmd.first = "check_service";
		split_to_list(args, cmd.second, "service");
		if (args.size() > 0 && *args.begin() == "service=ShowFail")
			args.erase(args.begin());
		if (args.size() > 0 && *args.begin() == "service=ShowAll") {
			args.erase(args.begin());
			args.push_back("top-syntax=${list}");
		}
		args.push_back("detail-syntax=${name}: ${legacy_state}");
		args.push_back("empty-syntax=OK: All services are in their appropriate state.");
		args.push_back("filter=none");
		args.push_back("crit=not state = 'running'");
		break;
	case REQ_PROCSTATE:
		cmd.first = "check_process";
		split_to_list(args, cmd.second, "process");
		if (args.size() > 0 && *args.begin() == "process=ShowFail")
			args.erase(args.begin());
		if (args.size() > 0 && *args.begin() == "process=ShowAll") {
			args.erase(args.begin());
			args.push_back("top-syntax=${list}");
		}
		args.push_back("detail-syntax=${exe}: ${legacy_state}");
		args.push_back("empty-syntax=OK: All processes are running.");
		break;
	case REQ_MEMUSE:
		cmd.first = "check_memory";
		args.push_back("warn=used<0");
		args.push_back("crit=used<0");
		args.push_back("filter=none");
		args.push_back("type=committed");
		args.push_back("perf-config=used(unit:B)free(unit:B)");
		break;
	case REQ_COUNTER:
		cmd.first = "check_pdh";
		args.push_back("counter=" + cmd.second);
		break;
	case REQ_FILEAGE:
		cmd.first = "check_files";
		args.push_back("path=" + cmd.second);
		args.push_back("crit=age<0");
		args.push_back("detail-syntax=${file} ${written}");
		args.push_back("top-syntax=${list}");
		break;
	case REQ_INSTANCES:
		return check_nt::packet(list_instance(cmd.second));
	default:
		return check_nt::packet("ERROR: Unknown command.");
	}

	std::string response;
	nscapi::core_helper ch(get_core(), get_id());
	NSC_DEBUG_MSG("Real command: " + cmd.first + " " + str::utils::joinEx(args, " "));
	if (!ch.simple_query(cmd.first, args, response)) {
		log_bad_command(cmd.first);
		return check_nt::packet("ERROR: Could not complete the request check log file for more information.");
	}

	::Plugin::QueryResponseMessage message;
	if (!message.ParseFromString(response))
		return check_nt::packet("ERROR: Failed to parse data from: " + cmd.first);
	if (message.payload_size() != 1)
		return check_nt::packet("ERROR: Command returned invalid number of payloads: " + cmd.first + ", " + str::xtos(message.payload_size()));
	const ::Plugin::QueryResponseMessage::Response &payload = message.payload(0);
	if (payload.lines_size() != 1) {
		return check_nt::packet("ERROR: Invalid number of lines returned from command: " + cmd.first + ", " + str::xtos(payload.lines_size()));
	}
	const ::Plugin::QueryResponseMessage::Response::Line &line = payload.lines(0);

	switch (c) {
	case REQ_CPULOAD:		// Return the first performance data value
	case REQ_UPTIME:
	case REQ_COUNTER:
		if (line.perf_size() < 1)
			return check_nt::packet("ERROR: No performance data from command: " + cmd.first);
		return check_nt::packet(extract_perf_value(line.perf(0)));

	case REQ_MEMUSE:
		if (line.perf_size() < 1)
			return check_nt::packet("ERROR: No performance data from command: " + cmd.first);
		return check_nt::packet(extract_perf_total(line.perf(0)) + "&" + extract_perf_value(line.perf(0)));
	case REQ_USEDDISKSPACE:
		if (line.perf_size() < 1)
			return check_nt::packet("ERROR: No performance data from command: " + cmd.first);
		return check_nt::packet(extract_perf_value(line.perf(0)) + "&" + extract_perf_total(line.perf(0)));
	case REQ_FILEAGE:
		if (line.perf_size() < 1)
			return check_nt::packet("ERROR: No performance data from command: " + cmd.first);
		return check_nt::packet(str::xtos_non_sci(extract_perf_value_i(line.perf(0)) / 60) + "&" + line.message());

	case REQ_SERVICESTATE:	// Some check_nt commands return the return code (coded as a string)
	case REQ_PROCSTATE:
		return check_nt::packet(str::xtos(payload.result()) + "& " + line.message());
	}

	return check_nt::packet("ERROR: Unknown command " + cmd.first);
}