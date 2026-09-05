// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <net/collectd/collectd_packet.hpp>

#include <gtest/gtest.h>

#include <boost/endian/conversion.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// A standalone decoder for the collectd binary network protocol, used to
// verify the bytes produced by collectd::packet / collectd_builder. It mirrors
// the receiver in tests/collectd-submit.test.ts but in C++.
// ============================================================================
namespace {

struct decoded_value_list {
  std::string host;
  std::string plugin;
  std::string plugin_instance;
  std::string type;
  std::string type_instance;
  unsigned long long time_hr = 0;
  unsigned long long interval_hr = 0;
  std::vector<uint8_t> value_types;  // 1=gauge, 2=derive
  std::vector<double> gauges;
  std::vector<int64_t> derives;
};

// Read a big-endian integer of width N from `p`.
template <class T>
T read_be(const unsigned char *p) {
  T v = 0;
  std::memcpy(&v, p, sizeof(T));
  return boost::endian::big_to_native(v);
}

// Walk the TLV part stream of one packet, emitting a decoded_value_list for
// every values part using the host/plugin/type context established so far.
std::vector<decoded_value_list> decode_packet(const std::string &buf) {
  std::vector<decoded_value_list> out;
  const unsigned char *p = reinterpret_cast<const unsigned char *>(buf.data());
  std::size_t off = 0;
  std::string host, plugin, plugin_instance, type, type_instance;
  unsigned long long time_hr = 0, interval_hr = 0;

  while (off + 4 <= buf.size()) {
    const uint16_t part_type = read_be<uint16_t>(p + off);
    const uint16_t part_len = read_be<uint16_t>(p + off + 2);
    if (part_len < 4 || off + part_len > buf.size()) break;
    const unsigned char *body = p + off + 4;
    const std::size_t body_len = part_len - 4;

    switch (part_type) {
      case collectd::part_host:
        host.assign(reinterpret_cast<const char *>(body), body_len ? body_len - 1 : 0);
        break;
      case collectd::part_plugin:
        plugin.assign(reinterpret_cast<const char *>(body), body_len ? body_len - 1 : 0);
        plugin_instance.clear();
        break;
      case collectd::part_plugin_instance:
        plugin_instance.assign(reinterpret_cast<const char *>(body), body_len ? body_len - 1 : 0);
        break;
      case collectd::part_type:
        type.assign(reinterpret_cast<const char *>(body), body_len ? body_len - 1 : 0);
        type_instance.clear();
        break;
      case collectd::part_type_instance:
        type_instance.assign(reinterpret_cast<const char *>(body), body_len ? body_len - 1 : 0);
        break;
      case collectd::part_time_hr:
        time_hr = read_be<uint64_t>(body);
        break;
      case collectd::part_interval_hr:
        interval_hr = read_be<uint64_t>(body);
        break;
      case collectd::part_values: {
        decoded_value_list vl;
        vl.host = host;
        vl.plugin = plugin;
        vl.plugin_instance = plugin_instance;
        vl.type = type;
        vl.type_instance = type_instance;
        vl.time_hr = time_hr;
        vl.interval_hr = interval_hr;
        const uint16_t count = read_be<uint16_t>(body);
        const unsigned char *types = body + 2;
        const unsigned char *values = types + count;
        for (uint16_t i = 0; i < count; ++i) {
          const uint8_t vt = types[i];
          vl.value_types.push_back(vt);
          const unsigned char *vp = values + i * 8;
          if (vt == collectd::value_gauge) {
            uint64_t le = 0;
            std::memcpy(&le, vp, sizeof(le));
            const uint64_t bits = boost::endian::little_to_native(le);
            double d = 0;
            std::memcpy(&d, &bits, sizeof(d));
            vl.gauges.push_back(d);
          } else {
            vl.derives.push_back(read_be<int64_t>(vp));
          }
        }
        out.push_back(vl);
        break;
      }
      default:
        break;
    }
    off += part_len;
  }
  return out;
}

}  // namespace

// ============================================================================
// packet: low-level part encoding
// ============================================================================

TEST(CollectdPacket, EmptyPacketHasEmptyBuffer) {
  collectd::packet p;
  EXPECT_TRUE(p.get_buffer().empty());
  EXPECT_EQ(p.get_size(), 0u);
}

