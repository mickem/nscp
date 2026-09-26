// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "hyperv_facts.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_facts_test_helper.hpp>

using check_hyperv::check_hyperv_internal::vm_record;

namespace {
using nscapi::facts::testing::gathered_of;
using nscapi::facts::testing::json_of;

std::string hyperv_json(const nscapi::facts::response &out) { return json_of(out, "hyperv"); }

vm_record web() {
  vm_record vm;
  vm.id = "1e4f6e3b-0f7c-4a51-9c2e-6f8a0b1c2d3e";
  vm.name = "web-01";
  vm.state_code = 2;
  vm.uptime_seconds = 86400;
  vm.memory_assigned = 3LL * 1024 * 1024 * 1024;
  vm.memory_startup = 2048LL * 1024 * 1024;
  vm.memory_minimum = 512LL * 1024 * 1024;
  vm.memory_maximum = 8192LL * 1024 * 1024;
  vm.dynamic_memory = true;
  vm.vcpus = 4;
  vm.cpu_load = 12.5;
  vm.generation = 2;
  vm.version = "9.0";
  vm.snapshots = 2;
  vm.replication_mode = 1;
  return vm;
}
}  // namespace

TEST(HyperVFacts, PublishesTheConfigurationNotTheState) {
  nscapi::facts::response out;
  hyperv_facts::publish({web()}, 0, out);
  // No state, uptime, load or assigned memory: those belong to the check.
  EXPECT_EQ(hyperv_json(out),
            "{\"vms\":[{\"id\":\"web-01\",\"name\":\"web-01\",\"vm_id\":\"1e4f6e3b-0f7c-4a51-9c2e-6f8a0b1c2d3e\",\"version\":\"9.0\",\"generation\":2,"
            "\"vcpus\":4,\"memory_startup_bytes\":2147483648,\"dynamic_memory\":true,\"memory_minimum_bytes\":536870912,"
            "\"memory_maximum_bytes\":8589934592,\"checkpoints\":2,\"replication_mode\":\"primary\"}]}");
}

TEST(HyperVFacts, AnUnknownValueIsOmittedNotWrittenEmpty) {
  // A VM whose settings rows did not join has no generation, processors or
  // memory figures; static memory has no minimum or maximum.
  vm_record bare;
  bare.id = "9a8b7c6d-5e4f-4321-8765-0fedcba98765";
  bare.name = "db-01";
  bare.memory_startup = 4096LL * 1024 * 1024;
  bare.memory_minimum = 4096LL * 1024 * 1024;  // Hyper-V reports these for static memory too
  bare.memory_maximum = 4096LL * 1024 * 1024;

  nscapi::facts::response out;
  hyperv_facts::publish({bare}, 0, out);
  EXPECT_EQ(hyperv_json(out),
            "{\"vms\":[{\"id\":\"db-01\",\"name\":\"db-01\",\"vm_id\":\"9a8b7c6d-5e4f-4321-8765-0fedcba98765\",\"memory_startup_bytes\":4294967296,"
            "\"dynamic_memory\":false,\"checkpoints\":0,\"replication_mode\":\"none\"}]}");
}

TEST(HyperVFacts, RecordIdsAreTheVmNamesMadeUnique) {
  vm_record a = web();
  vm_record b = web();
  b.id = "9a8b7c6d-5e4f-4321-8765-0fedcba98765";
  vm_record c = web();
  c.id = "00000000-0000-0000-0000-000000000001";
  c.name = "db-01";
  vm_record unnamed;
  unnamed.id = "00000000-0000-0000-0000-000000000002";

  const std::vector<std::string> ids = hyperv_facts::record_ids({a, b, c, unnamed});
  ASSERT_EQ(ids.size(), 4u);
  EXPECT_EQ(ids[0], "web-01 (1e4f6e3b-0f7c-4a51-9c2e-6f8a0b1c2d3e)");
  EXPECT_EQ(ids[1], "web-01 (9a8b7c6d-5e4f-4321-8765-0fedcba98765)");
  EXPECT_EQ(ids[2], "db-01");
  EXPECT_EQ(ids[3], "00000000-0000-0000-0000-000000000002");
}

TEST(HyperVFacts, NoVmsIsAnEmptyListNotAMissingSet) {
  // Enabled and collected, with nothing to report: that is an answer, and it
  // must not read as "not collected" (which is what an absent set means).
  nscapi::facts::response out;
  hyperv_facts::publish({}, 0, out);
  EXPECT_EQ(hyperv_json(out), "{\"vms\":[]}");
}

TEST(HyperVFacts, StampsWhenTheValuesWereRead) {
  nscapi::facts::response out;
  hyperv_facts::publish({}, 1790000000, out);
  EXPECT_EQ(gathered_of(out, "hyperv"), nscapi::facts::format_time(1790000000));
}
