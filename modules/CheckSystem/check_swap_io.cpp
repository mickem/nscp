// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_swap_io.hpp"

#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_query.hpp>
#include <win/sysinfo/win_sysinfo.hpp>  // pulls <win/windows.hpp> (GetSystemInfo/Sleep/SYSTEM_INFO)

namespace swap_io_check {

swap_obj make_swap_obj(double pages_in_per_sec, double pages_out_per_sec, long long swap_count, long long page_size) {
  swap_obj o;
  o.swap_count = swap_count;
  o.swap_in = pages_in_per_sec < 0 ? 0.0 : pages_in_per_sec;
  o.swap_out = pages_out_per_sec < 0 ? 0.0 : pages_out_per_sec;
  o.swap_in_bytes = static_cast<long long>(o.swap_in * static_cast<double>(page_size));
  o.swap_out_bytes = static_cast<long long>(o.swap_out * static_cast<double>(page_size));
  return o;
}

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &swap_obj::get_name, "Always 'swap' (single aggregate row)");
  registry_.add_int_var("swap_count", &swap_obj::get_swap_count, "Number of page files on the system");
  // Perf is emitted via the extra() perf-config; the default perf generator
  // names each metric "io_<keyword>" (e.g. io_swap_in, io_swap_in_bytes),
  // matching the Linux check_swap_io.
  registry_.add_float("swap_in", &swap_obj::get_swap_in, "Pages paged in from disk per second");
  registry_.add_float("swap_out", &swap_obj::get_swap_out, "Pages paged out to disk per second");
  registry_.add_int_var("swap_in_bytes", &swap_obj::get_swap_in_bytes, "Bytes paged in per second");
  registry_.add_int_var("swap_out_bytes", &swap_obj::get_swap_out_bytes, "Bytes paged out per second");
}

void check_swap_io_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                        double pages_in_per_sec, double pages_out_per_sec, long long swap_count, long long page_size) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${swap_count} page file(s), in ${swap_in} pages/s, out ${swap_out} pages/s", "io", "", "");
  filter_helper.set_default_perf_config("extra(swap_in;swap_out;swap_in_bytes;swap_out_bytes)");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<swap_obj> record(new swap_obj(make_swap_obj(pages_in_per_sec, pages_out_per_sec, swap_count, page_size)));
  filter.match(record);

  filter_helper.post_process(filter);
}

namespace {
// Create a static, English-named counter. The paths below are hard-coded in
// English, so resolution=english lets PDH translate them to the running
// system's locale (see check_pdh's `resolution` option).
PDH::pdh_instance make_paging_counter(PDH::PDHQuery &pdh, const std::string &path, const std::string &alias) {
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

void check_swap_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    PDH::PDHQuery pdh;
    const PDH::pdh_instance in_counter = make_paging_counter(pdh, "\\Memory\\Pages Input/sec", "pages_input");
    const PDH::pdh_instance out_counter = make_paging_counter(pdh, "\\Memory\\Pages Output/sec", "pages_output");

    // Rate counters need two samples an interval apart; take a 1s window (the
    // same interval the Linux implementation samples /proc/vmstat over).
    pdh.open();
    pdh.collect();
    Sleep(1000);
    pdh.gatherData();
    pdh.close();

    const double pages_in = in_counter->get_float_value();
    const double pages_out = out_counter->get_float_value();

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const long long page_size = si.dwPageSize > 0 ? static_cast<long long>(si.dwPageSize) : 4096;

    const long long swap_count = static_cast<long long>(windows::system_info::get_pagefile_info().size());

    check_swap_io_from(request, response, pages_in, pages_out, swap_count, page_size);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to sample paging counters: " + std::string(e.what()));
  }
}

}  // namespace swap_io_check
