// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <facts/network_facts.hpp>
#include <nscapi/nscapi_facts_helper.hpp>

// The value-deciding half of the network fact set: every spelling a Windows
// host and a unix host have to agree on. The gathering is platform code, and
// is covered by each CheckSystem module's own test and by the REST facts
// integration test.

namespace {
std::string network_json(const nscapi::facts::response &out) {
  const PB::Facts::FactsMessage message = out.to_message();
  if (message.payload_size() == 0) return "(no payload)";
  for (const PB::Facts::FactSet &set : message.payload(0).sets()) {
    if (set.id() == "network") return nscapi::facts::tree::to_json(set.facts());
  }
  return "(no network set)";
}
}  // namespace

TEST(network_facts_mac, both_platform_spellings_agree) {
  // Windows spells it with dashes in upper case, unix with colons in lower.
  EXPECT_EQ("00:1a:2b:3c:4d:5e", network_facts::normalize_mac("00-1A-2B-3C-4D-5E"));
  EXPECT_EQ("00:1a:2b:3c:4d:5e", network_facts::normalize_mac("00:1a:2b:3c:4d:5e"));
  EXPECT_EQ("00:1a:2b:3c:4d:5e", network_facts::normalize_mac("00:1A:2b:3C:4d:5E"));
}

TEST(network_facts_mac, an_all_zero_address_is_no_address) {
  // What a tunnel or a loopback reports: not a hardware address anyone can
  // look up, so it is omitted rather than published as one.
  EXPECT_EQ("", network_facts::normalize_mac("00:00:00:00:00:00"));
  const unsigned char zeros[6] = {0, 0, 0, 0, 0, 0};
  EXPECT_EQ("", network_facts::mac_from_bytes(zeros, 6));
}

TEST(network_facts_mac, anything_that_is_not_six_octets_is_dropped) {
  EXPECT_EQ("", network_facts::normalize_mac(""));
  EXPECT_EQ("", network_facts::normalize_mac("00:1a:2b:3c:4d"));
  EXPECT_EQ("", network_facts::normalize_mac("00:1a:2b:3c:4d:5e:6f"));
  EXPECT_EQ("", network_facts::normalize_mac("00:1a:2b:3c:4d:5e:"));
  EXPECT_EQ("", network_facts::normalize_mac("00:1a:2b:3c:4d:zz"));
  EXPECT_EQ("", network_facts::normalize_mac("001a2b3c4d5e"));
  // An InfiniBand address is 20 octets; it is not a MAC and is not published
  // as a truncated one.
  EXPECT_EQ("", network_facts::normalize_mac("80:00:02:08:fe:80:00:00:00:00:00:00:00:02:c9:03:00:0f:00:01"));
}

TEST(network_facts_mac, reads_the_bytes_windows_hands_out) {
  const unsigned char bytes[6] = {0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e};
  EXPECT_EQ("00:1a:2b:3c:4d:5e", network_facts::mac_from_bytes(bytes, 6));
  EXPECT_EQ("", network_facts::mac_from_bytes(bytes, 5));
  EXPECT_EQ("", network_facts::mac_from_bytes(nullptr, 6));
}

TEST(network_facts_address, the_zone_is_not_part_of_the_address) {
  // Windows appends the interface index, unix the interface name: the same
  // address, two spellings, and neither says anything the record does not.
  EXPECT_EQ("fe80::1", network_facts::normalize_address("fe80::1%12"));
  EXPECT_EQ("fe80::1", network_facts::normalize_address("fe80::1%eth0"));
  EXPECT_EQ("192.168.1.10", network_facts::normalize_address("192.168.1.10"));
  EXPECT_EQ("2001:db8::1", network_facts::normalize_address("2001:db8::1"));
}

TEST(network_facts_status, windows_numbers_map_to_the_words_linux_writes) {
  EXPECT_EQ("up", network_facts::status_from_oper_status(1));
  EXPECT_EQ("down", network_facts::status_from_oper_status(2));
  EXPECT_EQ("testing", network_facts::status_from_oper_status(3));
  EXPECT_EQ("unknown", network_facts::status_from_oper_status(4));
  EXPECT_EQ("dormant", network_facts::status_from_oper_status(5));
  EXPECT_EQ("notpresent", network_facts::status_from_oper_status(6));
  EXPECT_EQ("lowerlayerdown", network_facts::status_from_oper_status(7));
  EXPECT_EQ("unknown", network_facts::status_from_oper_status(0));
  EXPECT_EQ("unknown", network_facts::status_from_oper_status(99));
}

TEST(network_facts_publish, one_record_per_interface) {
  network_facts::interface_record eth;
  eth.id = "eth0";
  eth.mac = "00:1a:2b:3c:4d:5e";
  eth.status = "up";
  eth.speed_bps = 1000000000LL;
  eth.addresses = {"192.168.1.10", "fe80::1"};
  network_facts::interface_record wifi;
  wifi.id = "Intel(R) Wi-Fi 6 AX201 160MHz";
  wifi.display_name = "Wi-Fi";
  wifi.status = "down";

  nscapi::facts::response out;
  network_facts::publish({eth, wifi}, 0, out);
  EXPECT_EQ(network_json(out),
            "{\"interfaces\":["
            "{\"id\":\"eth0\",\"mac\":\"00:1a:2b:3c:4d:5e\",\"status\":\"up\",\"speed_bps\":1000000000,\"addresses\":[\"192.168.1.10\",\"fe80::1\"]},"
            "{\"id\":\"Intel(R) Wi-Fi 6 AX201 160MHz\",\"display_name\":\"Wi-Fi\",\"status\":\"down\"}]}");
}

TEST(network_facts_publish, no_interfaces_is_an_empty_list_not_a_missing_set) {
  nscapi::facts::response out;
  network_facts::publish({}, 0, out);
  EXPECT_EQ(network_json(out), "{\"interfaces\":[]}");
}

TEST(network_facts_publish, stamps_when_the_values_were_read) {
  nscapi::facts::response out;
  network_facts::publish({}, 1790000000, out);
  const PB::Facts::FactsMessage message = out.to_message();
  ASSERT_EQ(message.payload(0).sets_size(), 1);
  EXPECT_EQ(message.payload(0).sets(0).gathered(), nscapi::facts::format_time(1790000000));
}
