// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

namespace hardware_check {

// One physical memory module (DIMM) as reported by Win32_PhysicalMemory.
struct memory_module {
  std::string locator;       // DeviceLocator, e.g. "DIMM_A1" (BankLabel fallback)
  long long capacity;        // bytes; 0 if unknown
  long long speed;           // configured clock in MHz (Speed fallback); 0 if unknown
  std::string part_number;   // PartNumber
  std::string serial;        // SerialNumber
  std::string manufacturer;  // Manufacturer

  memory_module() : capacity(0), speed(0) {}
};

// A single aggregate row of BIOS/chassis/memory inventory, sourced from
// Win32_ComputerSystemProduct, Win32_SystemEnclosure, Win32_PhysicalMemory and
// Win32_PhysicalMemoryArray. The interesting alerts are pinned expectations
// and changes: the serial changed (re-imaged or replaced box), the chassis is
// a laptop in a server fleet, a DIMM dropped (modules/memory shrank).
struct hardware_info {
  std::string vendor;          // system vendor (Win32_ComputerSystemProduct.Vendor)
  std::string model;           // system model (.Name)
  std::string uuid;            // SMBIOS UUID (.UUID)
  std::string serial;          // system serial (.IdentifyingNumber); often blank/junk on VMs and OEM boards
  long long chassis_type;      // raw SMBIOS chassis type (Win32_SystemEnclosure.ChassisTypes[0]); 0 unknown
  std::string chassis;         // chassis type mapped to a name ("Desktop", "Rack Mount Chassis", ...)
  std::string chassis_serial;  // enclosure serial
  std::string asset_tag;       // SMBIOS asset tag
  long long slots;             // total memory sockets (Win32_PhysicalMemoryArray.MemoryDevices, summed); 0 unknown
  std::vector<memory_module> module_details;

  // Derived by recompute():
  long long modules;        // number of populated memory modules
  long long memory;         // total installed memory in bytes
  long long memory_speed;   // slowest module's clock (MHz); 0 unknown
  std::string module_list;  // "DIMM_A1: 32GB@4800MHz; ..." for rendering

  // Gather bookkeeping (not filter keywords): how many WMI classes answered.
  long long classes_ok;
  std::string gather_errors;

  hardware_info() : chassis_type(0), slots(0), modules(0), memory(0), memory_speed(0), classes_ok(0) {}

  // Fill the derived fields from module_details.
  void recompute();

  std::string get_vendor() const { return vendor; }
  std::string get_model() const { return model; }
  std::string get_uuid() const { return uuid; }
  std::string get_serial() const { return serial; }
  long long get_chassis_type() const { return chassis_type; }
  std::string get_chassis() const { return chassis; }
  std::string get_chassis_serial() const { return chassis_serial; }
  std::string get_asset_tag() const { return asset_tag; }
  long long get_slots() const { return slots; }
  long long get_modules() const { return modules; }
  long long get_memory() const { return memory; }
  std::string get_memory_human(parsers::where::evaluation_context context) const;
  long long get_memory_speed() const { return memory_speed; }
  std::string get_module_list() const { return module_list; }

  std::string show() const { return vendor + " " + model; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<hardware_info> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<hardware_info, filter_obj_handler> filter_type;

// Map a raw SMBIOS chassis type (System Enclosure, SMBIOS spec 7.4.1) to a
// human-readable name; unknown values render as "Unknown (<n>)".
std::string chassis_type_to_string(long long type);

// Parse the first integer out of the "[3, 4]"-style array rendering the WMI
// layer produces for ChassisTypes (bare "3" and empty/"<NULL>" tolerated).
long long parse_first_array_int(const std::string &s);

// Testable core: renders / thresholds a pre-gathered inventory row.
void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const hardware_info &info);

// Gather the inventory from WMI (root\CIMV2). Each class is best-effort so a
// missing class (common on stripped-down VMs) leaves its fields empty instead
// of failing the whole check; classes_ok/gather_errors record what happened.
hardware_info gather_hardware();

// Live check.
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace hardware_check
