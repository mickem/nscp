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

#include <protobuf/plugin.pb.h>

#include <client/command_line_parser.hpp>
#include <nscapi/targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>
#include "nrdp.hpp"

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class NRDPClient : public nscapi::impl::simple_plugin {
private:

	std::string channel_;
	std::string target_path;
	std::string hostname_;

	struct custom_reader {
		typedef nscapi::targets::target_object object_type;
		typedef nscapi::targets::target_object target_object;

		static void init_default(target_object &target) {
			target.set_property_int("timeout", 30);
			target.set_property_string("sender", "nscp@localhost");
			target.set_property_string("recipient", "nscp@localhost");
			target.set_property_string("template", "Hello, this is %source% reporting %message%!");
		}

		static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {
			settings.path(object.path).add_key()

				("timeout", sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, "timeout", _1), 30),
				"TIMEOUT", "Timeout when reading/writing packets to/from sockets.")

				("sender", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "sender", _1), "nscp@localhost"),
				"SENDER", "Sender of email message")

				("recipient", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "recipient", _1), "nscp@localhost"),
				"RECIPIENT", "Recipient of email message")

				("template", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "template", _1), "Hello, this is %source% reporting %message%!"),
				"TEMPLATE", "Template for message data")
			;
		}
		static void post_process_target(target_object &target) {
		}
	};

	nscapi::targets::handler<custom_reader> targets;
	client::command_manager commands;

public:
	struct connection_data {
		std::string token;
		std::string host;
		std::string port;
		std::string sender_hostname;
		int timeout;

		connection_data(nscapi::protobuf::types::destination_container arguments, nscapi::protobuf::types::destination_container target, nscapi::protobuf::types::destination_container sender) {
			arguments.import(target);
			timeout = arguments.get_int_data("timeout", 30);

			token = arguments.get_string_data("token");

			host = arguments.address.get_host();
			port = strEx::s::xtos(arguments.address.get_port(80));
			sender_hostname = sender.address.host;
			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
		}
		std::wstring to_wstring() const {
			return utf8::cvt<std::wstring>(to_string());
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << host;
			ss << ", port: " << port;
			ss << ", timeout: " << timeout;
			ss << ", token: " << token;
			ss << ", sender: " << sender_hostname;
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler, client::target_lookup_interface {

		NRDPClient *instance;
		clp_handler_impl(NRDPClient *instance) : instance(instance) {}

		int query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message);
		int submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message);
		int exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message);

		virtual nscapi::protobuf::types::destination_container lookup_target(std::string &id) {
			nscapi::targets::optional_target_object opt = instance->targets.find_object(id);
			if (opt)
				return opt->to_destination_container();
			nscapi::protobuf::types::destination_container ret;
			return ret;
		}
	};


public:
	NRDPClient();
	virtual ~NRDPClient();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
	bool commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message);

private:
	boost::tuple<int,std::wstring> send(connection_data data, const nrdp::data &nrdp_data);
	void add_options(po::options_description &desc, connection_data &command_data);
	static connection_data parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config, const ::Plugin::Common_Header& header);
	void add_command(std::string key, std::string args);
	void add_target(std::string key, std::string args);

};

