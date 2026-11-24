// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_gpu.hpp"

#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/format.hpp>

namespace gpu_check {

using parsers::where::evaluation_context;
using parsers::where::type_int;
using parsers::where::type_size;

namespace {
typedef std::shared_ptr<gpu_adapter> obj_type;

std::string human_bytes(const long long bytes, const evaluation_context &context) {
  return str::format::format_byte_units(bytes, context->get_number_format());
}
}  // namespace

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("name", [](obj_type o) { return o->name; },
                           "Adapter name as the driver reports it (e.g. 'NVIDIA RTX A4000'); the luid when the adapter could not be identified. "
                           "Adapters that would share a name get ' #0', ' #1', ... in DXGI enumeration order")
      .add_string_var("luid", [](obj_type o) { return o->luid; },
                      "Locally unique id of the adapter, as it appears in the counter instance names (0xHHHHHHHH_0xLLLLLLLL). Changes on reboot")
      .add_string_var("adapter_type", [](obj_type o) { return o->adapter_type; },
                      "hardware, software (the Microsoft Basic Render Driver) or unknown (the adapter could not be identified)")
      .add_string_var("busiest_engine", [](obj_type o) { return o->busiest_engine; },
                      "Type of the engine behind the utilization keyword (3D, Copy, VideoDecode, Compute_0, ...); empty when the adapter is idle");

  registry_.add_int_var("phys", type_int, [](obj_type o) { return o->phys; },
                        "Physical adapter index; above 0 only for linked adapters that share one luid").no_perf();
  registry_.add_int_var("utilization", type_int, [](obj_type o) { return o->utilization; },
                        "Load of the busiest engine of the adapter in percent, sampled over one second (what Task Manager shows for the GPU)")
      .add_int_perf("%", "", "_utilization");
  registry_.add_int_var("utilization_3d", type_int, [](obj_type o) { return o->utilization_3d; }, "Load of the busiest 3D engine (3D, High Priority 3D, ...) in percent")
      .add_int_perf("%", "", "_utilization_3d");
  registry_.add_int_var("utilization_copy", type_int, [](obj_type o) { return o->utilization_copy; }, "Load of the busiest copy engine in percent")
      .add_int_perf("%", "", "_utilization_copy");
  registry_.add_int_var("utilization_video_decode", type_int, [](obj_type o) { return o->utilization_video_decode; },
                        "Load of the busiest video decode engine (VideoDecode, or a combined Video Codec engine) in percent")
      .add_int_perf("%", "", "_utilization_video_decode");
  registry_.add_int_var("utilization_video_encode", type_int, [](obj_type o) { return o->utilization_video_encode; },
                        "Load of the busiest video encode engine (VideoEncode, or a combined Video Codec engine) in percent")
      .add_int_perf("%", "", "_utilization_video_encode");
  registry_.add_int_var("utilization_compute", type_int, [](obj_type o) { return o->utilization_compute; },
                        "Load of the busiest compute engine (Compute_n, High Priority Compute, Cuda) in percent")
      .add_int_perf("%", "", "_utilization_compute");

