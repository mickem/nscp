/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#ifdef WIN32
#include <objbase.h>
#endif

#include "check_network.hpp"

// ============================================================================
// helper::parse_prd_name tests
// ============================================================================

TEST(NetworkHelper, ParsePrdNameNoSpecialChars) { EXPECT_EQ(network_check::helper::parse_prd_name("Intel Ethernet"), "Intel Ethernet"); }

TEST(NetworkHelper, ParsePrdNameSquareBrackets) { EXPECT_EQ(network_check::helper::parse_prd_name("NIC [1]"), "NIC (1)"); }

TEST(NetworkHelper, ParsePrdNameHash) { EXPECT_EQ(network_check::helper::parse_prd_name("NIC#2"), "NIC_2"); }

TEST(NetworkHelper, ParsePrdNameSlashes) { EXPECT_EQ(network_check::helper::parse_prd_name("a/b\\c"), "a_b_c"); }

TEST(NetworkHelper, ParsePrdNameCombined) { EXPECT_EQ(network_check::helper::parse_prd_name("[eth#0/vlan\\1]"), "(eth_0_vlan_1)"); }

TEST(NetworkHelper, ParsePrdNameEmpty) { EXPECT_EQ(network_check::helper::parse_prd_name(""), ""); }

TEST(NetworkHelper, ParseNifNamePassthrough) {
  // parse_nif_name is identity
  EXPECT_EQ(network_check::helper::parse_nif_name("Ethernet 1"), "Ethernet 1");
}

// ============================================================================
// network_interface struct tests
// ============================================================================

TEST(NetworkInterface, DefaultConstruction) {
  network_check::network_interface n;
  EXPECT_EQ(n.name, "");
  EXPECT_EQ(n.NetConnectionID, "");
  EXPECT_EQ(n.MACAddress, "");
  EXPECT_EQ(n.NetConnectionStatus, "");
  EXPECT_EQ(n.NetEnabled, "");
  EXPECT_EQ(n.Speed, "");
  EXPECT_FALSE(n.has_nif);
  EXPECT_FALSE(n.has_prd);
  EXPECT_EQ(n.BytesReceivedPersec, 0);
  EXPECT_EQ(n.BytesSentPersec, 0);
  EXPECT_EQ(n.BytesTotalPersec, 0);
}

TEST(NetworkInterface, Accessors) {
  network_check::network_interface n;
  n.name = "Intel Ethernet";
  n.NetConnectionID = "Ethernet 1";
  n.MACAddress = "AA:BB:CC:DD:EE:FF";
  n.NetConnectionStatus = "2";
  n.NetEnabled = "true";
  n.Speed = "1000000000";
  n.BytesReceivedPersec = 500;
  n.BytesSentPersec = 300;
  n.BytesTotalPersec = 800;

  EXPECT_EQ(n.get_name(), "Intel Ethernet");
  EXPECT_EQ(n.get_NetConnectionID(), "Ethernet 1");
  EXPECT_EQ(n.get_MACAddress(), "AA:BB:CC:DD:EE:FF");
  EXPECT_EQ(n.get_NetConnectionStatus(), "2");
  EXPECT_EQ(n.get_NetEnabled(), "true");
  EXPECT_EQ(n.get_Speed(), "1000000000");
  EXPECT_EQ(n.getBytesReceivedPersec(), 500);
  EXPECT_EQ(n.getBytesSentPersec(), 300);
  EXPECT_EQ(n.getBytesTotalPersec(), 800);
}

TEST(NetworkInterface, ShowFormat) {
  network_check::network_interface n;
  n.name = "Intel Ethernet";
  n.NetConnectionID = "Ethernet 1";
  EXPECT_EQ(n.show(), "Intel Ethernet (Ethernet 1)");
}

TEST(NetworkInterface, IsCompleteRequiresNif) {
  network_check::network_interface n;
  EXPECT_FALSE(n.is_compleate());
  n.has_nif = true;
  EXPECT_TRUE(n.is_compleate());
}

TEST(NetworkInterface, CopyConstruction) {
  network_check::network_interface n;
  n.name = "eth0";
  n.NetConnectionID = "Ethernet";
  n.MACAddress = "11:22:33:44:55:66";
  n.has_nif = true;
  n.has_prd = true;
  n.BytesReceivedPersec = 1000;
  n.BytesSentPersec = 2000;
  n.BytesTotalPersec = 3000;

  const network_check::network_interface copy(n);
  EXPECT_EQ(copy.name, "eth0");
  EXPECT_EQ(copy.NetConnectionID, "Ethernet");
  EXPECT_EQ(copy.MACAddress, "11:22:33:44:55:66");
  EXPECT_TRUE(copy.has_nif);
  EXPECT_TRUE(copy.has_prd);
  EXPECT_EQ(copy.BytesReceivedPersec, 1000);
  EXPECT_EQ(copy.BytesSentPersec, 2000);
  EXPECT_EQ(copy.BytesTotalPersec, 3000);
}

TEST(NetworkInterface, BuildMetricsWithoutPrd) {
  network_check::network_interface n;
  n.name = "eth0";
  n.NetConnectionID = "Ethernet";
  n.MACAddress = "AA:BB:CC:DD:EE:FF";
  n.NetConnectionStatus = "2";
  n.NetEnabled = "true";
  n.Speed = "1000000000";
  n.has_prd = false;

  PB::Metrics::MetricsBundle section;
  n.build_metrics(&section);

  // Without prd data: 5 string metrics (NetConnectionID, MACAddress, NetConnectionStatus, NetEnabled, Speed)
  EXPECT_EQ(section.value_size(), 5);
}

TEST(NetworkInterface, BuildMetricsWithPrd) {
  network_check::network_interface n;
  n.name = "eth0";
  n.NetConnectionID = "Ethernet";
  n.MACAddress = "AA:BB:CC:DD:EE:FF";
  n.NetConnectionStatus = "2";
  n.NetEnabled = "true";
  n.Speed = "1000000000";
  n.has_prd = true;
  n.BytesReceivedPersec = 100;
  n.BytesSentPersec = 200;
  n.BytesTotalPersec = 300;

  PB::Metrics::MetricsBundle section;
  n.build_metrics(&section);

  // With prd data: 5 string metrics + 3 int metrics = 8
  EXPECT_EQ(section.value_size(), 8);
}

// ============================================================================
// network_data thread-safety tests (require COM for WMI)
// ============================================================================

class NetworkDataTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    com_initialized_ = SUCCEEDED(hr) || hr == S_FALSE;
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
  }

  static void TearDownTestSuite() {
    if (com_initialized_) {
      CoUninitialize();
    }
  }

  static bool com_initialized_;
};

bool NetworkDataTest::com_initialized_ = false;

TEST(NetworkData, GetReturnsEmptyBeforeFetch) {
  network_check::network_data data;
  const auto result = data.get();
  EXPECT_TRUE(result.empty());
}

TEST_F(NetworkDataTest, FetchDoesNotThrow) {
  network_check::network_data data;
  EXPECT_NO_THROW(data.fetch());
}

TEST_F(NetworkDataTest, FetchedDataHasNames) {
  network_check::network_data data;
  data.fetch();
  for (const auto &n : data.get()) {
    EXPECT_FALSE(n.name.empty());
  }
}

TEST_F(NetworkDataTest, FetchedInterfacesAreComplete) {
  network_check::network_data data;
  data.fetch();
  for (const auto &n : data.get()) {
    EXPECT_TRUE(n.is_compleate());
  }
}
