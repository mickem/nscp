// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

namespace nscapi {
namespace facts {
class response;
}
}  // namespace nscapi

// The `software` fact set: what is installed on this host, as the platform's
// own package database records it - the registry's Uninstall hives on Windows,
// dpkg/rpm/pacman on unix.
//
// The inventory, not the monitoring: what is installed and which version of
// it, never whether a process is running or a service is up. It is the one
// fact set an operator is most likely to want and the one that costs the most
// to carry, which is why it is off by default like every other set and why
// this file caps what it will ship (max_packages).
//
// The split mirrors network_facts: each CheckSystem module reads its own
// platform through the package inventory its check_installed_software already
// uses, and both publish through the one publish() below, so a package means
// the same thing on every host in a mixed fleet. Everything that decides what
// a record looks like - the ordering, the record ids, the architecture
// vocabulary - is here, shared, and unit-tested on every build.
namespace software_facts {

// The fact set CheckSystem produces, and the one list in it. The enableable
// id is the dotted path, `software.installed`: that is the settings key.
extern const char *const set_software;
extern const char *const key_installed;
extern const char *const id_installed;

// The most records this set will ship, and the reason it exists: the whole
// facts document lives inside `[/settings/facts] max size` (1 MiB by
// default), and a set that would push it past that budget is rejected *whole*
// by the core - which would leave a host with no software inventory at all
// rather than most of one. A desktop Linux install with a few thousand
// packages is a real machine, so the list is truncated at this count and the
// set carries an error saying how many were found. Truncating is also what
// keeps the set inside the document rules' own 5000-record list limit.
extern const std::size_t max_packages;

// One installed package, as a platform's gather reads it. Empty strings, a
// zero size and a zero date mean "not known" and are omitted from the record,
// never written empty.
struct package {
  // The record id, filled in by prepare(): the package name, with the version
  // appended when the host has more than one install of the same name. Not a
  // field a gather sets.
  std::string id;
  // What the platform calls it: the DisplayName of the Uninstall key on
  // Windows, the package name on unix. The `name` keyword of
  // check_installed_software on both.
  std::string name;
  std::string version;    // as recorded; never parsed or normalized
  std::string publisher;  // Publisher (Windows), maintainer or vendor (unix)
  // The package's architecture, in one vocabulary on every platform; see
  // normalize_architecture(). A gather passes the platform's spelling and
  // this file normalizes it.
  std::string architecture;
  // Which database the record came from: `registry` on Windows, and the
  // package manager (`dpkg`, `rpm`, `pacman`) on unix.
  std::string source;
  // `machine` for a host-wide install and `user` for one installed into a
  // single account's hive. Windows only: a unix package manager installs for
  // the machine, so the field would say the same thing for every record and
  // is left out there.
  std::string scope;
  std::time_t install_date = 0;       // published as a date, YYYY-MM-DD
  unsigned long long size_bytes = 0;  // installed size
};

// Sort, collapse duplicates and give every record its id. Pure, and applied
// by publish(), so the two platforms cannot disagree about it.
//
// Sorted because list order is part of the document: the core sorts an
// object's keys as it stores a set, but a list is ordered data it leaves
// alone, so an unsorted list would move the revision whenever the platform
// happened to enumerate in a different order.
//
// Duplicates are collapsed when they are the same package name, version and
// architecture: one product installed into three user hives is one entry in a
// host's inventory, not three, and three records would need three ids to tell
// apart that nobody asked for.
//
// The id is the name, which is what an operator reads it by. A host with two
// installs of the same name - Python 3.11 and 3.12, a package installed for
// two architectures - gets the version appended, then the architecture, and
// as a last resort ` #2`, so the ids are unique (the core rejects a list
// whose ids are not) and stable for as long as those installs are.
std::vector<package> prepare(std::vector<package> packages);

// Normalize a platform's spelling of a package architecture to the shared
// vocabulary, which is host_facts::normalize_arch's (`x86_64`, `arm64`,
// `x86`, ...) plus `noarch` for a package that has no architecture: rpm
// spells that `noarch`, dpkg `all` and pacman `any`, and all three mean the
// same thing. Anything unrecognised is lower-cased and returned as it came.
std::string normalize_architecture(const std::string &raw);

// Add the `software` set, with the packages as records of its `installed`
// list, to `out`. The list is prepared (sorted, deduplicated, ids assigned)
// and truncated at max_packages, in which case `out` also carries an error
// against the set saying how many were found. `taken_at` stamps when they
// were read.
void publish(const std::vector<package> &packages, std::time_t taken_at, nscapi::facts::response &out);

// Read this host's installed packages. Implemented per platform, by each
// CheckSystem module, on top of the inventory its check_installed_software
// already gathers.
//
// Throws when the platform's package database could not be read at all.
// That distinction matters more here than anywhere else in facts: an empty
// list is a legitimate answer ("nothing is installed"), so a failed query
// that returned nothing must not be published as one - it would blank the
// host's whole software inventory.
std::vector<package> gather();

}  // namespace software_facts
