// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Pure-logic tests for CheckActiveDirectory: the Kerberos AS-REQ probe
// encoder/classifier and the replication filter helpers. The encoder is
// verified with an independent DER reader written here (not the encoder's own
// helpers), so a structural encoding bug cannot cancel itself out.

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/helpers.hpp>
#include <string>
#include <vector>

#include "ad_replication_filter.hpp"
#include "kdc_probe.hpp"

// Normally provided by NSC_WRAP_DLL() in the auto-generated module.cpp; in the
// test binary there is no generated module, so define the singleton here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

using kdc_probe::bytes;

// --- independent minimal DER reader ----------------------------------------

struct tlv {
  unsigned char tag;
  bytes content;
};

bool read_der_len(const bytes &d, std::size_t &pos, std::size_t &out) {
  if (pos >= d.size()) return false;
  const unsigned char first = d[pos++];
  if ((first & 0x80) == 0) {
    out = first;
  } else {
    const std::size_t count = first & 0x7f;
    if (count == 0 || count > 4 || d.size() - pos < count) return false;
    out = 0;
    for (std::size_t i = 0; i < count; ++i) out = (out << 8) | d[pos++];
  }
  // Against the bytes remaining, never `pos + out`: that overflows size_t on a
  // 32-bit build and lets a crafted length walk the offset backwards.
  return out <= d.size() - pos;
}

// Parse a run of sibling TLVs covering `data` exactly.
std::vector<tlv> parse_tlvs(const bytes &data) {
  std::vector<tlv> out;
  std::size_t pos = 0;
  while (pos < data.size()) {
    tlv t;
    t.tag = data[pos++];
    std::size_t len = 0;
    if (!read_der_len(data, pos, len)) {
      ADD_FAILURE() << "malformed DER at offset " << pos;
      return out;
    }
    t.content.assign(data.begin() + pos, data.begin() + pos + len);
    pos += len;
    out.push_back(t);
  }
  return out;
}

const tlv *find_tag(const std::vector<tlv> &fields, unsigned char tag) {
  for (const tlv &t : fields) {
    if (t.tag == tag) return &t;
  }
  return nullptr;
}

long long read_int(const tlv &t) {
  EXPECT_EQ(0x02, t.tag) << "expected INTEGER";
  long long v = 0;
  for (const unsigned char b : t.content) v = (v << 8) | b;
  return v;
}

std::string read_string(const tlv &t) {
  EXPECT_EQ(0x1b, t.tag) << "expected GeneralString";
  return std::string(t.content.begin(), t.content.end());
}

// --- AS-REQ encoder ---------------------------------------------------------

class AsReqTest : public ::testing::Test {
 protected:
  bytes req;
  std::vector<tlv> body_fields;

  void SetUp() override {
    req = kdc_probe::build_as_req("EXAMPLE.COM", "nscp-probe", 12345678UL);

    const std::vector<tlv> outer = parse_tlvs(req);
    ASSERT_EQ(1u, outer.size());
    ASSERT_EQ(0x6a, outer[0].tag) << "AS-REQ is [APPLICATION 10]";

    const std::vector<tlv> kdc_req = parse_tlvs(outer[0].content);
    ASSERT_EQ(1u, kdc_req.size());
    ASSERT_EQ(0x30, kdc_req[0].tag);

    const std::vector<tlv> fields = parse_tlvs(kdc_req[0].content);
    const tlv *pvno = find_tag(fields, 0xa1);
    const tlv *msg_type = find_tag(fields, 0xa2);
    const tlv *body = find_tag(fields, 0xa4);
    ASSERT_NE(nullptr, pvno);
    ASSERT_NE(nullptr, msg_type);
    ASSERT_NE(nullptr, body);
    EXPECT_EQ(5, read_int(parse_tlvs(pvno->content)[0]));
    EXPECT_EQ(10, read_int(parse_tlvs(msg_type->content)[0]));

    const std::vector<tlv> body_seq = parse_tlvs(body->content);
    ASSERT_EQ(1u, body_seq.size());
    ASSERT_EQ(0x30, body_seq[0].tag);
    body_fields = parse_tlvs(body_seq[0].content);
  }

  const tlv *body_field(unsigned char tag) { return find_tag(body_fields, tag); }
};

