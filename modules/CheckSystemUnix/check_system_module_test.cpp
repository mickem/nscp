// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckSystemUnix module class (CheckSystem): the settings
// it registers, the collector lifecycle across load / reload / unload, the
// answers the collector-backed checks give when there is no data, and the
// fact sets, diagnostics and metrics it produces with everything switched
// off.
//
// Everything loads with NSCAPI::dontStart, so the collector is constructed
// but never started - which is exactly what makes "no samples yet" a
// deterministic state to test. The checks themselves (filters, parsers,
// sources) are covered by check_system_unix_test; none of the tests here
// depends on what the host is running.

#include "CheckSystem.h"

#include <gtest/gtest.h>

#include <facts/service_facts.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/facts.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

const char *const kRoot = "/settings/system/unix";

typedef PB::Commands::QueryRequestMessage::Request request_type;
typedef PB::Commands::QueryResponseMessage::Response response_type;

request_type make_request(const std::string &command, const std::vector<std::string> &args = {}) {
  request_type request;
  request.set_command(command);
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

std::string message_of(const response_type &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

nscapi::facts::request facts_request(const std::string &reason) {
  PB::Facts::FactsQueryMessage message;
  message.add_payload()->set_reason(reason);
  return nscapi::facts::request(message.SerializeAsString());
}

class CheckSystemModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override {
    module_.unloadModule();
    core().reset();
  }

  bool load(const NSCAPI::moduleLoadMode mode = NSCAPI::dontStart) { return module_.loadModuleEx("", mode); }

  typedef void (CheckSystem::*check_fn)(const request_type &, response_type *);
  response_type run(check_fn fn, const std::string &command, const std::vector<std::string> &args = {}) {
    response_type response;
    (module_.*fn)(make_request(command, args), &response);
    return response;
  }

  nscapi::test_helpers::stdout_capture out_;
  CheckSystem module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(CheckSystemModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "default buffer length"), "1h");
  EXPECT_EQ(core().default_for(kRoot, "process history"), "false");
  EXPECT_EQ(core().default_for("/settings/default", "timezone"), "local");
}

// Every diagnostics table and fact set costs something, so all are opt-in.
TEST_F(CheckSystemModule, DiagnosticsAndFactsAreOffByDefault) {
  ASSERT_TRUE(load());

  const std::string diagnostics = std::string(kRoot) + "/diagnostics";
  for (const char *key : {"processes", "services", "errors"}) EXPECT_EQ(core().default_for(diagnostics, key), "false") << key;
  const std::string facts = std::string(kRoot) + "/facts";
  for (const char *key : {"os", "hardware", "network.interfaces", "services.installed", "software.installed"}) {
    EXPECT_EQ(core().default_for(facts, key), "false") << key;
  }
}

TEST_F(CheckSystemModule, LoadRegistersTheRealTimeAndServiceTagSections) {
  ASSERT_TRUE(load());

  const std::vector<std::string> &paths = core().registered_paths();
  for (const std::string suffix : {"/real-time/cpu", "/real-time/memory", "/real-time/process", "/service-tags"}) {
    EXPECT_NE(std::find(paths.begin(), paths.end(), kRoot + suffix), paths.end()) << suffix;
  }
}

// ============================================================================
// The collector lifecycle
// ============================================================================

// dontStart builds the collector but never samples: the CPU check reports
// that, rather than OK over no data.
TEST_F(CheckSystemModule, ACollectorThatHasNotSampledIsReported) {
  ASSERT_TRUE(load());

  const response_type r = run(&CheckSystem::check_cpu, "check_cpu");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN) << message_of(r);
  EXPECT_EQ(message_of(r), "No CPU data available yet (collector still initializing)");
}

