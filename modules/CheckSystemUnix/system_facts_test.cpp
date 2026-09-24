// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "system_facts.h"

#include <gtest/gtest.h>

// The unix half of host facts: build() turns already-read file contents,
// uname fields and sysconf counters into the facts row, so every decision is
// testable without a machine that happens to be a VM, an ARM board or a
// container.

namespace {
const char *const ubuntu_os_release =
    "PRETTY_NAME=\"Ubuntu 24.04.1 LTS\"\n"
    "NAME=\"Ubuntu\"\n"
    "VERSION_ID=\"24.04\"\n"
    "VERSION=\"24.04.1 LTS (Noble Numbat)\"\n"
    "ID=ubuntu\n"
    "ID_LIKE=debian\n";

const char *const bare_metal_flags = "fpu vme de pse tsc msr pae mce cx8 apic sep";
const char *const virtualized_flags = "fpu vme de pse tsc msr pae mce cx8 apic sep hypervisor lahf_lm";

host_facts::facts build_default(const std::string &sys_vendor = "Dell Inc.", const std::string &product = "PowerEdge R650",
                                const std::string &flags = bare_metal_flags) {
  return system_facts::build(ubuntu_os_release, "Linux", "6.8.0-45-generic", "x86_64", "web01.corp.example.com", sys_vendor, product, flags, 16,
                             64ULL * 1024 * 1024 * 1024);
}
}  // namespace

TEST(system_facts_unix, the_distribution_is_the_product_name) { EXPECT_EQ("Ubuntu 24.04.1 LTS", build_default().os_name); }

TEST(system_facts_unix, the_kernel_release_is_the_os_version) {
  // Windows publishes its build number here, so a "still on the old kernel"
  // selector reads the same on both platforms.
  EXPECT_EQ("6.8.0-45-generic", build_default().os_version);
}

TEST(system_facts_unix, the_family_is_the_lowercased_kernel_name) {
  EXPECT_EQ("linux", build_default().os_family);
  const host_facts::facts freebsd = system_facts::build(ubuntu_os_release, "FreeBSD", "14.1-RELEASE", "amd64", "host", "", "", "", 4, 1024ULL * 1024 * 1024);
  EXPECT_EQ("freebsd", freebsd.os_family);
}

TEST(system_facts_unix, the_architecture_is_normalized) {
  EXPECT_EQ("x86_64", build_default().arch);
  const host_facts::facts arm =
      system_facts::build(ubuntu_os_release, "Linux", "6.8.0-45", "aarch64", "pi", "", "Raspberry Pi 5", "", 4, 8ULL * 1024 * 1024 * 1024);
  EXPECT_EQ("arm64", arm.arch);
}

TEST(system_facts_unix, size_comes_from_the_counters) {
  const host_facts::facts f = build_default();
  EXPECT_EQ(16, f.cpu_cores);
  EXPECT_EQ(64, f.memory_gb);
}

TEST(system_facts_unix, the_domain_comes_from_a_qualified_hostname) { EXPECT_EQ("corp.example.com", build_default().domain); }

TEST(system_facts_unix, an_unqualified_hostname_publishes_no_domain) {
  const host_facts::facts f =
      system_facts::build(ubuntu_os_release, "Linux", "6.8.0-45", "x86_64", "web01", "", "", bare_metal_flags, 2, 1024ULL * 1024 * 1024);
  EXPECT_EQ("", f.domain);
}

TEST(system_facts_unix, dmi_names_the_hardware) {
  const host_facts::facts f = build_default();
  EXPECT_EQ("Dell Inc.", f.manufacturer);
  EXPECT_EQ("PowerEdge R650", f.model);
  EXPECT_EQ("none", f.virtualization);
}

TEST(system_facts_unix, firmware_placeholders_are_not_facts) {
  // A whitebox board fills these in with the SMBIOS default strings; they
  // look like an answer and are not one.
  const host_facts::facts f = build_default("To Be Filled By O.E.M.", "Default string");
  EXPECT_EQ("", f.manufacturer);
  EXPECT_EQ("", f.model);
}

TEST(system_facts_unix, dmi_strings_are_trimmed) {
  // The /sys/class/dmi/id files end in a newline.
  const host_facts::facts f = build_default("Dell Inc.\n", "PowerEdge R650\n");
  EXPECT_EQ("Dell Inc.", f.manufacturer);
  EXPECT_EQ("PowerEdge R650", f.model);
}

TEST(system_facts_unix, a_named_hypervisor_is_named) {
  EXPECT_EQ("vmware", build_default("VMware, Inc.", "VMware Virtual Platform", virtualized_flags).virtualization);
  EXPECT_EQ("kvm", build_default("QEMU", "KVM Virtual Machine", virtualized_flags).virtualization);
  EXPECT_EQ("kvm", build_default("Amazon EC2", "m5.large", virtualized_flags).virtualization);
}

TEST(system_facts_unix, an_unnamed_hypervisor_is_still_not_bare_metal) {
  // The cpuinfo flag says virtualized and the DMI strings say nothing. "none"
  // here would make every "is this bare metal" selector match a VM.
  EXPECT_EQ("virtual", build_default("", "", virtualized_flags).virtualization);
}

TEST(system_facts_unix, named_hardware_outranks_the_hypervisor_flag) {
  // A KVM host reads its own OEM strings; on Windows the same rule keeps a
  // desktop with VBS enabled from calling itself a VM. Both platforms decide
  // this the same way, so the two must agree here.
  EXPECT_EQ("none", build_default("Dell Inc.", "PowerEdge R650", virtualized_flags).virtualization);
}

TEST(system_facts_unix, an_unreadable_cpuinfo_leaves_virtualization_unknown) {
  // No flags line and no DMI strings: nothing is known, so nothing is
  // published - rather than guessing bare metal.
  EXPECT_EQ("", build_default("", "", "").virtualization);
}

TEST(system_facts_unix, dmi_still_decides_when_cpuinfo_is_unreadable) {
  EXPECT_EQ("vmware", build_default("VMware, Inc.", "VMware Virtual Platform", "").virtualization);
}

TEST(system_facts_unix, a_host_without_os_release_falls_back_to_the_kernel_name) {
  const host_facts::facts f = system_facts::build("", "Linux", "6.8.0-45", "x86_64", "box", "", "", bare_metal_flags, 1, 1024ULL * 1024 * 1024);
  EXPECT_EQ("Linux", f.os_name);
  EXPECT_EQ("linux", f.os_family);
  EXPECT_EQ("6.8.0-45", f.os_version);
}

TEST(system_facts_unix, os_release_without_a_pretty_name_composes_one) {
  const char *const terse = "NAME=\"Alpine Linux\"\nVERSION_ID=3.20.3\nID=alpine\n";
  const host_facts::facts f = system_facts::build(terse, "Linux", "6.6.0", "x86_64", "box", "", "", bare_metal_flags, 1, 1024ULL * 1024 * 1024);
  EXPECT_EQ("Alpine Linux 3.20.3", f.os_name);
}

TEST(system_facts_unix, unknown_counters_publish_nothing) {
  const host_facts::facts f = system_facts::build(ubuntu_os_release, "Linux", "6.8.0-45", "x86_64", "box", "", "", bare_metal_flags, 0, 0);
  EXPECT_EQ(0, f.cpu_cores);
  EXPECT_EQ(0, f.memory_gb);
}
