// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the fact gatherers: the pure parsers get captured files from
// machines this one is not, and the live gatherers are checked for the
// invariants the document rules impose - a record id that is stable and
// unique, and a field that is absent rather than invented.

#include "facts.h"

#include <gtest/gtest.h>

#include <ctime>
#include <set>
#include <string>
#include <vector>

using namespace check_system_facts;

// ============================================================================
// /proc/cpuinfo
// ============================================================================

namespace {
// Two sockets, four cores each: the shape a `cpu cores` + `physical id`
// kernel reports, captured from a dual-socket x86 host.
const char *TWO_SOCKET_CPUINFO =
    "processor\t: 0\nphysical id\t: 0\ncpu cores\t: 4\nmodel name\t: Intel(R) Xeon(R) Gold 6338\n\n"
    "processor\t: 1\nphysical id\t: 0\ncpu cores\t: 4\nmodel name\t: Intel(R) Xeon(R) Gold 6338\n\n"
    "processor\t: 2\nphysical id\t: 1\ncpu cores\t: 4\nmodel name\t: Intel(R) Xeon(R) Gold 6338\n\n"
    "processor\t: 3\nphysical id\t: 1\ncpu cores\t: 4\nmodel name\t: Intel(R) Xeon(R) Gold 6338\n";

// An ARM board: no `model name`, no `physical id`, no `cpu cores`.
const char *ARM_CPUINFO =
    "processor\t: 0\nBogoMIPS\t: 108.00\n\n"
    "processor\t: 1\nBogoMIPS\t: 108.00\n\n"
    "Hardware\t: BCM2835\n";
}  // namespace

TEST(CheckSystemFactsCpuinfo, ReadsTheModelSocketsAndCores) {
  std::string model;
  long long sockets = 0;
  long long cores = 0;
  parse_cpuinfo(TWO_SOCKET_CPUINFO, model, sockets, cores);

  EXPECT_EQ("Intel(R) Xeon(R) Gold 6338", model);
  EXPECT_EQ(2, sockets);
  // Four cores per socket across two sockets - not the four logical
  // processors the file lists, which would halve a hyper-threaded machine.
  EXPECT_EQ(8, cores);
}

// A kernel that reports neither `physical id` nor `cpu cores` still has a
// processor, and reporting zero sockets would be a fact that is not true.
TEST(CheckSystemFactsCpuinfo, FallsBackToTheProcessorCountWhenTheKernelReportsNoTopology) {
  std::string model;
  long long sockets = 0;
  long long cores = 0;
  parse_cpuinfo(ARM_CPUINFO, model, sockets, cores);

  EXPECT_EQ("BCM2835", model);
  EXPECT_EQ(1, sockets);
  EXPECT_EQ(2, cores);
}

TEST(CheckSystemFactsCpuinfo, AnEmptyFileLeavesEverythingUnset) {
  std::string model = "stale";
  long long sockets = 7;
  long long cores = 7;
  parse_cpuinfo("", model, sockets, cores);

  EXPECT_EQ("", model);
  EXPECT_EQ(0, sockets);
  EXPECT_EQ(0, cores);
}

// ============================================================================
// /proc/meminfo
// ============================================================================

TEST(CheckSystemFactsMeminfo, ConvertsKibibytesToBytes) {
  // The key says bytes, and every other size in the document is in bytes.
  EXPECT_EQ(16384ull * 1024ull, parse_meminfo_total("MemTotal:       16384 kB\nMemFree:  100 kB\n"));
}

TEST(CheckSystemFactsMeminfo, AFileWithoutMemTotalReportsNothing) {
  EXPECT_EQ(0ull, parse_meminfo_total("MemFree:  100 kB\n"));
  EXPECT_EQ(0ull, parse_meminfo_total(""));
}

// ============================================================================
// /proc/uptime
// ============================================================================

TEST(CheckSystemFactsBootTime, IsNowMinusTheUptime) {
  const std::time_t now = 1788000000;
  EXPECT_EQ(now - 3600, parse_boot_time("3600.42 7200.00\n", now));
}

// Zero would render as 1970-01-01, which is a boot time no host has.
TEST(CheckSystemFactsBootTime, IsUnsetWhenTheUptimeCannotBeRead) {
  EXPECT_EQ(0, parse_boot_time("", 1788000000));
  EXPECT_EQ(0, parse_boot_time("not a number\n", 1788000000));
  EXPECT_EQ(0, parse_boot_time("3600.42\n", 0));
}

// ============================================================================
// The live gatherers
// ============================================================================

TEST(CheckSystemFactsOs, ReportsLinuxWithAKernelAndAnArchitecture) {
  std::string error;
  const os_facts found = gather_os(error);

  EXPECT_EQ("", error);
  EXPECT_EQ("linux", found.family);
  EXPECT_FALSE(found.kernel.empty());
  EXPECT_FALSE(found.arch.empty());
  // The pretty name, or the kernel identity when /etc/os-release is missing -
  // never empty, which is the same guarantee check_os_version's ${os} makes.
  EXPECT_FALSE(found.name.empty());
}

TEST(CheckSystemFactsIdentity, ReportsAHostnameAndAnFqdnThatAgreeWithIt) {
  std::string error;
  const identity_facts found = gather_identity(error);

  EXPECT_EQ("", error);
  EXPECT_FALSE(found.hostname.empty());
  // The FQDN falls back to the bare host name on a host without DNS, which is
  // every container; it is never empty and never unrelated to the host name.
  ASSERT_FALSE(found.fqdn.empty());
  EXPECT_EQ(0u, found.fqdn.find(found.hostname));
}

TEST(CheckSystemFactsHardware, ReportsWhatItCanReadWithoutInventingTheRest) {
  std::string error;
  const hardware_facts found = gather_hardware(error);

  // /proc/cpuinfo and /proc/meminfo are readable in every environment this
  // runs in, so the gather must not report itself unavailable.
  EXPECT_EQ("", error);
  EXPECT_GT(found.memory_total_bytes, 0ull);
  EXPECT_GT(found.cpu_cores, 0);
  EXPECT_GT(found.cpu_sockets, 0);
}

TEST(CheckSystemFactsInterfaces, EveryRecordIsIdentifiedByItsInterfaceName) {
  std::string error;
  const std::vector<interface_facts> found = gather_interfaces(error);

  EXPECT_EQ("", error);
  ASSERT_FALSE(found.empty()) << "every host has at least a loopback interface";
  std::set<std::string> ids;
  for (const interface_facts &entry : found) {
    EXPECT_FALSE(entry.id.empty()) << "a record without an id makes the core reject the whole set";
    // getifaddrs lists an interface once per address family; one record per
    // interface is what the document rules require.
    EXPECT_TRUE(ids.insert(entry.id).second) << "duplicate interface record: " << entry.id;
    EXPECT_FALSE(entry.state.empty()) << "state is unknown rather than absent: " << entry.id;
  }
}

TEST(CheckSystemFactsInterfaces, TheLoopbackInterfaceCarriesItsAddress) {
  std::string error;
  const std::vector<interface_facts> found = gather_interfaces(error);

  bool saw_loopback = false;
  for (const interface_facts &entry : found) {
    if (entry.id != "lo") continue;
    saw_loopback = true;
    bool saw_address = false;
    for (const std::string &address : entry.addresses) saw_address = saw_address || address == "127.0.0.1";
    EXPECT_TRUE(saw_address) << "the loopback interface must report 127.0.0.1";
  }
  EXPECT_TRUE(saw_loopback);
}
