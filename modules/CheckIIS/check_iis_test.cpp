// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <algorithm>
#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_iis_internal.hpp"

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

using namespace check_iis::check_iis_internal;

// ============================================================================
// Pool state names
// ============================================================================

TEST(CheckIisAppPools, StateNamesMatchTheWasDocumentation) {
  EXPECT_EQ(pool_state_name(1), "uninitialized");
  EXPECT_EQ(pool_state_name(2), "initialized");
  EXPECT_EQ(pool_state_name(3), "running");
  EXPECT_EQ(pool_state_name(4), "disabling");
  EXPECT_EQ(pool_state_name(5), "disabled");
  EXPECT_EQ(pool_state_name(6), "shutdown_pending");
  EXPECT_EQ(pool_state_name(7), "delete_pending");
  EXPECT_EQ(pool_state_name(0), "unknown");
  EXPECT_EQ(pool_state_name(99), "unknown");
}

// ============================================================================
// PDH + WMI merge (pools)
// ============================================================================

TEST(CheckIisAppPools, MergeTakesCountersFromPdhAndAutoStartFromWmi) {
  instance_values pdh;
  pdh["DefaultAppPool"]["Current Application Pool State"] = 3;
  pdh["DefaultAppPool"]["Current Application Pool Uptime"] = 1234;
  pdh["DefaultAppPool"]["Total Application Pool Recycles"] = 2;
  const std::map<std::string, bool> wmi = {{"DefaultAppPool", true}};

  const auto pools = merge_pools(pdh, wmi);
  ASSERT_EQ(pools.size(), 1u);
  EXPECT_EQ(pools[0].name, "DefaultAppPool");
  EXPECT_EQ(pools[0].state, 3);
  EXPECT_EQ(pools[0].uptime, 1234);
  EXPECT_EQ(pools[0].recycles, 2);
  EXPECT_EQ(pools[0].auto_start, 1);
}

TEST(CheckIisAppPools, MergeAddsWmiOnlyPoolsAsUnknownState) {
  instance_values pdh;
  pdh["Running"]["Current Application Pool State"] = 3;
  const std::map<std::string, bool> wmi = {{"Running", true}, {"NeverStarted", true}, {"ManualPool", false}};

  const auto pools = merge_pools(pdh, wmi);
  ASSERT_EQ(pools.size(), 3u);
  // WMI-only pools carry state 0 ("unknown") so 'state != running' catches them.
  const auto never_started = std::find_if(pools.begin(), pools.end(), [](const pool_record &p) { return p.name == "NeverStarted"; });
  ASSERT_NE(never_started, pools.end());
  EXPECT_EQ(never_started->state, 0);
  EXPECT_EQ(never_started->auto_start, 1);
  const auto manual = std::find_if(pools.begin(), pools.end(), [](const pool_record &p) { return p.name == "ManualPool"; });
  ASSERT_NE(manual, pools.end());
  EXPECT_EQ(manual->auto_start, 0);
}

TEST(CheckIisAppPools, MergeWithoutWmiLeavesAutoStartUnknown) {
  instance_values pdh;
  pdh["DefaultAppPool"]["Current Application Pool State"] = 5;

  const auto pools = merge_pools(pdh, {});
  ASSERT_EQ(pools.size(), 1u);
  EXPECT_EQ(pools[0].auto_start, -1);
  EXPECT_EQ(pools[0].state, 5);
}

// ============================================================================
// PDH + WMI merge (sites)
// ============================================================================

TEST(CheckIisSites, MergeReadsCountersAndFlagsPdhPresence) {
  instance_values pdh;
  pdh["Default Web Site"]["Current Connections"] = 12;
  pdh["Default Web Site"]["Service Uptime"] = 4711;
  pdh["Default Web Site"]["Total Method Requests/sec"] = 3.5;
  pdh["Default Web Site"]["Bytes Total/sec"] = 1024.5;

  const auto sites = merge_sites(pdh, {{"Default Web Site", true}, {"StoppedSite", true}});
  ASSERT_EQ(sites.size(), 2u);
  const auto def = std::find_if(sites.begin(), sites.end(), [](const site_record &s) { return s.name == "Default Web Site"; });
  ASSERT_NE(def, sites.end());
  EXPECT_TRUE(def->in_pdh);
  EXPECT_EQ(def->connections, 12);
  EXPECT_EQ(def->uptime, 4711);
  EXPECT_DOUBLE_EQ(def->requests_per_sec, 3.5);
  EXPECT_DOUBLE_EQ(def->bytes_per_sec, 1024.5);
  const auto stopped = std::find_if(sites.begin(), sites.end(), [](const site_record &s) { return s.name == "StoppedSite"; });
  ASSERT_NE(stopped, sites.end());
  EXPECT_FALSE(stopped->in_pdh);
  EXPECT_EQ(stopped->uptime, 0);
  EXPECT_EQ(stopped->auto_start, 1);
}

// ============================================================================
// Worker instance parsing
// ============================================================================

TEST(CheckIisWorkers, ParsesPidAndPoolFromTheInstanceName) {
  long long pid = -1;
  std::string pool;
  ASSERT_TRUE(parse_worker_instance("4711_DefaultAppPool", pid, pool));
  EXPECT_EQ(pid, 4711);
  EXPECT_EQ(pool, "DefaultAppPool");
}

TEST(CheckIisWorkers, PoolNamesWithUnderscoresKeepTheirTail) {
  long long pid = -1;
  std::string pool;
  ASSERT_TRUE(parse_worker_instance("123_my_app_pool", pid, pool));
  EXPECT_EQ(pid, 123);
  EXPECT_EQ(pool, "my_app_pool");
}

TEST(CheckIisWorkers, UnexpectedShapesFallBackToTheRawInstance) {
  long long pid = -1;
  std::string pool;
  EXPECT_FALSE(parse_worker_instance("no-separator", pid, pool));
  EXPECT_EQ(pid, 0);
  EXPECT_EQ(pool, "no-separator");
  EXPECT_FALSE(parse_worker_instance("notapid_pool", pid, pool));
  EXPECT_EQ(pool, "notapid_pool");
}