TEST(CollectdPacket, HostStringPartLayout) {
  collectd::packet p;
  p.add_host("myhost");
  const std::string &buf = p.get_buffer();
  // type(2) + length(2) + "myhost" + NUL = 4 + 6 + 1 = 11 bytes.
  ASSERT_EQ(buf.size(), 11u);
  EXPECT_EQ(read_be<uint16_t>(reinterpret_cast<const unsigned char *>(buf.data())), collectd::part_host);
  EXPECT_EQ(read_be<uint16_t>(reinterpret_cast<const unsigned char *>(buf.data()) + 2), 11u);
  EXPECT_EQ(buf[buf.size() - 1], '\0');
  EXPECT_EQ(buf.compare(4, 6, "myhost"), 0);
}

TEST(CollectdPacket, ClampsOverlongStringPart) {
  collectd::packet p;
  // A hostname far larger than a datagram: the length field must not wrap.
  p.add_host(std::string(40000, 'x'));
  const std::string buf = p.get_buffer();
  const uint16_t len = read_be<uint16_t>(reinterpret_cast<const unsigned char *>(buf.data()) + 2);
  // Length stays positive, within the datagram, and matches the actual part.
  EXPECT_GT(len, 0u);
  EXPECT_LE(len, collectd::max_packet_size);
  EXPECT_EQ(buf.size(), len);
  // Clamped to the receiver's identifier limit, leaving room for the rest of
  // the value-list rather than consuming the whole datagram.
  EXPECT_EQ(len, collectd::max_string_length + 5);

  // Still decodes cleanly to a (clamped, non-empty) host without overrunning.
  p.add_gauge_value({1.0});
  const auto lists = decode_packet(p.get_buffer());
  ASSERT_EQ(lists.size(), 1u);
  EXPECT_FALSE(lists[0].host.empty());
  EXPECT_LE(lists[0].host.size(), collectd::max_string_length);
}

TEST(CollectdPacket, ClampsOversizedValueList) {
  collectd::packet p;
  p.add_host("h");
  // Far more values than a 1452-byte datagram can carry (9 bytes each): the
  // part must be truncated to what fits rather than overflowing the datagram.
  std::list<double> values(500, 1.5);
  p.add_gauge_value(values);

  const std::string buf = p.get_buffer();
  EXPECT_LE(buf.size(), collectd::max_packet_size);
  EXPECT_GT(p.clamped_values(), 0u);

  const auto lists = decode_packet(buf);
  ASSERT_EQ(lists.size(), 1u);
  // Everything the part claims to carry is really there, and nothing was lost
  // silently: emitted + clamped accounts for every configured value.
  EXPECT_GT(lists[0].gauges.size(), 0u);
  EXPECT_EQ(lists[0].gauges.size() + p.clamped_values(), values.size());
  EXPECT_TRUE(std::all_of(lists[0].gauges.begin(), lists[0].gauges.end(), [](double d) { return d == 1.5; }));
}

TEST(CollectdPacket, ValuesPartLengthNeverWraps) {
  collectd::packet p;
  // Above ~3600 values the 16-bit length field wraps; the cap must make that
  // unreachable, so the encoded length stays positive and inside the datagram.
  p.add_derive_value(std::list<long long>(20000, 7));
  const std::string buf = p.get_buffer();
  ASSERT_GE(buf.size(), 6u);
  const uint16_t len = read_be<uint16_t>(reinterpret_cast<const unsigned char *>(buf.data()) + 2);
  const uint16_t count = read_be<uint16_t>(reinterpret_cast<const unsigned char *>(buf.data()) + 4);
  EXPECT_GT(len, 0u);
  EXPECT_EQ(len, buf.size());
  EXPECT_LE(buf.size(), collectd::max_packet_size);
  EXPECT_EQ(len, 6u + count * 9u);
}

TEST(CollectdPacket, DropsValuesWhenNoRoomIsLeft) {
  collectd::packet p;
  // Fill the datagram with identifier parts, then ask for values that cannot
  // fit at all: no malformed zero-value part may be emitted.
  while (p.remaining() >= collectd::max_string_length + 5) {
    p.add_plugin_instance(std::string(collectd::max_string_length, 'x'));
  }
  const std::size_t size_before = p.get_size();
  p.add_gauge_value({1.0, 2.0});
  EXPECT_EQ(p.get_size(), size_before);
  EXPECT_EQ(p.clamped_values(), 2u);
  EXPECT_LE(p.get_size(), collectd::max_packet_size);
}

