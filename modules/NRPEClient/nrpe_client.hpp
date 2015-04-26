#pragma once

#include <nscapi/nscapi_protobuf.hpp>
#include <client/command_line_parser.hpp>
#include <nrpe/packet.hpp>
#include <nrpe/client/nrpe_client_protocol.hpp>
#include <socket/client.hpp>


namespace nrpe_client {


	struct connection_data : public socket_helpers::connection_info {
		int buffer_length;

		connection_data(client::destination_container arguments, client::destination_container sender) {
			address = arguments.address.host;
			port_ = arguments.address.get_port_string("5666");
			ssl.enabled = arguments.get_bool_data("ssl");
			ssl.certificate = arguments.get_string_data("certificate");
			ssl.certificate_key = arguments.get_string_data("certificate key");
			ssl.certificate_key_format = arguments.get_string_data("certificate format");
			ssl.ca_path = arguments.get_string_data("ca");
			ssl.allowed_ciphers = arguments.get_string_data("allowed ciphers");
			ssl.dh_key = arguments.get_string_data("dh");
			ssl.verify_mode = arguments.get_string_data("verify mode");
			timeout = arguments.get_int_data("timeout", 30);
			retry = arguments.get_int_data("retry", 3);
			buffer_length = arguments.get_int_data("payload length", 1024);

			if (arguments.has_data("no ssl"))
				ssl.enabled = !arguments.get_bool_data("no ssl");
			if (arguments.has_data("use ssl"))
				ssl.enabled = arguments.get_bool_data("use ssl");


		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			ss << ", buffer_length: " << buffer_length;
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


	struct nrpe_client_handler : public client::handler_interface {

		std::string get_command(std::string alias, std::string command = "") {
			if (!alias.empty())
				return alias; 
			if (!command.empty())
				return command; 
			return "_NRPE_CHECK";
		}


		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nrpe_client::connection_data con(sender, target);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			for (int i=0;i<request_message.payload_size();i++) {
				std::string command = get_command(request_message.payload(i).alias(), request_message.payload(i).command());
				std::string data = command;
				for (int a=0;a<request_message.payload(i).arguments_size();a++) {
					data += "!" + request_message.payload(i).arguments(a);
				}
				boost::tuple<int,std::string> ret = send(con, data);
				strEx::s::token rdata = strEx::s::getToken(ret.get<1>(), '|');
				nscapi::protobuf::functions::append_simple_query_response_payload(response_message.add_payload(), command, ret.get<0>(), rdata.first, rdata.second);
			}
			return NSCAPI::isSuccess;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nrpe_client::connection_data con(sender, target);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			for (int i=0;i<request_message.payload_size();++i) {
				std::string command = get_command(request_message.payload(i).alias(), request_message.payload(i).command());
				std::string data = command;
				for (int a=0;a<request_message.payload(i).arguments_size();a++) {
					data += "!" + request_message.payload(i).arguments(i);
				}
				boost::tuple<int,std::string> ret = send(con, data);
				nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), command, ret.get<0>(), ret.get<1>());
			}
			return NSCAPI::isSuccess;
		}

		bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nrpe_client::connection_data con(sender, target);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			for (int i=0;i<request_message.payload_size();i++) {
				std::string command = get_command(request_message.payload(i).command());
				std::string data = command;
				for (int a=0;a<request_message.payload(i).arguments_size();a++)
					data += "!" + request_message.payload(i).arguments(a);
				boost::tuple<int,std::string> ret = send(con, data);
				nscapi::protobuf::functions::append_simple_exec_response_payload(response_message.add_payload(), command, ret.get<0>(), ret.get<1>());
			}
			return NSCAPI::isSuccess;
		}

		//////////////////////////////////////////////////////////////////////////
		// Protocol implementations
		//

		boost::tuple<int,std::string> send(nrpe_client::connection_data con, const std::string data) {
			try {
#ifndef USE_SSL
				if (con.ssl.enabled)
					return boost::make_tuple(NSCAPI::returnUNKNOWN, "SSL support not available (compiled without USE_SSL)");
#endif
				nrpe::packet packet = nrpe::packet::make_request(data, con.buffer_length);
				socket_helpers::client::client<nrpe::client::protocol> client(con, boost::make_shared<client_handler>());
				client.connect();
				std::list<nrpe::packet> responses = client.process_request(packet);
				client.shutdown();
				int result = NSCAPI::returnUNKNOWN;
				std::string payload;
				if (responses.size() > 0)
					result = static_cast<int>(responses.front().getResult());
				BOOST_FOREACH(const nrpe::packet &p, responses) {
					payload += p.getPayload();
				}
				return boost::make_tuple(result, payload);
			} catch (nrpe::nrpe_exception &e) {
				return boost::make_tuple(NSCAPI::returnUNKNOWN, std::string("NRPE Packet error: ") + e.what());
			} catch (std::runtime_error &e) {
				return boost::make_tuple(NSCAPI::returnUNKNOWN, "Socket error: " + utf8::utf8_from_native(e.what()));
			} catch (std::exception &e) {
				return boost::make_tuple(NSCAPI::returnUNKNOWN, "Error: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				return boost::make_tuple(NSCAPI::returnUNKNOWN, "Unknown error -- REPORT THIS!");
			}
		}

	};

}

