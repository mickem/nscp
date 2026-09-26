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
// An absent gauge renders as the same word the keyword compares equal to.
std::string human_bytes(const boost::optional<long long> &value, const parsers::where::evaluation_context &context) {
  if (!value) return "unknown";
  return str::format::format_byte_units(value.value(), context->get_number_format());
}

std::string plain_bytes(const boost::optional<long long> &value) { return value ? str::format::format_byte_units(value.value()) : "unknown"; }

double rate_of(const unsigned long long cur, const unsigned long long prev, const double dt) {
  if (cur < prev || dt <= 0) return 0.0;
  return static_cast<double>(cur - prev) / dt;
}
}  // namespace

std::string kernel_memory_obj::get_slab_human(parsers::where::evaluation_context context) const { return human_bytes(slab, context); }
std::string kernel_memory_obj::get_slab_unreclaimable_human(parsers::where::evaluation_context context) const {
  return human_bytes(slab_unreclaimable, context);
}
std::string kernel_memory_obj::get_wired_human(parsers::where::evaluation_context context) const { return human_bytes(wired, context); }
std::string kernel_memory_obj::get_compressed_human(parsers::where::evaluation_context context) const { return human_bytes(compressed, context); }
std::string kernel_memory_obj::get_cache_human(parsers::where::evaluation_context context) const {
  return str::format::format_byte_units(cache, context->get_number_format());
}

std::string kernel_memory_obj::show() const {
  // Debug output, so the plain rendering rather than the check's number format
  // (which lives on the evaluation context and is not in reach here).
  if (!slab && wired) {
    return "wired " + plain_bytes(wired) + ", compressed " + plain_bytes(compressed) + ", cache " + str::format::format_byte_units(cache);
  }
  return "slab " + plain_bytes(slab) + " (" + plain_bytes(slab_unreclaimable) + " unreclaimable), cache " + str::format::format_byte_units(cache);
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
  registry_.add_optional_int_var("slab", type_size, [](auto obj) { return obj->get_slab(); }, "unknown",
                        "Total kernel slab allocator bytes (Slab in /proc/meminfo; supports size units, e.g. 'slab > 2G'). 'unknown' on macOS, which has no slab allocator")
      .add_optional_int_var("slab_reclaimable", type_size, [](auto obj) { return obj->get_slab_reclaimable(); }, "unknown",
                   "Reclaimable slab bytes the kernel can drop under pressure, e.g. dentry/inode caches (SReclaimable in /proc/meminfo). 'unknown' on macOS")
      .add_optional_int_var("slab_unreclaimable", type_size, [](auto obj) { return obj->get_slab_unreclaimable(); }, "unknown",
                   "Unreclaimable (pinned) slab bytes (SUnreclaim in /proc/meminfo) — steady growth here is the classic kernel/driver leak signal. 'unknown' on macOS, where wired is the counterpart")
      .add_optional_int_var("wired", type_size, [](auto obj) { return obj->get_wired(); }, "unknown",
                   "macOS: wired memory in bytes - pages the kernel has pinned and cannot page out, its own allocations included; steady growth is the kernel/driver leak signal. 'unknown' on Linux")
      .add_optional_int_var("compressed", type_size, [](auto obj) { return obj->get_compressed(); }, "unknown",
                   "macOS: bytes held by the memory compressor - growth means memory pressure the host is absorbing by compressing rather than swapping. 'unknown' on Linux")
      .add_int_var("cache", type_size, &kernel_memory_obj::get_cache, "Page-cache bytes (Cached in /proc/meminfo; file-backed pages on macOS)");
  registry_.add_float("page_faults_per_sec", &kernel_memory_obj::get_page_faults,
                      "Total page faults per second, soft + hard (pgfault in /proc/vmstat, faults in the macOS VM statistics). Dominated by cheap soft "
                      "faults and routinely very large on a healthy host — alert on major_faults_per_sec instead")
      .add_float("major_faults_per_sec", &kernel_memory_obj::get_major_faults,
                 "Major (hard) faults per second (pgmajfault in /proc/vmstat; pageins on macOS): faults that had to read from disk — the fault-storm signal");
  // Render the byte gauges human-readable; expressions keep comparing bytes.
  registry_.add_human_string_context("slab", &kernel_memory_obj::get_slab_human, "Total slab as a human-readable size")
      .add_human_string_context("slab_unreclaimable", &kernel_memory_obj::get_slab_unreclaimable_human, "Unreclaimable slab as a human-readable size")
      .add_human_string_context("wired", &kernel_memory_obj::get_wired_human, "Wired memory as a human-readable size")
      .add_human_string_context("compressed", &kernel_memory_obj::get_compressed_human, "Compressed memory as a human-readable size")
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
  // The detail line names the gauges this kernel has: slab on Linux, wired
  // and compressed on macOS. Decided from the row rather than the platform,
  // so the shared code carries no platform test.
  const bool has_slab = static_cast<bool>(data.slab) || !data.wired;
  filter_helper.add_syntax("${status}: ${list}",
                           has_slab ? "slab ${slab} (${slab_unreclaimable} unreclaimable), cache ${cache}, ${major_faults_per_sec} major faults/s"
                                    : "wired ${wired}, compressed ${compressed}, cache ${cache}, ${major_faults_per_sec} major faults/s",
                           "kernel", "", "");
  // Gauges a kernel does not have emit no perf data, so one list serves both.
  filter_helper.set_default_perf_config("extra(slab;slab_reclaimable;slab_unreclaimable;wired;compressed;cache;page_faults_per_sec;major_faults_per_sec)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter_)) return;

  const std::shared_ptr<kernel_memory_obj> record(new kernel_memory_obj(data));
  filter_.match(record);

  filter_helper.post_process(filter_);
}

}  // namespace kernel_memory_check
