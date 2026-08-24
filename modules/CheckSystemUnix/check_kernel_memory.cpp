// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_kernel_memory.h"

#include <chrono>
#include <fstream>
#include <locale>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>
#include <str/format.hpp>
#include <thread>

namespace kernel_memory_check {

using parsers::where::type_size;

namespace {
std::string read_file(const std::string &path) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) return "";
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

double rate_of(const unsigned long long cur, const unsigned long long prev, const double dt) {
  if (cur < prev || dt <= 0) return 0.0;
  return static_cast<double>(cur - prev) / dt;
}
}  // namespace

std::string kernel_memory_obj::get_slab_human(parsers::where::evaluation_context context) const {
  return str::format::format_byte_units(slab, context->get_number_format());
}
std::string kernel_memory_obj::get_slab_unreclaimable_human(parsers::where::evaluation_context context) const {
  return str::format::format_byte_units(slab_unreclaimable, context->get_number_format());
}
std::string kernel_memory_obj::get_cache_human(parsers::where::evaluation_context context) const {
  return str::format::format_byte_units(cache, context->get_number_format());
}

std::string kernel_memory_obj::show() const {
  // Debug output, so the plain rendering rather than the check's number format
  // (which lives on the evaluation context and is not in reach here).
  return "slab " + str::format::format_byte_units(slab) + " (" + str::format::format_byte_units(slab_unreclaimable) + " unreclaimable), cache " +
         str::format::format_byte_units(cache);
}

// /proc/meminfo lines look like "Slab:  123456 kB".
meminfo_kernel parse_meminfo_kernel(const std::string &content) {
  meminfo_kernel out;
  bool have_slab = false, have_cache = false;
  std::istringstream lines(content);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream is(line);
    is.imbue(std::locale("C"));
    std::string key;
    long long value = 0;
    if (!(is >> key >> value)) continue;
    if (key == "Slab:") {
      out.slab = value * 1024;
      have_slab = true;
    } else if (key == "SReclaimable:") {
      out.slab_reclaimable = value * 1024;
    } else if (key == "SUnreclaim:") {
      out.slab_unreclaimable = value * 1024;
    } else if (key == "Cached:") {
      out.cache = value * 1024;
      have_cache = true;
    }
  }
  out.valid = have_slab && have_cache;
  return out;
}

vmstat_faults parse_vmstat_faults(const std::string &content) {
  vmstat_faults out;
  bool have_fault = false, have_major = false;
  std::istringstream lines(content);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream is(line);
    is.imbue(std::locale("C"));
    std::string key;
    is >> key;
    if (key == "pgfault") {
      is >> out.pgfault;
      have_fault = true;
    } else if (key == "pgmajfault") {
      is >> out.pgmajfault;
      have_major = true;
    }
  }
  out.valid = have_fault && have_major;
  return out;
}

kernel_memory_obj compute_kernel_memory(const meminfo_kernel &mem, const vmstat_faults &prev, const vmstat_faults &cur, const double elapsed_seconds) {
  kernel_memory_obj o;
  o.slab = mem.slab;
  o.slab_reclaimable = mem.slab_reclaimable;
  o.slab_unreclaimable = mem.slab_unreclaimable;
  o.cache = mem.cache;
  o.page_faults = rate_of(cur.pgfault, prev.pgfault, elapsed_seconds);
  o.major_faults = rate_of(cur.pgmajfault, prev.pgmajfault, elapsed_seconds);
  return o;
}

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_int_var("slab", type_size, &kernel_memory_obj::get_slab,
                        "Total kernel slab allocator bytes (Slab in /proc/meminfo; supports size units, e.g. 'slab > 2G')")
      .add_int_var("slab_reclaimable", type_size, &kernel_memory_obj::get_slab_reclaimable,
                   "Reclaimable slab bytes the kernel can drop under pressure, e.g. dentry/inode caches (SReclaimable in /proc/meminfo)")
      .add_int_var("slab_unreclaimable", type_size, &kernel_memory_obj::get_slab_unreclaimable,
                   "Unreclaimable (pinned) slab bytes (SUnreclaim in /proc/meminfo) — steady growth here is the classic kernel/driver leak signal")
      .add_int_var("cache", type_size, &kernel_memory_obj::get_cache, "Page-cache bytes (Cached in /proc/meminfo)");
  registry_.add_float("page_faults_per_sec", &kernel_memory_obj::get_page_faults,
                      "Total page faults per second, soft + hard (pgfault in /proc/vmstat). Dominated by cheap soft faults and routinely very large on a "
                      "healthy host — alert on major_faults_per_sec instead")
      .add_float("major_faults_per_sec", &kernel_memory_obj::get_major_faults,
                 "Major (hard) faults per second (pgmajfault in /proc/vmstat): faults that had to read from disk — the fault-storm signal");
  // Render the byte gauges human-readable; expressions keep comparing bytes.
  registry_.add_human_string_context("slab", &kernel_memory_obj::get_slab_human, "Total slab as a human-readable size")
      .add_human_string_context("slab_unreclaimable", &kernel_memory_obj::get_slab_unreclaimable_human, "Unreclaimable slab as a human-readable size")
      .add_human_string_context("cache", &kernel_memory_obj::get_cache_human, "Page cache as a human-readable size");
  // clang-format on
}

void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const kernel_memory_obj &data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter> filter_helper(request, response, mdata);

  filter filter_;
  // No default thresholds: slab sizes are absolute and host-specific
  // (baseline, then pin) and fault-storm levels are site policy. Mirrors the
  // Windows check_kernel_memory contract.
  filter_helper.add_options("", "", "", filter_.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "slab ${slab} (${slab_unreclaimable} unreclaimable), cache ${cache}, ${major_faults_per_sec} major faults/s",
                           "kernel", "", "");
  filter_helper.set_default_perf_config("extra(slab;slab_reclaimable;slab_unreclaimable;cache;page_faults_per_sec;major_faults_per_sec)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter_)) return;

  const std::shared_ptr<kernel_memory_obj> record(new kernel_memory_obj(data));
  filter_.match(record);

  filter_helper.post_process(filter_);
}

void check_kernel_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  const vmstat_faults prev = parse_vmstat_faults(read_file("/proc/vmstat"));
  if (!prev.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/vmstat");
  }
  // Rates are divided by the interval actually slept, not the requested one:
  // on a loaded host the wake-up can be noticeably late and a fixed 1.0 would
  // overstate the fault rates.
  const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  const vmstat_faults cur = parse_vmstat_faults(read_file("/proc/vmstat"));
  const meminfo_kernel mem = parse_meminfo_kernel(read_file("/proc/meminfo"));
  if (!cur.valid || !mem.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/vmstat or /proc/meminfo");
  }
  check_from(request, response, compute_kernel_memory(mem, prev, cur, elapsed));
}

}  // namespace kernel_memory_check
