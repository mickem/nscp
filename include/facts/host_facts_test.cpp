// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <facts/host_facts.hpp>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_facts_test_helper.hpp>

// The value-deciding half of host facts: everything a Windows host and a unix
// host have to agree on. The gathering itself is platform code and is covered
// by the REST tags integration test; what matters here is that the two
// platforms cannot answer the same question differently.

TEST(host_facts_arch, both_spellings_of_64_bit_intel_agree) {
  // Windows reports AMD64, unix reports x86_64. One fleet, one value.
  EXPECT_EQ("x86_64", host_facts::normalize_arch("AMD64"));
  EXPECT_EQ("x86_64", host_facts::normalize_arch("x86_64"));
  EXPECT_EQ("x86_64", host_facts::normalize_arch("x64"));
  EXPECT_EQ("x86_64", host_facts::normalize_arch("amd64"));
}

TEST(host_facts_arch, both_spellings_of_64_bit_arm_agree) {
  // Windows reports ARM64, unix reports aarch64.
  EXPECT_EQ("arm64", host_facts::normalize_arch("ARM64"));
  EXPECT_EQ("arm64", host_facts::normalize_arch("aarch64"));
}

TEST(host_facts_arch, every_32_bit_intel_spelling_collapses) {
  EXPECT_EQ("x86", host_facts::normalize_arch("x86"));
  EXPECT_EQ("x86", host_facts::normalize_arch("i386"));
  EXPECT_EQ("x86", host_facts::normalize_arch("i686"));
}

TEST(host_facts_arch, thirty_two_bit_arm_variants_collapse) {
  EXPECT_EQ("arm", host_facts::normalize_arch("arm"));
  EXPECT_EQ("arm", host_facts::normalize_arch("armv7l"));
  EXPECT_EQ("arm", host_facts::normalize_arch("armv6l"));
}

TEST(host_facts_arch, other_architectures_survive) {
  EXPECT_EQ("ia64", host_facts::normalize_arch("IA64"));
  EXPECT_EQ("riscv64", host_facts::normalize_arch("riscv64"));
}

TEST(host_facts_arch, an_unknown_architecture_is_published_lowercased_not_dropped) {
  // A wrong-looking value is debuggable; a missing fact is not.
  EXPECT_EQ("loongarch64", host_facts::normalize_arch("LoongArch64"));
}

TEST(host_facts_arch, empty_stays_empty) { EXPECT_EQ("", host_facts::normalize_arch("")); }

TEST(host_facts_hypervisor_id, the_known_vendors_map) {
  EXPECT_EQ("vmware", host_facts::virtualization_from_hypervisor_id("VMwareVMware"));
  EXPECT_EQ("hyperv", host_facts::virtualization_from_hypervisor_id("Microsoft Hv"));
  EXPECT_EQ("xen", host_facts::virtualization_from_hypervisor_id("XenVMMXenVMM"));
  EXPECT_EQ("virtualbox", host_facts::virtualization_from_hypervisor_id("VBoxVBoxVBox"));
  EXPECT_EQ("qemu", host_facts::virtualization_from_hypervisor_id("TCGTCGTCGTCG"));
  EXPECT_EQ("parallels", host_facts::virtualization_from_hypervisor_id("prl hyperv  "));
  EXPECT_EQ("bhyve", host_facts::virtualization_from_hypervisor_id("bhyve bhyve "));
  EXPECT_EQ("acrn", host_facts::virtualization_from_hypervisor_id("ACRNACRNACRN"));
}

TEST(host_facts_hypervisor_id, kvm_pads_its_id_with_nuls) {
  // CPUID hands back 12 raw bytes; KVM uses nine and leaves three NULs, which
  // are not whitespace and would defeat a plain trim.
  const std::string padded("KVMKVMKVM\0\0\0", 12);
  EXPECT_EQ("kvm", host_facts::virtualization_from_hypervisor_id(padded));
}

