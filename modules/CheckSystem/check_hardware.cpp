// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hardware.hpp"

#include <boost/algorithm/string.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <win/com_helpers.hpp>
#include <win/wmi/wmi_query.hpp>

namespace hardware_check {

using parsers::where::type_size;

std::string chassis_type_to_string(const long long type) {
  // SMBIOS System Enclosure types (spec 7.4.1), as surfaced by
  // Win32_SystemEnclosure.ChassisTypes.
  switch (type) {
    case 1:
      return "Other";
    case 2:
      return "Unknown";
    case 3:
      return "Desktop";
    case 4:
      return "Low Profile Desktop";
    case 5:
      return "Pizza Box";
    case 6:
      return "Mini Tower";
    case 7:
      return "Tower";
    case 8:
      return "Portable";
    case 9:
      return "Laptop";
    case 10:
      return "Notebook";
    case 11:
      return "Hand Held";
    case 12:
      return "Docking Station";
    case 13:
      return "All in One";
    case 14:
      return "Sub Notebook";
    case 15:
      return "Space-Saving";
    case 16:
      return "Lunch Box";
    case 17:
      return "Main System Chassis";
    case 18:
      return "Expansion Chassis";
    case 19:
      return "SubChassis";
    case 20:
      return "Bus Expansion Chassis";
    case 21:
      return "Peripheral Chassis";
    case 22:
      return "Storage Chassis";
    case 23:
      return "Rack Mount Chassis";
    case 24:
      return "Sealed-Case PC";
    case 30:
      return "Tablet";
    case 31:
      return "Convertible";
    case 32:
      return "Detachable";
    case 33:
      return "IoT Gateway";
    case 34:
      return "Embedded PC";
    case 35:
      return "Mini PC";
    case 36:
      return "Stick PC";
    default:
      return "Unknown (" + str::xtos(type) + ")";
  }
}

long long parse_first_array_int(const std::string &s) {
  const std::size_t first = s.find_first_of("0123456789");
  if (first == std::string::npos) return 0;
  std::size_t end = first;
  while (end < s.size() && s[end] >= '0' && s[end] <= '9') ++end;
  try {
    return std::stoll(s.substr(first, end - first));
  } catch (...) {
    return 0;
  }
}

void hardware_info::recompute() {
  modules = static_cast<long long>(module_details.size());
  memory = 0;
  memory_speed = 0;
  module_list.clear();
  for (const memory_module &m : module_details) {
    memory += m.capacity;
    if (m.speed > 0 && (memory_speed == 0 || m.speed < memory_speed)) memory_speed = m.speed;
    if (!module_list.empty()) module_list += "; ";
    module_list += m.locator.empty() ? "?" : m.locator;
    module_list += ": " + str::format::format_byte_units(m.capacity);
    if (m.speed > 0) module_list += "@" + str::xtos(m.speed) + "MHz";
  }
}

std::string hardware_info::get_memory_human(parsers::where::evaluation_context context) const {
  return str::format::format_byte_units(memory, context->get_number_format());
}

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("vendor", &hardware_info::get_vendor, "System vendor/manufacturer")
      .add_string_var("model", &hardware_info::get_model, "System model / product name")
      .add_string_var("uuid", &hardware_info::get_uuid, "SMBIOS system UUID")
      .add_string_var("serial", &hardware_info::get_serial, "System serial number (often blank or placeholder on VMs and OEM boards)")
      .add_string_var("chassis", &hardware_info::get_chassis, "Chassis type name (Desktop, Laptop, Rack Mount Chassis, ...)")
      .add_string_var("chassis_serial", &hardware_info::get_chassis_serial, "Enclosure serial number")
      .add_string_var("asset_tag", &hardware_info::get_asset_tag, "SMBIOS asset tag")
      .add_string_var("module_list", &hardware_info::get_module_list,
                      "Semicolon-separated per-DIMM inventory (slot: size@speed, e.g. 'DIMM_A1: 32GB@4800MHz')");
  registry_.add_int_var("chassis_type", &hardware_info::get_chassis_type, "Raw SMBIOS chassis type number (0 when unknown)")
      .add_int_var("slots", &hardware_info::get_slots, "Total memory sockets on the board (0 when not reported)")
      .add_int_var("modules", &hardware_info::get_modules, "Number of populated memory modules")
      .add_int_var("memory", type_size, &hardware_info::get_memory,
                   "Total installed memory (supports size units, e.g. 'memory < 64G'); renders human-readable")
      .add_int_var("memory_speed", &hardware_info::get_memory_speed, "Slowest populated module's configured clock in MHz (0 when unknown)");
  // Render ${memory} human-readable (64GB) while expressions keep comparing bytes.
  registry_.add_human_string_context("memory", &hardware_info::get_memory_human, "Total installed memory as a human-readable size");
  // clang-format on
}

