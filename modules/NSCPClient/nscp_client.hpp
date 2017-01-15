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

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include <client/command_line_parser.hpp>
#include <socket/client.hpp>
#include <socket/clients/http/http_client_protocol.hpp>

#include <str/format.hpp>

#include <boost/tuple/tuple.hpp>

namespace nscp_client {
	struct connection_data : public socket_helpers::connection_info {
		std::string password;
		std::string path;
		boost::shared_ptr<socket_helpers::client::client_handler> handler;

		connection_data(client::destination_container source, client::destination_container target, boost::shared_ptr<socket_helpers::client::client_handler> handler) : handler(handler) {
			address = target.address.host;
			port_ = target.address.get_port_string("8443");

			ssl.certificate = ""; // target.get_string_data("certificate", "${certificate-path}/certificate.pem");
			ssl.certificate_key = target.get_string_data("certificate key");
			ssl.certificate_key_format = target.get_string_data("certificate format", "PEM");
			ssl.ca_path = target.get_string_data("ca");
			ssl.allowed_ciphers = target.get_string_data("allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
			ssl.dh_key = target.get_string_data("dh");
			ssl.verify_mode = target.get_string_data("verify mode", "none");
			if (!ssl.certificate.empty())
				ssl.certificate = handler->expand_path(ssl.certificate);
			if (!ssl.certificate_key.empty())
				ssl.certificate_key = handler->expand_path(ssl.certificate_key);

			timeout = target.timeout;
			retry = target.retry;
			password = target.get_string_data("password", "");
			path = target.get_string_data("path", "/query.pb");

			if (target.has_data("no ssl"))
				ssl.enabled = !target.get_bool_data("no ssl");
			if (target.has_data("ssl"))
				ssl.enabled = target.get_bool_data("ssl");
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			ss << ", path: " << path;
			ss << ", password: " << password;
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

	template<class THandler=client_handler>
	struct nscp_client_handler : public client::handler_interface {
		boost::shared_ptr<THandler> handler_;
		nscp_client_handler() : handler_(boost::make_shared<THandler>()) {}

		std::string get_command(std::string alias, std::string command = "") {
			if (!alias.empty())
				return alias;
			if (!command.empty())
				return command;
			return "";
		}

		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nscp_client::connection_data con(sender, target, handler_);

			handler_->log_debug(__FILE__, __LINE__, "Connecting to: " + con.to_string());

			BOOST_FOREACH(const std::string &e, con.validate()) {
				handler_->log_error(__FILE__, __LINE__, e);
			}
			boost::tuple<bool, std::string> ret = send(con, request_message.SerializeAsString());
			if (ret.get<0>()) {
				response_message.ParseFromString(ret.get<1>());
			} else {
				nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);
				nscapi::protobuf::functions::append_simple_query_response_payload(response_message.add_payload(), "", NSCAPI::query_return_codes::returnUNKNOWN, ret.get<1>(), "");
			}
			return true;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nscp_client::connection_data con(sender, target, handler_);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			for (int i = 0; i < request_message.payload_size(); ++i) {
				std::string command = get_command(request_message.payload(i).alias(), request_message.payload(i).command());
				std::string data = command;
				for (int a = 0; a < request_message.payload(i).arguments_size(); a++) {
					data += "!" + request_message.payload(i).arguments(i);
				}
				boost::tuple<int, std::string> ret = send(con, data);
				bool wentOk = ret.get<0>() != NSCAPI::query_return_codes::returnUNKNOWN;
				nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), command, wentOk, ret.get<1>());
			}
			return true;
		}

		bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nscp_client::connection_data con(sender, target, handler_);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			for (int i = 0; i < request_message.payload_size(); i++) {
				std::string command = get_command(request_message.payload(i).command());
				std::string data = command;
				for (int a = 0; a < request_message.payload(i).arguments_size(); a++)
					data += "!" + request_message.payload(i).arguments(a);
				boost::tuple<int, std::string> ret = send(con, data);
				nscapi::protobuf::functions::append_simple_exec_response_payload(response_message.add_payload(), command, ret.get<0>(), ret.get<1>());
			}
			return true;
		}

		bool metrics(client::destination_container sender, client::destination_container target, const Plugin::MetricsMessage &request_message) {
			return false;
		}


		//////////////////////////////////////////////////////////////////////////
		// Protocol implementations
		//

		boost::tuple<bool, std::string> send(nscp_client::connection_data con, const std::string data) {
			try {
#ifndef USE_SSL
				if (con.ssl.enabled)
					return boost::make_tuple(false, "SSL support not available (compiled without USE_SSL)");
#endif
				http::packet packet("POST", con.path, data);
				socket_helpers::client::client<http::client::protocol> client(con, handler_);
				http::packet response = client.process_request(packet);
				return boost::make_tuple(true, response.get_payload());
			} catch (std::runtime_error &e) {
				return boost::make_tuple(false, "Socket error: " + utf8::utf8_from_native(e.what()));
			} catch (std::exception &e) {
				return boost::make_tuple(false, "Error: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				return boost::make_tuple(false, "Unknown error -- REPORT THIS!");
			}
		}
	};
}