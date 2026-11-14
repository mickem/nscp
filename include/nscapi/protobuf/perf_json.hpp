// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/json.hpp>
#include <nscapi/protobuf/command.hpp>

namespace perf_json {

// A result line's performance data as the JSON object the query endpoint has
// always served: keyed by alias, each with value/unit and whatever bounds and
// thresholds the check set. Shared with the failed-check log so a failure
// renders with the same shape as the live query it came from.
inline boost::json::object to_json(const PB::Commands::QueryResponseMessage::Response::Line &line) {
  boost::json::object perf;
  for (const PB::Common::PerformanceData &p : line.perf()) {
    boost::json::object pdata;

    if (p.has_float_value()) {
      const auto &fv = p.float_value();
      pdata["value"] = fv.value();
      if (fv.has_minimum()) pdata["minimum"] = fv.minimum().value();
      if (fv.has_maximum()) pdata["maximum"] = fv.maximum().value();
      // Threshold fields carry Nagios range syntax when the original input
      // used it (e.g. "4:5") - prefer that string over the numeric lower
      // bound so the API faithfully reports what the upstream plugin emitted
      // (issue #748). JSON consumers see `number` for plain thresholds and
      // `string` for range syntax; the api.ts type mirrors that union.
      if (!fv.warning_range().empty())
        pdata["warning"] = fv.warning_range();
      else if (fv.has_warning())
        pdata["warning"] = fv.warning().value();
      if (!fv.critical_range().empty())
        pdata["critical"] = fv.critical_range();
      else if (fv.has_critical())
        pdata["critical"] = fv.critical().value();
      pdata["unit"] = fv.unit();
    }
    if (p.has_string_value()) {
      pdata["value"] = p.string_value().value();
    }
    perf[p.alias()] = pdata;
  }
  return perf;
}

}  // namespace perf_json
