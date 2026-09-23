// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

namespace nscapi {
class core_wrapper;
namespace facts {
class response;
}
}  // namespace nscapi

namespace host_facts {

// Host facts: the small, static description of a machine that CheckSystem
// collects once - what OS it runs, how big it is, what it runs on and who
// made it.
//
// One gather, two destinations, and the split is deliberate:
//
//  - **Tags** (publish_tags) carry the handful an operator selects a *group*
//    of hosts on: os_name, os_version, os_family, arch, virtualization. They
//    are flat, always published, and they are what the fleet state report
//    actually uploads today.
//  - **Fact sets** (publish_facts) carry the inventory: the same OS fields
//    plus the host's domain, and a `hardware` set with the vendor, model and
//    size. These are opt-in per set, because an inventory is data an operator
//    did not necessarily agree to ship, and they travel as a document the
//    core stores rather than as a flat string a selector matches.
//
// Both CheckSystem modules (Windows and unix) fill the same struct and publish
// it through the same two functions below, so a fact means exactly the same
// thing on every host in a mixed fleet - the rule that already governs metric
// labels. The platform-specific gathering lives in each module
// (`system_facts.hpp`); everything that decides a *value* is here, shared and
// unit-tested, because two independent mappings would drift into
// `x86_64` on one platform and `amd64` on the other.
//
// A fact that could not be determined stays empty (or zero) and is not
// published at all: "the tag is absent" is the contract for unknown, rather
// than a tag reading "unknown" that a selector would have to special-case.
struct facts {
  std::string os_name;         // product name, e.g. "Windows Server 2022", "Ubuntu 24.04.1 LTS"
  std::string os_version;      // kernel version, e.g. "10.0.20348", "6.8.0-45-generic"
  std::string os_family;       // "windows", "linux", "darwin", ...
  std::string arch;            // normalized CPU architecture, see normalize_arch()
  std::string virtualization;  // hypervisor vocabulary below; "virtual" unnamed, "none" bare metal
  std::string manufacturer;    // system vendor, e.g. "Dell Inc."
  std::string model;           // system model, e.g. "PowerEdge R650"
  std::string domain;          // DNS domain the host is in; empty when it is in none
  long long cpu_cores;         // logical processors; 0 unknown
  long long memory_gb;         // installed physical memory in whole GB; 0 unknown

  facts() : cpu_cores(0), memory_gb(0) {}
};

// Tag names. Named here rather than spelled at each call site: these are the
// wire format - a fleet selector and a dashboard filter are written against
// them - so renaming one is a breaking change and must be a single edit.
extern const char *const tag_os_name;
extern const char *const tag_os_version;
extern const char *const tag_os_family;
extern const char *const tag_arch;
extern const char *const tag_virtualization;

// The fact sets this producer can be configured to build.
extern const char *const set_os;
extern const char *const set_hardware;

// Normalize a platform's spelling of a CPU architecture to the shared
// vocabulary: `x86_64`, `arm64`, `x86`, `arm`, `ia64`, `riscv64`. Windows
// spells 64-bit Intel `AMD64` and unix `x86_64`; unix spells 64-bit ARM
// `aarch64` and Windows `ARM64`. Unrecognised input is lower-cased and
// returned as it came, so a new architecture publishes something honest
// instead of nothing. Empty input yields an empty string.
std::string normalize_arch(const std::string &raw);

// Map a CPUID hypervisor vendor id (leaf 0x40000000, the 12 bytes of
// EBX:ECX:EDX) to the shared virtualization vocabulary. Returns an empty
// string when the id is not one we know, so the caller can fall back to the
// DMI strings rather than publish a vendor id nobody can select on.
//
// Vocabulary: vmware, hyperv, kvm, xen, virtualbox, qemu, parallels, bhyve,
// acrn, plus two values only a gather can conclude, never this function:
// "virtual" (a hypervisor announced itself but neither CPUID nor SMBIOS names
// it) and "none" (the hypervisor bit was readable and clear). A host where
// even that bit could not be read publishes no virtualization fact at all.
std::string virtualization_from_hypervisor_id(const std::string &id);

// Map the SMBIOS system vendor and product name to the same vocabulary.
// Returns an empty string when neither string is recognised. Matching is
// case-insensitive and on substrings, because vendors pad these fields
// ("VMware, Inc.", "Microsoft Corporation").
//
// This, not CPUID, is what a gather asks first. The hypervisor bit says "a
// hypervisor is running", which on Windows is also true of the machine
// *hosting* it: enabling Hyper-V, WSL2, Windows Sandbox or
// virtualization-based security moves the installed OS into the root
// partition, and it then reads its own hypervisor bit and the "Microsoft Hv"
// vendor id. SMBIOS does not have that problem - a guest gets the
// hypervisor's synthetic strings and a host keeps its OEM's.
std::string virtualization_from_dmi(const std::string &sys_vendor, const std::string &product_name);

// True when the SMBIOS strings name a real machine: both are present and
// neither says hypervisor. That is the signal that outranks the hypervisor
// bit - a box whose firmware calls it a "PowerEdge R650" is the physical
// host, whatever is running on top of it. An empty or placeholder-filled
// SMBIOS answers false, because it says nothing either way.
bool dmi_names_physical_hardware(const std::string &sys_vendor, const std::string &product_name);

// Installed memory in whole GB, rounded to nearest, for a byte count. Rounded
// rather than truncated because firmware reserves a slice of physical memory
// and a 64 GB machine reports something just under 64 GiB - truncation would
// publish `63` and make an exact-match selector useless. Returns 0 for 0
// bytes, and never returns 0 for a non-zero count (a machine with less than
// half a GB still reads as 1).
long long memory_gb_from_bytes(unsigned long long bytes);

// Strip the first label off a host name to leave the DNS domain
// ("web01.corp.example.com" -> "corp.example.com"). Returns an empty string
// for an unqualified name, a trailing-dot-only name, or an empty one.
std::string domain_from_fqdn(const std::string &fqdn);

// Publish the selector tags, and remove the tag of any that could not be
// determined. Shared so that "empty means absent" is decided once: a host
// that loses a fact between two starts (a VM migrated onto hardware the DMI
// does not name) sheds the tag rather than keeping a stale one forever.
//
// Only the five an operator groups hosts by - a tag is matched whole by a
// fleet selector, so this is the wrong channel for the inventory. The rest
// goes into the fact sets below.
//
// Safe to call with a core that predates the tag API; set_tag degrades to a
// no-op there.
void publish_tags(const nscapi::core_wrapper *core, const facts &f);

// Build the fact sets the module is configured to produce into `out`.
//
// `want_os` and `want_hardware` are the module's own settings
// (`[/settings/system/<platform>/facts] os` / `hardware`), so a set that is
// off is simply not written and the core drops it from the document - which
// is what turning a set off means. Shared between the two modules for the
// same reason publish_tags is: a Windows host and a Linux host have to put
// the same value under the same key, or an inventory query has to be written
// twice.
//
// A field the gather could not determine is omitted rather than written
// empty; the builder enforces that too, so there is no guard at each call.
void publish_facts(const facts &f, bool want_os, bool want_hardware, nscapi::facts::response &out);

}  // namespace host_facts
