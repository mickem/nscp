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
#include "stdafx.h"
#include "NSClientServer.h"

#include <boost/assign.hpp>

#include <strEx.h>
#include <time.h>
#include <config.h>
#include "handler_impl.hpp"
#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <socket/socket_settings_helper.hpp>

namespace sh = nscapi::settings_helper;

NSClientServer::NSClientServer() 
	: noPerfData_(false)
	, allowNasty_(false)
	, allowArgs_(false)
{
}
NSClientServer::~NSClientServer() {
}

bool NSClientServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias("NSClient", alias, "server");

	settings.alias().add_path_to_settings()
		("NSCLIENT SERVER SECTION", "Section for NSClient (NSClientServer.dll) (check_nt) protocol options.")
		;

	settings.alias().add_key_to_settings()

		("performance data", sh::bool_fun_key<bool>(boost::bind(&NSClientServer::set_perf_data, this, _1), true),
		"PERFORMANCE DATA", "Send performance data back to Nagios (set this to 0 to remove all performance data).")

		;

	socket_helpers::settings_helper::add_port_server_opts(settings, info_, "12489");
	socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, false);
	socket_helpers::settings_helper::add_core_server_opts(settings, info_);

	settings.alias().add_parent("/settings/default").add_key_to_settings()

		("password", sh::string_fun_key<std::string>(boost::bind(&NSClientServer::set_password, this, _1), ""),
		"PASSWORD", "Password used to authenticate against server")

		;

	settings.register_all();
	settings.notify();

#ifndef USE_SSL
	if (info_.use_ssl) {
		NSC_LOG_ERROR_STD(_T("SSL not avalible! (not compiled with openssl support)"));
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

bool NSClientServer::isPasswordOk(std::string remotePassword)  {
	std::string localPassword = get_password();
	if (localPassword == remotePassword) {
		return true;
	}
	if ((remotePassword == "None") && (localPassword.empty())) {
		return true;
	}
	return false;
}

void split_to_list(std::list<std::string> &list, std::string str) {
	std::list<std::string> add = strEx::s::splitEx(str, std::string("&"));
	list.insert(list.begin(), add.begin(), add.end());
}


std::string list_instance(std::string counter) {
	std::list<std::string> exeresult;
	nscapi::core_helper::exec_simple_command("*", "pdh", boost::assign::list_of(std::string("--list"))("--porcelain")("--counter")(counter)("--no-counters"), exeresult);
	std::string result;

	typedef std::basic_istringstream<char> wistringstream;
	typedef boost::tokenizer< boost::escaped_list_separator<char>, std::string::const_iterator, std::string > Tokenizer;
	BOOST_FOREACH(const std::string &s, exeresult) {
		std::istringstream iss(s);
		std::string line;
		while(std::getline(iss, line, '\n')) {
			Tokenizer tok(line);
			Tokenizer::const_iterator cit = tok.begin();
			int i = 1;
			while ((i-->0) && (cit != tok.end()))
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

	strEx::s::token pwd = strEx::s::getToken(buffer, '&');
	if (!isPasswordOk(pwd.first)) {
		return check_nt::packet("ERROR: Invalid password.");
	}
	if (pwd.second.empty())
		return check_nt::packet("ERROR: No command specified.");
	strEx::s::token cmd = strEx::s::getToken(pwd.second, '&');
	if (cmd.first.empty())
		return check_nt::packet("ERROR: No command specified.");

	int c = boost::lexical_cast<int>(cmd.first.c_str());

	std::list<std::string> args;

	// prefix various commands
	switch (c) {
		case REQ_CPULOAD:
			cmd.first = "checkCPU";
			split_to_list(args, cmd.second);
			args.push_back("nsclient");
			break;
		case REQ_UPTIME:
			cmd.first = "checkUpTime";
			args.push_back("nsclient");
			break;
		case REQ_USEDDISKSPACE:
			cmd.first = "CheckDriveSize";
			split_to_list(args, cmd.second);
			args.push_back("nsclient");
			break;
		case REQ_CLIENTVERSION:
			return nscapi::plugin_singleton->get_core()->getApplicationName() + " " + nscapi::plugin_singleton->get_core()->getApplicationVersionString();
		case REQ_SERVICESTATE:
			cmd.first = "checkServiceState";
			split_to_list(args, cmd.second);
			args.push_back("nsclient");
			break;
		case REQ_PROCSTATE:
			cmd.first = "checkProcState";
			split_to_list(args, cmd.second);
			args.push_back("nsclient");
			break;
		case REQ_MEMUSE:
			cmd.first = "checkMem";
			args.push_back("nsclient");
			break;
		case REQ_COUNTER:
			cmd.first = "checkCounter";
			args.push_back("Counter=" + cmd.second);
			args.push_back("nsclient");
			break;
		case REQ_FILEAGE:
			cmd.first = "getFileAge";
			args.push_back("path=" + cmd.second);
			break;
		case REQ_INSTANCES:
			return list_instance(cmd.second);


		default:
			split_to_list(args, cmd.second);
	}

	std::string message, perf;
	NSCAPI::nagiosReturn ret = nscapi::core_helper::simple_query(cmd.first, args, message, perf);
	if (!nscapi::plugin_helper::isNagiosReturnCode(ret)) {
		if (message.empty())
			return check_nt::packet("ERROR: Could not complete the request check log file for more information.");
		return check_nt::packet("ERROR: " + message);
	}
	switch (c) {
		case REQ_UPTIME:		// Some check_nt commands has no return code syntax
		case REQ_MEMUSE:
		case REQ_CPULOAD:
		case REQ_CLIENTVERSION:
		case REQ_USEDDISKSPACE:
		case REQ_COUNTER:
		case REQ_FILEAGE:
			return check_nt::packet(message);

		case REQ_SERVICESTATE:	// Some check_nt commands return the return code (coded as a string)
		case REQ_PROCSTATE:
			return check_nt::packet(strEx::s::xtos(nscapi::plugin_helper::nagios2int(ret)) + "& " + message);

		default:				// "New" check_nscp also returns performance data
			if (perf.empty())
				return check_nt::packet(nscapi::plugin_helper::translateReturn(ret) + "&" + message);
			return check_nt::packet(nscapi::plugin_helper::translateReturn(ret) + "&" + message + "&" + perf);
	}

	return check_nt::packet("FOO");
}
