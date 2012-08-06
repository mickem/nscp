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

namespace sh = nscapi::settings_helper;

NSClientServer::NSClientServer() 
	: noPerfData_(false)
	, allowNasty_(false)
	, allowArgs_(false)
{
}
NSClientServer::~NSClientServer() {
}

bool NSClientServer::loadModule() {
	return false;
}

bool NSClientServer::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NSClient"), alias, _T("server"));

		settings.alias().add_path_to_settings()
			(_T("NSCLIENT SERVER SECTION"), _T("Section for NSClient (NSClientServer.dll) (check_nt) protocol options."))
			;

		settings.alias().add_key_to_settings()
			(_T("port"), sh::uint_key(&info_.port, 12489),
			_T("PORT NUMBER"), _T("Port to use for check_nt."))

			(_T("performance data"), sh::bool_fun_key<bool>(boost::bind(&NSClientServer::set_perf_data, this, _1), true),
			_T("PERFORMANCE DATA"), _T("Send performance data back to nagios (set this to 0 to remove all performance data)."))

			;


		settings.alias().add_key_to_settings()
			(_T("thread pool"), sh::uint_key(&info_.thread_pool_size, 10),
			_T("THREAD POOL"), _T(""), true)

			(_T("socket queue size"), sh::int_key(&info_.back_log, 0),
			_T("LISTEN QUEUE"), _T("Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts."), true)

			(_T("use ssl"), sh::bool_key(&info_.ssl.enabled, false),
			_T("ENABLE SSL ENCRYPTION"), _T("This option controls if SSL should be enabled."), true)

			(_T("certificate"), sh::path_key(&info_.ssl.dh_key, "${certificate-path}/nrpe_dh_512.pem"),
			_T("DH KEY"), _T(""), true)

			(_T("certificate"), sh::path_key(&info_.ssl.certificate, "${certificate-path}/certificate.pem"),
			_T("SSL CERTIFICATE"), _T(""), true)

			(_T("certificate key"), sh::path_key(&info_.ssl.certificate_key, "${certificate-path}/certificate_key.pem"),
			_T("SSL CERTIFICATE"), _T(""), true)

			(_T("certificate format"), sh::string_key(&info_.ssl.certificate_format, "PEM"),
			_T("CERTIFICATE FORMAT"), _T(""), true)

			(_T("ca"), sh::path_key(&info_.ssl.ca_path, "${certificate-path}/ca.pem"),
			_T("CA"), _T(""), true)

			(_T("allowed ciphers"), sh::string_key(&info_.ssl.allowed_ciphers, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"),
			_T("ALLOWED CIPHERS"), _T(""), true)

			(_T("verify mode"), sh::string_key(&info_.ssl.verify_mode, "none"),
			_T("VERIFY MODE"), _T(""), true)

			;



		settings.alias().add_parent(_T("/settings/default")).add_key_to_settings()

			(_T("bind to"), sh::string_key(&info_.address),
			_T("BIND TO ADDRESS"), _T("Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses."))

			(_T("allowed hosts"), sh::string_fun_key<std::wstring>(boost::bind(&socket_helpers::allowed_hosts_manager::set_source, &info_.allowed_hosts, _1), _T("127.0.0.1")),
			_T("ALLOWED HOSTS"), _T("A comaseparated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges."))

			(_T("cache allowed hosts"), sh::bool_key(&info_.allowed_hosts.cached, true),
			_T("CACHE ALLOWED HOSTS"), _T("If hostnames should be cached, improves speed and security somewhat but wont allow you to have dynamic IPs for your nagios server."))

			(_T("timeout"), sh::uint_key(&info_.timeout, 30),
			_T("TIMEOUT"), _T("Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out."))

			(_T("password"), sh::string_fun_key<std::wstring>(boost::bind(&NSClientServer::set_password, this, _1), _T("")),
			_T("PASSWORD"), _T("Password used to authenticate againast server"))

			;

		settings.register_all();
		settings.notify();
	} catch (...) {}

#ifndef USE_SSL
	if (info_.use_ssl) {
		NSC_LOG_ERROR_STD(_T("SSL not avalible! (not compiled with openssl support)"));
	}
