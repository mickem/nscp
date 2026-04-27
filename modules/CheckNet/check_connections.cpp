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

#include "check_connections.h"
#include "check_connections_internal.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>

#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

namespace po = boost::program_options;

namespace check_net {
namespace check_connections_filter {

filter_obj_handler::filter_obj_handler() {
  registry_.add_string("protocol", &filter_obj::get_protocol, "Protocol of this bucket (tcp, tcp6, udp, udp6, total)");
  registry_.add_string("family", &filter_obj::get_family, "Address family (ipv4, ipv6, any)");
  registry_.add_string("state", &filter_obj::get_state, "TCP state name (ESTABLISHED, LISTEN, ...) or 'all'");
  registry_.add_int_x("count", parsers::where::type_int, &filter_obj::get_count, "Number of connections matching this bucket");
  registry_.add_int_x("total", parsers::where::type_int, &filter_obj::get_total, "Total number of connections (only on the 'total' bucket)");
  registry_.add_int_x("established", parsers::where::type_int, &filter_obj::get_established, "Number of TCP connections in ESTABLISHED state (total bucket)");
  registry_.add_int_x("listen", parsers::where::type_int, &filter_obj::get_listen, "Number of TCP sockets in LISTEN state (total bucket)");
  registry_.add_int_x("syn_sent", parsers::where::type_int, &filter_obj::get_syn_sent, "Number of TCP connections in SYN_SENT state (total bucket)");
  registry_.add_int_x("syn_recv", parsers::where::type_int, &filter_obj::get_syn_recv, "Number of TCP connections in SYN_RECV state (total bucket)");
  registry_.add_int_x("time_wait", parsers::where::type_int, &filter_obj::get_time_wait, "Number of TCP connections in TIME_WAIT state (total bucket)");
  registry_.add_int_x("close_wait", parsers::where::type_int, &filter_obj::get_close_wait, "Number of TCP connections in CLOSE_WAIT state (total bucket)");
  registry_.add_int_x("closing", parsers::where::type_int, &filter_obj::get_closing, "Number of TCP connections in CLOSING state (total bucket)");
  registry_.add_int_x("fin_wait", parsers::where::type_int, &filter_obj::get_fin_wait, "Number of TCP connections in FIN_WAIT* state (total bucket)");
  registry_.add_int_x("last_ack", parsers::where::type_int, &filter_obj::get_last_ack, "Number of TCP connections in LAST_ACK state (total bucket)");
  registry_.add_int_x("udp", parsers::where::type_int, &filter_obj::get_udp, "Number of UDP sockets (total bucket)");
}

}  // namespace check_connections_filter

namespace {

using check_connections_filter::filter_obj;

#if defined(__linux__)

using check_connections_internal::linux_tcp_state;

// Count rows in /proc/net/<file>. Each data line has the form:
//   "  N: local rem state ..."
// We only need the 4th field (state) as a hex byte.
void count_proc_net(const std::string &path, const std::string &proto, bool is_tcp,
                    std::map<std::string, long long> &per_state, long long &proto_total) {
  std::ifstream f(path);
  if (!f) return;
  std::string line;
  // Skip header.
  if (!std::getline(f, line)) return;
  while (std::getline(f, line)) {
    std::vector<std::string> parts;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) parts.push_back(tok);
    if (parts.size() < 4) continue;
    proto_total++;
    if (!is_tcp) {
      per_state["UDP_" + proto]++;
      continue;
    }
    unsigned int state_val = 0;
    try {
      state_val = std::stoul(parts[3], nullptr, 16);
    } catch (...) {
      continue;
    }
    per_state[linux_tcp_state(state_val)]++;
  }
}

bool collect_connections_linux(std::vector<boost::shared_ptr<filter_obj> > &out, std::string &error) {
  std::map<std::string, long long> per_state;
  long long tcp4 = 0, tcp6 = 0, udp4 = 0, udp6 = 0;

  count_proc_net("/proc/net/tcp", "tcp", true, per_state, tcp4);
  count_proc_net("/proc/net/tcp6", "tcp6", true, per_state, tcp6);
  count_proc_net("/proc/net/udp", "udp", false, per_state, udp4);
  count_proc_net("/proc/net/udp6", "udp6", false, per_state, udp6);

  if (tcp4 == 0 && tcp6 == 0 && udp4 == 0 && udp6 == 0) {
    error = "Failed to read /proc/net/{tcp,tcp6,udp,udp6}";
    return false;
  }

  // Per-protocol bucket.
  auto add_proto = [&](const std::string &proto, const std::string &family, long long n) {
    auto o = boost::make_shared<filter_obj>();
    o->protocol = proto;
    o->family = family;
    o->state = "all";
    o->count = n;
    out.push_back(o);
  };
  add_proto("tcp", "ipv4", tcp4);
  add_proto("tcp6", "ipv6", tcp6);
  add_proto("udp", "ipv4", udp4);
  add_proto("udp6", "ipv6", udp6);

  // Per-state bucket (TCP only).
  for (const auto &kv : per_state) {
    if (kv.first.rfind("UDP_", 0) == 0) continue;
    auto o = boost::make_shared<filter_obj>();
    o->protocol = "tcp";
    o->family = "any";
    o->state = kv.first;
    o->count = kv.second;
    out.push_back(o);
  }

  // Total bucket.
  auto total = boost::make_shared<filter_obj>();
  total->protocol = "total";
  total->family = "any";
  total->state = "all";
  total->total = tcp4 + tcp6 + udp4 + udp6;
  total->count = total->total;
  total->udp = udp4 + udp6;
  total->established = per_state["ESTABLISHED"];
  total->listen = per_state["LISTEN"];
  total->syn_sent = per_state["SYN_SENT"];
  total->syn_recv = per_state["SYN_RECV"];
  total->time_wait = per_state["TIME_WAIT"];
  total->close_wait = per_state["CLOSE_WAIT"];
  total->closing = per_state["CLOSING"];
  total->fin_wait = per_state["FIN_WAIT1"] + per_state["FIN_WAIT2"];
  total->last_ack = per_state["LAST_ACK"];
  out.push_back(total);

  return true;
}

#endif  // __linux__

#ifdef WIN32

const char *win_tcp_state(DWORD s) {
  switch (s) {
    case MIB_TCP_STATE_CLOSED:
      return "CLOSE";
    case MIB_TCP_STATE_LISTEN:
      return "LISTEN";
    case MIB_TCP_STATE_SYN_SENT:
      return "SYN_SENT";
    case MIB_TCP_STATE_SYN_RCVD:
      return "SYN_RECV";
    case MIB_TCP_STATE_ESTAB:
      return "ESTABLISHED";
    case MIB_TCP_STATE_FIN_WAIT1:
      return "FIN_WAIT1";
    case MIB_TCP_STATE_FIN_WAIT2:
      return "FIN_WAIT2";
    case MIB_TCP_STATE_CLOSE_WAIT:
      return "CLOSE_WAIT";
    case MIB_TCP_STATE_CLOSING:
      return "CLOSING";
    case MIB_TCP_STATE_LAST_ACK:
      return "LAST_ACK";
    case MIB_TCP_STATE_TIME_WAIT:
      return "TIME_WAIT";
    case MIB_TCP_STATE_DELETE_TCB:
      return "DELETE_TCB";
    default:
      return "UNKNOWN";
  }
}

bool collect_connections_windows(std::vector<boost::shared_ptr<filter_obj> > &out, std::string &error) {
  std::map<std::string, long long> per_state;
  long long tcp4 = 0, tcp6 = 0, udp4 = 0, udp6 = 0;

  // TCP IPv4
  {
    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    std::vector<char> buf(size);
    if (size && GetExtendedTcpTable(buf.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
      auto *table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID *>(buf.data());
      tcp4 = table->dwNumEntries;
      for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        per_state[win_tcp_state(table->table[i].dwState)]++;
      }
    }
  }

  // TCP IPv6
  {
    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
    std::vector<char> buf(size);
    if (size && GetExtendedTcpTable(buf.data(), &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
      auto *table = reinterpret_cast<MIB_TCP6TABLE_OWNER_PID *>(buf.data());
      tcp6 = table->dwNumEntries;
      for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        per_state[win_tcp_state(table->table[i].dwState)]++;
      }
    }
  }

  // UDP IPv4
  {
    DWORD size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    std::vector<char> buf(size);
    if (size && GetExtendedUdpTable(buf.data(), &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
      auto *table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID *>(buf.data());
      udp4 = table->dwNumEntries;
    }
  }

  // UDP IPv6
  {
    DWORD size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
    std::vector<char> buf(size);
    if (size && GetExtendedUdpTable(buf.data(), &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
      auto *table = reinterpret_cast<MIB_UDP6TABLE_OWNER_PID *>(buf.data());
      udp6 = table->dwNumEntries;
    }
  }

  if (tcp4 == 0 && tcp6 == 0 && udp4 == 0 && udp6 == 0) {
    error = "Failed to enumerate connections via iphlpapi";
    return false;
  }

  auto add_proto = [&](const std::string &proto, const std::string &family, long long n) {
    auto o = boost::make_shared<filter_obj>();
    o->protocol = proto;
    o->family = family;
    o->state = "all";
    o->count = n;
    out.push_back(o);
  };
  add_proto("tcp", "ipv4", tcp4);
  add_proto("tcp6", "ipv6", tcp6);
  add_proto("udp", "ipv4", udp4);
  add_proto("udp6", "ipv6", udp6);

  for (const auto &kv : per_state) {
    auto o = boost::make_shared<filter_obj>();
    o->protocol = "tcp";
    o->family = "any";
    o->state = kv.first;
    o->count = kv.second;
    out.push_back(o);
  }

  auto total = boost::make_shared<filter_obj>();
  total->protocol = "total";
  total->family = "any";
  total->state = "all";
  total->total = tcp4 + tcp6 + udp4 + udp6;
  total->count = total->total;
  total->udp = udp4 + udp6;
  total->established = per_state["ESTABLISHED"];
  total->listen = per_state["LISTEN"];
  total->syn_sent = per_state["SYN_SENT"];
  total->syn_recv = per_state["SYN_RECV"];
  total->time_wait = per_state["TIME_WAIT"];
  total->close_wait = per_state["CLOSE_WAIT"];
  total->closing = per_state["CLOSING"];
  total->fin_wait = per_state["FIN_WAIT1"] + per_state["FIN_WAIT2"];
  total->last_ack = per_state["LAST_ACK"];
  out.push_back(total);

  return true;
}

#endif  // WIN32

bool collect_connections(std::vector<boost::shared_ptr<filter_obj> > &out, std::string &error) {
#if defined(__linux__)
  return collect_connections_linux(out, error);
#elif defined(WIN32)
  return collect_connections_windows(out, error);
#else
  (void)out;
  error = "check_connections is not implemented on this platform";
  return false;
#endif
}

}  // namespace

void check_connections(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using check_connections_filter::filter;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter f;
  filter_helper.add_options("", "", "protocol = 'total'", f.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${protocol}/${state}: ${count}", "${protocol}_${state}", "No connection data",
                           "%(status): connections ok");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;

  std::vector<boost::shared_ptr<filter_obj> > items;
  std::string err;
  if (!collect_connections(items, err)) {
    return nscapi::protobuf::functions::set_response_bad(*response, err);
  }

  for (const auto &o : items) f.match(o);

  filter_helper.post_process(f);
}

}  // namespace check_net
