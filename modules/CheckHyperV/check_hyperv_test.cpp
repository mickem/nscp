// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_hyperv_internal.hpp"

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

using namespace check_hyperv::check_hyperv_internal;

namespace {
const char *const kVmA = "1e4f6e3b-0f7c-4a51-9c2e-6f8a0b1c2d3e";
const char *const kVmB = "9A8B7C6D-5E4F-4321-8765-0FEDCBA98765";  // upper-case as WMI may spell it
}  // namespace

// --- value tables -----------------------------------------------------------

TEST(CheckHyperV, StateNamesCoverTheHyperVValues) {
  EXPECT_EQ(vm_state_name(2), "running");
  EXPECT_EQ(vm_state_name(3), "off");
  EXPECT_EQ(vm_state_name(6), "saved");
  EXPECT_EQ(vm_state_name(9), "paused");
  EXPECT_EQ(vm_state_name(10), "starting");
  EXPECT_EQ(vm_state_name(32773), "saving");
  EXPECT_EQ(vm_state_name(32779), "fast_saved");
  EXPECT_EQ(vm_state_name(4242), "state_4242");
}

TEST(CheckHyperV, HealthAndStatusNames) {
  EXPECT_EQ(vm_health_name(5), "ok");
  EXPECT_EQ(vm_health_name(20), "major_failure");
  EXPECT_EQ(vm_health_name(25), "critical_failure");
  EXPECT_EQ(vm_health_name(7), "health_7");
  EXPECT_EQ(vm_operational_status_name(2), "ok");
  EXPECT_EQ(vm_operational_status_name(15), "dormant");
  EXPECT_EQ(vm_operation_name(32772), "merging_disks");
  EXPECT_EQ(vm_operation_name(32774), "migrating");
  EXPECT_EQ(heartbeat_name(2), "ok");
  EXPECT_EQ(heartbeat_name(12), "no_contact");
  EXPECT_EQ(heartbeat_name(13), "lost_communication");
  EXPECT_EQ(heartbeat_name(99), "heartbeat_99");
}

TEST(CheckHyperV, ReplicationNames) {
  EXPECT_EQ(replication_mode_name(0), "none");
  EXPECT_EQ(replication_mode_name(1), "primary");
  EXPECT_EQ(replication_mode_name(2), "replica");
  EXPECT_EQ(replication_state_name(0), "disabled");
  EXPECT_EQ(replication_state_name(3), "replicating");
  EXPECT_EQ(replication_state_name(8), "critical");
  EXPECT_EQ(replication_health_name(0), "not_applicable");
  EXPECT_EQ(replication_health_name(1), "ok");
  EXPECT_EQ(replication_health_name(3), "critical");
}

// --- parsing ----------------------------------------------------------------

TEST(CheckHyperV, IntArrayParsingReadsTheWmiRendering) {
  EXPECT_EQ(parse_int_array("[2]"), std::vector<long long>({2}));
  EXPECT_EQ(parse_int_array("[2, 32768]"), std::vector<long long>({2, 32768}));
  EXPECT_TRUE(parse_int_array("[]").empty());
  EXPECT_TRUE(parse_int_array("").empty());
  EXPECT_TRUE(parse_int_array("<NULL>").empty());
}

TEST(CheckHyperV, VmRowsAreToldApartFromTheHostRowByTheirGuid) {
  EXPECT_TRUE(is_vm_guid(kVmA));
  EXPECT_TRUE(is_vm_guid(kVmB));
  EXPECT_FALSE(is_vm_guid("HV-HOST-01"));
  EXPECT_FALSE(is_vm_guid(""));
  EXPECT_FALSE(is_vm_guid("1e4f6e3b-0f7c-4a51-9c2e-6f8a0b1c2d3"));   // one short
  EXPECT_FALSE(is_vm_guid("1e4f6e3b_0f7c_4a51_9c2e_6f8a0b1c2d3e"));  // wrong separators
  EXPECT_FALSE(is_vm_guid("1e4f6e3b-0f7c-4a51-9c2e-6f8a0b1c2d3g"));  // not hex
}

