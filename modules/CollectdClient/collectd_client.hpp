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

#include <collectd/collectd_packet.hpp>

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>


namespace collectd_client {

	class udp_sender {
	public:
		udp_sender(boost::asio::io_service& io_service, boost::asio::ip::udp::endpoint source_endpoint,
			const boost::asio::ip::address& multicast_address, unsigned short multicast_port)
			: endpoint_(multicast_address, multicast_port)
			, socket_(io_service, source_endpoint)
			//, timer_(io_service)
		{}

		udp_sender(boost::asio::io_service& io_service,
			const boost::asio::ip::address& multicast_address, unsigned short multicast_port)
			: endpoint_(multicast_address, multicast_port)
			, socket_(io_service, endpoint_.protocol())
			//, timer_(io_service)
		{}

		void send_data(const std::string &data) {
			payload = data;
			socket_.async_send_to(
				boost::asio::buffer(payload), endpoint_,
				boost::bind(&udp_sender::handle_send_to, this,
					boost::asio::placeholders::error));
		}

		void handle_send_to(const boost::system::error_code& error) {}

		// 	void handle_timeout(const boost::system::error_code& error) {
		// 		if (!error) {
		// 			socket_.async_send_to(
		// 				boost::asio::buffer(message_), endpoint_,
		// 				boost::bind(&udp_sender::handle_send_to, this,
		// 					boost::asio::placeholders::error));
		// 		}
		// 	}

	private:
		boost::asio::ip::udp::endpoint endpoint_;
		boost::asio::ip::udp::socket socket_;
		//boost::asio::deadline_timer timer_;
		std::string payload;
	};


	struct connection_data : public socket_helpers::connection_info {
		std::string sender_hostname;

		connection_data() {}

		connection_data(client::destination_container arguments, client::destination_container sender) {
			address = arguments.address.host;
			if (address.empty())
				address = "239.192.74.66";
			port_ = arguments.address.get_port_string("25826");
			ssl.enabled = false;
			timeout = arguments.get_int_data("timeout", 30);
			retry = arguments.get_int_data("retries", 3);
			sender_hostname = sender.address.host;
			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			ss << ", sender_hostname: " << sender_hostname;
			return ss.str();
		}
	};


	//////////////////////////////////////////////////////////////////////////
	// Protocol implementations
	//
	struct client_handler : public socket_helpers::client::client_handler {
		client_handler(const connection_data &con) {}
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

			send(con, list);
			return true;
		}

		bool exec(client::destination_container sender, client::destination_container target, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
			return false;
		}


		void flatten_metrics(collectd::collectd_builder &builder, const Plugin::Common::MetricsBundle &b, std::string path) {
			std::string mypath;
			if (!path.empty())
				mypath = path + ".";
			mypath += b.key();
			BOOST_FOREACH(const Plugin::Common::MetricsBundle &b2, b.children()) {
				flatten_metrics(builder, b2, mypath);
			}
			BOOST_FOREACH(const Plugin::Common::Metric &v, b.value()) {
				const ::Plugin::Common_AnyDataType &value = v.value();
				if (value.has_int_data()) {
					builder.set_metric(mypath + "." + v.key(), str::xtos(v.value().int_data()));
				} else if (value.has_float_data()) {
					builder.set_metric(mypath + "." + v.key(), str::xtos(v.value().float_data()));
				} else if (value.has_string_data()) {
					builder.set_metric(mypath + "." + v.key(), v.value().string_data());
				} else {
					NSC_LOG_ERROR_EX("Unknown metrics type");
				}
			}
		}