TEST(host_facts_hypervisor_id, an_unknown_vendor_yields_nothing_so_dmi_can_answer) {
  EXPECT_EQ("", host_facts::virtualization_from_hypervisor_id("SomeNewHyper"));
  EXPECT_EQ("", host_facts::virtualization_from_hypervisor_id(""));
  EXPECT_EQ("", host_facts::virtualization_from_hypervisor_id("            "));
}

TEST(host_facts_dmi, the_common_hypervisors_are_recognised_from_smbios) {
  EXPECT_EQ("vmware", host_facts::virtualization_from_dmi("VMware, Inc.", "VMware Virtual Platform"));
  EXPECT_EQ("hyperv", host_facts::virtualization_from_dmi("Microsoft Corporation", "Virtual Machine"));
  EXPECT_EQ("virtualbox", host_facts::virtualization_from_dmi("innotek GmbH", "VirtualBox"));
  EXPECT_EQ("xen", host_facts::virtualization_from_dmi("Xen", "HVM domU"));
  EXPECT_EQ("kvm", host_facts::virtualization_from_dmi("QEMU", "KVM Virtual Machine"));
  EXPECT_EQ("qemu", host_facts::virtualization_from_dmi("QEMU", "Standard PC (i440FX + PIIX, 1996)"));
  EXPECT_EQ("parallels", host_facts::virtualization_from_dmi("Parallels Software International Inc.", "Parallels Virtual Platform"));
}

TEST(host_facts_dmi, the_clouds_that_present_as_kvm_are_named_kvm) {
  EXPECT_EQ("kvm", host_facts::virtualization_from_dmi("Amazon EC2", "m5.large"));
  EXPECT_EQ("kvm", host_facts::virtualization_from_dmi("Google", "Google Compute Engine"));
  EXPECT_EQ("kvm", host_facts::virtualization_from_dmi("DigitalOcean", "Droplet"));
  EXPECT_EQ("kvm", host_facts::virtualization_from_dmi("OpenStack Foundation", "OpenStack Nova"));
}

TEST(host_facts_dmi, a_surface_is_not_hyper_v) {
  // "Microsoft Corporation" alone is real hardware; only the product name
  // makes it a VM. Getting this wrong tags a whole estate of laptops.
  EXPECT_EQ("", host_facts::virtualization_from_dmi("Microsoft Corporation", "Surface Laptop 5"));
}

TEST(host_facts_dmi, real_hardware_yields_nothing) {
  EXPECT_EQ("", host_facts::virtualization_from_dmi("Dell Inc.", "PowerEdge R650"));
  EXPECT_EQ("", host_facts::virtualization_from_dmi("LENOVO", "20XW"));
  EXPECT_EQ("", host_facts::virtualization_from_dmi("", ""));
}

TEST(host_facts_dmi, matching_ignores_case) {
  EXPECT_EQ("vmware", host_facts::virtualization_from_dmi("vmware, inc.", ""));
  EXPECT_EQ("vmware", host_facts::virtualization_from_dmi("VMWARE, INC.", ""));
}

TEST(host_facts_physical, an_oem_vendor_and_product_name_real_hardware) {
  EXPECT_TRUE(host_facts::dmi_names_physical_hardware("Dell Inc.", "PowerEdge R650"));
  EXPECT_TRUE(host_facts::dmi_names_physical_hardware("LENOVO", "20XW"));
  // The everyday Windows case this rule exists for: a desktop whose CPUID
  // hypervisor bit is set because Hyper-V, WSL2 or VBS is enabled. The
  // firmware still names the box, and the box is still bare metal.
  EXPECT_TRUE(host_facts::dmi_names_physical_hardware("Webhallen", "Gaming PC"));
}

