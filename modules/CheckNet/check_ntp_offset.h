// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace check_net {
namespace check_ntp_filter {

struct filter_obj {
  std::string server;
  long long port;
  // Offset between the local clock and the server clock, in milliseconds.
  // Positive means local clock is ahead of the server.
  long long offset_ms;
  long long stratum;
  long long time;  // round trip time, ms
  std::string result;

  // Number of samples actually collected, and the RMS variation between their
  // offsets. jitter is -1 when fewer than two samples were taken (the default),
  // since the variation between measurements needs at least two of them.
  long long samples = 0;
  long long jitter = -1;

  // The server's own advertised time quality, straight out of the packet
  // header: root_delay is the round-trip to its reference clock, and
  // root_dispersion the error bound it claims. Both in milliseconds.
  long long root_delay = 0;
  long long root_dispersion = 0;

  filter_obj() : port(0), offset_ms(0), stratum(0), time(0) {}

  std::string show() const { return server + " offset=" + std::to_string(offset_ms) + "ms stratum=" + std::to_string(stratum) + " (" + result + ")"; }

  std::string get_server() const { return server; }
  long long get_port() const { return port; }
  // Magnitude of the offset, useful for "offset > 1000" thresholds.
  long long get_offset() const { return offset_ms < 0 ? -offset_ms : offset_ms; }
  long long get_offset_signed() const { return offset_ms; }
  long long get_stratum() const { return stratum; }
  long long get_time() const { return time; }
  long long get_samples() const { return samples; }
  long long get_jitter() const { return jitter; }
  // Registered form: optional — jitter needs at least two samples to exist.
  boost::optional<long long> get_jitter_opt() const {
    if (jitter < 0) return boost::none;
    return jitter;
  }
  long long get_root_delay() const { return root_delay; }
  long long get_root_dispersion() const { return root_dispersion; }
  std::string get_result() const { return result; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_ntp_filter

void check_ntp_offset(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check_net
