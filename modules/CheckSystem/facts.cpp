// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckSystem.h"

#include <nscapi/nscapi_facts_helper.hpp>
#include <string>
#include <vector>

#include <win/sysinfo/win_sysinfo.hpp>

#include "check_hardware.hpp"
#include "check_hostname.hpp"
#include "facts.hpp"

// This module's contribution to the host inventory. Four sets, the same four
// the Unix module produces, with the same keys and the same meanings:
//
//   os                  what this machine runs
//   identity            what it calls itself
//   hardware            what it is made of
//   network.interfaces  what it is attached to
//
// Each is independent: a host can enable `os` alone, and a set that could not
// be collected reports why instead of arriving empty.
void CheckSystem::fetchFacts(const nscapi::facts::request &request, nscapi::facts::response &response) {
  if (request.wants("os")) {
    std::string error;
    const check_system_facts::os_facts found = check_system_facts::gather_os(error);
    if (!error.empty()) {
      response.error("os", error);
    } else {
      nscapi::facts::section os = response.set("os");
      os.value("family", found.family);
      os.value("name", found.name);
      os.value("version", found.version);
      os.value("kernel", found.kernel);
      os.value("arch", found.arch);
      os.time("boot_time", found.boot_time);
      // A machine waiting for a reboot is something an operator acts on, so
      // it is reported either way rather than only when true.
      os.value("pending_reboot", found.pending_reboot);
      os.value("bios_version", found.bios_version);
      os.value("bios_manufacturer", found.bios_manufacturer);
    }
  }

  if (request.wants("identity")) {
    // GetComputerNameEx + NetGetJoinInformation, the same gather check_hostname
    // uses, so the fact and the check never disagree about this machine's name.
    const hostname_check::host_identity found = hostname_check::gather_identity();
    if (found.hostname.empty() && found.fqdn.empty()) {
      response.error("identity", "could not read the computer name");
    } else {
      nscapi::facts::section identity = response.set("identity");
      identity.value("hostname", found.dns_hostname.empty() ? found.hostname : found.dns_hostname);
      identity.value("netbios_name", found.hostname);
      identity.value("fqdn", found.fqdn);
      identity.value("domain", found.domain);
      identity.value("join", found.join);
      identity.value("join_name", found.join_name);
      // `domain_joined` is the flat answer a fleet selector wants; `join`
      // keeps the three-way distinction for a reader.
      identity.value("domain_joined", found.join == "domain");
    }
  }

  if (request.wants("hardware")) {
    // Win32_ComputerSystemProduct, Win32_SystemEnclosure and
    // Win32_PhysicalMemory, best-effort per class as check_hardware gathers
    // them: a stripped-down VM answers with fewer fields rather than an error.
    const hardware_check::hardware_info found = hardware_check::gather_hardware();
    if (found.classes_ok == 0) {
      response.error("hardware", found.gather_errors.empty() ? "no hardware information available (WMI returned nothing)" : found.gather_errors);
    } else {
      nscapi::facts::section hardware = response.set("hardware");
      hardware.value("vendor", found.vendor);
      hardware.value("model", found.model);
      hardware.value("serial", found.serial);
      hardware.value("asset_tag", found.asset_tag);
      hardware.value("chassis", found.chassis);
      hardware.value("uuid", found.uuid);

      nscapi::facts::section cpu = hardware.sub("cpu");
      // The core count comes from the OS rather than WMI: it is the number
      // check_cpu reports per-core metrics for, and it needs no WMI class.
      const long long cores = windows::system_info::get_numberOfProcessorscores();
      if (cores > 0) cpu.value("cores", cores);

      nscapi::facts::section memory = hardware.sub("memory");
      if (found.memory > 0) memory.value("total_bytes", found.memory);
      if (found.slots > 0) memory.value("slots", found.slots);
      if (!found.module_details.empty()) {
        nscapi::facts::list modules = memory.list("modules");
        for (const hardware_check::memory_module &entry : found.module_details) {
          // The DIMM locator is the record id: it is what an operator reads
          // off the board and what a replacement changes.
          nscapi::facts::section record = modules.record(entry.locator);
          if (entry.capacity > 0) record.value("size_bytes", entry.capacity);
          if (entry.speed > 0) record.value("speed_mhz", entry.speed);
          record.value("manufacturer", entry.manufacturer);
          record.value("part_number", entry.part_number);
          record.value("serial", entry.serial);
        }
      }
    }
  }

  if (request.wants("network.interfaces")) {
    std::string error;
    const std::vector<check_system_facts::interface_facts> found = check_system_facts::gather_interfaces(error);
    if (!error.empty()) {
      response.error("network.interfaces", error);
    } else {
      nscapi::facts::list interfaces = response.set("network").list("interfaces");
      for (const check_system_facts::interface_facts &entry : found) {
        // The id is the adapter's connection name, which is also
        // check_network's instance name, so a failing check on "Ethernet 2"
        // and this record name the same adapter.
        nscapi::facts::section record = interfaces.record(entry.id);
        record.value("mac", entry.mac);
        record.strings("addresses", entry.addresses);
        if (entry.speed_bps > 0) record.value("speed_bps", entry.speed_bps);
        record.value("state", entry.state);
      }
    }
  }
}
