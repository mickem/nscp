// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace check_net {
namespace check_connections_filter {

struct filter_obj {
  std::string protocol;  // tcp, tcp6, udp, udp6
  std::string family;    // ipv4 or ipv6
  std::string state;     // ESTABLISHED, LISTEN, ...
  long long count;       // count for this bucket

  // Aggregated, only valid on the "total" bucket.
  long long total;
  long long established;
  long long listen;
  long long syn_sent;
  long long syn_recv;
  long long time_wait;
  long long close_wait;
  long long closing;
  long long fin_wait;
  long long last_ack;
  long long udp;

  filter_obj()
      : count(0), total(0), established(0), listen(0), syn_sent(0), syn_recv(0), time_wait(0), close_wait(0), closing(0), fin_wait(0), last_ack(0), udp(0) {}

  std::string show() const { return protocol + "/" + state + "=" + std::to_string(count); }

  std::string get_protocol() const { return protocol; }
  std::string get_family() const { return family; }
  std::string get_state() const { return state; }
  long long get_count() const { return count; }
  long long get_total() const { return total; }
  long long get_established() const { return established; }
  long long get_listen() const { return listen; }
  long long get_syn_sent() const { return syn_sent; }
  long long get_syn_recv() const { return syn_recv; }
  long long get_time_wait() const { return time_wait; }
  long long get_close_wait() const { return close_wait; }
  long long get_closing() const { return closing; }
  long long get_fin_wait() const { return fin_wait; }
  long long get_last_ack() const { return last_ack; }
  long long get_udp() const { return udp; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_connections_filter

void check_connections(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check_net
