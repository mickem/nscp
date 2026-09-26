// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include "collector_source.h"
#include "realtime_thread.hpp"

namespace {

collector_source::cpu_times make_times(const std::string &name, unsigned long long user, unsigned long long system, unsigned long long idle) {
  collector_source::cpu_times t;
  t.name = name;
  t.user = user;
  t.system = system;
  t.idle = idle;
  return t;
}

}  // namespace

TEST(collector_calc, core_index_reads_the_number_not_the_position) {
  EXPECT_EQ(collector_source::core_index("cpu"), -1);
  EXPECT_EQ(collector_source::core_index("cpu0"), 0);
  EXPECT_EQ(collector_source::core_index("cpu10"), 10);
  EXPECT_EQ(collector_source::core_index("cpux"), -2);
  EXPECT_EQ(collector_source::core_index("intr"), -2);
}

TEST(collector_calc, cores_past_ten_keep_their_own_numbers) {
  // std::map orders cpu10 and cpu11 before cpu2; each core must still be
  // reported under its own number.
  std::map<std::string, collector_source::cpu_times> before, after;
  before["cpu"] = make_times("cpu", 0, 0, 0);
  after["cpu"] = make_times("cpu", 120, 0, 1080);
  for (int i = 0; i < 12; ++i) {
    const std::string name = "cpu" + std::to_string(i);
    before[name] = make_times(name, 0, 0, 0);
    // Core i is busy i*5 percent of the interval.
    after[name] = make_times(name, static_cast<unsigned long long>(i) * 5, 0, 100 - static_cast<unsigned long long>(i) * 5);
  }
  const cpu_load load = collector_calc::calculate_cpu_load(before, after);
  ASSERT_EQ(load.cores, 12);
  for (int i = 0; i < 12; ++i) {
    EXPECT_EQ(load.core[i].core, i);
    EXPECT_DOUBLE_EQ(load.core[i].user, i * 5.0) << "core " << i;
  }
  EXPECT_DOUBLE_EQ(load.total.user, 10.0);
  EXPECT_DOUBLE_EQ(load.total.idle, 90.0);
}

TEST(collector_calc, rows_without_iowait_or_steal_still_add_up) {
  // The Darwin reader fills user, nice, system and idle only.
  std::map<std::string, collector_source::cpu_times> before, after;
  before["cpu"] = make_times("cpu", 100, 100, 800);
  after["cpu"] = make_times("cpu", 150, 125, 925);
  const cpu_load load = collector_calc::calculate_cpu_load(before, after);
  EXPECT_DOUBLE_EQ(load.total.user, 25.0);
  EXPECT_DOUBLE_EQ(load.total.kernel, 12.5);
  EXPECT_DOUBLE_EQ(load.total.idle, 62.5);
}

TEST(collector_calc, memory_views) {
  collector_source::memory_sample s;
  s.physical_total = 1000;
  s.physical_free = 100;
  s.cached_free = 400;
  s.swap_total = 50;
  s.swap_free = 20;
  const memory_info m = collector_calc::to_memory_info(s);
  EXPECT_EQ(m.physical.total, 1000u);
  EXPECT_EQ(m.physical.get_used(), 900u);
  EXPECT_EQ(m.cached.total, 1000u);
  EXPECT_EQ(m.cached.get_used(), 600u);
  EXPECT_EQ(m.swap.get_used(), 30u);
}

TEST(collector_calc, cached_free_never_exceeds_the_total) {
  collector_source::memory_sample s;
  s.physical_total = 1000;
  s.cached_free = 1200;
  EXPECT_EQ(collector_calc::to_memory_info(s).cached.get_used(), 0u);
}

TEST(collector_calc, network_rates_and_metadata) {
  std::map<std::string, collector_source::net_sample> before, after;
  collector_source::net_sample a;
  a.rx_bytes = 1000;
  a.tx_bytes = 500;
  a.rx_packets = 10;
  collector_source::net_sample b = a;
  b.rx_bytes = 3000;
  b.tx_bytes = 1500;
  b.rx_packets = 30;
  b.rx_errors = 4;
  b.status = "up";
  b.mac = "aa:bb:cc:dd:ee:ff";
  b.speed_bps = 1000000000LL;
  before["en0"] = a;
  after["en0"] = b;
  collector_source::net_sample fresh;
  after["utun0"] = fresh;

  const network_check::nics_type nics = collector_calc::calculate_network(before, after, 2.0);
  ASSERT_EQ(nics.size(), 2u);
  const network_check::network_interface &en0 = nics.front();
  EXPECT_EQ(en0.name, "en0");
  EXPECT_EQ(en0.rx_bytes_per_sec, 1000);
  EXPECT_EQ(en0.tx_bytes_per_sec, 500);
  EXPECT_EQ(en0.rx_packets_per_sec, 10);
  EXPECT_EQ(en0.rx_errors, 4);
  EXPECT_EQ(en0.status, "up");
  EXPECT_EQ(en0.mac, "aa:bb:cc:dd:ee:ff");
  EXPECT_EQ(en0.speed_bps, 1000000000LL);
  // An interface with no previous sample has no rate yet, and no status is
  // "unknown" rather than an empty string.
  EXPECT_EQ(nics.back().rx_bytes_per_sec, 0);
  EXPECT_EQ(nics.back().status, "unknown");
}

TEST(collector_calc, a_counter_that_went_backwards_reads_as_no_traffic) {
  std::map<std::string, collector_source::net_sample> before, after;
  before["en0"].rx_bytes = 5000;
  after["en0"].rx_bytes = 100;
  EXPECT_EQ(collector_calc::calculate_network(before, after, 1.0).front().rx_bytes_per_sec, 0);
}
