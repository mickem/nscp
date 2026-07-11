// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace defender_filter {

// Microsoft Defender status (a single MSFT_MpComputerStatus instance). Where
// Security Center (check_antivirus) only exposes coarse enabled/up-to-date bits
// for any registered AV, this surfaces the Defender-specific depth: signature
// and scan ages, real-time and tamper protection, engine/signature versions.
struct filter_obj {
  filter_obj()
      : enabled(0), realtime_enabled(0), tamper_protection(0), signature_age(-1), quick_scan_age(-1), full_scan_age(-1) {}

  long long get_enabled() const { return enabled; }
  long long get_realtime_enabled() const { return realtime_enabled; }
  long long get_tamper_protection() const { return tamper_protection; }
  long long get_signature_age() const { return signature_age; }
  long long get_quick_scan_age() const { return quick_scan_age; }
  long long get_full_scan_age() const { return full_scan_age; }
  std::string get_engine_version() const { return engine_version; }
  std::string get_signature_version() const { return signature_version; }
  std::string get_product_version() const { return product_version; }
  std::string show() const { return "Microsoft Defender"; }

  long long enabled;             // antivirus/service enabled
  long long realtime_enabled;    // real-time protection on
  long long tamper_protection;   // tamper protection on
  long long signature_age;       // AV signature age in days (-1 unknown)
  long long quick_scan_age;      // days since last quick scan (-1 never/unknown)
  long long full_scan_age;       // days since last full scan (-1 never/unknown)
  std::string engine_version;    // AMEngineVersion
  std::string signature_version; // AntivirusSignatureVersion
  std::string product_version;   // AMProductVersion
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace defender_filter

namespace defender_source {
// Windows only (WMI MSFT_MpComputerStatus in root\Microsoft\Windows\Defender).
// When Defender is not the active antivirus the query fails; that is reported as
// zero rows (UNKNOWN via empty-state), NOT as a hard error. The Unix stub sets
// `error`.
void gather(std::vector<defender_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace defender_source

namespace check_defender_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}

