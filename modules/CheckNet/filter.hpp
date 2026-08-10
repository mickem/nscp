// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <error/error.hpp>
#include <map>
#include <memory>
#include <net/pinger.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>
#include <parsers/where.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>
#include <str/format.hpp>
#include <string>

namespace ping_filter {

// Mean absolute difference between consecutive round trip times, in
// milliseconds. This is the definition ping and VoIP tooling reports as
// "jitter", so the number is directly comparable with those tools.
//
// Returns -1 for fewer than two replies: jitter describes the variation
// between packets and is undefined for one of them. -1 can never collide with
// a real reading, since a mean of absolute values is never negative.
inline long long mean_abs_delta_ms(const std::vector<std::size_t>& rtts) {
  if (rtts.size() < 2) return -1;
  long long total = 0;
  for (std::size_t i = 1; i < rtts.size(); ++i) {
    const long long a = static_cast<long long>(rtts[i]);
    const long long b = static_cast<long long>(rtts[i - 1]);
    total += a > b ? a - b : b - a;
  }
  return total / static_cast<long long>(rtts.size() - 1);
}

struct filter_obj {
  filter_obj(result_container result) : is_total_(false), result(result) {}
  filter_obj() : is_total_(true) {}
  const filter_obj& operator=(const filter_obj& other) {
    result = other.result;
    return *this;
  }

  static std::shared_ptr<ping_filter::filter_obj> get_total();

  std::string show() const {
    if (is_total_)
      return "total";
    else
      return result.destination_ + "(" + result.ip_ + ")";
  }

  //		std::string get_filename() { return filename; }
  //	std::string get_path(parsers::where::evaluation_context) { return path.string(); }

  std::string get_host() {
    if (is_total_) return "total";
    return result.destination_;
  }
  std::string get_ip() {
    if (is_total_) return "total";
    return result.ip_;
  }

 public:
  void add(std::shared_ptr<filter_obj> info);
  void make_total() { is_total_ = true; }
  bool is_total() const { return is_total_; }
  long long get_sent() { return result.num_send_; }
  long long get_recv() { return result.num_replies_; }
  long long get_timeout() { return result.num_timeouts_; }

  long long get_loss(parsers::where::evaluation_context c) {
    if (result.num_send_ == 0) {
      c->error("No packages were sent");
      return 0;
    }
    return result.num_timeouts_ * 100 / result.num_send_;
  }
  long long get_time() { return result.time_; }

  // Variation between the round trip times of the packets sent to this host.
  // Needs count >= 2 to mean anything; -1 until then.
  //
  // The total row cannot pool round trip times across hosts - the spread
  // between two different hosts' latencies is not jitter - so it carries the
  // worst per-host jitter instead, which is what a fleet-wide alert wants.
  long long get_jitter() const { return is_total_ ? total_jitter_ : mean_abs_delta_ms(result.rtts_); }

  // TTL of the last reply, or -1 when unknown (nothing came back, or IPv6).
  //
  // The total row carries the LOWEST TTL across the hosts: a low TTL is the
  // interesting end (a reply nearly out of hops, or a route that grew), so the
  // minimum is what a fleet-wide threshold wants.
  long long get_ttl() const { return is_total_ ? total_ttl_ : result.ttl_; }

  bool is_total_;
  long long total_jitter_ = -1;
  long long total_ttl_ = -1;
  result_container result;
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace ping_filter
