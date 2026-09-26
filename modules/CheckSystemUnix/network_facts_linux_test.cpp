// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <facts/network_facts.hpp>
#include <set>

// The live gather, on whatever host runs the tests. What it finds depends on
// the host, so this asserts the shape rather than the values: the rules every
// record has to follow wherever it runs.

TEST(network_facts_unix, every_record_follows_the_document_rules) {
  const std::vector<network_facts::interface_record> interfaces = network_facts::gather();
  std::set<std::string> ids;
  for (const network_facts::interface_record &nic : interfaces) {
    EXPECT_FALSE(nic.id.empty());
    EXPECT_TRUE(ids.insert(nic.id).second) << "duplicate id " << nic.id;
    // The loopback is left out, whatever it is called.
    EXPECT_NE(nic.id, "lo");
    if (!nic.mac.empty()) EXPECT_EQ(nic.mac, network_facts::normalize_mac(nic.mac)) << nic.id;
    EXPECT_GE(nic.speed_bps, 0) << nic.id;
    for (const std::string &address : nic.addresses) {
      EXPECT_FALSE(address.empty()) << nic.id;
      EXPECT_EQ(address.find('%'), std::string::npos) << nic.id << ": " << address;
    }
  }
}

TEST(network_facts_unix, the_same_network_is_the_same_list) {
  // Sorted by id, and each address list sorted: two reads of an unchanged
  // host compare equal, which is what keeps the revision from moving.
  const std::vector<network_facts::interface_record> first = network_facts::gather();
  const std::vector<network_facts::interface_record> second = network_facts::gather();
  ASSERT_EQ(first.size(), second.size());
  for (std::size_t i = 0; i < first.size(); ++i) {
    EXPECT_EQ(first[i].id, second[i].id);
    EXPECT_EQ(first[i].addresses, second[i].addresses) << first[i].id;
    if (i > 0) EXPECT_LT(first[i - 1].id, first[i].id);
  }
}
