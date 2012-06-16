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
#include <nscapi/nscapi_protobuf_types.hpp>

NSC_WRAPPERS_MAIN()
NSC_WRAPPERS_CLI()
NSC_WRAPPERS_CHANNELS()

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class GraphiteClient : public nscapi::impl::simple_plugin {
private:

	std::wstring channel_;
	std::wstring target_path;
	const static std::wstring command_prefix;
	std::string hostname_;
	bool cacheNscaHost_;
	long time_delta_;

	struct g_data {
		std::string path;
		std::string value;
	};

	struct custom_reader {
		typedef nscapi::targets::target_object object_type;
		typedef nscapi::targets::target_object target_object;

		static void init_default(target_object &target) {
			target.set_property_int(_T("timeout"), 30);
			target.set_property_string(_T("path"), _T("/nsclient++"));
		}

		static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {
			settings.path(object.path).add_key()

				(_T("path"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("path"), _1), _T("system.${hostname}.${check_alias}.${perf_alias}")),
				_T("PATH FOR VALUES"), _T(""))

				;
		}
		static void post_process_target(target_object &target) {
		}
	};

	nscapi::targets::handler<custom_reader> targets;
	client::command_manager commands;

	struct connection_data {
		std::string path;
		std::string host, port, sender_hostname;
		int timeout;

		connection_data(nscapi::protobuf::types::destination_container recipient, nscapi::protobuf::types::destination_container target, nscapi::protobuf::types::destination_container sender) {
			recipient.import(target);
			timeout = recipient.get_int_data("timeout", 30);
			path = recipient.get_string_data("path");
			host = recipient.address.get_host();
			port = strEx::s::xtos(recipient.address.get_port(2003));
			sender_hostname = sender.address.host;
			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
		}

		std::wstring to_wstring() {
			std::wstringstream ss;
			ss << _T("host: ") << utf8::cvt<std::wstring>(host);
			ss << _T(", port: ") << utf8::cvt<std::wstring>(port);
			ss << _T(", timeout: ") << timeout;
			ss << _T(", path: ") << utf8::cvt<std::wstring>(path);
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler, client::target_lookup_interface {

		GraphiteClient *instance;
		clp_handler_impl(GraphiteClient *instance) : instance(instance) {}

		int query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, std::string &reply);
		int submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, std::string &reply);
		int exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, std::string &reply);

		virtual nscapi::protobuf::types::destination_container lookup_target(std::wstring &id) {
			nscapi::targets::optional_target_object opt = instance->targets.find_object(id);
			if (opt)
				return opt->to_destination_container();
			nscapi::protobuf::types::destination_container ret;
			return ret;
		}
	};


public:
	GraphiteClient();
	virtual ~GraphiteClient();
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
		return std::wstring(_T("Graphite client"));
	}

	bool hasCommandHandler() { return true; };
	bool hasMessageHandler() { return true; };
	bool hasNotificationHandler() { return true; };
	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
	NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);

private:
	boost::tuple<int,std::wstring> send(connection_data data, const std::list<g_data> payload);
	void add_options(po::options_description &desc, connection_data &command_data);
	static connection_data parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config, const ::Plugin::Common_Header& header);
	void add_command(std::wstring key, std::wstring args);
	void add_target(std::wstring key, std::wstring args);

	static std::wstring getCryptos();
	void set_delay(std::wstring key) {
		time_delta_ = strEx::stol_as_time_sec(key, 1);
	}

};