#endif
	NSC_LOG_ERROR_LISTW(info_.validate());

	std::list<std::string> errors;
	info_.allowed_hosts.refresh(errors);
	BOOST_FOREACH(const std::string &e, errors) {
		NSC_LOG_ERROR_STD(utf8::cvt<std::wstring>(e));
	}
	NSC_DEBUG_MSG_STD(_T("Allowed hosts definition: ") + info_.allowed_hosts.to_wstring());

	boost::asio::io_service io_service_;

	if (mode == NSCAPI::normalStart) {
		try {
#ifndef USE_SSL
					if (info_.use_ssl) {
						NSC_LOG_ERROR_STD(_T("SSL is not supported (not compiled with openssl)"));
						return false;
					}
#endif
					server_.reset(new check_nt::server::server(boost::shared_ptr<check_nt::read_protocol>(new check_nt::read_protocol(info_, this))));
					if (!server_) {
						NSC_LOG_ERROR_STD(_T("Failed to create server instance!"));
						return false;
					}
					server_->start();
		} catch (std::exception &e) {
			NSC_LOG_ERROR_STD(_T("Exception caught: ") + to_wstring(e.what()));
			return false;
		} catch (...) {
			NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
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
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN>"));
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

bool NSClientServer::isPasswordOk(std::wstring remotePassword)  {
	std::wstring localPassword = get_password();
	if (localPassword == remotePassword) {
		return true;
	}
	if ((remotePassword == _T("None")) && (localPassword.empty())) {
		return true;
	}
	return false;
}

void split_to_list(std::list<std::wstring> &list, std::wstring str) {
	strEx::splitList add = strEx::splitEx(str, _T("&"));
	list.insert(list.begin(), add.begin(), add.end());
}


std::wstring list_instance(std::wstring counter) {
	std::list<std::wstring> exeresult;
	nscapi::core_helper::exec_simple_command(_T("*"), _T("pdh"), boost::assign::list_of(std::wstring(_T("--list")))(_T("--porcelain"))(_T("--counter"))(counter), exeresult);
	std::wstring result;

	typedef std::basic_istringstream<wchar_t> wistringstream;
	typedef boost::tokenizer< boost::escaped_list_separator<wchar_t>, std::wstring::const_iterator, std::wstring > Tokenizer;
	BOOST_FOREACH(std::wstring s, exeresult) {
		wistringstream iss(s);
		std::wstring line;
		while(std::getline(iss, line, L'\n')) {
			Tokenizer tok(line);
			Tokenizer::const_iterator cit = tok.begin();
			int i = 2;
			while ((i-->0) && (cit != tok.end()))
				++cit;
			if (i <= 1) {
				if (!result.empty())
					result += _T(",");
				result += *cit;
			} else {
				NSC_LOG_ERROR(_T("Invalid line: ") + line);
			}
		}
	}
	return result;
}

check_nt::packet NSClientServer::handle(check_nt::packet p) {
	std::wstring buffer = p.get_payload();
	NSC_DEBUG_MSG_STD(_T("Data: ") + buffer);

	std::wstring::size_type pos = buffer.find_first_of(_T("\n\r"));
	if (pos != std::wstring::npos) {
		std::wstring::size_type pos2 = buffer.find_first_not_of(_T("\n\r"), pos);
		if (pos2 != std::wstring::npos) {
			std::wstring rest = buffer.substr(pos2);
			NSC_DEBUG_MSG_STD(_T("Ignoring data: ") + rest);
		}
		buffer = buffer.substr(0, pos);
	}

	strEx::token pwd = strEx::getToken(buffer, '&');
	if (!isPasswordOk(pwd.first)) {
		NSC_LOG_ERROR_STD(_T("Invalid password (") + pwd.first + _T(")."));
		return check_nt::packet("ERROR: Invalid password.");
	}
	if (pwd.second.empty())
		return check_nt::packet("ERROR: No command specified.");
	strEx::token cmd = strEx::getToken(pwd.second, '&');
	if (cmd.first.empty())
		return check_nt::packet("ERROR: No command specified.");

	int c = boost::lexical_cast<int>(cmd.first.c_str());

	NSC_DEBUG_MSG_STD(_T("Data: ") + cmd.second);

	std::list<std::wstring> args;

	// prefix various commands
	switch (c) {
		case REQ_CPULOAD:
			cmd.first = _T("checkCPU");
			split_to_list(args, cmd.second);
			args.push_back(_T("nsclient"));
			break;
		case REQ_UPTIME:
			cmd.first = _T("checkUpTime");
			args.push_back(_T("nsclient"));
			break;
		case REQ_USEDDISKSPACE:
			cmd.first = _T("CheckDriveSize");
			split_to_list(args, cmd.second);
			args.push_back(_T("nsclient"));
			break;
		case REQ_CLIENTVERSION:
			{
				//std::wstring v = SETTINGS_GET_STRING(nsclient::VERSION);
				//if (v == _T("auto"))
				std::wstring v = nscapi::plugin_singleton->get_core()->getApplicationName() + _T(" ") + nscapi::plugin_singleton->get_core()->getApplicationVersionString();
				return strEx::wstring_to_string(v);
			}
		case REQ_SERVICESTATE:
			cmd.first = _T("checkServiceState");
			split_to_list(args, cmd.second);
			args.push_back(_T("nsclient"));
			break;
		case REQ_PROCSTATE:
			cmd.first = _T("checkProcState");
			split_to_list(args, cmd.second);
			args.push_back(_T("nsclient"));
			break;
		case REQ_MEMUSE:
			cmd.first = _T("checkMem");
			args.push_back(_T("nsclient"));
			break;
		case REQ_COUNTER:
			cmd.first = _T("checkCounter");
			args.push_back(_T("Counter=") + cmd.second);
			args.push_back(_T("nsclient"));
			break;
		case REQ_FILEAGE:
			cmd.first = _T("getFileAge");
			args.push_back(_T("path=") + cmd.second);
			break;
		case REQ_INSTANCES:
			return list_instance(cmd.second);


		default:
			split_to_list(args, cmd.second);
	}

	std::wstring message, perf;
	NSCAPI::nagiosReturn ret = nscapi::core_helper::simple_query(cmd.first.c_str(), args, message, perf);
	if (!nscapi::plugin_helper::isNagiosReturnCode(ret)) {
		if (message.empty())
			return check_nt::packet("ERROR: Could not complete the request check log file for more information.");
		return check_nt::packet("ERROR: " + strEx::wstring_to_string(message));
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
			return check_nt::packet(strEx::itos(nscapi::plugin_helper::nagios2int(ret)) + _T("& ") + message);

		default:				// "New" check_nscp also returns performance data
			if (perf.empty())
				return check_nt::packet(nscapi::plugin_helper::translateReturn(ret) + _T("&") + message);
			return check_nt::packet(nscapi::plugin_helper::translateReturn(ret) + _T("&") + message + _T("&") + perf);
	}

	return check_nt::packet("FOO");
}



NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(NSClientServer)
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_IGNORE_CMD_DEF()
