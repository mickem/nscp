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

// Linux reader (secureboot_source_linux.cpp), split from gather() into steps
// that each do one thing, so tests can drive the parsing and the decision
// without a filesystem and the file handling against a fixture directory.

// A UEFI boolean variable (SecureBoot, SetupMode) as the reader found it.
enum class flag_state {
  absent,     // neither interface has the variable
  off,        // value 0
  on,         // value 1
  unreadable  // it exists but could not be read or does not hold a boolean; `error` says why
};
struct efi_flag {
  flag_state state = flag_state::absent;
  std::string error;
};
// Whether the EFI variable interfaces list anything at all, which is what tells
// "firmware without this variable" from "the variables cannot be seen".
enum class variable_listing {
  some,   // at least one variable is listed
  none,   // the directories are missing or empty (efivarfs not mounted)
  failed  // a directory exists but cannot be listed
};

// The value byte of an EFI variable read through efivarfs, whose files are the
// 4-byte attribute word followed by the data. These variables are one byte, so
// anything but exactly five bytes is an error: a truncated file, or a variable
// that is not the boolean this check takes it for (the kernel rejects such a
// size the same way). False with `error` set then.
bool parse_efivarfs_byte(const std::string &content, long long &value, std::string &error);
// The same for the legacy sysfs `data` file, which holds the value alone:
// exactly one byte.
bool parse_sysfs_byte(const std::string &content, long long &value, std::string &error);
// A UEFI boolean is 0 or 1. Any other value is `unreadable`, not "off": the
// kernel reads non-zero as on, and the check should not pick a side.
efi_flag decode_flag(long long value);
// Read one boolean global variable below `efi_dir`: efivarfs first, then the
// legacy sysfs interface. A file that is missing is `absent`; one that exists
// but cannot be read or parsed is `unreadable` with the path in `error`.
efi_flag read_flag(const std::string &efi_dir, const std::string &name);
// Turn what was read into the row, or into `error` (and false) when the answer
// is not known:
//  - SecureBoot on/off: supported=1, enabled accordingly, setup_mode from
//    SetupMode when that is on/off and unknown otherwise (it is auxiliary: an
//    unreadable SetupMode does not fail a check whose main answer is known);
//  - SecureBoot absent, variables listed: firmware without Secure Boot
//    support, supported=0 enabled=0;
//  - SecureBoot absent and no variables listed, or the listing failed, or
//    SecureBoot unreadable: an error, never a made-up "disabled".
bool build_state(const efi_flag &secure_boot, const efi_flag &setup_mode, variable_listing listing, const std::string &efi_dir,
                 secureboot_filter::filter_obj &row, std::string &error);
// Read the Secure Boot state below `efi_dir` (normally /sys/firmware/efi):
//  - no `efi_dir` at all: a legacy BIOS boot, supported=0 enabled=0 (as on
//    Windows, not an error);
//  - `efi_dir` exists: read_flag() and build_state() as above;
//  - `efi_dir` cannot be examined (anything but "does not exist"): `error`,
//    since that is not evidence of a BIOS boot.
// On error no row is added, so the check reports UNKNOWN.
void gather_from(const std::string &efi_dir, std::vector<secureboot_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace secureboot_source

namespace check_secureboot_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
