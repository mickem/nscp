// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <ctime>
#include <string>
#include <vector>

// The data sources behind this module's fact sets. Each gatherer is a pure
// read of the system - no filter, no threshold - so the mapping into the
// document (facts.cpp) stays a mapping and each source can be unit tested on
// its own.
//
// The field names mirror the Windows module's (modules/CheckSystem/facts.h)
// on purpose: `os`, `identity`, `hardware` and `network.interfaces` mean the
// same thing on both platforms, and a fleet query over them must not have to
// know which OS answered. Where a platform has nothing to report the field is
// simply absent rather than filled with a placeholder.
namespace check_system_facts {

struct os_facts {
  std::string family;   // always "linux" here
  std::string name;     // the distribution's pretty name, e.g. "Ubuntu 24.04.1 LTS"
  std::string version;  // the distribution version, e.g. "24.04"
  std::string distribution;
  std::string kernel;  // the kernel release, e.g. "6.8.0-45-generic"
  std::string arch;    // the machine architecture, e.g. "x86_64"
  std::time_t boot_time = 0;
};

struct identity_facts {
  std::string hostname;
  std::string fqdn;
  std::string domain;
};

struct memory_module_facts {
  std::string id;  // DIMM locator
  unsigned long long size_bytes = 0;
  long long speed_mhz = 0;
  std::string manufacturer;
  std::string part_number;
};

struct hardware_facts {
  std::string vendor;
  std::string model;
  std::string serial;
  std::string asset_tag;
  std::string chassis;
  std::string uuid;
  std::string cpu_model;
  long long cpu_sockets = 0;
  long long cpu_cores = 0;
  unsigned long long memory_total_bytes = 0;
  std::vector<memory_module_facts> memory_modules;
};

struct interface_facts {
  std::string id;  // the interface name, e.g. "eth0" - the check's instance name
  std::string mac;
  std::vector<std::string> addresses;
  long long speed_bps = 0;
  std::string state;  // "up", "down", "unknown"
};

// Each gatherer returns what it could read and sets `error` only when the
// whole source was unavailable - which the core reports on the fact set,
// keeping the previous value, rather than blanking the inventory.
os_facts gather_os(std::string &error);
identity_facts gather_identity(std::string &error);
hardware_facts gather_hardware(std::string &error);
std::vector<interface_facts> gather_interfaces(std::string &error);

// Read a whole file, or an empty string when it is not there. Exposed because
// every gatherer above is a parse of one of these and the parsers are what the
// unit tests drive.
std::string read_file(const std::string &path);

// /proc/cpuinfo -> model name, socket count, core count. Pure, so the test can
// feed it a captured file from a machine this one is not.
void parse_cpuinfo(const std::string &content, std::string &model, long long &sockets, long long &cores);

// /proc/meminfo -> MemTotal in bytes (0 when absent).
unsigned long long parse_meminfo_total(const std::string &content);

// /proc/uptime -> the boot time, given the current time (0 when unparseable).
std::time_t parse_boot_time(const std::string &uptime_content, std::time_t now);

}  // namespace check_system_facts
