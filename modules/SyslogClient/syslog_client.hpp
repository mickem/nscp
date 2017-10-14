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

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <socket/socket_helpers.hpp>

#include <str/format.hpp>

#include <boost/asio.hpp>

namespace syslog_client {
	struct connection_data : public socket_helpers::connection_info {
		std::string severity;
		std::string facility;
		std::string tag_syntax;
		std::string message_syntax;
		std::string ok_severity, warn_severity, crit_severity, unknown_severity;

		typedef std::map<std::string, int> syslog_map;
		syslog_map facilities;
		syslog_map severities;

		std::string	parse_priority(std::string severity, std::string facility) {
			syslog_map::const_iterator cit1 = facilities.find(facility);
			if (cit1 == facilities.end()) {
				NSC_LOG_ERROR("Undefined facility: " + facility);
				return "<0>";
			}
			syslog_map::const_iterator cit2 = severities.find(severity);
			if (cit2 == severities.end()) {
				NSC_LOG_ERROR("Undefined severity: " + severity);
				return "<0>";
			}
			std::stringstream ss;
			ss << '<' << (cit1->second * 8 + cit2->second) << '>';
			return ss.str();
		}

		connection_data(client::destination_container arguments, client::destination_container sender) {
			address = arguments.address.host;
			port_ = arguments.address.get_port_string("514");
			timeout = arguments.get_int_data("timeout", 30);
			retry = arguments.get_int_data("retry", 3);
			severity = arguments.data["severity"];
			facility = arguments.data["facility"];
			tag_syntax = arguments.data["tag template"];
			message_syntax = arguments.data["message template"];

			ok_severity = arguments.data["ok severity"];
			warn_severity = arguments.data["warning severity"];
			crit_severity = arguments.data["critical severity"];
			unknown_severity = arguments.data["unknown severity"];

			facilities["kernel"] = 0;
			facilities["user"] = 1;
			facilities["mail"] = 2;
			facilities["system"] = 3;
			facilities["security"] = 4;
			facilities["internal"] = 5;
			facilities["printer"] = 6;
			facilities["news"] = 7;
			facilities["UUCP"] = 8;
			facilities["clock"] = 9;
			facilities["authorization"] = 10;
			facilities["FTP"] = 11;
			facilities["NTP"] = 12;
			facilities["audit"] = 13;
			facilities["alert"] = 14;
			facilities["clock"] = 15;
			facilities["local0"] = 16;
			facilities["local1"] = 17;
			facilities["local2"] = 18;
			facilities["local3"] = 19;
			facilities["local4"] = 20;
			facilities["local5"] = 21;
			facilities["local6"] = 22;
			facilities["local7"] = 23;
			severities["emergency"] = 0;
			severities["alert"] = 1;
			severities["critical"] = 2;
			severities["error"] = 3;
			severities["warning"] = 4;
			severities["notice"] = 5;
			severities["informational"] = 6;
			severities["debug"] = 7;
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			ss << ", severity: " << severity;
			ss << ", facility: " << facility;
			ss << ", tag_syntax: " << tag_syntax;
			ss << ", message_syntax: " << message_syntax;
			return ss.str();
		}
	};

	struct g_data {
		std::string path;
		std::string value;
	};

	struct syslog_client_handler : public client::handler_interface {
		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			return false;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			connection_data con(sender, target);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			std::list<std::string> messages;

			BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response &p, request_message.payload()) {
				boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
				std::string date = str::format::format_date(now, "%b %e %H:%M:%S");
				std::string tag = con.tag_syntax;
				std::string message = con.message_syntax;
				std::string nagios_msg = nscapi::protobuf::functions::query_data_to_nagios_string(p, nscapi::protobuf::functions::no_truncation);
				str::utils::replace(message, "%message%", nagios_msg);
				str::utils::replace(tag, "%message%", nagios_msg);

				std::string severity = con.severity;
				if (p.result() == ::Plugin::Common_ResultCode_OK)
					severity = con.ok_severity;
				if (p.result() == ::Plugin::Common_ResultCode_WARNING)
					severity = con.warn_severity;
				if (p.result() == ::Plugin::Common_ResultCode_CRITICAL)
					severity = con.crit_severity;
				if (p.result() == ::Plugin::Common_ResultCode_UNKNOWN)
					severity = con.unknown_severity;

				messages.push_back(con.parse_priority(severity, con.facility) + date + " " + tag + " " + message);
			}
			send(response_message.add_payload(), con, messages);
			return true;
		}

		bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
			return false;
		}

		bool metrics(client::destination_container sender, client::destination_container target, const Plugin::MetricsMessage &request_message) {
			return false;
		}


		void send(Plugin::SubmitResponseMessage::Response *payload, connection_data con, const std::list<std::string> &messages) {
			try {
				NSC_DEBUG_MSG_STD("Connection details: " + con.to_string());

				boost::asio::io_service io_service;
				boost::asio::ip::udp::resolver resolver(io_service);
				boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), con.address, con.get_port());
				boost::asio::ip::udp::endpoint receiver_endpoint = *resolver.resolve(query);

				boost::asio::ip::udp::socket socket(io_service);
				socket.open(boost::asio::ip::udp::v4());

				BOOST_FOREACH(const std::string msg, messages) {
					NSC_DEBUG_MSG_STD("Sending data: " + msg);
					socket.send_to(boost::asio::buffer(msg), receiver_endpoint);
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