TEST(CheckHyperV, SettingsOwnerIsTheVmGuidOfLiveConfigurationsOnly) {
  EXPECT_EQ(settings_owner_guid(std::string("Microsoft:") + kVmA + "\\4764334d-e001-4176-82ee-5594ec9b530e"), kVmA);
  EXPECT_EQ(settings_owner_guid(std::string("Microsoft:") + kVmB + "\\b637f346-6a0e-4dec-af52-bd70cb80a21d"), to_lower(kVmB));
  EXPECT_EQ(settings_owner_guid("Microsoft:Definition\\4764334d-e001-4176-82ee-5594ec9b530e"), "");
  EXPECT_EQ(settings_owner_guid(std::string("Microsoft:") + kVmA + ":ad3b4a5c-1234-4a51-9c2e-6f8a0b1c2d3e\\4764334d-e001-4176-82ee-5594ec9b530e"), "");
  EXPECT_EQ(settings_owner_guid("Something else"), "");
  EXPECT_EQ(settings_owner_guid(""), "");
}

TEST(CheckHyperV, GenerationIsParsedFromTheSubType) {
  EXPECT_EQ(generation_of("Microsoft:Hyper-V:SubType:1"), 1);
  EXPECT_EQ(generation_of("Microsoft:Hyper-V:SubType:2"), 2);
  EXPECT_EQ(generation_of(""), 0);
  EXPECT_EQ(generation_of("Microsoft:Hyper-V:SubType:"), 0);
  EXPECT_EQ(generation_of("Microsoft:Hyper-V:SubType:x"), 0);
}

// --- the join ---------------------------------------------------------------

raw_rows two_vms() {
  raw_rows rows;
  raw_vm a;
  a.id = kVmA;
  a.name = "web-01";
  a.enabled_state = 2;
  a.health_state = 5;
  a.operational_status = {2, 32772};
  a.uptime_ms = 90 * 1000 + 500;
  a.last_state_change_epoch = 1700000000;
  a.process_id = 4321;
  a.replication_mode = 1;
  a.replication_state = 3;
  a.replication_health = 1;
  rows.vms.push_back(a);
  raw_vm b;
  b.id = to_lower(kVmB);
  b.name = "db-01";
  b.enabled_state = 3;
  b.health_state = 5;
  b.operational_status = {10};
  rows.vms.push_back(b);

  raw_heartbeat hb;
  hb.vm_id = kVmA;
  hb.enabled_state = 2;
  hb.operational_status = {2, 32775};
  rows.heartbeats.push_back(hb);
  raw_heartbeat hb_off;
  hb_off.vm_id = to_lower(kVmB);
  hb_off.enabled_state = 3;  // service turned off in the VM settings
  hb_off.operational_status = {12};
  rows.heartbeats.push_back(hb_off);

  raw_memory_setting ms;
  ms.vm_id = kVmA;
  ms.startup_mb = 2048;
  ms.minimum_mb = 512;
  ms.maximum_mb = 8192;
  ms.dynamic = true;
  rows.memory_settings.push_back(ms);
  raw_memory_setting ms_b;
  ms_b.vm_id = to_lower(kVmB);
  ms_b.startup_mb = 4096;
  rows.memory_settings.push_back(ms_b);

  raw_memory m;
  m.vm_id = kVmA;
  m.bytes = 3LL * 1024 * 1024 * 1024;
  rows.memory.push_back(m);

  raw_processor_setting ps;
  ps.vm_id = kVmA;
  ps.count = 4;
  rows.processor_settings.push_back(ps);

  for (const long long load : {10, 20, 30, 40}) {
    raw_processor p;
    p.vm_id = kVmA;
    p.load_percentage = load;
    rows.processors.push_back(p);
  }

  raw_settings live;
  live.vm_id = kVmA;
  live.sub_type = "Microsoft:Hyper-V:SubType:2";
  live.version = "9.0";
  rows.settings.push_back(live);
  for (const long long created : {1600000000, 1500000000, 1650000000}) {
    raw_settings snap;
    snap.vm_id = kVmA;
    snap.snapshot = true;
    snap.creation_epoch = created;
    rows.settings.push_back(snap);
  }

  // A row for a VM that is not in the list (deleted between two queries)
  // must be dropped, not crash or create a record.
  raw_processor orphan;
  orphan.vm_id = "00000000-0000-0000-0000-000000000000";
  orphan.load_percentage = 100;
  rows.processors.push_back(orphan);
  return rows;
}

