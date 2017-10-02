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

#include "smtp.hpp"

#include <socket/socket_helpers.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

namespace smtp_client {
	struct connection_data : public socket_helpers::connection_info {
		typedef socket_helpers::connection_info parent;
		std::string recipient_str;
		std::string sender_hostname;
		std::string template_string;

		connection_data(client::destination_container arguments, client::destination_container sender) {
			address = arguments.address.host;
			port_ = arguments.address.get_port_string("25");
			timeout = arguments.get_int_data("timeout", 30);
			retry = arguments.get_int_data("retry", 3);

			recipient_str = arguments.get_string_data("recipient");
			template_string = arguments.get_string_data("template");

			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << parent::to_string();
			ss << ", recipient: " << recipient_str;
			ss << ", sender: " << sender_hostname;
			ss << ", template: " << template_string;
			return ss.str();
		}
	};

	struct g_data {
		std::string path;
		std::string value;
	};

	struct smtp_client_handler : public client::handler_interface {
		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			return false;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			connection_data con(sender, target);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			std::list<g_data> list;

			BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response &p, request_message.payload()) {
				boost::asio::io_service io_service;
				boost::shared_ptr<smtp::client::smtp_client> client(new smtp::client::smtp_client(io_service));
				std::list<std::string> recipients;
				std::string message = con.template_string;

				str::utils::replace(message, "%message%", nscapi::protobuf::functions::query_data_to_nagios_string(p, nscapi::protobuf::functions::no_truncation));
				recipients.push_back(con.recipient_str);
				client->send_mail(con.sender_hostname, recipients, "Hello world\n");
				io_service.run();
				nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), "TODO", true, "Message send successfully");
			}
			return true;
		}

		bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
			return false;
		}

		bool metrics(client::destination_container sender, client::destination_container target, const Plugin::MetricsMessage &request_message) {
			return false;
		}


		void send(Plugin::SubmitResponseMessage::Response *payload, connection_data con, const std::list<g_data> &data) {
			try {
				boost::asio::io_service io_service;
				boost::asio::ip::tcp::resolver resolver(io_service);
				boost::asio::ip::tcp::resolver::query query(con.get_address(), con.get_port());
				boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
				boost::asio::ip::tcp::resolver::iterator end;

				boost::asio::ip::tcp::socket socket(io_service);
				boost::system::error_code error = boost::asio::error::host_not_found;
				while (error && endpoint_iterator != end) {
					socket.close();
					socket.connect(*endpoint_iterator++, error);
				}
				if (error)
					throw boost::system::system_error(error);

				boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));
				boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
				boost::posix_time::time_duration diff = now - time_t_epoch;
				int x = diff.total_seconds();

				BOOST_FOREACH(const g_data &d, data) {
					std::string msg = d.path + " " + d.value + " " + boost::lexical_cast<std::string>(x) + "\n";
					socket.send(boost::asio::buffer(msg));
				}
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