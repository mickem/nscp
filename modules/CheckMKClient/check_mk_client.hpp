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

#pragma once

#include <check_mk/client/client_protocol.hpp>
#include <socket/client.hpp>

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

namespace check_mk_client {
	struct connection_data : public socket_helpers::connection_info {
		connection_data(client::destination_container arguments, client::destination_container sender) {
			address = arguments.address.host;
			port_ = arguments.address.get_port_string("5667");
			ssl.enabled = arguments.get_bool_data("ssl");
			ssl.certificate = arguments.get_string_data("certificate");
			ssl.certificate_key = arguments.get_string_data("certificate key");
			ssl.certificate_key_format = arguments.get_string_data("certificate format");
			ssl.ca_path = arguments.get_string_data("ca");
			ssl.allowed_ciphers = arguments.get_string_data("allowed ciphers");
			ssl.dh_key = arguments.get_string_data("dh");
			ssl.verify_mode = arguments.get_string_data("verify mode");
			timeout = arguments.get_int_data("timeout", 30);
			if (arguments.has_data("no ssl"))
				ssl.enabled = !arguments.get_bool_data("no ssl");
			if (arguments.has_data("use ssl"))
				ssl.enabled = arguments.get_bool_data("use ssl");
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			ss << ", ssl: " << ssl.to_string();
			return ss.str();
		}
	};

	struct client_handler : public socket_helpers::client::client_handler {
		void log_debug(std::string file, int line, std::string msg) const {
			if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
				GET_CORE()->log(NSCAPI::log_level::debug, file, line, msg);
			}
		}
		void log_error(std::string file, int line, std::string msg) const {
			if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
				GET_CORE()->log(NSCAPI::log_level::error, file, line, msg);
			}
		}
		std::string expand_path(std::string path) {
			return GET_CORE()->expand_path(path);
		}
	};

	struct check_mk_client_handler : public client::handler_interface {
		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			check_mk_client::connection_data con(sender, target);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			send(response_message.add_payload(), con);
			return true;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			return false;
		}

		bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
			return false;
		}

		bool metrics(client::destination_container sender, client::destination_container target, const Plugin::MetricsMessage &request_message) {
			return false;
		}


		NSCAPI::nagiosReturn parse_data(lua::script_information *information, lua::lua_traits::function_type c, const check_mk::packet &packet) {
			lua::lua_wrapper instance(lua::lua_runtime::prep_function(information, c));
			int args = 1;
			if (c.object_ref != 0)
				args = 2;
			check_mk::check_mk_packet_wrapper* obj = Luna<check_mk::check_mk_packet_wrapper>::createNew(instance);
			obj->packet = packet;
			if (instance.pcall(args, LUA_MULTRET, 0) != 0) {
				NSC_LOG_ERROR_STD("Failed to process check_mk result: " + instance.pop_string());
				return NSCAPI::query_return_codes::returnUNKNOWN;
			}
			instance.gc(LUA_GCCOLLECT, 0);
			return NSCAPI::query_return_codes::returnUNKNOWN;
		}

		void send(Plugin::QueryResponseMessage::Response *payload, connection_data &con) {
			try {
				socket_helpers::client::client<check_mk::client::protocol> client(con, boost::make_shared<client_handler>());
				NSC_DEBUG_MSG("Connecting to: " + con.to_string());
				if (con.ssl.enabled) {
#ifndef USE_SSL
					NSC_LOG_ERROR_STD(_T("SSL not available (compiled without USE_SSL)"));
					return response;
#endif
				}
				client.connect();
				std::string dummy;
				check_mk::packet packet = client.process_request(dummy);
				boost::optional<scripts::command_definition<lua::lua_traits> > cmd; // = scripts_->find_command("check_mk", "c_callback");
				if (cmd) {
					parse_data(cmd->information, cmd->function, packet);
				} else {
					NSC_LOG_ERROR_STD("No check_mk callback found!");
				}
				//lua_runtime_->on_query()
				client.shutdown();
				nscapi::protobuf::functions::set_response_good(*payload, "Data presumably sent successfully");
			} catch (const std::runtime_error &e) {
				nscapi::protobuf::functions::set_response_bad(*payload, "Socket error: " + utf8::utf8_from_native(e.what()));
			} catch (const std::exception &e) {
				nscapi::protobuf::functions::set_response_bad(*payload, "Error: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				nscapi::protobuf::functions::set_response_bad(*payload, "Unknown error -- REPORT THIS!");
			}
		}
	};
}