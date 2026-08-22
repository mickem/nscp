// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_kernel_memory.hpp"

#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_query.hpp>
#include <win/sysinfo/win_sysinfo.hpp>  // pulls <win/windows.hpp> (Sleep)

namespace kernel_memory_check {

using parsers::where::type_size;

std::string kernel_memory_obj::get_pool_paged_human(parsers::where::evaluation_context context) const {
  return str::format::format_byte_units(pool_paged, context->get_number_format());
}
std::string kernel_memory_obj::get_pool_nonpaged_human(parsers::where::evaluation_context context) const {
  return str::format::format_byte_units(pool_nonpaged, context->get_number_format());
}
std::string kernel_memory_obj::get_cache_human(parsers::where::evaluation_context context) const {
  return str::format::format_byte_units(cache, context->get_number_format());
}

std::string kernel_memory_obj::show() const {
  // Debug output, so the plain rendering rather than the check's number format
  // (which lives on the evaluation context and is not in reach here).
  return "paged pool " + str::format::format_byte_units(pool_paged) + ", nonpaged pool " + str::format::format_byte_units(pool_nonpaged) + ", cache " +
         str::format::format_byte_units(cache);
}

kernel_memory_obj make_kernel_memory_obj(const long long pool_paged, const long long pool_nonpaged, const long long cache, const double page_faults,
                                         const double transition_faults, const double hard_faults) {
  kernel_memory_obj o;
  o.pool_paged = pool_paged < 0 ? 0 : pool_paged;
  o.pool_nonpaged = pool_nonpaged < 0 ? 0 : pool_nonpaged;
  o.cache = cache < 0 ? 0 : cache;
  o.page_faults = page_faults < 0 ? 0.0 : page_faults;
  o.transition_faults = transition_faults < 0 ? 0.0 : transition_faults;
  o.hard_faults = hard_faults < 0 ? 0.0 : hard_faults;
  return o;
}

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_int_var("pool_paged", type_size, &kernel_memory_obj::get_pool_paged,
                        "Paged pool bytes (counter 'Pool Paged Bytes'; supports size units, e.g. 'pool_paged > 2G'); renders human-readable")
      .add_int_var("pool_nonpaged", type_size, &kernel_memory_obj::get_pool_nonpaged,
                   "Nonpaged pool bytes (counter 'Pool Nonpaged Bytes') — steady growth here is the classic driver-leak signal")
      .add_int_var("cache", type_size, &kernel_memory_obj::get_cache, "System file-cache working set in bytes (counter 'Cache Bytes')");
  registry_.add_float("page_faults_per_sec", &kernel_memory_obj::get_page_faults,
                      "Total page faults per second (counter 'Page Faults/sec', soft + hard). Dominated by cheap soft faults and routinely very large on a "
                      "healthy host — alert on hard_faults_per_sec instead")
      .add_float("transition_faults_per_sec", &kernel_memory_obj::get_transition_faults,
                 "Transition (soft) faults per second (counter 'Transition Faults/sec'), resolved without disk I/O — the dominant soft-fault kind")
      .add_float("hard_faults_per_sec", &kernel_memory_obj::get_hard_faults,
                 "Hard faults per second (Page Reads/sec): faults that had to read from disk — the fault-storm signal");
  // Render the three byte gauges human-readable; expressions keep comparing bytes.
  registry_.add_human_string_context("pool_paged", &kernel_memory_obj::get_pool_paged_human, "Paged pool as a human-readable size")
      .add_human_string_context("pool_nonpaged", &kernel_memory_obj::get_pool_nonpaged_human, "Nonpaged pool as a human-readable size")
      .add_human_string_context("cache", &kernel_memory_obj::get_cache_human, "File cache as a human-readable size");
  // clang-format on
}

void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const kernel_memory_obj &data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  // No default thresholds: pool sizes are absolute and host-specific
  // (baseline, then pin) and fault-storm levels are site policy.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}",
                           "paged pool ${pool_paged}, nonpaged pool ${pool_nonpaged}, cache ${cache}, ${hard_faults_per_sec} hard faults/s", "kernel", "", "");
  filter_helper.set_default_perf_config("extra(pool_paged;pool_nonpaged;cache;page_faults_per_sec;transition_faults_per_sec;hard_faults_per_sec)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<kernel_memory_obj> record(new kernel_memory_obj(data));
  filter.match(record);

  filter_helper.post_process(filter);
}

namespace {
// Create a static, English-named counter (same recipe as check_swap_io):
// the paths are hard-coded in English and resolution=english lets PDH
// translate them on localized systems.
PDH::pdh_instance make_memory_counter(PDH::PDHQuery &pdh, const std::string &path, const std::string &alias) {
  PDH::pdh_object obj;
  obj.set_counter(path);
  obj.set_alias(alias);
  obj.set_strategy_static();
  obj.set_type("double");
  obj.set_resolution("english");
  PDH::pdh_instance instance = PDH::factory::create(obj);
  pdh.addCounter(instance);
  return instance;
}
}  // namespace

void check_kernel_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    PDH::PDHQuery pdh;
    const PDH::pdh_instance pool_paged = make_memory_counter(pdh, "\\Memory\\Pool Paged Bytes", "pool_paged");
    const PDH::pdh_instance pool_nonpaged = make_memory_counter(pdh, "\\Memory\\Pool Nonpaged Bytes", "pool_nonpaged");
    const PDH::pdh_instance cache = make_memory_counter(pdh, "\\Memory\\Cache Bytes", "cache");
    const PDH::pdh_instance page_faults = make_memory_counter(pdh, "\\Memory\\Page Faults/sec", "page_faults");
    const PDH::pdh_instance transition_faults = make_memory_counter(pdh, "\\Memory\\Transition Faults/sec", "transition_faults");
    // Page Reads/sec counts hard-fault *events* (disk read operations). Pages
    // Input/sec counts the pages those reads brought in — several per fault —
    // so it would overstate the fault rate this keyword documents.
    const PDH::pdh_instance hard_faults = make_memory_counter(pdh, "\\Memory\\Page Reads/sec", "hard_faults");

    // The fault counters are rates and need two samples an interval apart;
    // the byte gauges simply read their current value on the second sample.
    pdh.open();
    pdh.collect();
    Sleep(1000);
    pdh.gatherData();
    pdh.close();

    const kernel_memory_obj data = make_kernel_memory_obj(
        static_cast<long long>(pool_paged->get_float_value()), static_cast<long long>(pool_nonpaged->get_float_value()),
        static_cast<long long>(cache->get_float_value()), page_faults->get_float_value(), transition_faults->get_float_value(), hard_faults->get_float_value());

    check_from(request, response, data);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to sample kernel memory counters: " + std::string(e.what()));
  }
}

}  // namespace kernel_memory_check