  registry_.add_int_var("dedicated_used", type_size, [](obj_type o) { return o->dedicated_used; },
                        "Dedicated (on-board) GPU memory in use, in bytes (supports size units, e.g. 'dedicated_used > 6G'); renders human-readable")
      .add_int_perf("B", "", "_dedicated_used");
  registry_.add_int_var("dedicated_total", type_size, [](obj_type o) { return o->dedicated_total; },
                        "Dedicated GPU memory the adapter has, in bytes; 0 when unknown (the adapter could not be identified, or the 32-bit agent "
                        "cannot tell the size of a card with 4 GB or more)")
      .add_int_perf("B", "", "_dedicated_total");
  registry_.add_int_var("dedicated_pct", type_int, [](obj_type o) { return o->get_dedicated_pct(); },
                        "Dedicated GPU memory in use, in percent of dedicated_total; 0 when the total is unknown")
      .add_int_perf("%", "", "_dedicated_pct");
  registry_.add_int_var("shared_used", type_size, [](obj_type o) { return o->shared_used; },
                        "Shared GPU memory (system RAM the adapter borrows) in use, in bytes")
      .add_int_perf("B", "", "_shared_used");
  registry_.add_int_var("shared_total", type_size, [](obj_type o) { return o->shared_total; },
                        "System RAM the adapter may borrow as shared GPU memory, in bytes; 0 when unknown (as for dedicated_total)")
      .add_int_perf("B", "", "_shared_total");
  registry_.add_int_var("shared_pct", type_int, [](obj_type o) { return o->get_shared_pct(); },
                        "Shared GPU memory in use, in percent of shared_total; 0 when the total is unknown")
      .add_int_perf("%", "", "_shared_pct");
  registry_.add_int_var("committed", type_size, [](obj_type o) { return o->committed; },
                        "GPU memory committed on the adapter, in bytes (counter 'Total Committed')")
      .add_int_perf("B", "", "_committed");
  registry_.add_int_var("local_used", type_size, [](obj_type o) { return o->local_used; },
                        "Memory in use in the adapter's local segments, in bytes (counter set 'GPU Local Adapter Memory', all partitions)")
      .add_int_perf("B", "", "_local_used");
  registry_.add_int_var("processes", type_int, [](obj_type o) { return o->processes; },
                        "Number of processes holding dedicated or shared memory on the adapter")
      .add_int_perf("", "", "_processes");

  // Render the byte gauges human-readable; expressions keep comparing bytes.
  registry_.add_human_string_context("dedicated_used", [](obj_type o, evaluation_context c) { return human_bytes(o->dedicated_used, c); },
                                     "Dedicated GPU memory in use as a human-readable size")
      .add_human_string_context("dedicated_total", [](obj_type o, evaluation_context c) { return human_bytes(o->dedicated_total, c); },
                                "Dedicated GPU memory of the adapter as a human-readable size")
      .add_human_string_context("shared_used", [](obj_type o, evaluation_context c) { return human_bytes(o->shared_used, c); },
                                "Shared GPU memory in use as a human-readable size")
      .add_human_string_context("shared_total", [](obj_type o, evaluation_context c) { return human_bytes(o->shared_total, c); },
                                "Shared GPU memory limit as a human-readable size")
      .add_human_string_context("committed", [](obj_type o, evaluation_context c) { return human_bytes(o->committed, c); },
                                "Committed GPU memory as a human-readable size")
      .add_human_string_context("local_used", [](obj_type o, evaluation_context c) { return human_bytes(o->local_used, c); },
                                "Local segment usage as a human-readable size");
  // clang-format on
}

void check_gpu_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                    const std::vector<gpu_adapter> &adapters) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  // The defaults watch memory, not load: a GPU pinned at 100% is the normal
  // state of a render or training box, while dedicated memory running out is
  // what makes a session host fall back to software rendering. The default
  // filter drops the Microsoft Basic Render Driver every Windows host carries,
  // so a host without a real GPU lands in the empty state.
  filter_helper.add_options("dedicated_pct > 85", "dedicated_pct > 95", "adapter_type != 'software'", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${utilization}% busy, ${dedicated_used} dedicated memory used (${dedicated_pct}%)", "${name}",
                           "No GPU adapters found", "");
  filter_helper.set_default_perf_config("extra(utilization;dedicated_used)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  for (const gpu_adapter &adapter : adapters) {
    const std::shared_ptr<gpu_adapter> record(new gpu_adapter(adapter));
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

std::string enumeration_error(const std::string &reason) {
  return "GPU adapters could not be identified through DXGI, so their names, types and memory sizes are unknown (" + reason + ")";
}

void check_gpu_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                    const counter_snapshot &snapshot, const adapter_enumeration &enumeration) {
  if (!enumeration.error.empty()) return nscapi::protobuf::functions::set_response_bad(*response, enumeration_error(enumeration.error));
  check_gpu_from(request, response, build_adapters(snapshot, enumeration.adapters));
}

}  // namespace gpu_check