TEST_F(AsReqTest, EncodesKdcOptionsAsEmpty32BitString) {
  const tlv *options = body_field(0xa0);
  ASSERT_NE(nullptr, options);
  const std::vector<tlv> bits = parse_tlvs(options->content);
  ASSERT_EQ(1u, bits.size());
  EXPECT_EQ(0x03, bits[0].tag);
  EXPECT_EQ(bytes({0x00, 0x00, 0x00, 0x00, 0x00}), bits[0].content);
}

TEST_F(AsReqTest, EncodesClientPrincipal) {
  const tlv *cname = body_field(0xa1);
  ASSERT_NE(nullptr, cname);
  const std::vector<tlv> pn = parse_tlvs(parse_tlvs(cname->content)[0].content);
  const tlv *name_type = find_tag(pn, 0xa0);
  const tlv *name_string = find_tag(pn, 0xa1);
  ASSERT_NE(nullptr, name_type);
  ASSERT_NE(nullptr, name_string);
  EXPECT_EQ(1, read_int(parse_tlvs(name_type->content)[0]));  // NT-PRINCIPAL
  const std::vector<tlv> names = parse_tlvs(parse_tlvs(name_string->content)[0].content);
  ASSERT_EQ(1u, names.size());
  EXPECT_EQ("nscp-probe", read_string(names[0]));
}

TEST_F(AsReqTest, EncodesRealmAndKrbtgtServicePrincipal) {
  const tlv *realm = body_field(0xa2);
  ASSERT_NE(nullptr, realm);
  EXPECT_EQ("EXAMPLE.COM", read_string(parse_tlvs(realm->content)[0]));

  const tlv *sname = body_field(0xa3);
  ASSERT_NE(nullptr, sname);
  const std::vector<tlv> pn = parse_tlvs(parse_tlvs(sname->content)[0].content);
  const tlv *name_type = find_tag(pn, 0xa0);
  const tlv *name_string = find_tag(pn, 0xa1);
  ASSERT_NE(nullptr, name_type);
  ASSERT_NE(nullptr, name_string);
  EXPECT_EQ(2, read_int(parse_tlvs(name_type->content)[0]));  // NT-SRV-INST
  const std::vector<tlv> names = parse_tlvs(parse_tlvs(name_string->content)[0].content);
  ASSERT_EQ(2u, names.size());
  EXPECT_EQ("krbtgt", read_string(names[0]));
  EXPECT_EQ("EXAMPLE.COM", read_string(names[1]));
}

TEST_F(AsReqTest, EncodesTillNonceAndEtypes) {
  const tlv *till = body_field(0xa5);
  ASSERT_NE(nullptr, till);
  const std::vector<tlv> time = parse_tlvs(till->content);
  ASSERT_EQ(1u, time.size());
  EXPECT_EQ(0x18, time[0].tag);  // GeneralizedTime
  ASSERT_EQ(15u, time[0].content.size());
  EXPECT_EQ('Z', time[0].content.back());

  const tlv *nonce = body_field(0xa7);
  ASSERT_NE(nullptr, nonce);
  EXPECT_EQ(12345678, read_int(parse_tlvs(nonce->content)[0]));

  const tlv *etypes = body_field(0xa8);
  ASSERT_NE(nullptr, etypes);
  const std::vector<tlv> etype_list = parse_tlvs(parse_tlvs(etypes->content)[0].content);
  ASSERT_EQ(3u, etype_list.size());
  EXPECT_EQ(18, read_int(etype_list[0]));  // aes256-cts-hmac-sha1-96
  EXPECT_EQ(17, read_int(etype_list[1]));  // aes128-cts-hmac-sha1-96
  EXPECT_EQ(23, read_int(etype_list[2]));  // rc4-hmac
}

TEST(AsReq, EncodesElementsLargerThan64k) {
  // A realm this large pushes the req-body (where the realm appears twice)
  // past 0xFFFF, forcing 3-byte long-form DER lengths; an encoder that
  // truncates to 2 length octets would emit a self-inconsistent blob this
  // independent reader could not walk.
  const std::string realm(70000, 'R');
  const bytes req = kdc_probe::build_as_req(realm, "c", 1UL);
  const std::vector<tlv> outer = parse_tlvs(req);
  ASSERT_EQ(1u, outer.size());
  ASSERT_EQ(0x6a, outer[0].tag);
  const std::vector<tlv> fields = parse_tlvs(parse_tlvs(outer[0].content)[0].content);
  const tlv *body = find_tag(fields, 0xa4);
  ASSERT_NE(nullptr, body);
  const std::vector<tlv> body_fields = parse_tlvs(parse_tlvs(body->content)[0].content);
  const tlv *realm_field = find_tag(body_fields, 0xa2);
  ASSERT_NE(nullptr, realm_field);
  EXPECT_EQ(realm, read_string(parse_tlvs(realm_field->content)[0]));
}