TEST(CollectdPacket, StringPartsNeverExceedTheDatagram) {
  collectd::packet p;
  // Every identifier part is clamped, so no sequence of them can push the
  // packet past the receiver's read size.
  for (int i = 0; i < 100; ++i) {
    p.add_plugin(std::string(4000, 'p'));
    p.add_type(std::string(4000, 't'));
  }
  EXPECT_LE(p.get_size(), collectd::max_packet_size);
}

TEST(CollectdPacket, TimeAndIntervalPartsAreBigEndian) {
  collectd::packet p;
  p.add_time_hr(0x1122334455667788ULL);
  p.add_interval_hr(10ULL << 30);
  const auto lists = [&] {
    // Append a values part so decode_packet emits a record carrying the context.
    p.add_gauge_value({1.0});
    return decode_packet(p.get_buffer());
  }();
  ASSERT_EQ(lists.size(), 1u);
  EXPECT_EQ(lists[0].time_hr, 0x1122334455667788ULL);
  EXPECT_EQ(lists[0].interval_hr, 10ULL << 30);
}

TEST(CollectdPacket, GaugeValueRoundTripsAsLittleEndianDouble) {
  collectd::packet p;
  p.add_host("h");
  p.add_gauge_value({1024.5, -0.25});
  const auto lists = decode_packet(p.get_buffer());
  ASSERT_EQ(lists.size(), 1u);
  ASSERT_EQ(lists[0].gauges.size(), 2u);
  EXPECT_DOUBLE_EQ(lists[0].gauges[0], 1024.5);
  EXPECT_DOUBLE_EQ(lists[0].gauges[1], -0.25);
  EXPECT_EQ(lists[0].value_types[0], collectd::value_gauge);
}

TEST(CollectdPacket, DeriveValueRoundTripsAsBigEndianInt64) {
  collectd::packet p;
  p.add_host("h");
  p.add_derive_value({42, 9000000000LL});
  const auto lists = decode_packet(p.get_buffer());
  ASSERT_EQ(lists.size(), 1u);
  ASSERT_EQ(lists[0].derives.size(), 2u);
  EXPECT_EQ(lists[0].derives[0], 42);
  EXPECT_EQ(lists[0].derives[1], 9000000000LL);
  EXPECT_EQ(lists[0].value_types[0], collectd::value_derive);
}

// ============================================================================
// collectd_builder: metric mapping + rendering
// ============================================================================

TEST(CollectdBuilder, RendersDeriveMetricWithPluginAndTypeInstances) {
  collectd::collectd_builder b;
  b.set_time(123ULL << 30, 10ULL << 30);
  b.set_host("myhost");
  b.set_metric("system.cpu.total.user", "42");
  b.add_metric("cpu-total/cpu-user", "derive:system.cpu.total.user");

  collectd::collectd_builder::packet_list packets;
  b.render(packets);
  ASSERT_EQ(packets.size(), 1u);
  const auto lists = decode_packet(packets.front().get_buffer());
  ASSERT_EQ(lists.size(), 1u);
  EXPECT_EQ(lists[0].host, "myhost");
  EXPECT_EQ(lists[0].plugin, "cpu");
  EXPECT_EQ(lists[0].plugin_instance, "total");
  EXPECT_EQ(lists[0].type, "cpu");
  EXPECT_EQ(lists[0].type_instance, "user");
  ASSERT_EQ(lists[0].derives.size(), 1u);
  EXPECT_EQ(lists[0].derives[0], 42);
}

TEST(CollectdBuilder, RendersGaugeMetricFromMetricReference) {
  collectd::collectd_builder b;
  b.set_time(1ULL << 30, 1ULL << 30);
  b.set_host("h");
  b.set_metric("system.mem.physical.avail", "1024.5");
  b.add_metric("memory-/memory-available", "gauge:system.mem.physical.avail");

  collectd::collectd_builder::packet_list packets;
  b.render(packets);
  ASSERT_EQ(packets.size(), 1u);
  const auto lists = decode_packet(packets.front().get_buffer());
  ASSERT_EQ(lists.size(), 1u);
  EXPECT_EQ(lists[0].plugin, "memory");
  EXPECT_EQ(lists[0].type, "memory");
  EXPECT_EQ(lists[0].type_instance, "available");
  ASSERT_EQ(lists[0].gauges.size(), 1u);
  EXPECT_DOUBLE_EQ(lists[0].gauges[0], 1024.5);
}

