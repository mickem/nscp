// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// software_facts::gather() for Windows: the installed programs recorded under
// the CurrentVersion\Uninstall keys, which is the same walk
// check_installed_software does - the 64-bit and 32-bit machine views plus
// every loaded per-user hive. The registry, never Win32_Product: querying that
// WMI class triggers an MSI consistency check that can reconfigure every
// installed package on the host.

#include <exception>
#include <facts/software_facts.hpp>
#include <stdexcept>
#include <string>
#include <vector>
#include <win/registry.hpp>

#include "check_installed_software.hpp"

namespace software_facts {

std::vector<package> gather() {
  std::vector<installed_software_check::software_entry> entries;
  try {
    entries = installed_software_check::gather_installed_software();
  } catch (const win_registry::registry_exception &e) {
    // registry_exception is not a std::exception, and the facts round only
    // catches those: rethrow so a hive that cannot be read is reported
    // against the set rather than escaping into the plugin glue.
    throw std::runtime_error(e.reason());
  }

  std::vector<package> packages;
  packages.reserve(entries.size());
  for (const installed_software_check::software_entry &e : entries) {
    // Entries hidden from Programs and Features (SystemComponent=1) are the
    // runtimes, patches and bookkeeping keys a component left behind. They
    // roughly double the record count on a developer machine and are not what
    // anyone means by "what is installed here", so the inventory is the list
    // an operator would see in the control panel.
    if (e.system_component) continue;
    if (e.name.empty()) continue;
    package p;
    p.name = e.name;
    p.version = e.version;
    p.publisher = e.publisher;
    // The registry view the entry came from, which is what says whether the
    // program is 64- or 32-bit. A per-user entry has none: that hive is not
    // WOW64-redirected, so there is nothing to read it from.
    p.architecture = normalize_architecture(e.architecture);
    p.source = "registry";
    p.scope = e.hive;
    p.install_date = static_cast<std::time_t>(e.install_date_epoch);
    p.size_bytes = e.size_kb > 0 ? static_cast<unsigned long long>(e.size_kb) * 1024ULL : 0ULL;
    packages.push_back(p);
  }
  return packages;
}

}  // namespace software_facts
