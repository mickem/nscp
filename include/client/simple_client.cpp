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

#include <client/simple_client.hpp>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <utf8.hpp>
#include <str/utils.hpp>

#include <boost/foreach.hpp>
#include <boost/function.hpp>

static void create_registry_query(const nscapi::core_wrapper *core, const std::string command, const Plugin::Registry_ItemType &type, Plugin::RegistryResponseMessage &response_message) {
	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
	if (!command.empty()) {
		payload->mutable_inventory()->set_name(command);
		payload->mutable_inventory()->set_fetch_all(true);
	}
	payload->mutable_inventory()->add_type(type);
	std::string pb_response;
	core->registry_query(rrm.SerializeAsString(), pb_response);
	response_message.ParseFromString(pb_response);
}

std::string render_command(const ::Plugin::RegistryResponseMessage::Response::Inventory& inv) {
	std::string data = "command:\t" + inv.name() + "\n" + inv.info().description() + "\n\nParameters:\n";
	for (int i = 0; i < inv.parameters().parameter_size(); i++) {
		::Plugin::Registry::ParameterDetail p = inv.parameters().parameter(i);
		std::string desc = p.long_description();
		std::size_t pos = desc.find('\n');
		if (pos != std::string::npos)
			desc = desc.substr(0, pos - 1);
		data += p.name() + "\t" + desc + "\n";
	}
	return data;
}
std::string render_plugin(const ::Plugin::RegistryResponseMessage::Response::Inventory& inv) {
	std::string loaded = "[ ]";
	for (int i = 0; i < inv.info().metadata_size(); i++) {
		if (inv.info().metadata(i).key() == "loaded" && inv.info().metadata(i).value() == "true")
			loaded = "[X]";
	}
	return loaded + "\t" + inv.name() + "\t-" + inv.info().description();
}
std::string render_query(const ::Plugin::RegistryResponseMessage::Response::Inventory& inv) {
	return inv.name() + "\t-" + inv.info().description();
}

static std::string render_list(const Plugin::RegistryResponseMessage &response_message, boost::function<std::string(const ::Plugin::RegistryResponseMessage::Response::Inventory&)> renderer) {
	std::string list;
	BOOST_FOREACH(const ::Plugin::RegistryResponseMessage::Response &pl, response_message.payload()) {
		BOOST_FOREACH(const ::Plugin::RegistryResponseMessage_Response_Inventory& i, pl.inventory()) {
			if (!list.empty())
				list += "\n";
			list += renderer(i);
		}
		if (pl.result().code() != ::Plugin::Common_Result_StatusCodeType_STATUS_OK) {
			return "Error: " + pl.result().message();
		}
	}
	return list;
}

