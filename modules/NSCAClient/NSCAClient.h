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
#pragma once


#include <client/command_line_parser.hpp>
#include <boost/program_options.hpp>

#include <nsca/nsca_packet.hpp>

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CHANNELS();

class NSCAAgent : public nscapi::impl::simple_plugin {
private:

	std::string hostname_;
	std::wstring nscahost_;
	unsigned int nscaport_;
	unsigned int payload_length_;
	bool cacheNscaHost_;
	std::string password_;
	int encryption_method_;
	unsigned int timeout_;
	std::wstring channel_;
	int time_delta_;


	struct sender_information {
		sender_information(nscapi::functions::destination_container &src) {
			net::url u = src.get_url(5667);
			host = u.host;
			port = u.port;
			password = src.data["password"];
			encryption = src.data["encryption"];
		}
		std::string password;
		std::string encryption;
		std::string host;
		unsigned int timeout;
		int port;
		unsigned int get_encryption() {
			return nsca::nsca_encrypt::helpers::encryption_to_int(encryption);
		}
	};

	struct nscp_connection_data : public client::nscp_cli_data {
		unsigned int payload_length;
		int time_delta;
		std::string encryption;
		std::wstring cert;
		nscp_connection_data(unsigned int payload_length, int time_delta) : payload_length(payload_length), time_delta(time_delta) {}

		std::string parse_encryption() {

		}
		std::string get_encryption_string() {

			if (encryption.size() > 1 && std::isalnum(encryption[0]))
				encryption = parse_encryption();
			return encryption;
		}

		std::wstring to_wstring() {
			std::wstringstream ss;
			ss << _T(", cert: ") << cert;
			ss << _T(", no_ssl: ") << utf8::cvt<std::wstring>(get_encryption_string());
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler {

		NSCAAgent *instance;
		clp_handler_impl(NSCAAgent *instance, unsigned int payload_length, int time_delta) : instance(instance), local_data(payload_length, time_delta) {}
		nscp_connection_data local_data;

		int query(client::configuration::data_type data, std::string request, std::string &reply);
		std::list<std::string> submit(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &response);
		int exec(client::configuration::data_type data, std::string request, std::string &reply);
	};

public:
	NSCAAgent();
	virtual ~NSCAAgent();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	/**
	* Return the module name.
	* @return The module name
	*/
	static std::wstring getModuleName() {
#ifdef HAVE_LIBCRYPTOPP
		return _T("NSCAAgent (w/ encryption)");
#else
		return _T("NSCAAgent");
#endif
	}
	/**
	* Module version
	* @return module version
	*/
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 3, 0 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return std::wstring(_T("Passive check support (needs NSCA on nagios server).\nAvalible crypto are: ")) + getCryptos();
	}
	bool hasNotificationHandler() { return true; }

	static std::wstring getCryptos();

	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
	//NSCAPI::nagiosReturn handleSimpleNotification(const std::wstring channel, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf);
	NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	int commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);

	NSCAPI::nagiosReturn send(sender_information &data, const std::list<nsca::packet> packets);


	std::wstring setup(client::configuration &config, const std::wstring &command);
	void add_local_options(boost::program_options::options_description &desc, nscp_connection_data &command_data);

	void set_delay(std::wstring key) {
		time_delta_ = strEx::stol_as_time_sec(key, 1);
	}

};