		void set_metrics(collectd::collectd_builder &builder, const Plugin::MetricsMessage &data) {
			BOOST_FOREACH(const Plugin::MetricsMessage::Response &p, data.payload()) {
				BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, p.bundles()) {
					flatten_metrics(builder, b, "");
				}
			}
		}

		bool metrics(client::destination_container sender, client::destination_container target, const Plugin::MetricsMessage &request_message) {

			collectd::collectd_builder builder;
			set_metrics(builder, request_message);

			boost::posix_time::ptime const time_epoch(boost::gregorian::date(1970, 1, 1));

			unsigned long long ms = (boost::posix_time::microsec_clock::universal_time() - time_epoch).total_seconds();
			unsigned long long int_ms = 5;
			builder.set_time(ms << 30, int_ms << 30);
			builder.set_host(sender.get_host());

			builder.add_variable("diskid", "system.metrics.pdh.disk_queue_length.disk_queue_length_(.*)$");
			builder.add_variable("core", "system.cpu.core (.*).user");

			builder.add_metric("memory-/memory-available", "gauge:system.mem.physical.avail");
			//builder.add_variable("memory-/memory-pool_nonpaged", "gauge:0");
			//builder.add_variable("memory-/memory-pool_paged", "gauge:0");
			//builder.add_variable("memory-/memory-system_cache", "gauge:0");
			//builder.add_variable("memory-/memory-system_code", "gauge:0"); // 0?
			//builder.add_variable("memory-/memory-system_driver", "gauge:0");

			//builder.add_variable("interface-${nic}/if_octets-", "derive:0,0");
			//builder.add_variable("interface-${nic}/if_packets-", "derive:0,0");

			//builder.add_variable("disk-${diskid}/disk_octets", "derive:0,0");
			//builder.add_variable("disk-${diskid}/disk_ops", "derive:0,0");
			builder.add_metric("disk-${diskid}/queue_length", "gauge:system.metrics.pdh.disk_queue_length.disk_queue_length_${diskid}");

			builder.add_metric("processes-/ps_count", "gauge:system.metrics.procs.procs,system.metrics.procs.threads");


			builder.add_metric("memory-pagefile/memory-used", "gauge:system.mem.commited.used");
			builder.add_metric("memory-pagefile/memory-free", "gauge:system.mem.commited.free");

			//builder.add_variable("df-${drive}/df_complex-free", "gauge:0");
			//builder.add_variable("df-${drive}/df_complex-reserved", "gauge:0");
			//builder.add_variable("df-${drive}/df_complex-used", "gauge:0");

			//builder.add_variable("memory-/working_set-available", "gauge:0");
			//builder.add_variable("memory-/working_set-pool_nonpaged", "gauge:0");
			//builder.add_variable("memory-/working_set-pool_paged", "gauge:0");

			builder.add_metric("cpu-${core}/cpu-user", "derive:system.cpu.core ${core}.user");
			builder.add_metric("cpu-${core}/cpu-system", "derive:system.cpu.core ${core}.kernel");
			//builder.add_metric("cpu-${core}/cpu-interrupt", "derive:0");
			builder.add_metric("cpu-${core}/cpu-idle", "derive:system.cpu.core ${core}.idle");

			builder.add_metric("cpu-total/cpu-user", "derive:system.cpu.total.user");
			builder.add_metric("cpu-total/cpu-system", "derive:system.cpu.total.kernel");
			//builder.add_metric("cpu-total/cpu-interrupt", "derive:0");
			builder.add_metric("cpu-total/cpu-idle", "derive:system.cpu.total.idle");

			//NSC_DEBUG_MSG("--->" + builder.to_string());
			collectd::collectd_builder::packet_list packets;

			builder.render(packets);
			connection_data con(target, sender);
			send(con, packets);
			return true;
		}


		void send(const connection_data target, const collectd::collectd_builder::packet_list &packets) {
			NSC_TRACE_ENABLED() {
				NSC_TRACE_MSG("Sending " + str::xtos(packets.size()) + " packets to: " + target.to_string());
			}
			BOOST_FOREACH(const collectd::packet &p, packets) {
				try {
					boost::asio::io_service io_service;
					std::list<boost::shared_ptr<udp_sender>> senders;

					boost::asio::ip::address target_address = boost::asio::ip::address::from_string(target.get_address());

					boost::asio::ip::udp::resolver resolver(io_service);
					boost::asio::ip::udp::resolver::query query(boost::asio::ip::host_name(), "");
					boost::asio::ip::udp::resolver::iterator endpoint_iterator = resolver.resolve(query);
					boost::asio::ip::udp::resolver::iterator end;
					bool is_multicast = false;
					if (target_address.is_v4()) {
						is_multicast = target_address.to_v4().is_multicast();
					}
#if BOOST_VERSION >= 105300
					else if (target_address.is_v6()) {
						is_multicast = target_address.to_v6().is_multicast();
					}
#endif

					if (is_multicast) {
						while (endpoint_iterator != end) {
							std::string ss = endpoint_iterator->endpoint().address().to_string();
							if (target_address.is_v4() && endpoint_iterator->endpoint().address().is_v4()) {
								boost::shared_ptr<udp_sender> s = boost::make_shared<udp_sender>(io_service, endpoint_iterator->endpoint(), target_address, target.get_int_port());
								senders.push_back(s);
								s->send_data(p.get_buffer());
								io_service.run();
							}
							endpoint_iterator++;
						}
					} else {
						boost::shared_ptr<udp_sender> s = boost::make_shared<udp_sender>(io_service, target_address, target.get_int_port());
						senders.push_back(s);
						s->send_data(p.get_buffer());
						io_service.run();

					}

					senders.clear();

				} catch (std::exception& e) {
					NSC_LOG_ERROR_STD(utf8::utf8_from_native(e.what()));
				}
			}
		}
	};
}