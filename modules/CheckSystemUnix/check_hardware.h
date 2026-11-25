// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_CHECK_HARDWARE_H
#define NSCP_CHECK_HARDWARE_H

#include <boost/optional.hpp>
#include <cstdint>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

namespace hardware_check {

// One populated memory module (SMBIOS type 17, Memory Device).
struct memory_module {
  std::string locator;       // Device Locator, e.g. "DIMM_A1" (Bank Locator fallback)
  long long capacity;        // bytes; 0 if unknown
  long long speed;           // configured clock in MHz (rated Speed fallback); 0 if unknown
  std::string part_number;   // Part Number
  std::string serial;        // Serial Number
  std::string manufacturer;  // Manufacturer

  memory_module() : capacity(0), speed(0) {}
};

// A single aggregate row of system/chassis/memory inventory: the unix
// counterpart to the Windows check_hardware, with the same keywords. The
// identity strings come from /sys/class/dmi/id (the kernel's copy of SMBIOS
// types 1 and 3), the memory inventory from the raw SMBIOS table
// (/sys/firmware/dmi/tables/DMI, types 16 and 17).
struct hardware_info {
  std::string vendor;          // sys_vendor
  std::string model;           // product_name
  std::string uuid;            // product_uuid, upper case (root only)
  std::string serial;          // product_serial (root only)
  long long chassis_type;      // chassis_type: raw SMBIOS chassis type; 0 unknown
  std::string chassis;         // chassis type mapped to a name
  std::string chassis_serial;  // chassis_serial (root only)
  std::string asset_tag;       // chassis_asset_tag
  std::vector<memory_module> module_details;

  // Memory inventory. Unset when it could not be read, so that `modules < 8`
  // never fires on an agent that simply may not read the SMBIOS table. It is
  // all or nothing: a malformed table, or one with no populated module, leaves
  // slots/modules/memory_speed unset, and memory is the SMBIOS sum only when
  // every module reports its size.
  boost::optional<long long> slots;         // memory sockets (type 16 "Number of Memory Devices", summed)
  boost::optional<long long> modules;       // populated modules
  boost::optional<long long> memory;        // installed memory in bytes
  boost::optional<long long> memory_speed;  // slowest module's clock (MHz)
  std::string module_list;                  // "DIMM_A1: 32GB@4800MHz; ..." for rendering
  std::string memory_source;                // "smbios", "sysfs" or "none"

  // Gather bookkeeping (not filter keywords): which sources answered.
  bool dmi_ok;     // /sys/class/dmi/id was there
  bool smbios_ok;  // the SMBIOS table parsed
  std::string gather_errors;

  hardware_info() : chassis_type(0), memory_source("none"), dmi_ok(false), smbios_ok(false) {}

  // Derive modules/memory/memory_speed/module_list from module_details.
  // Called when the SMBIOS table was read.
  void recompute();

  std::string get_vendor() const { return vendor; }
  std::string get_model() const { return model; }
  std::string get_uuid() const { return uuid; }
  std::string get_serial() const { return serial; }
  long long get_chassis_type() const { return chassis_type; }
  std::string get_chassis() const { return chassis; }
  std::string get_chassis_serial() const { return chassis_serial; }
  std::string get_asset_tag() const { return asset_tag; }
  boost::optional<long long> get_slots() const { return slots; }
  boost::optional<long long> get_modules() const { return modules; }
  boost::optional<long long> get_memory() const { return memory; }
  std::string get_memory_human(parsers::where::evaluation_context context) const;
  boost::optional<long long> get_memory_speed() const { return memory_speed; }
  std::string get_module_list() const { return module_list; }
  std::string get_memory_source() const { return memory_source; }

  std::string show() const { return vendor + " " + model; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<hardware_info> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<hardware_info, filter_obj_handler> filter_type;

// Map a raw SMBIOS chassis type (System Enclosure, SMBIOS spec 7.4.1) to a
// human-readable name; unknown values render as "Unknown (<n>)". The same
// table as the Windows check.
std::string chassis_type_to_string(long long type);

// --- SMBIOS (pure, unit-tested; the input is untrusted bytes) -----------------

// What the entry point (/sys/firmware/dmi/tables/smbios_entry_point) says
// about the table: its version and how much of it is valid.
struct smbios_entry_point {
  int major;
  int minor;
  std::uint64_t table_length;     // 2.x: exact length; 3.x: maximum size
  std::uint32_t structure_count;  // 2.x only; 0 when the entry point does not say

  smbios_entry_point() : major(0), minor(0), table_length(0), structure_count(0) {}
};

// Parse a 32-bit ("_SM_") or 64-bit ("_SM3_") entry point; none when the
// bytes are neither (or too short for the length they claim).
boost::optional<smbios_entry_point> parse_smbios_entry_point(const std::string &bytes);

struct smbios_inventory {
  // Type 1 / type 3, used where /sys/class/dmi/id has nothing.
  std::string vendor, model, serial, uuid;
  long long chassis_type;
  std::string chassis_serial, asset_tag;
  // Type 16 (system memory arrays only) / type 17.
  bool have_arrays;
  long long slots;                     // sum of "Number of Memory Devices"
  long long device_records;            // type 17 structures (one per socket), less those of non-system arrays
  std::vector<memory_module> modules;  // populated ones, same filter
  long long structures;                // structures walked
  bool truncated;                      // the walk stopped at a malformed or cut-off structure

  smbios_inventory() : chassis_type(0), have_arrays(false), slots(0), device_records(0), structures(0), truncated(false) {}
};

// Walk the structure table. Every header, formatted area, string set and
// string index is bounds-checked; the walk stops (truncated = true) at the
// first structure that does not fit, keeping what was read before it.
// The entry point, when given, caps the length and structure count.
smbios_inventory parse_smbios_table(const std::string &table, const boost::optional<smbios_entry_point> &entry);

// Format the 16 SMBIOS UUID bytes as the canonical upper-case string (the
// first three fields little-endian, as SMBIOS 2.6+ and dmidecode do; before
// 2.6 the byte order was not specified and they are read as stored). Empty
// for the "not present" (all 00) and "not settable" (all FF) values.
std::string format_smbios_uuid(const unsigned char *bytes, bool little_endian = true);

// --- Gathering ----------------------------------------------------------------

// Gather from a filesystem rooted at `root` ("" for the real one):
// <root>/sys/class/dmi/id/*, <root>/sys/firmware/dmi/tables/{DMI,smbios_entry_point}
// and, when the SMBIOS table cannot be read, the memory blocks under
// <root>/sys/devices/system/memory for the installed-memory total.
hardware_info gather_hardware_from(const std::string &root);

// Platform reader (hardware_source_linux.cpp / hardware_source_darwin.cpp).
hardware_info gather_hardware();

// The no-data result: what check() reports when the host exposes neither
// /sys/class/dmi/id nor an SMBIOS table (containers, microVMs, boards without
// SMBIOS). Exposed for the unit tests.
extern const char *const no_dmi_message;

// Testable core: renders / thresholds a pre-gathered inventory row, or
// reports the no-data result when no DMI source answered.
void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const hardware_info &info);

// Live check.
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace hardware_check

#endif  // NSCP_CHECK_HARDWARE_H
