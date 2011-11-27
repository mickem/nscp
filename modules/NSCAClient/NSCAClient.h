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

#include <boost/tuple/tuple.hpp>

#include <client/command_line_parser.hpp>
#include <nscapi/targets.hpp>

#include <nsca/nsca_packet.hpp>

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();
NSC_WRAPPERS_CHANNELS();

namespace po = boost::program_options;

class NSCAAgent : public nscapi::impl::simple_command_handler, public nscapi::impl::simple_plugin, public nscapi::impl::simple_command_line_exec {
private:

	std::wstring channel_;
	std::wstring target_path;
	std::string hostname_;
	bool cacheNscaHost_;
	long time_delta_;

	nscapi::target_handler targets;
	client::command_manager commands;

	struct connection_data {
		std::string password;
		std::string encryption;
		std::string host, port;
		std::string sender_hostname;
		int timeout;
		int buffer_length;
		int time_delta;

		connection_data(nscapi::functions::destination_container recipient, nscapi::functions::destination_container sender) {
			timeout = recipient.get_int_data("timeout", 30);
			buffer_length = recipient.get_int_data("payload length", 1024);
			password = recipient.get_string_data("password");
			encryption = recipient.get_string_data("encryption");
			time_delta = strEx::stol_as_time_sec(recipient.get_string_data("time offset"));
			net::url url = recipient.get_url(5667);
			host = url.host;
			port = url.get_port();
			sender_hostname = sender.get_string_data("host");
		}
		unsigned int get_encryption() {
			return nsca::nsca_encrypt::helpers::encryption_to_int(encryption);
		}

		std::wstring to_wstring() {
			std::wstringstream ss;
			ss << _T("host: ") << utf8::cvt<std::wstring>(host);
			ss << _T(", port: ") << utf8::cvt<std::wstring>(port);
			ss << _T(", timeout: ") << timeout;
			ss << _T(", buffer_length: ") << buffer_length;
			ss << _T(", time_delta: ") << time_delta;
			ss << _T(", password: ") << utf8::cvt<std::wstring>(password);
			ss << _T(", encryption: ") << utf8::cvt<std::wstring>(encryption);
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler, client::target_lookup_interface {

		NSCAAgent *instance;
		clp_handler_impl(NSCAAgent *instance) : instance(instance) {}

		int query(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply);
		int submit(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply);
		int exec(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply);

		virtual nscapi::functions::destination_container lookup_target(std::wstring &id) {
			nscapi::functions::destination_container ret;
			nscapi::target_handler::optarget t = instance->targets.find_target(id);
			if (t) {
				if (!t->alias.empty())
					ret.id = utf8::cvt<std::string>(t->alias);
				if (!t->host.empty())
					ret.host = utf8::cvt<std::string>(t->host);
				if (t->has_option("address"))
					ret.address = utf8::cvt<std::string>(t->options[_T("address")]);
				else 
					ret.address = utf8::cvt<std::string>(t->host);
				BOOST_FOREACH(const nscapi::target_handler::target::options_type::value_type &kvp, t->options) {
					ret.data[utf8::cvt<std::string>(kvp.first)] = utf8::cvt<std::string>(kvp.second);
				}
			}
			return ret;
		}
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
		return _T("NSCAClient");
#else
		return _T("NSCAClient (without encryption support)");
#endif
	}
	/**
	* Module version
	* @return module version
	*/
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 4, 0 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return std::wstring(_T("Passive check support (needs NSCA on nagios server).\nAvalible crypto are: ")) + getCryptos();
	}

	bool hasCommandHandler() { return true; };
	bool hasMessageHandler() { return true; };
	bool hasNotificationHandler() { return true; };
	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
	NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	int commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);

private:
	boost::tuple<int,std::wstring> send(connection_data data, const std::list<nsca::packet> packets);
	void add_options(po::options_description &desc, connection_data &command_data);
	static connection_data parse_header(const ::Plugin::Common_Header &header);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config);
	void add_command(std::wstring key, std::wstring args);
	void add_target(std::wstring key, std::wstring args);

	static std::wstring getCryptos();
	void set_delay(std::wstring key) {
		time_delta_ = strEx::stol_as_time_sec(key, 1);
	}

};
