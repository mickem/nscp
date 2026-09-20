// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hyperv_host.hpp"

#include <boost/algorithm/string/trim.hpp>
#include <boost/program_options.hpp>
#include <map>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_object_gather.hpp>

#include "check_hyperv_internal.hpp"

namespace po = boost::program_options;

namespace check_hyperv {

using check_hyperv_internal::processor_sample;
using check_hyperv_internal::with_total;
using PDH::value_of;

namespace {

const char *const kHealthObject = "Hyper-V Virtual Machine Health Summary";
const char *const kHypervisorObject = "Hyper-V Hypervisor";
const char *const kLogicalProcessorObject = "Hyper-V Hypervisor Logical Processor";

std::string counters_error(const std::string &object, const PDH::pdh_exception &e) {
  // PDH messages come from FormatMessage and end in \r\n; trim so the check
  // output stays a single line.
  std::string reason = e.reason();
  boost::algorithm::trim(reason);
  return "Hyper-V counters (" + object + ") not available - is the Hyper-V role installed and the hypervisor running on this host? (" + reason + ")";
}

}  // namespace

std::map<std::string, double> fetch_host_counters() {
  // Two single-instance objects. The health summary counts virtual machines
  // by health, the hypervisor object reports its capacity; both are plain
  // values, so a single sample is enough.
  std::map<std::string, double> values = PDH::gather_object_values(kHealthObject, {"Health Ok", "Health Critical"}, false);
  const std::map<std::string, double> hv =
      PDH::gather_object_values(kHypervisorObject, {"Logical Processors", "Virtual Processors", "Partitions", "Total Pages"}, false);
  values.insert(hv.begin(), hv.end());
  return values;
}

// ---------------------------------------------------------------------------
// check_hyperv_host — VM health summary and hypervisor capacity
// ---------------------------------------------------------------------------

namespace hyperv_host_filter {

struct filter_obj {
  long long health_ok = 0;
  long long health_critical = 0;
  long long logical_processors = 0;
  long long virtual_processors = 0;
  long long partitions = 0;
  long long total_pages = 0;

  std::string show() const { return std::to_string(health_ok) + " ok, " + std::to_string(health_critical) + " critical"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_int_var("health_ok", parsers::where::type_int, [](auto obj) { return obj->health_ok; }, "Virtual machines whose health is ok")
        .add_int_perf("", "", "_ok");
    registry_.add_int_var("health_critical", parsers::where::type_int, [](auto obj) { return obj->health_critical; },
                          "Virtual machines whose health is critical (the host could not keep them running as configured)")
        .add_int_perf("", "", "_critical");
    registry_.add_int_var("logical_processors", parsers::where::type_int, [](auto obj) { return obj->logical_processors; },
                          "Logical processors the hypervisor manages on this host")
        .add_int_perf("", "", "_logical_processors");
    registry_.add_int_var("virtual_processors", parsers::where::type_int, [](auto obj) { return obj->virtual_processors; },
                          "Virtual processors currently allocated to running partitions (root and guests)")
        .add_int_perf("", "", "_virtual_processors");
    registry_.add_int_var("partitions", parsers::where::type_int, [](auto obj) { return obj->partitions; },
                          "Running partitions, including the root (host) partition: the number of running virtual machines plus one")
        .add_int_perf("", "", "_partitions");
    registry_.add_int_var("total_pages", parsers::where::type_int, [](auto obj) { return obj->total_pages; },
                          "Memory pages the hypervisor has allocated for its own use")
        .add_int_perf("", "", "_total_pages");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace hyperv_host_filter

void check_hyperv_host(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using hyperv_host_filter::filter;
  using hyperv_host_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter f;
  filter_helper.add_options("", "health_critical > 0", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}",
                           "${health_ok} VMs ok, ${health_critical} critical, ${partitions} partitions on ${logical_processors} logical processors "
                           "(${virtual_processors} virtual)",
                           "vms", "No Hyper-V counters found", "");
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("health_ok");
  f.add_manual_perf("health_critical");
  f.add_manual_perf("logical_processors");
  f.add_manual_perf("virtual_processors");
  f.add_manual_perf("partitions");

  std::map<std::string, double> values;
  try {
    values = fetch_host_counters();
  } catch (const PDH::pdh_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, counters_error(std::string(kHealthObject) + ", " + kHypervisorObject, e));
  }