TEST(CheckHyperV, RecordsJoinEveryClassOntoTheVm) {
  const std::vector<vm_record> records = build_records(two_vms());
  ASSERT_EQ(records.size(), 2u);
  const vm_record &a = records[0];
  EXPECT_EQ(a.id, kVmA);
  EXPECT_EQ(a.name, "web-01");
  EXPECT_EQ(a.state(), "running");
  EXPECT_TRUE(a.is_running());
  EXPECT_EQ(a.health(), "ok");
  EXPECT_EQ(a.operational_status(), "ok");
  EXPECT_EQ(a.operation(), "merging_disks");
  EXPECT_EQ(a.heartbeat, "ok");
  EXPECT_EQ(a.uptime_seconds, 90);
  EXPECT_EQ(a.last_state_change_epoch, 1700000000);
  EXPECT_EQ(a.process_id, 4321);
  EXPECT_EQ(a.memory_assigned, 3LL * 1024 * 1024 * 1024);
  EXPECT_EQ(a.memory_startup, 2048LL * 1024 * 1024);
  EXPECT_EQ(a.memory_minimum, 512LL * 1024 * 1024);
  EXPECT_EQ(a.memory_maximum, 8192LL * 1024 * 1024);
  EXPECT_TRUE(a.dynamic_memory);
  EXPECT_EQ(a.vcpus, 4);
  EXPECT_DOUBLE_EQ(a.cpu_load, 25.0);
  EXPECT_EQ(a.generation, 2);
  EXPECT_EQ(a.version, "9.0");
  EXPECT_EQ(a.snapshots, 3);
  EXPECT_EQ(a.oldest_snapshot_epoch, 1500000000);
  EXPECT_EQ(a.replication_mode_s(), "primary");
  EXPECT_EQ(a.replication_state_s(), "replicating");
  EXPECT_EQ(a.replication_health_s(), "ok");
}

TEST(CheckHyperV, AnOffVmKeepsItsDefaults) {
  const std::vector<vm_record> records = build_records(two_vms());
  ASSERT_EQ(records.size(), 2u);
  const vm_record &b = records[1];
  EXPECT_EQ(b.name, "db-01");
  EXPECT_EQ(b.state(), "off");
  EXPECT_FALSE(b.is_running());
  EXPECT_EQ(b.operational_status(), "stopped");
  EXPECT_EQ(b.operation(), "none");
  EXPECT_EQ(b.heartbeat, "disabled");
  EXPECT_EQ(b.uptime_seconds, 0);
  EXPECT_EQ(b.memory_assigned, 0);
  EXPECT_EQ(b.memory_startup, 4096LL * 1024 * 1024);
  EXPECT_FALSE(b.dynamic_memory);
  EXPECT_EQ(b.vcpus, 0);
  EXPECT_DOUBLE_EQ(b.cpu_load, 0.0);
  EXPECT_EQ(b.generation, 0);
  EXPECT_EQ(b.snapshots, 0);
  EXPECT_EQ(b.oldest_snapshot_epoch, 0);
  EXPECT_EQ(b.replication_mode_s(), "none");
  EXPECT_EQ(b.replication_health_s(), "not_applicable");
}

