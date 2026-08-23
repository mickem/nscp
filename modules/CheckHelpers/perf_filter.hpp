// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/xtos.hpp>
#include <string>

namespace perf_filter {
struct filter_obj {
  const PB::Common::PerformanceData &data;

  filter_obj(const PB::Common::PerformanceData &data) : data(data) {}

  std::string show() const { return data.alias() + "=" + get_value() + get_unit(); }
  std::string get_key() const { return data.alias(); }
  std::string get_unit() const {
    if (data.has_float_value()) return data.float_value().unit();
    return "";
  }
  std::string get_value() const {
    if (data.has_float_value()) return str::xtos(data.float_value().value());
    if (data.has_string_value()) return data.string_value().value();
    return "";
  }
  // %(warn) / %(crit) template substitutions: prefer the original Nagios
  // range syntax (e.g. "4:5") when present, fall back to the numeric
  // lower bound for plain-number thresholds (issue #748). Without this,
  // `render_perf` would replace a "4:5" warning with just "4".
  std::string get_warn() const {
    if (!data.has_float_value()) return "";
    if (!data.float_value().warning_range().empty()) return data.float_value().warning_range();
    if (data.float_value().has_warning()) return str::xtos(data.float_value().warning().value());
    return "";
  }
  std::string get_crit() const {
    if (!data.has_float_value()) return "";
    if (!data.float_value().critical_range().empty()) return data.float_value().critical_range();
    if (data.float_value().has_critical()) return str::xtos(data.float_value().critical().value());
    return "";
  }
  std::string get_max() const {
    if (data.has_float_value() && data.float_value().has_maximum()) return str::xtos(data.float_value().maximum().value());
    return "";
  }
  std::string get_min() const {
    if (data.has_float_value() && data.float_value().has_minimum()) return str::xtos(data.float_value().minimum().value());
    return "";
  }
};
typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace perf_filter
