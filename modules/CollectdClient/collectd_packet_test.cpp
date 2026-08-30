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

  // Still decodes cleanly to a (clamped, non-empty) host without overrunning.
  p.add_gauge_value({1.0});
  const auto lists = decode_packet(p.get_buffer());
  ASSERT_EQ(lists.size(), 1u);
  EXPECT_FALSE(lists[0].host.empty());
  EXPECT_LE(lists[0].host.size(), collectd::max_string_length);
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

// ============================================================================
// String part sanitization: fields are NUL-terminated on the wire and '/' is
// the identifier separator, so neither may pass through into a single part.
// ============================================================================

TEST(CollectdPacket, SanitizesStringParts) {
  collectd::packet p;
  p.add_host(std::string("bad\x01host\x7f\n\0with", 16) + "/slash");
  p.add_gauge_value({1.0});
  const auto lists = decode_packet(p.get_buffer());
  ASSERT_EQ(lists.size(), 1u);
  EXPECT_EQ(lists[0].host, "badhostwith_slash");
}

// ============================================================================
// Reserve-aware fragmentation: when the caller will wrap each packet
// (signature/encryption), render() must leave that many bytes free.
// ============================================================================

TEST(CollectdBuilder, FragmentsWithinMtuMinusReserve) {
  const std::size_t reserve = 128;  // larger than any real sign/encrypt header
  collectd::collectd_builder b;
  b.set_time(1ULL << 30, 1ULL << 30);
  b.set_host("a-reasonably-long-hostname-for-padding");
  for (int i = 0; i < 500; ++i) {
    const std::string key = "metric_" + std::to_string(i);
    b.set_metric(key, std::to_string(i));
    b.add_metric("plugin" + std::to_string(i) + "-/gauge-value", "gauge:" + key);
  }
  collectd::collectd_builder::packet_list packets;
  b.render(packets, reserve);
  EXPECT_GT(packets.size(), 1u);
  std::size_t total_value_lists = 0;
  for (const auto &pk : packets) {
    EXPECT_LE(pk.get_size() + reserve, collectd::max_packet_size);
    total_value_lists += decode_packet(pk.get_buffer()).size();
  }
  EXPECT_EQ(total_value_lists, 500u);
}

// ============================================================================
// Security level parsing.
// ============================================================================

#include <net/collectd/collectd_crypto.hpp>

TEST(CollectdCrypto, ParsesSecurityLevels) {
  collectd::crypto::security_level level;
  EXPECT_TRUE(collectd::crypto::parse_security_level("", level));
  EXPECT_EQ(level, collectd::crypto::security_level::none);
  EXPECT_TRUE(collectd::crypto::parse_security_level("none", level));
  EXPECT_EQ(level, collectd::crypto::security_level::none);
  EXPECT_TRUE(collectd::crypto::parse_security_level("Sign", level));
  EXPECT_EQ(level, collectd::crypto::security_level::sign);
  EXPECT_TRUE(collectd::crypto::parse_security_level("ENCRYPT", level));
  EXPECT_EQ(level, collectd::crypto::security_level::encrypt);
  EXPECT_FALSE(collectd::crypto::parse_security_level("tls", level));
}

TEST(CollectdCrypto, RefusesEmptyUsername) {
  std::string out, error;
  EXPECT_FALSE(collectd::crypto::sign_packet("payload", "", "secret", out, error));
  EXPECT_FALSE(collectd::crypto::encrypt_packet("payload", "", "secret", out, error));
}

#ifdef USE_SSL

#include <openssl/evp.h>
#include <openssl/hmac.h>

// ============================================================================
// Signature part (0x0200): layout and an independently computed HMAC, per
// collectd's network.c (hash over username || packet, key = password).
// ============================================================================

