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

#include <nsca/nsca_packet.hpp>
#include <nsca/client/nsca_client_protocol.hpp>
#include <nscapi/nscapi_protobuf_nagios.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <client/command_line_parser.hpp>
#include <socket/client.hpp>

#include <str/format.hpp>

namespace nsca_client {
	struct connection_data : public socket_helpers::connection_info {
		std::string password;
		std::string encryption;
		std::string sender_hostname;
		int buffer_length;
		int time_delta;
		std::string encoding;

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
			retry = arguments.get_int_data("retries", 3);
			buffer_length = arguments.get_int_data("payload length", 512);
			password = arguments.get_string_data("password");
			encryption = arguments.get_string_data("encryption");
			encoding = arguments.get_string_data("encoding");
			std::string tmp = arguments.get_string_data("time offset");
			if (!tmp.empty())
				time_delta = str::format::stox_as_time_sec<int>(arguments.get_string_data("time offset"), "s");
			else
				time_delta = 0;
			sender_hostname = sender.address.host;
			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
		}
		unsigned int get_encryption() const {
			return nscp::encryption::helpers::encryption_to_int(encryption);
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			ss << ", buffer_length: " << buffer_length;
			ss << ", time_delta: " << time_delta;
			ss << ", password: " << password;
			ss << ", encryption: " << encryption << "(" << nscp::encryption::helpers::encryption_to_int(encryption) << ")";
			ss << ", hostname: " << sender_hostname;
			ss << ", encoding: " << encoding;
			ss << ", ssl: " << ssl.to_string();
			return ss.str();
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Protocol implementations
	//
	struct client_handler : public socket_helpers::client::client_handler {
		unsigned int encryption_;
		std::string password_;
		client_handler(const connection_data &con)
			: encryption_(con.get_encryption())
			, password_(con.password) {}
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
		unsigned int get_encryption() {
			return encryption_;
		}
		std::string get_password() {
			return password_;
		}
		std::string expand_path(std::string path) {
			return GET_CORE()->expand_path(path);
		}
	};

	struct nsca_client_handler : public client::handler_interface {
		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			return false;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);
			connection_data con(target, sender);

			NSC_TRACE_ENABLED() {
				NSC_TRACE_MSG("Target configuration: " + target.to_string());
			}
			unsigned int len = 512;
			if (target.has_data("buffer length"))
				len = target.get_int_data("buffer length", 512);
			else if (target.has_data("payload length"))
				len = target.get_int_data("payload length", 512);
			std::list<nsca::packet> list;
			BOOST_FOREACH(const Plugin::QueryResponseMessage::Response &payload, request_message.payload()) {
				nsca::packet packet(sender.get_host(), len, 0);
				std::string alias = payload.alias();
				if (alias.empty())
					alias = payload.command();
				packet.code = nscapi::protobuf::functions::gbp_to_nagios_status(payload.result());
				packet.result = nscapi::protobuf::functions::query_data_to_nagios_string(payload, len);
				if (alias != "host_check")
					packet.service = alias;
				NSC_TRACE_ENABLED() {
					NSC_TRACE_MSG("Scheduling packet: " + packet.to_string());
				}
				list.push_back(packet);
			}

			send(response_message.add_payload(), con, list);
			return true;
		}

		bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
			return false;
		}

		bool metrics(client::destination_container sender, client::destination_container target, const Plugin::MetricsMessage &request_message) {
			return false;
		}


		void send(Plugin::SubmitResponseMessage::Response *payload, const connection_data con, const std::list<nsca::packet> packets) {
			try {
				socket_helpers::client::client<nsca::client::protocol<client_handler> > client(con, boost::make_shared<client_handler>(con));
				NSC_TRACE_ENABLED() {
					NSC_TRACE_MSG("Connecting to: " + con.to_string());
				}
				client.connect();

				BOOST_FOREACH(const nsca::packet &packet, packets) {
					client.process_request(packet);
				}
				client.shutdown();
				nscapi::protobuf::functions::set_response_good(*payload, "Submission successful");
			} catch (const nscp::encryption::encryption_exception &e) {
				nscapi::protobuf::functions::set_response_bad(*payload, "NSCA error: " + utf8::utf8_from_native(e.what()));
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