// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// software_facts::gather() for unix: the installed packages the host's own
// package manager lists, through the same query check_installed_software runs
// (dpkg-query, rpm or pacman, each by absolute path so an inherited PATH
// cannot redirect the collector).

#include <facts/software_facts.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "check_installed_software.h"

namespace software_facts {

std::vector<package> gather() {
  const installed_software::package_manager manager = installed_software::detect_manager();
  if (manager.empty()) {
    // Not an empty inventory: this host keeps its package list somewhere we
    // cannot read (apk, nix, a container built without a package database),
    // and publishing an empty list would say it has no software installed.
    throw std::runtime_error("No supported package manager found (dpkg/rpm/pacman)");
  }
  const installed_software::fetch_result fetched = installed_software::fetch_installed(manager, installed_software::run_command);
  if (!fetched.ok) {
    // A failed query yields an empty list too, and for the same reason it must
    // not be published as one: it would blank the host's whole inventory the
    // first time the package database was locked.
    throw std::runtime_error("Failed to query installed software from " + manager.name + " (" + manager.binary + ")");
  }

  std::vector<package> packages;
  packages.reserve(fetched.entries.size());
  for (const installed_software::software_entry &e : fetched.entries) {
    if (e.name.empty()) continue;
    package p;
    p.name = e.name;
    p.version = e.version;
    p.publisher = e.publisher;
    p.architecture = normalize_architecture(e.architecture);
    // Which database the record came from - dpkg, rpm, pacman - which is also
    // what says how much of the record to expect: pacman -Q reports a name and
    // a version and nothing else.
    p.source = e.manager;
    // No `scope`: a unix package manager installs for the machine, so the
    // field would say the same thing for every record on every host.
    p.install_date = static_cast<std::time_t>(e.install_date_epoch);
    p.size_bytes = e.size_bytes > 0 ? static_cast<unsigned long long>(e.size_bytes) : 0ULL;
    packages.push_back(p);
  }
  return packages;
}

}  // namespace software_facts
