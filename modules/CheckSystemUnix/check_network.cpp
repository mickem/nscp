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

// check_network reports per-interface throughput. The cumulative byte/packet
// counters from /proc/net/dev are sampled once per second by the collector
// (realtime_thread) which computes the per-second rates; this file only renders
// the cached snapshot through the where-filter, mirroring check_network on
// Windows.

#include "check_network.h"

#include <boost/program_options.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>

#include "realtime_thread.hpp"

namespace po = boost::program_options;

namespace network_check {

std::string network_interface::get_received_human() const { return str::format::format_byte_units(rx_bytes_per_sec); }
std::string network_interface::get_sent_human() const { return str::format::format_byte_units(tx_bytes_per_sec); }
std::string network_interface::get_total_human() const { return str::format::format_byte_units(get_total()); }

typedef network_interface filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Network interface name")
      .add_string_var("status", &filter_obj::get_status, "Link operational state (up/down/unknown)")
      .add_string_var("enabled", &filter_obj::get_enabled, "True if the interface link is up")
      .add_string_var("MAC", &filter_obj::get_mac, "The hardware (MAC) address");

  registry_.add_int_var("received", &filter_obj::get_received, "Bytes received per second")
      .add_int_perf("Bps")
      .add_int_var("sent", &filter_obj::get_sent, "Bytes sent per second")
      .add_int_perf("Bps")
      .add_int_var("total", &filter_obj::get_total, "Bytes total (received + sent) per second")
      .add_int_perf("Bps")
      .add_int_var("received_packets", &filter_obj::get_received_packets, "Packets received per second")
      .add_int_var("sent_packets", &filter_obj::get_sent_packets, "Packets sent per second")
      .add_int_var("rx_errors", &filter_obj::get_rx_errors, "Cumulative receive errors since boot")
      .add_int_var("tx_errors", &filter_obj::get_tx_errors, "Cumulative transmit errors since boot");

  // Negotiated link speed and best-effort percent-of-link usage. Speed is
  // unreliable on virtual interfaces (reads 0); filter on speed_bps > 0 before
  // applying the usage_* percent thresholds.
  registry_.add_int_var("speed_bps", &filter_obj::get_speed_bps, "Link speed in bits/sec (0 when unknown, e.g. virtual interfaces)")
      .add_int_var("usage_in", &filter_obj::get_usage_in, "Percent of link speed used by received traffic (0 when speed unknown)")
      .add_int_perf("%")
      .add_int_var("usage_out", &filter_obj::get_usage_out, "Percent of link speed used by sent traffic (0 when speed unknown)")
      .add_int_perf("%")
      .add_int_var("usage_total", &filter_obj::get_usage_total, "Percent of link speed used by total traffic (0 when speed unknown)")
      .add_int_perf("%");

  registry_.add_string_var("received_human", &filter_obj::get_received_human, "Bytes received per second (human readable, auto-scaled)")
      .add_string_var("sent_human", &filter_obj::get_sent_human, "Bytes sent per second (human readable, auto-scaled)")
      .add_string_var("total_human", &filter_obj::get_total_human, "Bytes total per second (human readable, auto-scaled)");
}

void check_network(std::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
                   PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("total > 10000", "total > 100000", "", filter.get_filter_syntax(), "critical");
  filter_helper.add_syntax("${status}: ${list}", "${name} >${sent_human}/s <${received_human}/s", "${name}", "",
                           "%(status): Network interfaces seem ok.");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  if (!collector) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Network collector not initialized");
  }
  if (!collector->has_network_data()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No network data available yet (collector still initializing)");
  }

  for (const network_interface &v : collector->get_network()) {
    const std::shared_ptr<filter_obj> record(new filter_obj(v));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

void build_network_metrics(PB::Metrics::MetricsBundle *parent, const nics_type &data) {
  using namespace nscapi::metrics;

  PB::Metrics::MetricsBundle *bundle = parent->add_children();
  bundle->set_key("network");
  for (const network_interface &v : data) {
    add_metric(bundle, v.name + ".received", v.rx_bytes_per_sec);
    add_metric(bundle, v.name + ".sent", v.tx_bytes_per_sec);
    add_metric(bundle, v.name + ".total", v.get_total());
    add_metric(bundle, v.name + ".status", v.status);
  }
}

}  // namespace network_check
