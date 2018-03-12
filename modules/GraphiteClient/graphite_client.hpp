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

#include <socket/socket_helpers.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_protobuf_nagios.hpp>

#include <boost/tuple/tuple.hpp>

namespace graphite_client {
	struct connection_data : public socket_helpers::connection_info {
		std::string ppath;
		std::string spath;
		std::string mpath;
		std::string sender_hostname;
		bool send_perf;
		bool send_status;

		connection_data(client::destination_container sender, client::destination_container target) {
			address = target.address.host;
			port_ = target.address.get_port_string("2003");
			timeout = target.get_int_data("timeout", 30);
			retry = target.get_int_data("retry", 3);
			ppath = target.get_string_data("perf path");
			spath = target.get_string_data("status path");
			send_perf = target.get_bool_data("send perfdata");
			send_status = target.get_bool_data("send status");
			mpath = target.get_string_data("metric path");
			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
			else 
				sender_hostname = sender.get_host();
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

	std::string fix_graphite_string(const std::string &s) {
		std::string sc = s;
		str::utils::replace(sc, " ", "_");
		str::utils::replace(sc, "\\", "_");
		str::utils::replace(sc, "[", "_");
		str::utils::replace(sc, "]", "_");
		str::utils::replace(sc, "(", "_");
		str::utils::replace(sc, ")", "_");
		str::utils::replace(sc, "%", "percent");
		return sc;
	}
	struct graphite_client_handler : public client::handler_interface {
		bool query(client::destination_container sender, client::destination_container target, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			return false;
		}

		bool submit(client::destination_container sender, client::destination_container target, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			connection_data con(sender, target);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);
			std::string ppath = con.ppath;
			std::string spath = con.spath;
			str::utils::replace(ppath, "${hostname}", con.sender_hostname);
			str::utils::replace(spath, "${hostname}", con.sender_hostname);

			std::list<g_data> list;

			BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response &p, request_message.payload()) {
				std::string tmp_path = ppath;
				str::utils::replace(tmp_path, "${check_alias}", p.alias());

				if (con.send_perf) {
					BOOST_FOREACH(const ::Plugin::QueryResponseMessage::Response::Line &l, p.lines()) {
						BOOST_FOREACH(const ::Plugin::Common_PerformanceData &perf, l.perf()) {
							g_data d;
							d.path = tmp_path;
							str::utils::replace(d.path, "${perf_alias}", perf.alias());
							d.value = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
							d.path = fix_graphite_string(d.path);
							list.push_back(d);
						}
					}
				}
				if (con.send_status) {
					g_data d;
					d.path = spath;
					str::utils::replace(d.path, "${check_alias}", p.alias());
					str::utils::replace(d.path, " ", "_");
					d.value = str::xtos(nscapi::protobuf::functions::gbp_to_nagios_status(p.result()));
					list.push_back(d);
				}

			}
			if (list.empty()) {
				nscapi::protobuf::functions::set_response_bad(*response_message.add_payload(), std::string("No performance data to send"));
				return true;
			}
			boost::tuple<int, std::string> ret = send(con, list);
			if (ret.get<0>())
				nscapi::protobuf::functions::set_response_good(*response_message.add_payload(), ret.get<1>());
			else
				nscapi::protobuf::functions::set_response_bad(*response_message.add_payload(), ret.get<1>());

			return true;
		}

		bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
			return false;
		}


		void push_metrics(std::list<graphite_client::g_data> &list, const Plugin::Common::MetricsBundle &b, std::string path, std::string mpath) {
			std::string mypath;
			if (!path.empty())
				mypath = path + ".";
			mypath += b.key();
			BOOST_FOREACH(const Plugin::Common::MetricsBundle &b2, b.children()) {
				push_metrics(list, b2, mypath, mpath);
			}
			BOOST_FOREACH(const Plugin::Common::Metric &v, b.value()) {
				graphite_client::g_data d;
				const ::Plugin::Common_AnyDataType &value = v.value();
				d.path = mpath;
				str::utils::replace(d.path, "${metric}", mypath + "." + v.key());
				d.path = fix_graphite_string(d.path);
				if (value.has_int_data()) {
					d.value = str::xtos(v.value().int_data());
					list.push_back(d);
				} else if (value.has_float_data()) {
					d.value = str::xtos(v.value().float_data());
					list.push_back(d);
				}
			}
		}

		bool metrics(client::destination_container sender, client::destination_container target, const Plugin::MetricsMessage &request_message) {
			std::list<graphite_client::g_data> list;
			connection_data con(sender, target);
			std::string mpath = con.mpath;
			str::utils::replace(mpath, "${hostname}", con.sender_hostname);

			BOOST_FOREACH(const Plugin::MetricsMessage::Response &r, request_message.payload()) {
				BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, r.bundles()) {
					push_metrics(list, b, "", mpath);
				}
			}
			send(con, list);
			return true;
		}


		boost::tuple<bool, std::string> send(connection_data con, const std::list<g_data> &data) {
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
				return boost::make_tuple(true, "Data presumably sent successfully");
			} catch (const std::runtime_error &e) {
				return boost::make_tuple(false, "Socket error: " + utf8::utf8_from_native(e.what()));
			} catch (const std::exception &e) {
				return boost::make_tuple(false, "Error: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				return boost::make_tuple(false, "Unknown error -- REPORT THIS!");
			}
		}
	};
}