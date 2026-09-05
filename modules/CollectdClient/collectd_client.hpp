// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <net/collectd/collectd_packet.hpp>
#include <net/collectd/collectd_sender.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/metrics.hpp>

namespace collectd_client {

struct connection_data : public socket_helpers::connection_info {
  std::string sender_hostname;
  // Which local interface(s) a multicast target's datagrams leave through; see
  // collectd::sender_config.
  std::string multicast_interfaces;

  connection_data() {}

  connection_data(client::destination_container arguments, client::destination_container sender) {
    address = arguments.address.host;
    if (address.empty()) address = "239.192.74.66";
    port_ = arguments.address.get_port_string("25826");
    ssl.enabled = false;
    // destination_container routes the well-known "timeout" key into a typed
    // field (see set_string_data), never into the free-form data map, so
    // get_int_data("timeout") could not see a configured value and the fallback
    // always won. "retries" is not one of those keys and does land in the map.
    // The settings layer notifies the documented defaults (30 / 3) for targets
    // that set neither.
    if (arguments.timeout > 0) timeout = arguments.timeout;
    retry = arguments.get_int_data("retries", arguments.retry);
    multicast_interfaces = arguments.get_string_data("multicast interface", "auto");
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
  // Built-in mapping used when none is configured in settings. The metric
  // namespace CheckSystem emits differs between platforms (Windows exposes
  // committed/virtual/page memory, PDH counters and "core N"; the Unix module
  // exposes physical/cached/swap memory and "core_N"), so the defaults are
  // platform-specific — otherwise one platform would forward fabricated zeros
  // for metrics the other never produces.
  static mapping_list default_variables() {
    mapping_list m;
#ifdef WIN32
    // CheckSystem emits per-core CPU as "system.cpu.core 0.user" (space).
    m.push_back(std::make_pair("core", "system.cpu.core (.*)\\.user"));
#else
    // CheckSystemUnix normalises per-core CPU to "system.cpu.core_0.user".
    m.push_back(std::make_pair("core", "system.cpu.core_(.*)\\.user"));
#endif
    return m;
  }
  static mapping_list default_metrics() {
    mapping_list m;
    // CPU total + per-core (derive) — available on both platforms.
    m.push_back(std::make_pair("cpu-total/cpu-user", "derive:system.cpu.total.user"));
    m.push_back(std::make_pair("cpu-total/cpu-system", "derive:system.cpu.total.kernel"));
    m.push_back(std::make_pair("cpu-total/cpu-idle", "derive:system.cpu.total.idle"));
    // Uptime in seconds (collectd "uptime" GAUGE type) — both platforms expose
    // it as the numeric system.uptime.ticks.raw (system.uptime.uptime is a
    // human-readable string and is not usable here).
    m.push_back(std::make_pair("uptime/uptime", "gauge:system.uptime.ticks.raw"));
#ifdef WIN32
    m.push_back(std::make_pair("cpu-${core}/cpu-user", "derive:system.cpu.core ${core}.user"));
    m.push_back(std::make_pair("cpu-${core}/cpu-system", "derive:system.cpu.core ${core}.kernel"));
    m.push_back(std::make_pair("cpu-${core}/cpu-idle", "derive:system.cpu.core ${core}.idle"));
    // Physical + page-file (committed) memory.
    m.push_back(std::make_pair("memory-/memory-available", "gauge:system.mem.physical.avail"));
    m.push_back(std::make_pair("memory-pagefile/memory-used", "gauge:system.mem.commited.used"));
    m.push_back(std::make_pair("memory-pagefile/memory-free", "gauge:system.mem.commited.avail"));
    // Process / thread counts (Windows-only metric family).
    m.push_back(std::make_pair("processes-/ps_count", "gauge:system.metrics.procs.procs,system.metrics.procs.threads"));
#else
    m.push_back(std::make_pair("cpu-${core}/cpu-user", "derive:system.cpu.core_${core}.user"));
    m.push_back(std::make_pair("cpu-${core}/cpu-system", "derive:system.cpu.core_${core}.kernel"));
    m.push_back(std::make_pair("cpu-${core}/cpu-idle", "derive:system.cpu.core_${core}.idle"));
    // Physical memory (collectd "memory" type) + swap (collectd "swap" type).
    m.push_back(std::make_pair("memory-/memory-used", "gauge:system.mem.physical.used"));
    m.push_back(std::make_pair("memory-/memory-free", "gauge:system.mem.physical.avail"));
    m.push_back(std::make_pair("swap-/swap-used", "gauge:system.mem.swap.used"));
    m.push_back(std::make_pair("swap-/swap-free", "gauge:system.mem.swap.avail"));
#endif
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

    // Interval reported to collectd: a per-target "interval" overrides the
    // module-level default (interval_seconds_) when set.
    const unsigned long long interval = static_cast<unsigned long long>(target.get_int_data("interval", static_cast<int>(interval_seconds_)));

    // collectd "high-resolution" time/interval are in units of 2^-30 seconds.
    builder.set_time(now_seconds << 30, interval << 30);
    builder.set_host(sender.get_host());

    // Variables must be expanded (against the flattened metric names) before
    // the metric templates that reference them are added.
    const mapping_list &variables = variables_.empty() ? default_variables_ : variables_;
    const mapping_list &metrics = metrics_.empty() ? default_metrics_ : metrics_;
    for (const auto &v : variables) builder.add_variable(v.first, v.second);
    for (const auto &m : metrics) builder.add_metric(m.first, m.second);

    collectd::collectd_builder::packet_list packets;
    builder.render(packets);

    // A value-list longer than a datagram can carry is truncated by the
    // encoder rather than emitted malformed; say so, or the missing values
    // look like a receiver-side problem.
    std::size_t clamped = 0;
    for (const collectd::packet &p : packets) clamped += p.clamped_values();
    if (clamped > 0) {
      NSC_LOG_ERROR("Dropped " + str::xtos(clamped) +
                    " collectd value(s): a single value list does not fit one datagram (max " + str::xtos(collectd::max_packet_size) +
                    " bytes). Split the metric across several entries.");
    }

    connection_data con(target, sender);
    send(con, packets);
    return true;
  }

  void send(const connection_data &target, const collectd::collectd_builder::packet_list &packets) {
    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Sending " + str::xtos(packets.size()) + " packets to: " + target.to_string()); }
    std::list<std::string> datagrams;
    for (const collectd::packet &p : packets) {
      if (p.get_size() == 0) continue;  // never put an empty datagram on the wire
      datagrams.push_back(p.get_buffer());
    }
    const collectd::sender_config config(target.get_address(), target.get_port(), target.retry, target.timeout, target.multicast_interfaces);
    const collectd::sender_result result = collectd::send_datagrams(config, datagrams);
    for (const std::string &error : result.errors) {
      NSC_LOG_ERROR(error);
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