TEST(AsReq, NonceWithHighBitGetsLeadingZeroByte) {
  // 0x80000000 would read as negative without a leading 0x00 pad octet.
  const bytes req = kdc_probe::build_as_req("R", "c", 0x80000000UL);
  // Locate the nonce ([7]) via the reader and confirm the INTEGER is 5 bytes.
  const std::vector<tlv> outer = parse_tlvs(req);
  ASSERT_EQ(1u, outer.size());
  const std::vector<tlv> fields = parse_tlvs(parse_tlvs(outer[0].content)[0].content);
  const tlv *body = find_tag(fields, 0xa4);
  ASSERT_NE(nullptr, body);
  const std::vector<tlv> body_fields = parse_tlvs(parse_tlvs(body->content)[0].content);
  const tlv *nonce = find_tag(body_fields, 0xa7);
  ASSERT_NE(nullptr, nonce);
  const tlv integer = parse_tlvs(nonce->content)[0];
  ASSERT_EQ(5u, integer.content.size());
  EXPECT_EQ(0x00, integer.content[0]);
  EXPECT_EQ(0x80, integer.content[1]);
}

// --- response classifier ----------------------------------------------------

TEST(ClassifyResponse, AsRepMeansAlive) {
  const bytes as_rep = {0x6b, 0x03, 0x30, 0x01, 0x00};
  const kdc_probe::classification c = kdc_probe::classify_response(as_rep);
  EXPECT_EQ(kdc_probe::response_kind::as_rep, c.kind);
  EXPECT_TRUE(c.alive());
  EXPECT_EQ(-1, c.error_code);
}

TEST(ClassifyResponse, KrbErrorPreauthRequiredIsAliveWithCode) {
  // KRB-ERROR { pvno [0] 5, msg-type [1] 30, error-code [6] 25 }
  const bytes krb_error = {0x7e, 0x11, 0x30, 0x0f, 0xa0, 0x03, 0x02, 0x01, 0x05, 0xa1, 0x03, 0x02, 0x01, 0x1e, 0xa6, 0x03, 0x02, 0x01, 0x19};
  const kdc_probe::classification c = kdc_probe::classify_response(krb_error);
  EXPECT_EQ(kdc_probe::response_kind::krb_error, c.kind);
  EXPECT_TRUE(c.alive());
  EXPECT_EQ(25, c.error_code);
  EXPECT_NE(std::string::npos, c.describe().find("KDC_ERR_PREAUTH_REQUIRED"));
}

TEST(ClassifyResponse, SkipsUnknownFieldsBeforeErrorCode) {
  // Realistic KRB-ERROR carries stime [4]/susec [5] before error-code [6].
  const bytes krb_error = {0x7e, 0x2c, 0x30, 0x2a, 0xa0, 0x03, 0x02, 0x01, 0x05, 0xa1, 0x03, 0x02, 0x01, 0x1e, 0xa4, 0x11,
                           0x18, 0x0f, '2',  '0',  '2',  '6',  '0',  '1',  '0',  '1',  '0',  '0',  '0',  '0',  '0',  '0',
                           'Z',  0xa5, 0x05, 0x02, 0x03, 0x01, 0x02, 0x03, 0xa6, 0x04, 0x02, 0x02, 0x00, 0x44};
  const kdc_probe::classification c = kdc_probe::classify_response(krb_error);
  EXPECT_EQ(kdc_probe::response_kind::krb_error, c.kind);
  EXPECT_EQ(68, c.error_code);
  EXPECT_NE(std::string::npos, c.describe().find("KRB_ERR_WRONG_REALM"));
}

