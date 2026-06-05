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

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <net/collectd/collectd_packet.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/metrics.hpp>

namespace collectd_client {

class udp_sender {
 public:
  // Multicast: bind the socket to a specific local interface (source_endpoint)
  // so the datagram leaves through it.
  udp_sender(boost::asio::io_context &io_service, boost::asio::ip::udp::endpoint source_endpoint, const boost::asio::ip::address &target_address,
             unsigned short target_port)
      : endpoint_(target_address, target_port), socket_(io_service, source_endpoint) {}

  // Unicast (or default-interface multicast): open a socket for the target's
  // protocol family and let the OS pick the source.
  udp_sender(boost::asio::io_context &io_service, const boost::asio::ip::address &target_address, unsigned short target_port)
      : endpoint_(target_address, target_port), socket_(io_service, endpoint_.protocol()) {}

  // Queue one datagram. The payload is owned by a shared_ptr captured in the
  // completion handler so it stays alive until the async send finishes — this
  // lets us queue several packets before a single io_context.run() drains them
  // (the previous single-member buffer was overwritten by the next send).
  void send_data(const std::string &data) {
    auto payload = std::make_shared<std::string>(data);
    socket_.async_send_to(boost::asio::buffer(*payload), endpoint_, [payload](const boost::system::error_code &, std::size_t) {});
  }

 private:
  boost::asio::ip::udp::endpoint endpoint_;
  boost::asio::ip::udp::socket socket_;
};

struct connection_data : public socket_helpers::connection_info {
  std::string sender_hostname;

  connection_data() {}

  connection_data(client::destination_container arguments, client::destination_container sender) {
    address = arguments.address.host;
    if (address.empty()) address = "239.192.74.66";
    port_ = arguments.address.get_port_string("25826");
    ssl.enabled = false;
    timeout = arguments.get_int_data("timeout", 30);
    retry = arguments.get_int_data("retries", 3);
    sender_hostname = sender.address.host;
    if (sender.has_data("host")) sender_hostname = sender.get_string_data("host");
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
  std::string expand_path(std::string path) { return GET_CORE()->expand_path(path); }
};

// A single mapping entry: a collectd key (e.g. "cpu-total/cpu-user") and the
// expression that resolves its value(s) (e.g. "derive:system.cpu.total.user").
typedef std::list<std::pair<std::string, std::string> > mapping_list;

struct collectd_client_handler : public client::handler_interface {
  // Built-in mapping used when none is configured in settings. Kept identical
  // to the historical hard-coded mapping so default behaviour is preserved.
  static mapping_list default_variables() {
    mapping_list m;
    m.push_back(std::make_pair("diskid", "system.metrics.pdh.disk_queue_length.disk_queue_length_(.*)$"));
    m.push_back(std::make_pair("core", "system.cpu.core (.*).user"));
    return m;
  }
  static mapping_list default_metrics() {
    mapping_list m;
    m.push_back(std::make_pair("memory-/memory-available", "gauge:system.mem.physical.avail"));
    m.push_back(std::make_pair("disk-${diskid}/queue_length", "gauge:system.metrics.pdh.disk_queue_length.disk_queue_length_${diskid}"));
    m.push_back(std::make_pair("processes-/ps_count", "gauge:system.metrics.procs.procs,system.metrics.procs.threads"));
    m.push_back(std::make_pair("memory-pagefile/memory-used", "gauge:system.mem.commited.used"));
    m.push_back(std::make_pair("memory-pagefile/memory-free", "gauge:system.mem.commited.free"));
    m.push_back(std::make_pair("cpu-${core}/cpu-user", "derive:system.cpu.core ${core}.user"));
    m.push_back(std::make_pair("cpu-${core}/cpu-system", "derive:system.cpu.core ${core}.kernel"));
    m.push_back(std::make_pair("cpu-${core}/cpu-idle", "derive:system.cpu.core ${core}.idle"));
    m.push_back(std::make_pair("cpu-total/cpu-user", "derive:system.cpu.total.user"));
    m.push_back(std::make_pair("cpu-total/cpu-system", "derive:system.cpu.total.kernel"));
    m.push_back(std::make_pair("cpu-total/cpu-idle", "derive:system.cpu.total.idle"));
    return m;
  }

  collectd_client_handler() : interval_seconds_(10) {}

  // Configuration (populated from settings; empty => defaults are used).
  void add_variable(const std::string &key, const std::string &value) { variables_.push_back(std::make_pair(key, value)); }
  void add_metric(const std::string &key, const std::string &value) { metrics_.push_back(std::make_pair(key, value)); }
  void set_interval(unsigned long long seconds) { interval_seconds_ = seconds; }

  bool query(client::destination_container sender, client::destination_container target, const PB::Commands::QueryRequestMessage &request_message,
             PB::Commands::QueryResponseMessage &response_message) {
    return false;
  }

  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
              PB::Commands::SubmitResponseMessage &response_message) {
    // collectd is a metrics protocol; it has no concept of a passive check
    // result. Report that clearly instead of emitting empty datagrams.
    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_message.header());
    nscapi::protobuf::functions::set_response_bad(*response_message.add_payload(),
                                                  "The collectd client only forwards metrics; submitting passive check results is not supported.");
    return true;
  }