void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const hardware_info &info) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  // No default thresholds: a bare call is an inventory line. Policy is pinned
  // expectations, e.g. crit=serial != 'ABC123', warn=modules < 8,
  // crit=chassis like 'Laptop', warn=memory < 64G.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${vendor} ${model} (${chassis}), serial=${serial}, ${modules} memory module(s), ${memory}", "hardware", "",
                           "");
  filter_helper.set_default_perf_config("extra(memory;modules)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<hardware_info> record(new hardware_info(info));
  filter.match(record);

  filter_helper.post_process(filter);
}

namespace {

// Per-field readers tolerating both missing columns (older Windows lacks e.g.
// ConfiguredClockSpeed) and WMI's "<NULL>" rendering of empty values.
std::string get_s(const wmi_impl::row &r, const char *col) {
  try {
    std::string v = boost::trim_copy(r.get_string(col));
    if (v == "<NULL>") return "";
    return v;
  } catch (...) {
    return "";
  }
}

long long get_ll(const wmi_impl::row &r, const char *col) {
  const std::string v = get_s(r, col);
  if (v.empty()) return 0;
  try {
    return std::stoll(v);
  } catch (...) {
    return 0;
  }
}

// Run one WQL query against root\CIMV2, feeding each row to on_row. Returns
// true on success; on failure appends the reason to errors.
template <class OnRow>
bool for_each_row(const std::string &wql, const std::string &what, OnRow on_row, std::string &errors) {
  try {
    wmi_impl::query q(wql, "root\\CIMV2", "", "");
    wmi_impl::row_enumerator rows = q.execute();
    while (rows.has_next()) {
      on_row(rows.get_next());
    }
    return true;
  } catch (const wmi_impl::wmi_exception &e) {
    if (!errors.empty()) errors += "; ";
    errors += what + ": " + e.reason();
  } catch (const std::exception &e) {
    if (!errors.empty()) errors += "; ";
    errors += what + ": " + std::string(e.what());
  } catch (...) {
    if (!errors.empty()) errors += "; ";
    errors += what + ": unknown error";
  }
  return false;
}

}  // namespace

hardware_info gather_hardware() {
  hardware_info out;
  // Scoped COM init (tolerates an apartment already initialised elsewhere).
  const com_helper::mta_scope com;

  if (for_each_row(
          "SELECT Vendor, Name, UUID, IdentifyingNumber FROM Win32_ComputerSystemProduct", "Win32_ComputerSystemProduct",
          [&out](const wmi_impl::row &r) {
            out.vendor = get_s(r, "Vendor");
            out.model = get_s(r, "Name");
            out.uuid = get_s(r, "UUID");
            out.serial = get_s(r, "IdentifyingNumber");
          },
          out.gather_errors)) {
    out.classes_ok++;
  }

  if (for_each_row(
          "SELECT ChassisTypes, SerialNumber, SMBIOSAssetTag FROM Win32_SystemEnclosure", "Win32_SystemEnclosure",
          [&out](const wmi_impl::row &r) {
            // ChassisTypes is an array; the first entry is the chassis kind.
            if (out.chassis_type == 0) {
              out.chassis_type = parse_first_array_int(get_s(r, "ChassisTypes"));
              out.chassis = chassis_type_to_string(out.chassis_type);
              out.chassis_serial = get_s(r, "SerialNumber");
              out.asset_tag = get_s(r, "SMBIOSAssetTag");
            }
          },
          out.gather_errors)) {
    out.classes_ok++;
  }

  if (for_each_row(
          "SELECT DeviceLocator, BankLabel, Capacity, ConfiguredClockSpeed, Speed, PartNumber, SerialNumber, Manufacturer FROM Win32_PhysicalMemory",
          "Win32_PhysicalMemory",
          [&out](const wmi_impl::row &r) {
            memory_module m;
            m.locator = get_s(r, "DeviceLocator");
            if (m.locator.empty()) m.locator = get_s(r, "BankLabel");
            m.capacity = get_ll(r, "Capacity");
            m.speed = get_ll(r, "ConfiguredClockSpeed");
            if (m.speed == 0) m.speed = get_ll(r, "Speed");
            m.part_number = get_s(r, "PartNumber");
            m.serial = get_s(r, "SerialNumber");
            m.manufacturer = get_s(r, "Manufacturer");
            out.module_details.push_back(m);
          },
          out.gather_errors)) {
    out.classes_ok++;
  }

  if (for_each_row(
          "SELECT MemoryDevices FROM Win32_PhysicalMemoryArray", "Win32_PhysicalMemoryArray",
          [&out](const wmi_impl::row &r) { out.slots += get_ll(r, "MemoryDevices"); }, out.gather_errors)) {
    out.classes_ok++;
  }

  out.recompute();
  return out;
}

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    const hardware_info info = gather_hardware();
    if (info.classes_ok == 0) {
      // Every class failed (WMI down): an all-empty inventory row would be a
      // misleading OK, so fail loudly instead.
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read hardware inventory from WMI: " + info.gather_errors);
    }
    check_from(request, response, info);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to read hardware inventory: " + std::string(e.what()));
  }
}

}  // namespace hardware_check