TEST(CheckHyperV, AVmWithoutAHeartbeatComponentSaysSo) {
  raw_rows rows;
  raw_vm a;
  a.id = kVmA;
  a.name = "linux-01";
  a.enabled_state = 2;
  rows.vms.push_back(a);
  const std::vector<vm_record> records = build_records(rows);
  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records[0].heartbeat, "none");
  EXPECT_EQ(records[0].operational_status(), "status_0");
}

TEST(CheckHyperV, NoVmsGivesNoRecords) { EXPECT_TRUE(build_records(raw_rows()).empty()); }

TEST(CheckHyperV, OnlyRealisedCheckpointsAreCheckpoints) {
  EXPECT_TRUE(is_checkpoint_type("Microsoft:Hyper-V:Snapshot:Realized"));
  EXPECT_FALSE(is_checkpoint_type("Microsoft:Hyper-V:Snapshot:Recovery"));
  EXPECT_FALSE(is_checkpoint_type("Microsoft:Hyper-V:Snapshot:Replica"));
  EXPECT_FALSE(is_checkpoint_type("Microsoft:Hyper-V:Snapshot:Planned"));
  EXPECT_FALSE(is_checkpoint_type("Microsoft:Hyper-V:System:Realized"));
}

TEST(CheckHyperV, VmsTheCountersSeeButWmiHidesAreExplained) {
  EXPECT_EQ(hidden_vms_reason(0, 1),
            "Hyper-V reports 1 virtual machine(s) on this host but none are visible to this account: run as an elevated administrator or a member "
            "of Hyper-V Administrators");
}

TEST(CheckHyperV, AgreeingOrUnreadableCountersExplainNothing) {
  EXPECT_EQ(hidden_vms_reason(0, 0), "");
  EXPECT_EQ(hidden_vms_reason(0, -1), "");
  EXPECT_EQ(hidden_vms_reason(2, 2), "");
  EXPECT_EQ(hidden_vms_reason(1, 3), "");
}

// --- logical processors -----------------------------------------------------

TEST(CheckHyperV, TotalIsTheMeanOfThePercentagesAndTheSumOfTheSwitches) {
  std::vector<processor_sample> lps;
  for (int i = 0; i < 4; ++i) {
    processor_sample s;
    s.processor = "Hv LP " + std::to_string(i);
    s.total_run_time = 10.0 * (i + 1);  // 10, 20, 30, 40
    s.guest_run_time = 8.0 * (i + 1);
    s.hypervisor_run_time = 2.0 * (i + 1);
    s.idle_time = 100.0 - 10.0 * (i + 1);
    s.context_switches = 1000.0;
    lps.push_back(s);
  }
  const std::vector<processor_sample> all = with_total(lps);
  ASSERT_EQ(all.size(), 5u);
  EXPECT_EQ(all[0].processor, "Hv LP 0");
  const processor_sample &total = all.back();
  EXPECT_EQ(total.processor, "total");
  EXPECT_DOUBLE_EQ(total.total_run_time, 25.0);
  EXPECT_DOUBLE_EQ(total.guest_run_time, 20.0);
  EXPECT_DOUBLE_EQ(total.hypervisor_run_time, 5.0);
  EXPECT_DOUBLE_EQ(total.idle_time, 75.0);
  EXPECT_DOUBLE_EQ(total.context_switches, 4000.0);
}

TEST(CheckHyperV, NoProcessorsGivesNoTotal) { EXPECT_TRUE(with_total({}).empty()); }

TEST(CheckHyperV, PercentagesAreRoundedToOneDecimal) {
  EXPECT_DOUBLE_EQ(round1(26.87731), 26.9);
  EXPECT_DOUBLE_EQ(round1(0.569985), 0.6);
  EXPECT_DOUBLE_EQ(round1(0.04), 0.0);
  EXPECT_DOUBLE_EQ(round1(100.0), 100.0);
  std::vector<processor_sample> lps(3);
  lps[0].total_run_time = 1.0;
  lps[1].total_run_time = 2.0;
  lps[2].total_run_time = 2.0;
  EXPECT_DOUBLE_EQ(with_total(lps).back().total_run_time, 1.7);
}
