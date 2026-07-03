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

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace cpu_utilization_check {

// Raw cumulative jiffie counters from the aggregate "cpu" line of /proc/stat.
struct cpu_jiffies {
  unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
  bool valid;
  cpu_jiffies() : user(0), nice(0), system(0), idle(0), iowait(0), irq(0), softirq(0), steal(0), guest(0), guest_nice(0), valid(false) {}
  unsigned long long total() const { return user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice; }
};

// Utilization breakdown in percent-of-all-cores (a fully busy machine sums to
// ~100). `total` is the non-idle share (100 - idle).
struct util_obj {
  double total, user, system, iowait, irq, softirq, steal, guest, idle;
  util_obj() : total(0), user(0), system(0), iowait(0), irq(0), softirq(0), steal(0), guest(0), idle(0) {}

  std::string get_name() const { return "total"; }
  double get_total() const { return total; }
  double get_user() const { return user; }
  double get_system() const { return system; }
  double get_iowait() const { return iowait; }
  double get_irq() const { return irq; }
  double get_softirq() const { return softirq; }
  double get_steal() const { return steal; }
  double get_guest() const { return guest; }
  double get_idle() const { return idle; }
  std::string show() const { return "cpu utilization total: " + std::to_string(total) + "%"; }
};

// Parse the aggregate "cpu" line from /proc/stat contents. Missing trailing
// fields (older kernels) default to 0. Returns a cpu_jiffies with valid=false
// on a malformed input.
cpu_jiffies parse_proc_stat_cpu(const std::string &content);

// Compute the utilization breakdown from two cumulative snapshots. `user`
// folds in nice, `guest` folds in guest_nice. A zero (or negative) total delta
// yields an all-zero result.
util_obj compute_utilization(const cpu_jiffies &prev, const cpu_jiffies &cur);

typedef parsers::where::filter_handler_impl<std::shared_ptr<util_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<util_obj, filter_obj_handler> filter_type;

// Testable variant: computes utilization from two pre-sampled snapshots
// instead of reading /proc/stat and sleeping.
void check_cpu_utilization_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                                const cpu_jiffies &prev, const cpu_jiffies &cur);

void check_cpu_utilization(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace cpu_utilization_check
