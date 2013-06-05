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

#include <protobuf/plugin.pb.h>

#include <client/command_line_parser.hpp>
#include <nscapi/targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class SyslogClient : public nscapi::impl::simple_plugin {
private:

	std::string channel_;
	std::string target_path;

	struct custom_reader {
		typedef nscapi::targets::target_object object_type;
		typedef nscapi::targets::target_object target_object;

		static void init_default(target_object &target) {
			target.set_property_string("severity", "error");
			target.set_property_string("facility", "kernel");
			target.set_property_string("tag syntax", "NSCA");
			target.set_property_string("message syntax", "%message%");
			target.set_property_string("ok severity", "informational");
			target.set_property_string("warning severity", "warning");
			target.set_property_string("critical severity", "critical");
			target.set_property_string("unknown severity", "emergency");
		}

		static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool is_sample) {
			nscapi::settings_helper::path_extension root_path = settings.path(object.path);
			if (is_sample)
				root_path.set_sample();
			root_path.add_key()

				("severity", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "severity", _1), "error"),
				"TODO", "")

				("facility", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "facility", _1), "kernel"),
				"TODO", "")

				("tag_syntax", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "tag syntax", _1), "NSCA"),
				"TODO", "")

				("message_syntax", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "message syntax", _1), "%message%"),
				"TODO", "")

				("ok severity", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "ok severity", _1), "informational"),
				"TODO", "")

				("warning severity", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "warning severity", _1), "warning"),
				"TODO", "")

				("critical severity", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "critical severity", _1), "critical"),
				"TODO", "")

				("unknown severity", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "unknown severity", _1), "emergency"),
				"TODO", "")
				;
		}
		static void post_process_target(target_object &target) {
		}
	};

	nscapi::targets::handler<custom_reader> targets;
	client::command_manager commands;

	typedef std::map<std::string,int> syslog_map;
	syslog_map facilities;
	syslog_map severities;
	std::string hostname_;

	struct connection_data {
		std::string severity;
		std::string facility;
		std::string tag_syntax;
		std::string message_syntax;
		std::string host;
		int port;
		std::string ok_severity, warn_severity, crit_severity, unknown_severity;

		connection_data(nscapi::protobuf::types::destination_container arguments, nscapi::protobuf::types::destination_container target) {
			arguments.import(target);
			severity = arguments.data["severity"];
			facility = arguments.data["facility"];
			tag_syntax = arguments.data["tag template"];
			message_syntax = arguments.data["message template"];

			ok_severity = arguments.data["ok severity"];
			warn_severity = arguments.data["warning severity"];
			crit_severity = arguments.data["critical severity"];
			unknown_severity = arguments.data["unknown severity"];

			host = arguments.address.host;
			port = arguments.address.get_port(514);
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << host;
			ss << ", port: " << port;
			ss << ", severity: " << severity;
			ss << ", facility: " << facility;
			ss << ", tag_syntax: " << tag_syntax;
			ss << ", message_syntax: " << message_syntax;
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler, client::target_lookup_interface {

		SyslogClient *instance;
		clp_handler_impl(SyslogClient *instance) : instance(instance) {}

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
	SyslogClient();
	virtual ~SyslogClient();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
	bool commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message);

private:
	boost::tuple<int,std::wstring> send(connection_data con, std::list<std::string> messages);
	static connection_data parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config, const ::Plugin::Common_Header& header);
	void add_command(std::string key, std::string args);
	void add_target(std::string key, std::string args);
	std::string	parse_priority(std::string severity, std::string facility);

};

