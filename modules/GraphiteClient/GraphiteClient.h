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
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class GraphiteClient : public nscapi::impl::simple_plugin {
private:

	std::string channel_;
	std::string target_path;
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
			target.set_property_int("timeout", 30);
			target.set_property_string("path", "/nsclient++");
		}

		static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool is_sample) {
			nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
			if (is_sample)
				root_path.set_sample();
			root_path.add_key()

				("path", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "path", _1), "system.${hostname}.${check_alias}.${perf_alias}"),
				"PATH FOR VALUES", "Path mapping for metrics")

				;
		}
		static void post_process_target(target_object &target) {
		}
	};

	nscapi::targets::handler<custom_reader> targets;
	client::command_manager commands;
public:
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

		std::string to_string() {
			std::stringstream ss;
			ss << "host: " << host;
			ss << ", port: " << port;
			ss << ", timeout: " << timeout;
			ss << ", path: " << path;
			return ss.str();
		}
	};

	struct target_handler : public client::target_lookup_interface {
		target_handler(const nscapi::targets::handler<custom_reader> &targets) : targets_(targets) {}
		nscapi::protobuf::types::destination_container lookup_target(std::string &id) const;
		bool apply(nscapi::protobuf::types::destination_container &dst, const std::string key);
		bool has_object(std::string alias) const;
		const nscapi::targets::handler<custom_reader> &targets_;
	};


	struct clp_handler_impl : public client::clp_handler {

		int query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message);
		int submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message);
		int exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message);

		boost::tuple<int,std::string> send(connection_data data, const std::list<g_data> payload);

	};


public:
	GraphiteClient();
	virtual ~GraphiteClient();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
	bool commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message);

private:
	void add_options(po::options_description &desc, connection_data &command_data);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config, const ::Plugin::Common_Header& header);
	void add_command(std::string key, std::string args);
	void add_target(std::string key, std::string args);

	void set_delay(std::string key) {
		time_delta_ = strEx::stol_as_time_sec(key, 1);
	}

};
