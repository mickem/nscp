#pragma once

#include <socket/socket_helpers.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

namespace graphite_client {
	struct connection_data : public socket_helpers::connection_info {
		std::string path;
		std::string sender_hostname;

		connection_data(client::destination_container arguments, client::destination_container sender) {
			address = arguments.address.host;
			port_ = arguments.address.get_port_string("2003");
			timeout = arguments.get_int_data("timeout", 30);
			retry = arguments.get_int_data("retry", 3);
			path = arguments.get_string_data("path");
			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			return ss.str();
		}
	};

	struct g_data {
		std::string path;
		std::string value;
	};

	struct graphite_client_handler : public client::handler_interface {
		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			return false;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			connection_data con(sender, target);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);
			std::string path = con.path;
			strEx::replace(path, "${hostname}", con.sender_hostname);

			std::list<g_data> list;

			BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response &p, request_message.payload()) {
				std::string tmp_path = path;
				strEx::replace(tmp_path, "${check_alias}", p.alias());

				BOOST_FOREACH(const ::Plugin::QueryResponseMessage::Response::Line &l, p.lines()) {
					BOOST_FOREACH(const ::Plugin::Common_PerformanceData &perf, l.perf()) {
						g_data d;
						double value = 0.0;
						d.path = tmp_path;
						strEx::replace(d.path, "${perf_alias}", perf.alias());
						if (perf.has_float_value()) {
							if (perf.float_value().has_value())
								value = perf.float_value().value();
							else
								NSC_LOG_ERROR("Unsopported performance data (no value)");
						} else if (perf.has_int_value()) {
							if (perf.int_value().has_value())
								value = static_cast<double>(perf.int_value().value());
							else
								NSC_LOG_ERROR("Unsopported performance data (no value)");
						} else {
							NSC_LOG_ERROR("Unsopported performance data type: " + perf.alias());
							continue;
						}
						strEx::replace(d.path, " ", "_");
						d.value = strEx::s::xtos(value);
						list.push_back(d);
					}
				}
			}
			send(response_message.add_payload(), con, list);
			return true;
		}

		bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
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