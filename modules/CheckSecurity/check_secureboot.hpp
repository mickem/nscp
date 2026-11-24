// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace secureboot_filter {

// A single result describing the UEFI Secure Boot state.
struct filter_obj {
  filter_obj() : enabled(0), supported(0) {}
  long long get_enabled() const { return enabled; }
  long long get_supported() const { return supported; }
  boost::optional<long long> get_setup_mode() const { return setup_mode; }
  std::string show() const { return enabled ? "enabled" : "disabled"; }

  long long enabled;    // 1 if UEFI Secure Boot is enabled
  long long supported;  // 1 if the platform exposes the Secure Boot state
  // 1 if the firmware is in Setup Mode (no Platform Key enrolled, so Secure Boot
  // is not enforced and anyone can enrol keys), 0 if in User Mode. Empty when it
  // is not known: legacy BIOS boots, firmware without the variable, and Windows
  // (which does not read it).
  boost::optional<long long> setup_mode;
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace secureboot_filter

namespace secureboot_source {
// Windows: registry SecureBoot\State. Linux: the UEFI SecureBoot/SetupMode
// variables under /sys/firmware/efi. macOS: a stub that sets `error`.
void gather(std::vector<secureboot_filter::filter_obj_ptr> &out, std::string &error);

// Linux reader (secureboot_source_linux.cpp), split from gather() so tests can
// point it at a fixture directory.
//
// The value byte of an EFI variable read through efivarfs, whose files are the
// 4-byte attribute word followed by the data. False (and `error` set) when the
// content is too short to hold one.
bool parse_efivarfs_byte(const std::string &content, long long &value, std::string &error);
// Read the Secure Boot state below `efi_dir` (normally /sys/firmware/efi):
//  - no `efi_dir` at all: a legacy BIOS boot, supported=0 enabled=0 (as on
//    Windows, not an error);
//  - efivarfs (`efi_dir`/efivars) or the legacy sysfs interface
//    (`efi_dir`/vars/<name>/data) holds the SecureBoot variable: supported=1,
//    enabled = (value == 1) unless shim's MokSBStateRT says validation is
//    disabled, setup_mode from SetupMode when present;
//  - EFI variables readable but no SecureBoot variable: firmware without Secure
//    Boot support, supported=0 enabled=0;
//  - `efi_dir` or the variables cannot be read at all (efivarfs not mounted, I/O or
//    permission error, malformed content): `error` is set and no row is added,
//    so the check reports UNKNOWN rather than a made-up "disabled".
void gather_from(const std::string &efi_dir, std::vector<secureboot_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace secureboot_source

namespace check_secureboot_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
