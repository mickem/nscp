// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <check/access_policy.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/settings/object.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "pdh_thread.hpp"

namespace check_pdh {

// `check_pdh::check` would otherwise shadow the `check` namespace the policy
// lives in, so name it once here.
typedef ::check::access::policy access_policy;

struct counter_config_object : public nscapi::settings_objects::object_instance_interface {
  typedef nscapi::settings_objects::object_instance_interface parent;

  bool debug;
  std::string collection_strategy;
  std::string counter;
  std::string instances;
  std::string buffer_size;
  std::string type;
  std::string flags;
  std::string resolution;

  counter_config_object(std::string alias, std::string path)
      : parent(alias, path), collection_strategy("static"), instances("auto"), type("double"), resolution("auto") {}

  // Runtime items

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);

  std::string to_string() const {
    std::stringstream ss;
    ss << parent::to_string() << "{counter: " << counter << ", " << collection_strategy << ", " << type << "}";
    return ss.str();
  }
};

typedef nscapi::settings_objects::object_handler<counter_config_object> counter_config_handler;

struct filter_obj {
  std::string alias;
  std::string counter;
  std::string time;
  long long value_i;
  double value_f;

  filter_obj(std::string alias, std::string counter, std::string time, long long value_i, double value_f)
      : alias(alias), counter(counter), time(time), value_i(value_i), value_f(value_f) {}

  std::string show() const { return counter + "=" + str::xtos(value_f) + " (" + str::xtos(value_i) + ")"; }
  long long get_value_i() const { return value_i; }
  double get_value_f() const { return value_f; }
  std::string get_counter() const { return counter; }
  std::string get_alias() const { return alias; }
  std::string get_time() const { return time; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

struct check {
  counter_config_handler counters_;
  // Which counters a caller may name in `counter=`. Open by default, so an
  // upgrade changes nothing. The predefined entries are the counters already
  // configured in [/settings/system/windows/counters], so `predefined` mode
  // needs no second list; see docs/docs/concepts/check-access.md.
  access_policy counter_access_;

  check() : counter_access_("counter", "counters", "/settings/system/windows") {}

  void check_pdh(std::shared_ptr<pdh_thread> &collector, const PB::Commands::QueryRequestMessage::Request &request,
                 PB::Commands::QueryResponseMessage::Response *response);
  void add_counter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
  void add_rrd_counter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
  void clear();

  // Hold one counter argument against the access mode. `is_named` marks the
  // form which refers to a counter configured in [/settings/system/windows/counters]
  // rather than a raw PDH path. Returns false and fills `error` when refused.
  bool allow_counter(const std::string &counter, bool is_named, std::string &error) const;
};
}  // namespace check_pdh