TEST(CollectdBuilder, GaugeAcceptsLiteralValue) {
  collectd::collectd_builder b;
  b.set_time(1ULL << 30, 1ULL << 30);
  b.set_host("h");
  b.add_metric("load-/gauge-value", "gauge:7");

  collectd::collectd_builder::packet_list packets;
  b.render(packets);
  const auto lists = decode_packet(packets.front().get_buffer());
  ASSERT_EQ(lists.size(), 1u);
  ASSERT_EQ(lists[0].gauges.size(), 1u);
  EXPECT_DOUBLE_EQ(lists[0].gauges[0], 7.0);
}

TEST(CollectdBuilder, ExpandsVariablesFromMatchingMetricNames) {
  collectd::collectd_builder b;
  b.set_time(1ULL << 30, 1ULL << 30);
  b.set_host("h");
  // Two cores, each with a .user metric.
  b.set_metric("system.cpu.core 0.user", "100");
  b.set_metric("system.cpu.core 1.user", "200");
  b.add_variable("core", "system.cpu.core (.*).user");
  b.add_metric("cpu-${core}/cpu-user", "derive:system.cpu.core ${core}.user");

  collectd::collectd_builder::packet_list packets;
  b.render(packets);
  std::vector<decoded_value_list> all;
  for (const auto &pk : packets) {
    const auto lists = decode_packet(pk.get_buffer());
    all.insert(all.end(), lists.begin(), lists.end());
  }
  ASSERT_EQ(all.size(), 2u);
  // The variable should have expanded into per-core plugin instances 0 and 1.
  std::vector<std::string> instances;
  std::vector<int64_t> values;
  for (const auto &vl : all) {
    EXPECT_EQ(vl.plugin, "cpu");
    EXPECT_EQ(vl.type, "cpu");
    EXPECT_EQ(vl.type_instance, "user");
    instances.push_back(vl.plugin_instance);
    ASSERT_EQ(vl.derives.size(), 1u);
    values.push_back(vl.derives[0]);
  }
  EXPECT_NE(std::find(instances.begin(), instances.end(), "0"), instances.end());
  EXPECT_NE(std::find(instances.begin(), instances.end(), "1"), instances.end());
  EXPECT_NE(std::find(values.begin(), values.end(), 100), values.end());
  EXPECT_NE(std::find(values.begin(), values.end(), 200), values.end());
}

TEST(CollectdBuilder, UnmatchedVariableProducesNoMetrics) {
  collectd::collectd_builder b;
  b.set_time(1ULL << 30, 1ULL << 30);
  b.set_host("h");
  // No metric matches the variable's regex, so the templated metric expands
  // to nothing.
  b.add_variable("diskid", "system.metrics.pdh.disk_queue_length.disk_queue_length_(.*)$");
  b.add_metric("disk-${diskid}/queue_length", "gauge:system.metrics.pdh.disk_queue_length.disk_queue_length_${diskid}");

  collectd::collectd_builder::packet_list packets;
  b.render(packets);
  // Nothing rendered, so there must be no value-lists — and crucially no empty
  // trailing packet that would go out as a zero-length datagram.
  std::size_t value_lists = 0;
  for (const auto &pk : packets) value_lists += decode_packet(pk.get_buffer()).size();
  EXPECT_EQ(value_lists, 0u);
  for (const auto &pk : packets) EXPECT_GT(pk.get_size(), 0u);
}

// ============================================================================
// Fragmentation: a large metric set must split into multiple packets, each of
// which stays within the collectd network buffer size.
// ============================================================================

TEST(CollectdBuilder, FragmentsLargeMetricSetWithinMtu) {
  collectd::collectd_builder b;
  b.set_time(1ULL << 30, 1ULL << 30);
  b.set_host("a-reasonably-long-hostname-for-padding");
  for (int i = 0; i < 500; ++i) {
    const std::string key = "metric_" + std::to_string(i);
    b.set_metric(key, std::to_string(i));
    b.add_metric("plugin" + std::to_string(i) + "-/gauge-value", "gauge:" + key);
  }

  collectd::collectd_builder::packet_list packets;
  b.render(packets);
  EXPECT_GT(packets.size(), 1u);

  std::size_t total_value_lists = 0;
  for (const auto &pk : packets) {
    EXPECT_LE(pk.get_size(), collectd::max_packet_size);
    EXPECT_GT(pk.get_size(), 0u);  // no empty trailing packet even when fragmenting
    total_value_lists += decode_packet(pk.get_buffer()).size();
  }
  EXPECT_EQ(total_value_lists, 500u);
}
