// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithm>
#include <cctype>
#include <facts/host_facts.hpp>
#include <facts/software_facts.hpp>
#include <map>
#include <nscapi/nscapi_facts_helper.hpp>
#include <set>
#include <string>

namespace software_facts {

const char *const set_software = "software";
const char *const key_installed = "installed";
const char *const id_installed = "software.installed";

// Room for a large server's package list with the rest of the document, and
// short of the 5000 records the document rules allow in one list. A Debian
// host with more packages than this has an inventory that is truncated
// and says so, which is the outcome an operator can act on; the alternative
// is the core rejecting the set over the size budget and the host reporting
// no software at all.
const std::size_t max_packages = 2500;

namespace {

std::string to_lower(const std::string &value) {
  std::string out = value;
  for (char &c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

// The order a record is written in, and the order that makes prepare()
// deterministic: two reads of an unchanged host produce the same list
// whatever order the registry or the package manager enumerated in.
bool before(const package &lhs, const package &rhs) {
  if (lhs.name != rhs.name) return lhs.name < rhs.name;
  if (lhs.version != rhs.version) return lhs.version < rhs.version;
  if (lhs.architecture != rhs.architecture) return lhs.architecture < rhs.architecture;
  if (lhs.scope != rhs.scope) return lhs.scope < rhs.scope;
  return lhs.source < rhs.source;
}

// The same package, as far as an inventory is concerned. Not the whole
// record: a product installed into two user hives differs in nothing a fact
// consumer can use, and the machine-wide install of the same version is the
// same software being on the host.
bool same_package(const package &lhs, const package &rhs) { return lhs.name == rhs.name && lhs.version == rhs.version && lhs.architecture == rhs.architecture; }

}  // namespace

std::string normalize_architecture(const std::string &raw) {
  const std::string value = to_lower(raw);
  // Not an architecture but a platform's spelling of "this package does not
  // have one": rpm prints "(none)" for a tag a package lacks, which is what
  // its pseudo-packages (the imported gpg-pubkey keys) carry. An unknown
  // value is omitted from the document, never published as a word no other
  // platform uses - and unlike a real architecture it must not fall through
  // to the vocabulary below, which passes anything it does not know.
  if (value == "(none)" || value == "(null)") return "";
  // A package that runs anywhere: rpm's noarch, dpkg's all, pacman's any.
  // One word for it, as with every other value two platforms spell
  // differently.
  if (value == "noarch" || value == "all" || value == "any") return "noarch";
  // Everything else is a CPU architecture, and there is already one
  // vocabulary for those - the one the `os` fact set publishes.
  return host_facts::normalize_arch(value);
}

std::vector<package> prepare(std::vector<package> packages) {
  std::sort(packages.begin(), packages.end(), before);
  packages.erase(std::unique(packages.begin(), packages.end(), same_package), packages.end());

  // How many installs share a name, and how many share a name and a version,
  // decides which of them can carry the id - counted over the whole list
  // before any id is handed out, so two records that need telling apart are
  // told apart the same way round. Deciding it record by record would give
  // the first one the bare name and only the second one a suffix, which reads
  // as two different kinds of thing.
  std::map<std::string, std::size_t> by_name;
  std::map<std::string, std::size_t> by_version;
  for (const package &p : packages) {
    ++by_name[p.name];
    ++by_version[p.name + '\n' + p.version];
  }

  std::set<std::string> used;
  for (package &p : packages) {
    std::string id = p.name;
    // Two installs of one product: the version is what tells them apart in
    // Programs and Features too, and the architecture after it for the
    // package installed once per architecture.
    if (by_name[p.name] > 1 && !p.version.empty()) id += " " + p.version;
    if (by_version[p.name + '\n' + p.version] > 1 && !p.architecture.empty()) id += " (" + p.architecture + ")";
    // A last resort, for the collision no field can resolve: a product
    // literally named "Foo 1.0" next to version 1.0 of one named "Foo". It is
    // deterministic only because the list is sorted - and it has to exist,
    // because the core rejects a whole set over two records sharing an id.
    if (used.find(id) != used.end()) {
      const std::string base = id;
      for (std::size_t n = 2; used.find(id) != used.end(); ++n) id = base + " #" + std::to_string(n);
    }
    used.insert(id);
    p.id = id;
  }
  return packages;
}

void publish(const std::vector<package> &packages, const std::time_t taken_at, nscapi::facts::response &out) {
  const std::vector<package> prepared = prepare(packages);
  // Written even when empty: "enabled, collected, nothing installed" is an
  // answer, and an absent list would read as "not collected".
  nscapi::facts::record_list list = out.set(set_software).list(key_installed);
  const std::size_t count = std::min(prepared.size(), max_packages);
  for (std::size_t i = 0; i < count; ++i) {
    const package &p = prepared[i];
    nscapi::facts::section record = list.record(p.id);
    record.value("name", p.name)
        .value("version", p.version)
        .value("publisher", p.publisher)
        .value("architecture", p.architecture)
        .value("source", p.source)
        .value("scope", p.scope);
    record.date("install_date", p.install_date);
    // Zero is "not recorded" (the registry often has no EstimatedSize), and
    // the builder writes a number as it is given, so the guard is here.
    if (p.size_bytes > 0) record.value("size_bytes", p.size_bytes);
  }
  if (prepared.size() > count) {
    // The set is still published: most of an inventory, and the reason it is
    // not all of it, beats the core rejecting it over the size budget.
    out.error(set_software, "Host has " + std::to_string(prepared.size()) + " installed packages; only the first " + std::to_string(count) +
                                " are reported, to keep the facts document inside its size budget");
  }
  out.gathered(set_software, taken_at);
}

}  // namespace software_facts