  bool exec(client::destination_container sender, client::destination_container target, const PB::Commands::ExecuteRequestMessage &request_message,
            PB::Commands::ExecuteResponseMessage &response_message) {
    return false;
  }

  void flatten_metrics(collectd::collectd_builder &builder, const PB::Metrics::MetricsBundle &b, std::string path) {
    std::string mypath;
    if (!path.empty()) mypath = path + ".";
    mypath += b.key();
    for (const PB::Metrics::MetricsBundle &b2 : b.children()) {
      flatten_metrics(builder, b2, mypath);
    }
    for (const PB::Metrics::Metric &v : b.value()) {
      if (v.has_gauge_value()) {
        builder.set_metric(mypath + "." + v.key(), str::xtos(v.gauge_value().value()));
      } else if (v.has_counter_value()) {
        builder.set_metric(mypath + "." + v.key(), str::xtos(v.counter_value().value()));
      } else if (v.has_untyped_value()) {
        builder.set_metric(mypath + "." + v.key(), str::xtos(v.untyped_value().value()));
      } else if (v.has_string_value()) {
        builder.set_metric(mypath + "." + v.key(), v.string_value().value());
      } else {
        NSC_LOG_ERROR_EX("Unsupported metrics type for: " + mypath + "." + v.key());
      }
    }
  }

  void set_metrics(collectd::collectd_builder &builder, const PB::Metrics::MetricsMessage &data) {
    for (const PB::Metrics::MetricsMessage::Response &p : data.payload()) {
      for (const PB::Metrics::MetricsBundle &b : p.bundles()) {
        flatten_metrics(builder, b, "");
      }
    }
  }

  bool metrics(client::destination_container sender, client::destination_container target, const PB::Metrics::MetricsMessage &request_message) {
    collectd::collectd_builder builder;
    set_metrics(builder, request_message);

    boost::posix_time::ptime const time_epoch(boost::gregorian::date(1970, 1, 1));
    const unsigned long long now_seconds = (boost::posix_time::microsec_clock::universal_time() - time_epoch).total_seconds();

    // collectd "high-resolution" time/interval are in units of 2^-30 seconds.
    builder.set_time(now_seconds << 30, interval_seconds_ << 30);
    builder.set_host(sender.get_host());

    // Variables must be expanded (against the flattened metric names) before
    // the metric templates that reference them are added.
    const mapping_list &variables = variables_.empty() ? default_variables_ : variables_;
    const mapping_list &metrics = metrics_.empty() ? default_metrics_ : metrics_;
    for (const auto &v : variables) builder.add_variable(v.first, v.second);
    for (const auto &m : metrics) builder.add_metric(m.first, m.second);

    collectd::collectd_builder::packet_list packets;
    builder.render(packets);
    connection_data con(target, sender);
    send(con, packets);
    return true;
  }

  void send(const connection_data &target, const collectd::collectd_builder::packet_list &packets) {
    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Sending " + str::xtos(packets.size()) + " packets to: " + target.to_string()); }
    try {
      boost::asio::io_context io_service;
      const boost::asio::ip::address target_address = boost::asio::ip::make_address(target.get_address());
      const unsigned short target_port = target.get_int_port();

      const bool is_multicast = (target_address.is_v4() && target_address.to_v4().is_multicast()) ||
                                (target_address.is_v6() && target_address.to_v6().is_multicast());

      // Build the set of sockets to send through. For unicast that is a single
      // OS-routed socket; for multicast we send out every local interface of
      // the matching address family.
      std::list<std::shared_ptr<udp_sender> > senders;
      if (is_multicast) {
        boost::asio::ip::udp::resolver resolver(io_service);
        for (const auto &entry : resolver.resolve(boost::asio::ip::host_name(), "")) {
          const boost::asio::ip::address &local = entry.endpoint().address();
          if (local.is_v4() == target_address.is_v4()) {
            senders.push_back(std::make_shared<udp_sender>(io_service, entry.endpoint(), target_address, target_port));
          }
        }
      }
      if (senders.empty()) {
        // Unicast, or multicast with no enumerable matching interface: fall
        // back to a default-bound socket for the target's address family.
        senders.push_back(std::make_shared<udp_sender>(io_service, target_address, target_port));
      }

      for (const collectd::packet &p : packets) {
        for (const std::shared_ptr<udp_sender> &s : senders) {
          s->send_data(p.get_buffer());
        }
      }
      io_service.run();
    } catch (std::exception &e) {
      NSC_LOG_ERROR_STD(utf8::utf8_from_native(e.what()));
    }
  }

 private:
  mapping_list variables_;
  mapping_list metrics_;
  unsigned long long interval_seconds_;
  const mapping_list default_variables_ = default_variables();
  const mapping_list default_metrics_ = default_metrics();
};
}  // namespace collectd_client