  auto obj = std::make_shared<filter_obj>();
  obj->health_ok = static_cast<long long>(value_of(values, "Health Ok"));
  obj->health_critical = static_cast<long long>(value_of(values, "Health Critical"));
  obj->logical_processors = static_cast<long long>(value_of(values, "Logical Processors"));
  obj->virtual_processors = static_cast<long long>(value_of(values, "Virtual Processors"));
  obj->partitions = static_cast<long long>(value_of(values, "Partitions"));
  obj->total_pages = static_cast<long long>(value_of(values, "Total Pages"));
  f.match(obj);

  filter_helper.post_process(f);
}

// ---------------------------------------------------------------------------
// check_hyperv_cpu — logical processor load
// ---------------------------------------------------------------------------

namespace hyperv_cpu_filter {

struct filter_obj {
  processor_sample sample;
  explicit filter_obj(processor_sample sample) : sample(std::move(sample)) {}

  std::string show() const { return sample.processor + ": " + std::to_string(static_cast<long long>(sample.total_run_time)) + "%"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("processor", [](auto obj) { return obj->sample.processor; },
                             "Logical processor instance ('Hv LP 0', 'Hv LP 1', ...) or 'total' for the average over all of them");
    registry_.add_numbers("total_run_time", parsers::where::type_float, [](auto obj) { return static_cast<long long>(obj->sample.total_run_time); },
                          [](auto obj) { return obj->sample.total_run_time; },
                          "% of time the logical processor ran guest or hypervisor code (the host's real CPU usage, which Task Manager on the host "
                          "under-reports)")
        .add_int_perf("%", "", "");
    registry_.add_numbers("guest_run_time", parsers::where::type_float, [](auto obj) { return static_cast<long long>(obj->sample.guest_run_time); },
                          [](auto obj) { return obj->sample.guest_run_time; }, "% of time spent running guest (and root partition) code")
        .add_int_perf("%", "", "_guest");
    registry_.add_numbers("hypervisor_run_time", parsers::where::type_float,
                          [](auto obj) { return static_cast<long long>(obj->sample.hypervisor_run_time); },
                          [](auto obj) { return obj->sample.hypervisor_run_time; }, "% of time spent in the hypervisor itself (scheduling, intercepts)")
        .add_int_perf("%", "", "_hypervisor");
    registry_.add_numbers("idle_time", parsers::where::type_float, [](auto obj) { return static_cast<long long>(obj->sample.idle_time); },
                          [](auto obj) { return obj->sample.idle_time; }, "% of time the logical processor was idle")
        .add_int_perf("%", "", "_idle");
    registry_.add_numbers("context_switches", parsers::where::type_float, [](auto obj) { return static_cast<long long>(obj->sample.context_switches); },
                          [](auto obj) { return obj->sample.context_switches; }, "Virtual processor context switches per second on the logical processor")
        .add_int_perf("", "", "_context_switches");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace hyperv_cpu_filter

void check_hyperv_cpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using hyperv_cpu_filter::filter;
  using hyperv_cpu_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);
  bool averages = true;

  filter f;
  filter_helper.add_options("total_run_time > 80", "total_run_time > 90", "processor = 'total'", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${processor}: ${total_run_time}% total (${guest_run_time}% guest, ${hypervisor_run_time}% hypervisor)",
                           "${processor}", "No logical processor counters found", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("averages", po::value<bool>(&averages)->implicit_value(true)->default_value(true),
        "Sample the counters twice, one second apart. The run-time counters are rates, so this is what gives them a value; "
        "averages=false skips the wait and reports 0 for every rate.")
    ;
  // clang-format on
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("total_run_time");
  f.add_manual_perf("guest_run_time");
  f.add_manual_perf("hypervisor_run_time");

  PDH::object_instance_values instances;
  try {
    instances = PDH::gather_object_instances(
        kLogicalProcessorObject, {"% Total Run Time", "% Guest Run Time", "% Hypervisor Run Time", "% Idle Time", "Context Switches/sec"}, averages);
  } catch (const PDH::pdh_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, counters_error(kLogicalProcessorObject, e));
  }

  std::vector<processor_sample> processors;
  for (const auto &entry : instances) {
    processor_sample sample;
    sample.processor = entry.first;
    sample.total_run_time = value_of(entry.second, "% Total Run Time");
    sample.guest_run_time = value_of(entry.second, "% Guest Run Time");
    sample.hypervisor_run_time = value_of(entry.second, "% Hypervisor Run Time");
    sample.idle_time = value_of(entry.second, "% Idle Time");
    sample.context_switches = value_of(entry.second, "Context Switches/sec");
    processors.push_back(sample);
  }
  for (const processor_sample &sample : with_total(processors)) f.match(std::make_shared<filter_obj>(sample));

  filter_helper.post_process(f);
}

}  // namespace check_hyperv