TEST(host_facts_physical, a_guests_synthetic_strings_are_not_hardware) {
  EXPECT_FALSE(host_facts::dmi_names_physical_hardware("VMware, Inc.", "VMware Virtual Platform"));
  EXPECT_FALSE(host_facts::dmi_names_physical_hardware("Microsoft Corporation", "Virtual Machine"));
  EXPECT_FALSE(host_facts::dmi_names_physical_hardware("QEMU", "KVM Virtual Machine"));
  EXPECT_FALSE(host_facts::dmi_names_physical_hardware("Amazon EC2", "m5.large"));
}

TEST(host_facts_physical, a_silent_smbios_says_nothing_either_way) {
  // A container and a board with no SMBIOS both land here. "Not hardware" is
  // not the same as "a VM", so the caller falls through to the CPU.
  EXPECT_FALSE(host_facts::dmi_names_physical_hardware("", ""));
  EXPECT_FALSE(host_facts::dmi_names_physical_hardware("Dell Inc.", ""));
  EXPECT_FALSE(host_facts::dmi_names_physical_hardware("", "PowerEdge R650"));
}

TEST(host_facts_memory, a_whole_number_of_gigabytes_is_itself) {
  EXPECT_EQ(64, host_facts::memory_gb_from_bytes(64ULL * 1024 * 1024 * 1024));
  EXPECT_EQ(1, host_facts::memory_gb_from_bytes(1024ULL * 1024 * 1024));
}

TEST(host_facts_memory, firmware_reserved_memory_still_reads_as_the_installed_size) {
  // A 64 GB machine reports a little under 64 GiB because firmware holds some
  // back. Truncating would publish 63 and break an exact-match selector.
  const unsigned long long just_under_64gb = 64ULL * 1024 * 1024 * 1024 - 300ULL * 1024 * 1024;
  EXPECT_EQ(64, host_facts::memory_gb_from_bytes(just_under_64gb));
  const unsigned long long just_under_8gb = 8ULL * 1024 * 1024 * 1024 - 120ULL * 1024 * 1024;
  EXPECT_EQ(8, host_facts::memory_gb_from_bytes(just_under_8gb));
}

TEST(host_facts_memory, a_small_machine_never_rounds_away_to_zero) {
  // 0 means "not determined" and is not published, so a 512 MB box has to
  // read as 1 rather than vanish.
  EXPECT_EQ(1, host_facts::memory_gb_from_bytes(512ULL * 1024 * 1024));
  EXPECT_EQ(1, host_facts::memory_gb_from_bytes(1));
}

TEST(host_facts_memory, zero_bytes_is_unknown) { EXPECT_EQ(0, host_facts::memory_gb_from_bytes(0)); }

TEST(host_facts_domain, the_first_label_is_dropped) {
  EXPECT_EQ("corp.example.com", host_facts::domain_from_fqdn("web01.corp.example.com"));
  EXPECT_EQ("example.com", host_facts::domain_from_fqdn("host.example.com"));
}

TEST(host_facts_domain, a_root_dot_is_not_part_of_the_domain) { EXPECT_EQ("corp.example.com", host_facts::domain_from_fqdn("web01.corp.example.com.")); }

TEST(host_facts_domain, an_unqualified_name_has_no_domain) {
  EXPECT_EQ("", host_facts::domain_from_fqdn("web01"));
  EXPECT_EQ("", host_facts::domain_from_fqdn(""));
  EXPECT_EQ("", host_facts::domain_from_fqdn("."));
}

// The two destinations. A gather produces one row; the tags take the five an
// operator groups hosts by, the fact sets take the inventory. What must not
// happen is the two disagreeing about a value, or the inventory leaking into
// the tags - a tag is uploaded to the fleet server on every state report,
// where a fact set is only collected once an operator turns it on.

