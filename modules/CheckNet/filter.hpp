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

  bool is_total_;
  result_container result;
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace ping_filter
