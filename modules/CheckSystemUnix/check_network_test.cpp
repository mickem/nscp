// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_network.h"

#include <gtest/gtest.h>

using network_check::network_interface;

namespace {
network_interface make_nic() {
  network_interface nic;
  nic.name = "eth0";
  nic.status = "up";
  nic.mac = "00:11:22:33:44:55";
  nic.speed_bps = 1000LL * 1000 * 1000;  // 1 GBit/s
  nic.rx_bytes_per_sec = 10LL * 1000 * 1000;
  nic.tx_bytes_per_sec = 5LL * 1000 * 1000;
  nic.rx_packets_per_sec = 800;
  nic.tx_packets_per_sec = 400;
  nic.rx_errors = 3;
  nic.tx_errors = 1;
  return nic;
}
}  // namespace

TEST(CheckNetworkInterface, DefaultsAreZeroed) {
  const network_interface nic;
  EXPECT_EQ(0, nic.get_speed_bps());
  EXPECT_EQ(0, nic.get_received());
  EXPECT_EQ(0, nic.get_sent());
  EXPECT_EQ(0, nic.get_total());
  EXPECT_EQ(0, nic.get_received_packets());
  EXPECT_EQ(0, nic.get_sent_packets());
  EXPECT_EQ(0, nic.get_rx_errors());
  EXPECT_EQ(0, nic.get_tx_errors());
}

TEST(CheckNetworkInterface, MetadataGetters) {
  const network_interface nic = make_nic();
  EXPECT_EQ("eth0", nic.get_name());
  EXPECT_EQ("up", nic.get_status());
  EXPECT_EQ("00:11:22:33:44:55", nic.get_mac());
  EXPECT_EQ(1000LL * 1000 * 1000, nic.get_speed_bps());
  EXPECT_EQ("eth0", nic.show());
}

TEST(CheckNetworkInterface, EnabledMirrorsOperstateUp) {
  network_interface nic = make_nic();
  EXPECT_EQ("true", nic.get_enabled());
  nic.status = "down";
  EXPECT_EQ("false", nic.get_enabled());
  nic.status = "unknown";
  EXPECT_EQ("false", nic.get_enabled());
}

TEST(CheckNetworkInterface, TrafficGetters) {
  const network_interface nic = make_nic();
  EXPECT_EQ(10LL * 1000 * 1000, nic.get_received());
  EXPECT_EQ(5LL * 1000 * 1000, nic.get_sent());
  EXPECT_EQ(15LL * 1000 * 1000, nic.get_total());
  EXPECT_EQ(800, nic.get_received_packets());
  EXPECT_EQ(400, nic.get_sent_packets());
  EXPECT_EQ(3, nic.get_rx_errors());
  EXPECT_EQ(1, nic.get_tx_errors());
}

TEST(CheckNetworkInterface, UsagePercentOfLinkSpeed) {
  // 10 MB/s = 80 MBit/s on a 1 GBit/s link = 8%; 5 MB/s = 4%; total 12%.
  const network_interface nic = make_nic();
  EXPECT_EQ(8, nic.get_usage_in());
  EXPECT_EQ(4, nic.get_usage_out());
  EXPECT_EQ(12, nic.get_usage_total());
}

TEST(CheckNetworkInterface, UsageIsZeroWhenLinkSpeedUnknown) {
  // Virtual interfaces / link-down report speed 0 (or -1 in raw sysfs): the
  // documented contract is usage reads 0 rather than dividing by zero.
  network_interface nic = make_nic();
  nic.speed_bps = 0;
  EXPECT_EQ(0, nic.get_usage_in());
  EXPECT_EQ(0, nic.get_usage_out());
  EXPECT_EQ(0, nic.get_usage_total());
  nic.speed_bps = -1;
  EXPECT_EQ(0, nic.get_usage_in());
  EXPECT_EQ(0, nic.get_usage_out());
  EXPECT_EQ(0, nic.get_usage_total());
}

TEST(CheckNetworkInterface, CopyAndAssignPreserveCounters) {
  const network_interface nic = make_nic();
  const network_interface copy(nic);
  EXPECT_EQ(nic.get_total(), copy.get_total());
  network_interface assigned;
  assigned = nic;
  EXPECT_EQ(nic.get_mac(), assigned.get_mac());
  EXPECT_EQ(nic.get_usage_total(), assigned.get_usage_total());
}
