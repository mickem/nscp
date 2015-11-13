#pragma once

#include <collectd/collectd_packet.hpp>
#include <collectd/client/collectd_client_protocol.hpp>

namespace collectd_client {
	struct connection_data : public socket_helpers::connection_info {
		std::string password;
		std::string sender_hostname;
		int buffer_length;
		int time_delta;
		std::string encoding;

		connection_data() {}

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
			encoding = arguments.get_string_data("encoding");
			std::string tmp = arguments.get_string_data("time offset");
			if (!tmp.empty())
				time_delta = strEx::stol_as_time_sec(arguments.get_string_data("time offset"));
			else
				time_delta = 0;
			sender_hostname = sender.address.host;
			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			ss << ", buffer_length: " << buffer_length;
			ss << ", time_delta: " << time_delta;
			ss << ", password: " << password;
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
		std::string password_;
		client_handler(const connection_data &con)
			: password_(con.password) {}
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
		std::string get_password() {
			return password_;
		}
		std::string expand_path(std::string path) {
			return GET_CORE()->expand_path(path);
		}
	};

	struct collectd_client_handler : public client::handler_interface {
		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			return false;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);
			connection_data con(target, sender);

			std::list<collectd::packet> list;
			for (int i = 0; i < request_message.payload_size(); ++i) {
				collectd::packet packet;
				//packet.add_string(0, "Hello WOrld");
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


		void send(Plugin::SubmitResponseMessage::Response *payload, const connection_data con, const std::list<collectd::packet> packets) {}
	};
}