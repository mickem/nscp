// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckSystem.h"

#include <nscapi/nscapi_facts_helper.hpp>
#include <string>
#include <vector>

#include "facts.h"

// This module's contribution to the host inventory. Four sets, the same four
// the Windows module produces, with the same keys and the same meanings:
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
      os.value("distribution", found.distribution);
      os.value("kernel", found.kernel);
      os.value("arch", found.arch);
      os.time("boot_time", found.boot_time);
    }
  }

  if (request.wants("identity")) {
    std::string error;
    const check_system_facts::identity_facts found = check_system_facts::gather_identity(error);
    if (!error.empty()) {
      response.error("identity", error);
    } else {
      nscapi::facts::section identity = response.set("identity");
      identity.value("hostname", found.hostname);
      identity.value("fqdn", found.fqdn);
      identity.value("domain", found.domain);
      // Windows reports whether the machine is domain-joined; Linux has no
      // equivalent, so the key is absent rather than false, which would be a
      // claim about something this platform does not have.
    }
  }

  if (request.wants("hardware")) {
    std::string error;
    const check_system_facts::hardware_facts found = check_system_facts::gather_hardware(error);
    if (!error.empty()) {
      response.error("hardware", error);
    } else {
      nscapi::facts::section hardware = response.set("hardware");
      hardware.value("vendor", found.vendor);
      hardware.value("model", found.model);
      hardware.value("serial", found.serial);
      hardware.value("asset_tag", found.asset_tag);
      hardware.value("chassis", found.chassis);
      hardware.value("uuid", found.uuid);

      nscapi::facts::section cpu = hardware.sub("cpu");
      cpu.value("model", found.cpu_model);
      if (found.cpu_sockets > 0) cpu.value("sockets", found.cpu_sockets);
      if (found.cpu_cores > 0) cpu.value("cores", found.cpu_cores);

      nscapi::facts::section memory = hardware.sub("memory");
      if (found.memory_total_bytes > 0) memory.value("total_bytes", found.memory_total_bytes);
      if (!found.memory_modules.empty()) {
        nscapi::facts::list modules = memory.list("modules");
        for (const check_system_facts::memory_module_facts &entry : found.memory_modules) {
          nscapi::facts::section record = modules.record(entry.id);
          if (entry.size_bytes > 0) record.value("size_bytes", entry.size_bytes);
          if (entry.speed_mhz > 0) record.value("speed_mhz", entry.speed_mhz);
          record.value("manufacturer", entry.manufacturer);
          record.value("part_number", entry.part_number);
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
        // The id is the interface name, which is also check_network's
        // instance name, so a failing check on eth0 and this record name the
        // same interface.
        nscapi::facts::section record = interfaces.record(entry.id);
        record.value("mac", entry.mac);
        record.strings("addresses", entry.addresses);
        if (entry.speed_bps > 0) record.value("speed_bps", entry.speed_bps);
        record.value("state", entry.state);
      }
    }
  }
}