namespace {
host_facts::facts sample_row() {
  host_facts::facts f;
  f.os_name = "Ubuntu 24.04.1 LTS";
  f.os_version = "6.8.0-45-generic";
  f.os_family = "linux";
  f.arch = "x86_64";
  f.virtualization = "kvm";
  f.manufacturer = "Dell Inc.";
  f.model = "PowerEdge R650";
  f.domain = "corp.example.com";
  f.cpu_cores = 16;
  f.memory_gb = 64;
  return f;
}

// The same row as a snapshot, with a fixed gather time so a test can assert
// on the stamp rather than on "some time around now".
const std::time_t kTakenAt = 1790000000;  // 2026-09-21T14:13:20Z

host_facts::snapshot sample_snapshot(const host_facts::facts &values = sample_row(), const std::time_t taken_at = kTakenAt) {
  host_facts::snapshot snap;
  snap.values = values;
  snap.taken_at = taken_at;
  return snap;
}

using nscapi::facts::testing::gathered_of;
using nscapi::facts::testing::json_of;

bool has_set(const nscapi::facts::response &out, const std::string &id) { return json_of(out, id).compare(0, 12, "(no such set") != 0; }
}  // namespace

TEST(host_facts_sets, the_os_set_carries_the_identity_and_the_domain) {
  nscapi::facts::response out;
  host_facts::publish_facts(sample_snapshot(), true, false, out);
  EXPECT_EQ(
      "{\"family\":\"linux\",\"name\":\"Ubuntu 24.04.1 LTS\",\"version\":\"6.8.0-45-generic\","
      "\"arch\":\"x86_64\",\"virtualization\":\"kvm\",\"domain\":\"corp.example.com\"}",
      json_of(out, "os"));
  EXPECT_FALSE(has_set(out, "hardware")) << "a set that is off is not written at all, which is what drops it from the document";
}

TEST(host_facts_sets, the_hardware_set_carries_the_vendor_and_the_size) {
  nscapi::facts::response out;
  host_facts::publish_facts(sample_snapshot(), false, true, out);
  EXPECT_EQ("{\"manufacturer\":\"Dell Inc.\",\"model\":\"PowerEdge R650\",\"cpu_cores\":16,\"memory_gb\":64}", json_of(out, "hardware"));
  EXPECT_FALSE(has_set(out, "os"));
}

TEST(host_facts_sets, both_sets_are_built_when_both_are_on) {
  nscapi::facts::response out;
  host_facts::publish_facts(sample_snapshot(), true, true, out);
  EXPECT_TRUE(has_set(out, "os"));
  EXPECT_TRUE(has_set(out, "hardware"));
}

TEST(host_facts_sets, nothing_is_written_when_nothing_is_enabled) {
  nscapi::facts::response out;
  host_facts::publish_facts(sample_snapshot(), false, false, out);
  const PB::Facts::FactsMessage message = out.to_message();
  ASSERT_EQ(1, message.payload_size());
  EXPECT_EQ(0, message.payload(0).sets_size());
}

TEST(host_facts_sets, a_fact_that_was_not_determined_is_omitted_not_written_empty) {
  // A VM with no SMBIOS strings and an unqualified name. The keys have to be
  // absent: a consumer reads "absent" as unknown, where an empty string reads
  // as an answer.
  host_facts::facts f = sample_row();
  f.manufacturer = "";
  f.model = "";
  f.domain = "";
  f.cpu_cores = 0;
  f.memory_gb = 0;
  nscapi::facts::response out;
  host_facts::publish_facts(sample_snapshot(f), true, true, out);
  EXPECT_EQ("{\"family\":\"linux\",\"name\":\"Ubuntu 24.04.1 LTS\",\"version\":\"6.8.0-45-generic\",\"arch\":\"x86_64\",\"virtualization\":\"kvm\"}",
            json_of(out, "os"));
  EXPECT_EQ("{}", json_of(out, "hardware"));
}

TEST(host_facts_sets, the_keys_are_valid_document_keys) {
  // The builder drops a key that breaks the document rules and records a
  // problem against the set, so a typo here would silently lose a field.
  for (const char *key : {"family", "name", "version", "arch", "virtualization", "domain", "manufacturer", "model", "cpu_cores", "memory_gb"}) {
    EXPECT_TRUE(nscapi::facts::is_valid_key(key)) << key;
  }
}

