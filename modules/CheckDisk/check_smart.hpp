// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <functional>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <vector>

#include "smart_data.hpp"

// check_smart: SMART health and predictive-failure attributes per physical
// drive - reallocated and pending sectors, uncorrectable errors, media wear
// and temperature, plus the drive's own pass/fail verdict - read natively
// (no smartctl): DeviceIoControl and root\WMI FailurePredict on Windows,
// SG_IO ATA pass-through and the NVMe admin ioctl on Linux.
//
// The record and the decoders live in smart_data.hpp; the readers in
// smart_source_win.cpp / smart_source_linux.cpp.
namespace smart_check {

typedef parsers::where::filter_handler_impl<std::shared_ptr<smart_drive> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<smart_drive, filter_obj_handler> filter_type;

// Testable variant: runs the check over the drives `reader` returns. The
// reader is only called once the options parsed, so `help` and a bad option
// never touch the hardware.
void check_smart_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                      const std::function<std::vector<smart_drive>()> &reader);

// Live check: reads every drive (read_drives()) and runs check_smart_from.
void check_smart(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace smart_check