// warmup-state picks the status of that answer, for a check polled right
// after a restart.
TEST_F(CheckSystemModule, WarmupStateDecidesTheNoDataStatus) {
  ASSERT_TRUE(load());

  const response_type r = run(&CheckSystem::check_cpu, "check_cpu", {"warmup-state=ok"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(message_of(r), "No CPU data available yet (collector still initializing)");
}

TEST_F(CheckSystemModule, ChecksAfterUnloadSayTheCollectorIsGone) {
  ASSERT_TRUE(load());
  ASSERT_TRUE(module_.unloadModule());

  EXPECT_EQ(message_of(run(&CheckSystem::check_cpu, "check_cpu")), "CPU collector not initialized");
  EXPECT_EQ(message_of(run(&CheckSystem::check_memory, "check_memory")), "Memory collector not initialized");
  EXPECT_EQ(message_of(run(&CheckSystem::check_pagefile, "check_pagefile")), "Pagefile collector not initialized");
  EXPECT_EQ(message_of(run(&CheckSystem::check_network, "check_network")), "Network collector not initialized");
  EXPECT_EQ(message_of(run(&CheckSystem::check_process_history, "check_process_history")), "Process history collector not initialized");
}

TEST_F(CheckSystemModule, ProcessHistoryOffNamesTheSettingToTurnItOn) {
  ASSERT_TRUE(load());

  const std::string m = message_of(run(&CheckSystem::check_process_history, "check_process_history"));
  EXPECT_EQ(m, "Process history is not enabled (set 'process history = true' under /settings/system/unix)");
  EXPECT_EQ(message_of(run(&CheckSystem::check_process_history_new, "check_process_history_new")), m);
}

TEST_F(CheckSystemModule, ProcessHistoryOnIsAccepted) {
  core().set_setting(kRoot, "process history", "true");
  ASSERT_TRUE(load());

  const std::string m = message_of(run(&CheckSystem::check_process_history, "check_process_history"));
  EXPECT_EQ(m.find("Process history is not enabled"), std::string::npos) << m;
}

// A reload replaces the collector; the replacement picks up the new settings.
TEST_F(CheckSystemModule, ReloadReplacesTheCollector) {
  ASSERT_TRUE(load());
  core().set_setting(kRoot, "process history", "true");
  ASSERT_TRUE(load());

  EXPECT_EQ(message_of(run(&CheckSystem::check_process_history, "check_process_history")).find("Process history is not enabled"), std::string::npos);
}

TEST_F(CheckSystemModule, CheckMemoryBeforeTheFirstSampleSaysSo) {
  ASSERT_TRUE(load());
  const response_type r = run(&CheckSystem::check_memory, "check_memory");
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "No memory data available yet (collector still initializing)");
}

// The rest dispatch straight to their check: an option none of them knows is
// rejected by each one's own parser, which is what shows the call arrived.
// Left out: check_cpu_utilization, check_kernel_stats, check_swap_io and
// check_kernel_memory take a one second sample before they parse their
// options, check_installed_software queries the package manager first and
// check_cpu_frequency reads sysfs first - none of that belongs in a unit test.
TEST_F(CheckSystemModule, EveryCheckIsDispatched) {
  ASSERT_TRUE(load());

  const std::vector<std::pair<check_fn, std::string> > checks = {
      {&CheckSystem::check_service, "check_service"},
      {&CheckSystem::check_process, "check_process"},
      {&CheckSystem::check_load, "check_load"},
      {&CheckSystem::check_uptime, "check_uptime"},
      {&CheckSystem::check_os_version, "check_os_version"},
      {&CheckSystem::check_os_updates, "check_os_updates"},
      {&CheckSystem::check_hostname, "check_hostname"},
      {&CheckSystem::check_temperature, "check_temperature"},
      {&CheckSystem::check_battery, "check_battery"},
  };
  for (const auto &c : checks) {
    const response_type r = run(c.first, c.second, {"no-such-option=1"});
    EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN) << c.second;
    EXPECT_NE(message_of(r).find("no-such-option"), std::string::npos) << c.second << ": " << message_of(r);
  }
}

// ============================================================================
// Facts, diagnostics and metrics with everything off
// ============================================================================

TEST_F(CheckSystemModule, NoFactSetsAreProducedByDefault) {
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("scheduled"), response);
  const PB::Facts::FactsMessage message = response.to_message();
  ASSERT_EQ(message.payload_size(), 1);
  EXPECT_EQ(message.payload(0).sets_size(), 0);
}

// Installed services are a bounded but forked query: never on the startup
// round, which says so instead of reporting an empty inventory.
TEST_F(CheckSystemModule, InstalledServicesAreNotCollectedAtStartup) {
  core().set_setting(std::string(kRoot) + "/facts", "services.installed", "true");
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("startup"), response);
  const PB::Facts::FactsMessage message = response.to_message();
  ASSERT_EQ(message.payload_size(), 1);
  bool found = false;
  for (const PB::Facts::FactSet &set : message.payload(0).sets()) {
    if (set.id() != service_facts::set_services) continue;
    found = true;
    EXPECT_EQ(set.error().find("Not collected during startup"), 0u) << set.error();
  }
  EXPECT_TRUE(found);
}

// A reload that turns a set off stops producing it.
TEST_F(CheckSystemModule, ReloadTurnsAFactSetOff) {
  core().set_setting(std::string(kRoot) + "/facts", "services.installed", "true");
  ASSERT_TRUE(load());
  core().set_setting(std::string(kRoot) + "/facts", "services.installed", "false");
  ASSERT_TRUE(load());

  nscapi::facts::response response;
  module_.fetchFacts(facts_request("startup"), response);
  EXPECT_EQ(response.to_message().payload(0).sets_size(), 0);
}

TEST_F(CheckSystemModule, NoDiagnosticsAreProducedByDefault) {
  ASSERT_TRUE(load());

  PB::Diagnostics::DiagnosticsMessage message;
  module_.fetchDiagnostics(&message);
  EXPECT_EQ(message.ByteSizeLong(), 0u);
}

// Before the collector has sampled, the CPU and memory sections are there
// but empty - absent values rather than zeros.
TEST_F(CheckSystemModule, MetricsBeforeTheFirstSampleCarryNoCpuOrMemoryValues) {
  ASSERT_TRUE(load());

  PB::Metrics::MetricsMessage::Response response;
  module_.fetchMetrics(&response);
  ASSERT_EQ(response.bundles_size(), 1);
  const PB::Metrics::MetricsBundle &system = response.bundles(0);
  EXPECT_EQ(system.key(), "system");
  ASSERT_GE(system.value_size(), 1);
  EXPECT_EQ(system.value(0).key(), "refresh_interval");
  bool saw_cpu = false;
  for (const PB::Metrics::MetricsBundle &child : system.children()) {
    if (child.key() == "cpu") {
      saw_cpu = true;
      EXPECT_EQ(child.value_size(), 0);
      EXPECT_EQ(child.children_size(), 0);
    }
  }
  EXPECT_TRUE(saw_cpu);
}

TEST_F(CheckSystemModule, MetricsAfterUnloadOnlyCarryTheRefreshInterval) {
  ASSERT_TRUE(load());
  ASSERT_TRUE(module_.unloadModule());

  PB::Metrics::MetricsMessage::Response response;
  module_.fetchMetrics(&response);
  ASSERT_EQ(response.bundles_size(), 1);
  EXPECT_EQ(response.bundles(0).value_size(), 1);
  EXPECT_EQ(response.bundles(0).children_size(), 0);
}
