// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

#include "gpu_counters.hpp"

// check_gpu: GPU utilisation and memory per adapter, from the vendor-neutral
// "GPU Engine" / "GPU Adapter Memory" / "GPU Local Adapter Memory" / "GPU
// Process Memory" performance counter sets Windows has shipped since Windows
// 10 1709 (the numbers behind Task Manager's GPU graphs). No driver SDK is
// involved, so an NVIDIA, AMD or Intel adapter reads the same.
//
// The record model and the aggregation live in gpu_counters.hpp; the PDH and
// DXGI readers in gpu_source_win.cpp.
namespace gpu_check {

typedef parsers::where::filter_handler_impl<std::shared_ptr<gpu_adapter> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<gpu_adapter, filter_obj_handler> filter_type;

// Testable variant: runs the check over adapters that were already built.
void check_gpu_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                    const std::vector<gpu_adapter> &adapters);

// Live check (gpu_source_win.cpp): samples the counter sets - the engine
// utilisation is a rate, so this takes a second - and names the adapters
// through DXGI.
void check_gpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace gpu_check