namespace client {
	void cli_client::handle_command(const std::string &command) {
		if (command == "plugins") {
			Plugin::RegistryResponseMessage response_message;
			create_registry_query(handler->get_core(), "", Plugin::Registry_ItemType_MODULE, response_message);
			std::string list = render_list(response_message, &render_plugin);
			handler->output_message(list.empty() ? "Nothing found" : list);
		} else if (command == "help") {
			handler->output_message("Commands: \n"
				"\thelp\t\t\t-get help\n"
				"\tqueries\t\t\t-List all available queries\n"
				"\taliases\t\t\t-List all available aliases\n"
				"\tload <module>\t\t-Will load the module\n"
				"\tunload <module>\t\t-Will unload the module\n"
				"\tdesc <query>\t\t-Describe a query\n"
				"\tplugins\t\t\t-list all plugins\n"
				"\t<any other command>\t-Will be executed as a query");
		} else if (command == "reload") {
			handler->get_core()->reload("delayed,service");
		} else if (command.size() > 6 && command.substr(0, 6) == "enable") {
			std::string name = command.substr(7);
			bool has_errors = false;
			{
				Plugin::SettingsRequestMessage srm;
				Plugin::SettingsRequestMessage::Request *r = srm.add_payload();
				r->mutable_update()->mutable_node()->set_path("/modules");
				r->mutable_update()->mutable_node()->set_key(name);
				r->mutable_update()->mutable_node()->set_value("enabled");
				r->set_plugin_id(handler->get_plugin_id());
				std::string response;
				handler->get_core()->settings_query(srm.SerializeAsString(), response);
				Plugin::SettingsResponseMessage response_message;
				response_message.ParseFromString(response);
				for (int i = 0; i < response_message.payload_size(); i++) {
					if (response_message.payload(i).result().code() != ::Plugin::Common_Result_StatusCodeType_STATUS_OK) {
						handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
						has_errors = true;
					}
				}
			}
			{
				Plugin::SettingsRequestMessage srm;
				Plugin::SettingsRequestMessage::Request *r = srm.add_payload();
				r->mutable_control()->set_command(::Plugin::Settings_Command_SAVE);
				r->set_plugin_id(handler->get_plugin_id());
				std::string response;
				handler->get_core()->settings_query(srm.SerializeAsString(), response);
				Plugin::SettingsResponseMessage response_message;
				response_message.ParseFromString(response);
				for (int i = 0; i < response_message.payload_size(); i++) {
					if (response_message.payload(i).result().code() != ::Plugin::Common_Result_StatusCodeType_STATUS_OK) {
						handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
						has_errors = true;
					}
				}
			}
			if (!has_errors)
				handler->output_message(name + " enabled successfully...");
		} else if (command.size() > 7 && command.substr(0, 7) == "disable") {
			std::string name = command.substr(8);
			bool has_errors = false;
			{
				Plugin::SettingsRequestMessage srm;
				Plugin::SettingsRequestMessage::Request *r = srm.add_payload();
				r->mutable_update()->mutable_node()->set_path("/modules");
				r->mutable_update()->mutable_node()->set_key(name);
				r->mutable_update()->mutable_node()->set_value("disabled");
				r->set_plugin_id(handler->get_plugin_id());
				std::string response;
				handler->get_core()->settings_query(srm.SerializeAsString(), response);
				Plugin::SettingsResponseMessage response_message;
				response_message.ParseFromString(response);
				for (int i = 0; i < response_message.payload_size(); i++) {
					if (response_message.payload(i).result().code() != ::Plugin::Common_Result_StatusCodeType_STATUS_OK) {
						handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
						has_errors = true;
					}
				}
			}
			{
				Plugin::SettingsRequestMessage srm;
				Plugin::SettingsRequestMessage::Request *r = srm.add_payload();
				r->mutable_control()->set_command(::Plugin::Settings_Command_SAVE);
				r->set_plugin_id(handler->get_plugin_id());
				std::string response;
				handler->get_core()->settings_query(srm.SerializeAsString(), response);
				Plugin::SettingsResponseMessage response_message;
				response_message.ParseFromString(response);
				for (int i = 0; i < response_message.payload_size(); i++) {
					if (response_message.payload(i).result().code() != ::Plugin::Common_Result_StatusCodeType_STATUS_OK) {
						handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
						has_errors = true;
					}
				}
			}
			if (!has_errors)
				handler->output_message(name + " disabled successfully...");
		} else if (command.size() > 4 && command.substr(0, 4) == "load") {
			Plugin::RegistryRequestMessage rrm;
			Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
			std::string name = command.substr(5);
			payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
			payload->mutable_control()->set_command(Plugin::Registry_Command_LOAD);
			payload->mutable_control()->set_name(name);
			std::string pb_response, json_response;
			handler->get_core()->registry_query(rrm.SerializeAsString(), pb_response);
			Plugin::RegistryResponseMessage response_message;
			response_message.ParseFromString(pb_response);
			bool has_errors = false;
			for (int i = 0; i < response_message.payload_size(); i++) {
				if (response_message.payload(i).result().code() != ::Plugin::Common_Result_StatusCodeType_STATUS_OK) {
					handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
					has_errors = true;
				}
			}
			if (!has_errors)
				handler->output_message(name + " loaded successfully...");
		} else if (command.size() > 6 && command.substr(0, 6) == "unload") {
			Plugin::RegistryRequestMessage rrm;
			Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
			std::string name = command.substr(7);
			payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
			payload->mutable_control()->set_command(Plugin::Registry_Command_UNLOAD);
			payload->mutable_control()->set_name(name);
			std::string pb_response, json_response;
			handler->get_core()->registry_query(rrm.SerializeAsString(), pb_response);
			Plugin::RegistryResponseMessage response_message;
			response_message.ParseFromString(pb_response);
			bool has_errors = false;
			for (int i = 0; i < response_message.payload_size(); i++) {
				if (response_message.payload(i).result().code() != ::Plugin::Common_Result_StatusCodeType_STATUS_OK) {
					handler->output_message("Failed to unload module: " + response_message.payload(i).result().message());
					has_errors = true;
				}
			}
			if (!has_errors)
				handler->output_message(name + " unloaded successfully...");
		} else if (command == "queries" || command == "commands") {
			Plugin::RegistryResponseMessage response_message;
			create_registry_query(handler->get_core(), "", Plugin::Registry_ItemType_QUERY, response_message);
			std::string list = render_list(response_message, &render_query);
			handler->output_message(list.empty() ? "Nothing found" : list);
		} else if (command == "aliases") {
			Plugin::RegistryResponseMessage response_message;
			create_registry_query(handler->get_core(), "", Plugin::Registry_ItemType_QUERY_ALIAS, response_message);
			std::string list = render_list(response_message, &render_query);
			handler->output_message(list.empty() ? "Nothing found" : list);
		} else if (command.size() > 5 && command.substr(0, 4) == "desc") {
			Plugin::RegistryResponseMessage response_message;
			create_registry_query(handler->get_core(), command.substr(5), Plugin::Registry_ItemType_QUERY, response_message);
			std::string data = render_list(response_message, &render_command);
			handler->output_message(data.empty() ? "Command not found" : data);
		} else if (command == "list") {
			Plugin::RegistryResponseMessage response_message;
			create_registry_query(handler->get_core(), "", Plugin::Registry_ItemType_QUERY, response_message);
			std::string list = render_list(response_message, &render_query);
			create_registry_query(handler->get_core(), "", Plugin::Registry_ItemType_QUERY_ALIAS, response_message);
			list = render_list(response_message, &render_query);
			handler->output_message(list.empty() ? "Nothing found" : list);
		} else if (command.size() >= 7 && command.substr(0, 7) == "metrics") {
			BOOST_FOREACH(const metrics::metrics_store::values_map::value_type &v, metrics_store.get(command.substr(7))) {
				handler->output_message(v.first + "=" + v.second);
			}
		} else if (command.size() > 4 && command.substr(0, 4) == "exec") {
			try {
				std::list<std::string> args;
				str::utils::parse_command(command, args);
				if (args.size() < 3) {
					handler->output_message("Usage: exec <target> <command> [args]");
					return;
				}
				args.pop_front();
				std::string target = args.front(); args.pop_front();
				std::string cmd = args.front(); args.pop_front();
				std::list<std::string> result;
				nscapi::core_helper helper(handler->get_core(), handler->get_plugin_id());
				helper.exec_simple_command(target, cmd, args, result);
				BOOST_FOREACH(const std::string &s, result)
					handler->output_message(s);
			} catch (const std::exception &e) {
				handler->output_message("Exception: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				handler->output_message("Unknown exception");
			}
		} else if (command.size() >= 8 && command.substr(0, 8) == "settings") {
			namespace pf = nscapi::protobuf::functions;

			pf::settings_query q(handler->get_plugin_id());
			q.list("/", true);

			handler->get_core()->settings_query(q.request(), q.response());
			if (!q.validate_response()) {
				handler->output_message("ERROR: " + q.get_response_error());
			} else {
				BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
					std::string tmp;
					tmp += val.path();
					tmp += "/" + val.key();
					tmp += "=" + val.get_string();
					handler->output_message(tmp);
				}
			}
		} else if (!command.empty()) {
			try {
				std::list<std::string> args;
				str::utils::parse_command(command, args);
				std::string cmd = args.front(); args.pop_front();
				std::string msg, perf;
				nscapi::core_helper helper(handler->get_core(), handler->get_plugin_id());
				std::string response;
				NSCAPI::nagiosReturn ret = helper.simple_query(cmd, args, response);
				if (!response.empty()) {
					try {
						Plugin::QueryResponseMessage message;
						message.ParseFromString(response);

						BOOST_FOREACH(const Plugin::QueryResponseMessage::Response payload, message.payload()) {
							BOOST_FOREACH(const Plugin::QueryResponseMessage::Response::Line &l, payload.lines()) {
								std::string msg = nscapi::plugin_helper::translateReturn(payload.result()) + ": " + l.message();
								handler->output_message(msg);
								std::string perf = nscapi::protobuf::functions::build_performance_data(l, nscapi::protobuf::functions::no_truncation);
								handler->output_message(" Performance data: " + perf);
							}
							//return gbp_to_nagios_status(payload.result());
						}
					} catch (std::exception &e) {
						std::string msg = "Failed to extract return message: " + utf8::utf8_from_native(e.what());
						handler->output_message(msg);
					}
				}
			} catch (const std::exception &e) {
				handler->output_message("Exception: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				handler->output_message("Unknown exception");
			}
		}
	}

	void cli_client::push_metrics(const Plugin::MetricsMessage &response) {
		metrics_store.set(response);
	}

}