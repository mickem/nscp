/*
 * Copyright (C) 2004-2026 Michael Medin
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

#pragma once

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <sstream>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <string>
#include <vector>

namespace cron_parser {
struct next_value {
  long long value;
  bool overflow;
  next_value(const long long value, const bool overflow) : value(value), overflow(overflow) {}
};
struct schedule_item {
  std::vector<long long> value_;
  long long min_;
  long long max_;
  bool star_;
  schedule_item() : min_(0), max_(0), star_(false) {}

  static schedule_item parse(const std::string &value, const long long min_value, const long long max_value) {
    schedule_item v;
    v.min_ = min_value;
    v.max_ = max_value;
    if (value == "*") {
      v.star_ = true;
      return v;
    }
    std::vector<std::string> split;
    boost::algorithm::split(split, value, boost::algorithm::is_any_of(","));
    for (const std::string &val : split) {
      long long i_val;
      try {
        i_val = std::stoll(val);
      } catch (const std::exception &) {
        throw nsclient::nsclient_exception("Invalid value: " + value);
      }
      if (i_val < v.min_ || i_val > v.max_) throw nsclient::nsclient_exception("Value out of range: " + value);
      v.value_.push_back(i_val);
    }
    return v;
  }
  bool is_valid_for(const long long v) const {
    if (star_) return true;
    for (const long long &val : value_) {
      if (val == v) return true;
    }
    return false;
  }

  next_value find_next(const long long value) const {
    for (long long i = value; i <= max_; i++) {
      if (is_valid_for(i)) {
        return {i, false};
      }
    }
    for (long long i = min_; i < value; i++) {
      if (is_valid_for(i)) {
        return {i, true};
      }
    }
    throw nsclient::nsclient_exception("Failed to find match for: " + str::xtos(value));
  }

  std::string to_string() const {
    if (star_) return "*";
    std::stringstream ss;
    bool first = true;
    for (const long long &v : value_) {
      if (!first) {
        ss << ",";
      }
      ss << str::xtos(v);
      first = false;
    }
    return ss.str();
  }
};
struct schedule {
  schedule_item min;
  schedule_item hour;
  schedule_item dom;
  schedule_item mon;
  schedule_item dow;

  bool is_valid_for(const boost::posix_time::ptime now_time) const {
    return mon.is_valid_for(now_time.date().month()) && dom.is_valid_for(now_time.date().day()) && dow.is_valid_for(now_time.date().day_of_week()) &&
           hour.is_valid_for(now_time.time_of_day().hours()) && min.is_valid_for(now_time.time_of_day().minutes());
  }

  boost::posix_time::ptime find_next(boost::posix_time::ptime now_time) const {
    using namespace boost::posix_time;
    using namespace boost::gregorian;

    // Never run "Now" always wait for next...
    if (is_valid_for(now_time)) now_time += minutes(1);

    int year = now_time.date().year();

    for (const int end_year = year + 4; year <= end_year; ++year) {
      const long long mon_start = (year == static_cast<int>(now_time.date().year())) ? now_time.date().month().as_number() : mon.min_;
      for (next_value nmon = mon.find_next(mon_start); !nmon.overflow; nmon = mon.find_next(nmon.value + 1)) {
        const long long dom_start = (year == static_cast<int>(now_time.date().year()) && nmon.value == now_time.date().month().as_number())
                                        ? now_time.date().day().as_number()
                                        : dom.min_;
        for (next_value ndom = dom.find_next(dom_start); !ndom.overflow; ndom = dom.find_next(ndom.value + 1)) {
          // Validate the date is real (e.g., Feb 30 doesn't exist)
          try {
            const date test_d(static_cast<unsigned short>(year), static_cast<unsigned short>(nmon.value), static_cast<unsigned short>(ndom.value));
            (void)test_d;
          } catch (...) {
            break;  // Invalid date in this month, advance month
          }

          date d(static_cast<unsigned short>(year), static_cast<unsigned short>(nmon.value), static_cast<unsigned short>(ndom.value));

          // Check day-of-week constraint
          if (!dow.is_valid_for(d.day_of_week().as_number())) continue;

          const long long hour_start = (d == now_time.date()) ? now_time.time_of_day().hours() : hour.min_;
          for (next_value nhour = hour.find_next(hour_start); !nhour.overflow; nhour = hour.find_next(nhour.value + 1)) {
            const long long min_start = (d == now_time.date() && nhour.value == now_time.time_of_day().hours()) ? now_time.time_of_day().minutes() : min.min_;
            const next_value nmin = min.find_next(min_start);
            if (nmin.overflow) continue;

            const time_duration t(hours(static_cast<long>(nhour.value)) + minutes(static_cast<long>(nmin.value)));
            return ptime(d, t);
          }
        }
      }
    }

    throw nsclient::nsclient_exception("Failed to find next schedule match within 4 years");
  }

  std::string to_string() const { return min.to_string() + " " + hour.to_string() + " " + dom.to_string() + " " + mon.to_string() + " " + dow.to_string(); }
};

inline schedule parse(const std::string &s) {
  // min hour dom mon dow
  // min: 0-59, hour: 0-23, dom: 1-31, mon: 1-12, dow: 0-6
  typedef std::vector<std::string> vec;
  const vec v = str::utils::split<vec>(s, " ");
  schedule ret;
  if (v.size() != 5) throw nsclient::nsclient_exception("invalid cron syntax: " + s);
  ret.min = schedule_item::parse(v[0], 0, 59);
  ret.hour = schedule_item::parse(v[1], 0, 23);
  ret.dom = schedule_item::parse(v[2], 1, 31);
  ret.mon = schedule_item::parse(v[3], 1, 12);
  ret.dow = schedule_item::parse(v[4], 0, 6);
  return ret;
}
}  // namespace cron_parser