// When the values were read, as opposed to when the core asked for them.
// The distinction only exists because the producer caches; if it collected on
// every round the two would always be the same moment and nobody would need
// to tell them apart.

TEST(host_facts_gathered, every_set_carries_when_its_values_were_read) {
  nscapi::facts::response out;
  host_facts::publish_facts(sample_snapshot(), true, true, out);
  EXPECT_EQ("2026-09-21T14:13:20Z", gathered_of(out, "os"));
  EXPECT_EQ("2026-09-21T14:13:20Z", gathered_of(out, "hardware"));
}

TEST(host_facts_gathered, a_snapshot_with_no_time_leaves_the_sets_unstamped) {
  // The core then falls back to the time of the round, which is right for a
  // producer that did not bother to say.
  nscapi::facts::response out;
  host_facts::publish_facts(sample_snapshot(sample_row(), 0), true, true, out);
  EXPECT_EQ("", gathered_of(out, "os"));
}

TEST(host_facts_cache, the_first_round_has_to_read_the_machine) {
  EXPECT_TRUE(host_facts::should_regather("startup", false));
  EXPECT_TRUE(host_facts::should_regather("scheduled", false));
  EXPECT_TRUE(host_facts::should_regather("reload", false));
}

TEST(host_facts_cache, a_scheduled_round_reports_what_is_held) {
  // The whole point: an hourly round on a machine whose OS and hardware
  // cannot change without a reboot should cost nothing.
  EXPECT_FALSE(host_facts::should_regather("scheduled", true));
  EXPECT_FALSE(host_facts::should_regather("reload", true));
}

TEST(host_facts_cache, an_operator_asking_for_a_refresh_gets_a_real_one) {
  // The escape hatch for the host that did change underneath - memory added,
  // re-imaged - without making it wait for a restart.
  EXPECT_TRUE(host_facts::should_regather("manual", true));
  EXPECT_TRUE(host_facts::should_regather("startup", true));
}

TEST(host_facts_cache, an_unknown_reason_reads_as_scheduled) {
  // A core that grows a new reason must not silently turn a cached producer
  // into a collecting one.
  EXPECT_FALSE(host_facts::should_regather("something-new", true));
  EXPECT_FALSE(host_facts::should_regather("", true));
}

TEST(host_facts_cache, the_machine_is_read_once_and_reported_thereafter) {
  host_facts::snapshot_cache cache;
  int gathers = 0;
  const auto gather = [&gathers]() {
    ++gathers;
    return sample_row();
  };

  const host_facts::snapshot first = cache.get("startup", gather);
  EXPECT_EQ(1, gathers);
  EXPECT_TRUE(first.valid());
  EXPECT_EQ("Ubuntu 24.04.1 LTS", first.values.os_name);

  const host_facts::snapshot second = cache.get("scheduled", gather);
  EXPECT_EQ(1, gathers) << "a scheduled round must not read the machine again";
  EXPECT_EQ(first.taken_at, second.taken_at) << "and the values must keep the age they were read at";

  cache.get("reload", gather);
  EXPECT_EQ(1, gathers);

  cache.get("manual", gather);
  EXPECT_EQ(2, gathers) << "a manual refresh is the one that collects";
}

TEST(host_facts_cache, a_scheduled_first_round_still_collects) {
  // A module loaded after the core booted has no snapshot yet; it must not
  // report an empty inventory until someone asks by hand.
  host_facts::snapshot_cache cache;
  int gathers = 0;
  const auto gather = [&gathers]() {
    ++gathers;
    return sample_row();
  };

  const host_facts::snapshot first = cache.get("scheduled", gather);
  EXPECT_EQ(1, gathers);
  EXPECT_TRUE(first.valid());
  cache.get("scheduled", gather);
  EXPECT_EQ(1, gathers);
}