TEST(ClassifyResponse, NegativeErrorCodeSignExtends) {
  // error-code [6] is an Int32, so a short two's-complement encoding must
  // extend rather than read as a huge positive number.
  const bytes krb_error = {0x7e, 0x09, 0x30, 0x07, 0xa6, 0x05, 0x02, 0x03, 0xff, 0xff, 0xfb};
  EXPECT_EQ(-5, kdc_probe::classify_response(krb_error).error_code);
}

TEST(ClassifyResponse, WrappingFieldLengthTerminates) {
  // A 4-octet length of 0xfffffff2 at this offset makes `pos + len` wrap on a
  // 32-bit build: the old bounds check passed and the walk offset then moved
  // *backwards*, spinning the parser forever on a response the probe took from
  // whatever host it was pointed at. The length must be rejected against the
  // bytes remaining instead. (On 64-bit there is no wrap, so this only guards
  // the rule - it is the x86 packages that were exposed.)
  const bytes krb_error = {0x7e, 0x18, 0x30, 0x16, 0xa0, 0x03, 0x02, 0x01, 0x05, 0xa1, 0x03, 0x02, 0x01,
                           0x1e, 0xa4, 0x84, 0xff, 0xff, 0xff, 0xf2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const kdc_probe::classification c = kdc_probe::classify_response(krb_error);
  EXPECT_EQ(kdc_probe::response_kind::krb_error, c.kind);
  EXPECT_EQ(-1, c.error_code);
}

TEST(ClassifyResponse, TruncatedFieldInsideSequenceTerminates) {
  const bytes krb_error = {0x7e, 0x06, 0x30, 0x04, 0xa6, 0x7f, 0x02, 0x01};
  const kdc_probe::classification c = kdc_probe::classify_response(krb_error);
  EXPECT_EQ(kdc_probe::response_kind::krb_error, c.kind);
  EXPECT_EQ(-1, c.error_code);
}

TEST(ClassifyResponse, GarbageIsInvalid) {
  EXPECT_FALSE(kdc_probe::classify_response({}).alive());
  EXPECT_FALSE(kdc_probe::classify_response({0x00}).alive());
  EXPECT_FALSE(kdc_probe::classify_response({0x42, 0x01, 0x00}).alive());
  EXPECT_EQ(kdc_probe::response_kind::invalid, kdc_probe::classify_response({0x16, 0x03, 0x01}).kind);  // e.g. a TLS server on the port
}

TEST(ClassifyResponse, TruncatedKrbErrorIsStillAliveWithoutCode) {
  // The tag byte proves a Kerberos speaker even when the body is cut short.
  const kdc_probe::classification c = kdc_probe::classify_response({0x7e, 0x82});
  EXPECT_EQ(kdc_probe::response_kind::krb_error, c.kind);
  EXPECT_TRUE(c.alive());
  EXPECT_EQ(-1, c.error_code);
}

// --- replication filter helpers ----------------------------------------------

TEST(ReplicationFilter, ExtractsServerFromNtdsDsaDn) {
  EXPECT_EQ("DC02", ad_replication_filter::extract_server_from_ntds_dn(
                        "CN=NTDS Settings,CN=DC02,CN=Servers,CN=Default-First-Site-Name,CN=Sites,CN=Configuration,DC=example,DC=com"));
  EXPECT_EQ("DC02", ad_replication_filter::extract_server_from_ntds_dn("CN=NTDS Settings, CN=DC02, CN=Servers"));
}

TEST(ReplicationFilter, FallsBackToVerbatimDnOnUnexpectedShape) {
  EXPECT_EQ("just-a-name", ad_replication_filter::extract_server_from_ntds_dn("just-a-name"));
  EXPECT_EQ("CN=NTDS Settings,OU=weird", ad_replication_filter::extract_server_from_ntds_dn("CN=NTDS Settings,OU=weird"));
}

TEST(ReplicationFilter, DerivedFieldsAndNeverDates) {
  ad_replication_filter::filter_obj obj;
  EXPECT_EQ(0, obj.get_failed());
  EXPECT_EQ("never", obj.get_last_success_su());
  EXPECT_EQ("never", obj.get_last_attempt_su());

  obj.last_error = 8524;  // ERROR_DS_DRA_GENERIC / "the DSA operation..."
  obj.last_success = 1700000000;
  EXPECT_EQ(1, obj.get_failed());
  EXPECT_NE("never", obj.get_last_success_su());
}

}  // namespace