TEST(CollectdCrypto, SignedPacketMatchesReferenceLayout) {
  collectd::packet p;
  p.add_host("h");
  p.add_gauge_value({1.5});
  const std::string payload = p.get_buffer();

  const std::string user = "nscp";
  const std::string password = "secret-key";
  std::string out, error;
  ASSERT_TRUE(collectd::crypto::sign_packet(payload, user, password, out, error)) << error;

  ASSERT_EQ(out.size(), 36u + user.size() + payload.size());
  const unsigned char *buf = reinterpret_cast<const unsigned char *>(out.data());
  EXPECT_EQ(read_be<uint16_t>(buf), 0x0200u);
  EXPECT_EQ(read_be<uint16_t>(buf + 2), 36u + user.size());
  // Username and untouched payload follow the 32-byte hash.
  EXPECT_EQ(out.substr(36, user.size()), user);
  EXPECT_EQ(out.substr(36 + user.size()), payload);

  // Independently recompute the HMAC over username || payload.
  const std::string authed = user + payload;
  unsigned char expected[EVP_MAX_MD_SIZE];
  unsigned int expected_len = 0;
  ASSERT_NE(HMAC(EVP_sha256(), password.data(), static_cast<int>(password.size()), reinterpret_cast<const unsigned char *>(authed.data()), authed.size(),
                 expected, &expected_len),
            nullptr);
  ASSERT_EQ(expected_len, 32u);
  EXPECT_EQ(std::memcmp(buf + 4, expected, 32), 0);
}

// ============================================================================
// Encryption part (0x0210): decrypt with AES-256/OFB (key = SHA-256 of the
// password) and verify the embedded SHA-1 checksum and payload, mirroring
// collectd's parse_part_encr_aes256.
// ============================================================================

TEST(CollectdCrypto, EncryptedPacketDecryptsToPayload) {
  collectd::packet p;
  p.add_host("h");
  p.add_derive_value({42});
  const std::string payload = p.get_buffer();

  const std::string user = "nscp";
  const std::string password = "secret-key";
  std::string out, error;
  ASSERT_TRUE(collectd::crypto::encrypt_packet(payload, user, password, out, error)) << error;

  ASSERT_EQ(out.size(), 42u + user.size() + payload.size());
  const unsigned char *buf = reinterpret_cast<const unsigned char *>(out.data());
  EXPECT_EQ(read_be<uint16_t>(buf), 0x0210u);
  EXPECT_EQ(read_be<uint16_t>(buf + 2), out.size());
  ASSERT_EQ(read_be<uint16_t>(buf + 4), user.size());
  EXPECT_EQ(out.substr(6, user.size()), user);
  const unsigned char *iv = buf + 6 + user.size();
  const unsigned char *encrypted = iv + 16;
  const std::size_t encrypted_len = out.size() - (6 + user.size() + 16);
  ASSERT_EQ(encrypted_len, 20 + payload.size());

  unsigned char key[EVP_MAX_MD_SIZE];
  unsigned int key_len = 0;
  ASSERT_EQ(EVP_Digest(password.data(), password.size(), key, &key_len, EVP_sha256(), nullptr), 1);
  ASSERT_EQ(key_len, 32u);

  std::vector<unsigned char> plain(encrypted_len);
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  ASSERT_NE(ctx, nullptr);
  int plain_len = 0, final_len = 0;
  ASSERT_EQ(EVP_DecryptInit_ex(ctx, EVP_aes_256_ofb(), nullptr, key, iv), 1);
  ASSERT_EQ(EVP_DecryptUpdate(ctx, plain.data(), &plain_len, encrypted, static_cast<int>(encrypted_len)), 1);
  ASSERT_EQ(EVP_DecryptFinal_ex(ctx, plain.data() + plain_len, &final_len), 1);
  EVP_CIPHER_CTX_free(ctx);
  ASSERT_EQ(static_cast<std::size_t>(plain_len + final_len), encrypted_len);

  // First 20 bytes: SHA-1 of the payload; the rest: the payload itself.
  unsigned char checksum[EVP_MAX_MD_SIZE];
  unsigned int checksum_len = 0;
  ASSERT_EQ(EVP_Digest(payload.data(), payload.size(), checksum, &checksum_len, EVP_sha1(), nullptr), 1);
  ASSERT_EQ(checksum_len, 20u);
  EXPECT_EQ(std::memcmp(plain.data(), checksum, 20), 0);
  EXPECT_EQ(std::string(reinterpret_cast<const char *>(plain.data()) + 20, plain.size() - 20), payload);
}

#endif  // USE_SSL
