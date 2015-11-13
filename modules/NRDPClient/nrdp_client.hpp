#pragma once

#include <socket/socket_helpers.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include "nrdp.hpp"
#include <http/client.hpp>

namespace nrdp_client {
	struct connection_data : public socket_helpers::connection_info {
		std::string token;

		std::string sender_hostname;

		connection_data(client::destination_container arguments, client::destination_container sender) {
			address = arguments.address.host;
			port_ = arguments.address.get_port_string("80");
			timeout = arguments.get_int_data("timeout", 30);
			token = arguments.get_string_data("token");
			retry = arguments.get_int_data("retry", 3);

			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			ss << ", port: " << port_;
			ss << ", timeout: " << timeout;
			ss << ", token: " << token;
			ss << ", sender: " << sender_hostname;
			return ss.str();
		}
	};

	struct g_data {
		std::string path;
		std::string value;
	};

	struct nrdp_client_handler : public client::handler_interface {
		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			return false;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			connection_data con(sender, target);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			nrdp::data nrdp_data;

			BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response &p, request_message.payload()) {
				std::string msg = nscapi::protobuf::functions::query_data_to_nagios_string(p);
				std::string alias = p.alias();
				if (alias.empty())
					alias = p.command();
				int result = nscapi::protobuf::functions::gbp_to_nagios_status(p.result());
				if (alias == "host_check")
					nrdp_data.add_host(con.sender_hostname, result, msg);
				else
					nrdp_data.add_service(con.sender_hostname, alias, result, msg);
			}
			send(response_message.add_payload(), con, nrdp_data);
			return true;
		}

		bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
			return false;
		}

		bool metrics(client::destination_container sender, client::destination_container target, const Plugin::MetricsMessage &request_message) {
			return false;
		}

		void send(Plugin::SubmitResponseMessage::Response *payload, connection_data con, const nrdp::data &nrdp_data) {
			try {
				NSC_DEBUG_MSG_STD("Connection details: " + con.to_string());
				http::client c;
				http::client::request_type request;
				request.add_default_headers();
				http::client::request_type::post_map_type post;
				post["token"] = con.token;
				post["XMLDATA"] = nrdp_data.render_request();
				post["cmd"] = "submitcheck";
				request.add_post_payload(post);
				NSC_DEBUG_MSG_STD(nrdp_data.render_request());
				NSC_DEBUG_MSG_STD(request.payload);
				http::client::response_type response = c.execute(con.get_address(), con.get_port(), "/nrdp/server/", request);
				NSC_DEBUG_MSG_STD(response.payload);
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