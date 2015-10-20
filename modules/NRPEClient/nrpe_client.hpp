#pragma once

#include <nscapi/nscapi_protobuf.hpp>
#include <client/command_line_parser.hpp>
#include <nrpe/packet.hpp>
#include <nrpe/client/nrpe_client_protocol.hpp>
#include <socket/client.hpp>


namespace nrpe_client {


	struct connection_data : public socket_helpers::connection_info {
		int buffer_length;
		boost::shared_ptr<socket_helpers::client::client_handler> handler;

		connection_data(client::destination_container source, client::destination_container target, boost::shared_ptr<socket_helpers::client::client_handler> handler) : buffer_length(0), handler(handler) {
			address = target.address.host;
			port_ = target.address.get_port_string("5666");

			ssl.enabled = target.get_bool_data("ssl", true);
			if (target.get_bool_data("insecure", false)) {
				ssl.certificate = target.get_string_data("certificate");
				ssl.certificate_key = target.get_string_data("certificate key");
				ssl.certificate_key_format = target.get_string_data("certificate format");
				ssl.ca_path = target.get_string_data("ca");
				ssl.allowed_ciphers = target.get_string_data("allowed ciphers", "ADH");
				ssl.dh_key = target.get_string_data("dh", "${certificate-path}/nrpe_dh_512.pem");
				ssl.verify_mode = target.get_string_data("verify mode");
				
			} else {
				ssl.certificate = target.get_string_data("certificate", "${certificate-path}/certificate.pem");
				ssl.certificate_key = target.get_string_data("certificate key");
				ssl.certificate_key_format = target.get_string_data("certificate format", "PEM");
				ssl.ca_path = target.get_string_data("ca");
				ssl.allowed_ciphers = target.get_string_data("allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
				ssl.dh_key = target.get_string_data("dh");
				ssl.verify_mode = target.get_string_data("verify mode", "none");
			}
			if (!ssl.dh_key.empty())
				ssl.dh_key = handler->expand_path(ssl.dh_key);
			if (!ssl.certificate.empty())
				ssl.certificate = handler->expand_path(ssl.certificate);
			if (!ssl.certificate_key.empty())
				ssl.certificate_key = handler->expand_path(ssl.certificate_key);

			timeout = target.timeout;
			retry = target.retry;
			buffer_length = target.get_int_data("payload length", 1024);

			if (target.has_data("no ssl"))
				ssl.enabled = !target.get_bool_data("no ssl");
			if (target.has_data("use ssl"))
				ssl.enabled = target.get_bool_data("use ssl");


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

	template<class TCoreHandler=client_handler>
	struct nrpe_client_handler : public client::handler_interface {

		boost::shared_ptr<TCoreHandler> handler_;
		nrpe_client_handler() : handler_(boost::make_shared<TCoreHandler>()) {}

		std::string get_command(std::string alias, std::string command = "") {
			if (!alias.empty())
				return alias; 
			if (!command.empty())
				return command; 
			return "_NRPE_CHECK";
		}


		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nrpe_client::connection_data con(sender, target, handler_);
			
			handler_->log_debug(__FILE__, __LINE__, "Connecting to: " + con.to_string());

			BOOST_FOREACH(const std::string &e, con.validate()) {
				handler_->log_error(__FILE__, __LINE__, e);
			}
			

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			if (request_message.payload_size() == 0) {
				std::string command = get_command("");
				boost::tuple<int,std::string> ret = send(con, command);
				strEx::s::token rdata = strEx::s::getToken(ret.get<1>(), '|');
				nscapi::protobuf::functions::append_simple_query_response_payload(response_message.add_payload(), command, ret.get<0>(), rdata.first, rdata.second);
			} else {
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
			}
			return NSCAPI::isSuccess;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nrpe_client::connection_data con(sender, target, handler_);

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
			nrpe_client::connection_data con(sender, target, handler_);

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
				socket_helpers::client::client<nrpe::client::protocol> client(con, handler_);
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

