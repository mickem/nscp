// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <error/error.hpp>
#include <map>
#include <memory>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>
#include <string>

namespace counter_filter {
struct filter_obj {
  std::string counter;
  long long value;
  filter_obj(std::string counter) : counter(counter) {}
  std::string show() const { return counter + "=" + str::xtos(value); }

  std::string get_counter() const { return counter; }
};
typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace counter_filter
