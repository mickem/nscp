// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "perf_filter.hpp"

namespace perf_filter {
filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("key", &filter_obj::get_key, "The name (alias) of the performance data entry")
      .add_string_var("value", &filter_obj::get_value, "The value of the performance data entry")
      .add_string_var("unit", &filter_obj::get_unit, "The unit of the performance data entry")
      .add_string_var("warn", &filter_obj::get_warn, "The warning threshold (range when set, otherwise the numeric bound)")
      .add_string_var("crit", &filter_obj::get_crit, "The critical threshold (range when set, otherwise the numeric bound)")
      .add_string_var("max", &filter_obj::get_max, "The maximum bound of the performance data entry")
      .add_string_var("min", &filter_obj::get_min, "The minimum bound of the performance data entry")
      .add_string_var("message", &filter_obj::get_key, "The name (alias) of the performance data entry");
}
}  // namespace perf_filter
