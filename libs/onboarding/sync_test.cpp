// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>
#include <onboarding/sync.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <boost/json.hpp>
#include <bytes/base64.hpp>
#include <memory>
#include <string>

namespace json = boost::json;

namespace {

struct evp_pkey_deleter {
  void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); }
};
struct evp_md_ctx_deleter {
  void operator()(EVP_MD_CTX *p) const { EVP_MD_CTX_free(p); }
};
struct x509_deleter {
  void operator()(X509 *p) const { X509_free(p); }
};
struct bio_deleter {
  void operator()(BIO *p) const { BIO_free(p); }
};
typedef std::unique_ptr<EVP_PKEY, evp_pkey_deleter> pkey_ptr;

pkey_ptr generate_ed25519() {
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
  EVP_PKEY *key = nullptr;
  EVP_PKEY_keygen_init(ctx);
  EVP_PKEY_keygen(ctx, &key);
  EVP_PKEY_CTX_free(ctx);
  return pkey_ptr(key);
}

std::string public_key_pem(EVP_PKEY *key) {
  const std::unique_ptr<BIO, bio_deleter> bio(BIO_new(BIO_s_mem()));
  PEM_write_bio_PUBKEY(bio.get(), key);
  char *data = nullptr;
  const long len = BIO_get_mem_data(bio.get(), &data);
  return std::string(data, static_cast<std::size_t>(len));
}

// Sign the fleet way: an Ed25519 signature over the 32-byte SHA-256 digest of
// the payload, base64 encoded.
std::string sign_bundle(EVP_PKEY *key, const std::string &payload) {
  unsigned char digest[32];
  unsigned int digest_len = 0;
  EVP_Digest(payload.data(), payload.size(), digest, &digest_len, EVP_sha256(), nullptr);
  const std::unique_ptr<EVP_MD_CTX, evp_md_ctx_deleter> ctx(EVP_MD_CTX_new());
  EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key);
  std::size_t sig_len = 0;
  EVP_DigestSign(ctx.get(), nullptr, &sig_len, digest, digest_len);
  std::string signature(sig_len, '\0');
  EVP_DigestSign(ctx.get(), reinterpret_cast<unsigned char *>(&signature[0]), &sig_len, digest, digest_len);
  signature.resize(sig_len);
  return bytes::base64_encode(signature);
}

// A minimal self-signed certificate expiring `days` from now.
std::string make_cert_expiring_in(const long days) {
  const pkey_ptr key = generate_ed25519();
  const std::unique_ptr<X509, x509_deleter> cert(X509_new());
  X509_set_version(cert.get(), 2);
  ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
  X509_gmtime_adj(X509_getm_notBefore(cert.get()), -3600);
  X509_gmtime_adj(X509_getm_notAfter(cert.get()), days * 24 * 3600);
  X509_set_pubkey(cert.get(), key.get());
  X509_NAME *name = X509_get_subject_name(cert.get());
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("test"), -1, -1, 0);
  X509_set_issuer_name(cert.get(), name);
  X509_sign(cert.get(), key.get(), nullptr);
  const std::unique_ptr<BIO, bio_deleter> bio(BIO_new(BIO_s_mem()));
  PEM_write_bio_X509(bio.get(), cert.get());
  char *data = nullptr;
  const long len = BIO_get_mem_data(bio.get(), &data);
  return std::string(data, static_cast<std::size_t>(len));
}

}  // namespace

// --- desired state ----------------------------------------------------------

TEST(SyncDesiredState, ParsesAndSortsBundlesByPriority) {
  const std::string body =
      "{\"state_hash\": \"h1\", \"next_poll_in_seconds\": 30, \"merged_config_json\": {},"
      "\"bundles\": ["
      "{\"id\": \"b2\", \"name\": \"second\", \"version\": \"2.0\", \"sha256\": \"beef\", \"signature\": \"sig2\", \"url\": \"/agent/v1/bundles/b2\", \"priority\": 200},"
      "{\"id\": \"b1\", \"name\": \"first\", \"version\": \"1.0\", \"sha256\": \"dead\", \"signature\": \"sig1\", \"url\": \"/agent/v1/bundles/b1\", \"priority\": 100}"
      "]}";
  const onboarding::desired_state state = onboarding::parse_desired_state(body);
  EXPECT_EQ(state.state_hash, "h1");
  EXPECT_EQ(state.next_poll_in_seconds, 30u);
  EXPECT_EQ(state.merged_config_json, "{}");
  ASSERT_EQ(state.bundles.size(), 2u);
  EXPECT_EQ(state.bundles[0].id, "b1");
  EXPECT_EQ(state.bundles[0].priority, 100);
  EXPECT_EQ(state.bundles[1].id, "b2");
  EXPECT_EQ(state.bundles[1].url, "/agent/v1/bundles/b2");
}

TEST(SyncDesiredState, MissingStateHashThrows) {
  EXPECT_THROW(onboarding::parse_desired_state("{\"next_poll_in_seconds\": 30}"), onboarding::onboarding_error);
}

TEST(SyncDesiredState, EmptyBundlesIsFine) {
  const onboarding::desired_state state = onboarding::parse_desired_state("{\"state_hash\": \"h\", \"bundles\": []}");
  EXPECT_TRUE(state.bundles.empty());
  EXPECT_EQ(state.next_poll_in_seconds, 60u) << "default poll interval when the field is absent";
}

TEST(SyncDesiredState, ParseNextPoll) {
  EXPECT_EQ(onboarding::parse_next_poll("{\"next_poll_in_seconds\": 42}").value_or(0), 42u);
  EXPECT_FALSE(onboarding::parse_next_poll("{}"));
  EXPECT_FALSE(onboarding::parse_next_poll("not json"));
}

// --- merge patch (RFC 7396) --------------------------------------------------

TEST(SyncMergePatch, ObjectsDeepMerge) {
  const json::value target = json::parse("{\"a\": {\"b\": 1}, \"keep\": true}");
  const json::value patch = json::parse("{\"a\": {\"c\": 2}}");
  EXPECT_EQ(json::serialize(onboarding::json_merge_patch(target, patch)), json::serialize(json::parse("{\"a\":{\"b\":1,\"c\":2},\"keep\":true}")));
}

TEST(SyncMergePatch, ScalarsAndArraysReplaceWholesale) {
  const json::value target = json::parse("{\"a\": [1, 2, 3], \"b\": \"old\"}");
  const json::value patch = json::parse("{\"a\": [9], \"b\": \"new\"}");
  EXPECT_EQ(json::serialize(onboarding::json_merge_patch(target, patch)), json::serialize(json::parse("{\"a\":[9],\"b\":\"new\"}")));
}

TEST(SyncMergePatch, NullDeletesKey) {
  const json::value target = json::parse("{\"a\": 1, \"b\": {\"c\": 2, \"d\": 3}}");
  const json::value patch = json::parse("{\"a\": null, \"b\": {\"c\": null}}");
  EXPECT_EQ(json::serialize(onboarding::json_merge_patch(target, patch)), json::serialize(json::parse("{\"b\":{\"d\":3}}")));
}

TEST(SyncMergePatch, NonObjectPatchReplacesTarget) {
  const json::value target = json::parse("{\"a\": 1}");
  EXPECT_EQ(json::serialize(onboarding::json_merge_patch(target, json::value(7))), "7");
}

TEST(SyncMergePatch, PatchOntoNonObjectStartsFresh) {
  const json::value patch = json::parse("{\"a\": 1}");
  EXPECT_EQ(json::serialize(onboarding::json_merge_patch(json::value("scalar"), patch)), json::serialize(json::parse("{\"a\":1}")));
}

// --- INI rendering -----------------------------------------------------------

TEST(SyncRenderIni, RendersSectionsFromNestedObjects) {
  const json::value config = json::parse(
      "{\"modules\": {\"CheckSystem\": \"enabled\"},"
      " \"settings\": {\"NRPE\": {\"server\": {\"port\": 5666, \"ssl\": true}}}}");
  const std::string ini = onboarding::render_ini(config);
  EXPECT_NE(ini.find("[/modules]\nCheckSystem=enabled\n"), std::string::npos) << ini;
  EXPECT_NE(ini.find("[/settings/NRPE/server]\nport=5666\nssl=true\n"), std::string::npos) << ini;
  // Sections are emitted in sorted order: /modules before /settings/...
  EXPECT_LT(ini.find("[/modules]"), ini.find("[/settings/NRPE/server]"));
}

TEST(SyncRenderIni, ArraysBecomeCommaSeparatedLists) {
  const json::value config = json::parse("{\"settings\": {\"log\": {\"levels\": [\"error\", \"warning\", 5]}}}");
  const std::string ini = onboarding::render_ini(config);
  EXPECT_NE(ini.find("levels=error,warning,5"), std::string::npos) << ini;
}

TEST(SyncRenderIni, DeterministicRegardlessOfInsertionOrder) {
  const json::value one = json::parse("{\"b\": {\"y\": 1, \"x\": 2}, \"a\": {\"k\": 3}}");
  const json::value two = json::parse("{\"a\": {\"k\": 3}, \"b\": {\"x\": 2, \"y\": 1}}");
  EXPECT_EQ(onboarding::render_ini(one), onboarding::render_ini(two));
}

TEST(SyncRenderIni, NonObjectThrows) { EXPECT_THROW(onboarding::render_ini(json::value(1)), onboarding::onboarding_error); }

// --- bundle verification -----------------------------------------------------

TEST(SyncVerify, Sha256KnownVector) {
  EXPECT_EQ(onboarding::sha256_hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(SyncVerify, AcceptsAValidSignature) {
  const pkey_ptr key = generate_ed25519();
  const std::string payload = "bundle-bytes-here";
  std::string error;
  EXPECT_TRUE(onboarding::verify_bundle(public_key_pem(key.get()), payload, onboarding::sha256_hex(payload), sign_bundle(key.get(), payload), error))
      << error;
}

TEST(SyncVerify, RejectsChecksumMismatch) {
  const pkey_ptr key = generate_ed25519();
  const std::string payload = "bundle-bytes-here";
  std::string error;
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), payload + "tampered", onboarding::sha256_hex(payload),
                                         sign_bundle(key.get(), payload), error));
  EXPECT_NE(error.find("checksum"), std::string::npos);
}

TEST(SyncVerify, RejectsSignatureFromAnotherKey) {
  const pkey_ptr key = generate_ed25519();
  const pkey_ptr other = generate_ed25519();
  const std::string payload = "bundle-bytes-here";
  std::string error;
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), payload, onboarding::sha256_hex(payload), sign_bundle(other.get(), payload), error));
  EXPECT_NE(error.find("signature"), std::string::npos);
}

TEST(SyncVerify, RejectsGarbageSignature) {
  const pkey_ptr key = generate_ed25519();
  const std::string payload = "bundle-bytes-here";
  std::string error;
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), payload, onboarding::sha256_hex(payload), "!!!not-base64!!!", error));
}

// --- transport error classification ------------------------------------------

TEST(SyncTransportErrors, CertificateRequiredMeansIdentityRejected) {
  // The exact wrapped form the HTTP client produces: the connection prefix
  // must not shadow the TLS alert into the "network" bucket.
  const onboarding::transport_error_info info =
      onboarding::classify_transport_error("Failed to connect to localhost:8443: tlsv13 alert certificate required (SSL routines)");
  EXPECT_EQ(info.kind, onboarding::transport_error_kind::tls_identity);
  EXPECT_NE(info.advice.find("re-enroll"), std::string::npos);
}

TEST(SyncTransportErrors, BadCertificateAndUnknownCaAreIdentity) {
  EXPECT_EQ(onboarding::classify_transport_error("sslv3 alert bad certificate").kind, onboarding::transport_error_kind::tls_identity);
  EXPECT_EQ(onboarding::classify_transport_error("tlsv1 alert unknown ca (SSL routines)").kind, onboarding::transport_error_kind::tls_identity);
}

TEST(SyncTransportErrors, VerifyFailureIsServerTrust) {
  const onboarding::transport_error_info info = onboarding::classify_transport_error("certificate verify failed (SSL routines)");
  EXPECT_EQ(info.kind, onboarding::transport_error_kind::tls_server_trust);
  EXPECT_NE(info.advice.find("pinned"), std::string::npos);
}

TEST(SyncTransportErrors, ConnectionProblemsAreNetwork) {
  EXPECT_EQ(onboarding::classify_transport_error("Failed to connect to fleet.example.com:8443: Connection refused").kind,
            onboarding::transport_error_kind::network);
  EXPECT_EQ(onboarding::classify_transport_error("Failed to resolve fleet.example.com:8443: Host not found").kind,
            onboarding::transport_error_kind::network);
  EXPECT_EQ(onboarding::classify_transport_error("No connection could be made because the target machine actively refused it").kind,
            onboarding::transport_error_kind::network);
}

TEST(SyncTransportErrors, GenericTlsFailure) {
  EXPECT_EQ(onboarding::classify_transport_error("alert handshake failure (SSL routines)").kind, onboarding::transport_error_kind::tls_other);
}

TEST(SyncTransportErrors, UnknownHasNoAdvice) {
  const onboarding::transport_error_info info = onboarding::classify_transport_error("something completely different");
  EXPECT_EQ(info.kind, onboarding::transport_error_kind::unknown);
  EXPECT_TRUE(info.advice.empty());
}

// --- certificate lifecycle ---------------------------------------------------

TEST(SyncCert, DaysUntilExpiry) {
  const long days = onboarding::days_until_expiry(make_cert_expiring_in(30));
  EXPECT_GE(days, 29);
  EXPECT_LE(days, 30);
}

TEST(SyncCert, ExpiredCertIsNegative) { EXPECT_LT(onboarding::days_until_expiry(make_cert_expiring_in(-10)), 0); }

TEST(SyncCert, GarbagePemThrows) { EXPECT_THROW(onboarding::days_until_expiry("not a pem"), onboarding::onboarding_error); }

TEST(SyncRenew, KeepsUrlsAndSwapsMaterial) {
  onboarding::enrolled_identity current;
  current.private_key_pem = "OLD-KEY";
  current.cert_pem = "OLD-CERT";
  current.server_url = "https://api.example.com";
  current.mtls_url = "https://mtls.example.com";
  onboarding::identity fresh;
  fresh.private_key_pem = "NEW-KEY";
  const std::string body =
      "{\"cert_pem\": \"NEW-CERT\", \"ca_pem\": \"NEW-CA\", \"bundle_signing_pub_pem\": \"NEW-BUNDLE-KEY\","
      " \"mtls_server_cert_pem\": \"NEW-MTLS-CERT\"}";
  const onboarding::enrolled_identity renewed = onboarding::parse_renew_response(body, fresh, current);
  EXPECT_EQ(renewed.private_key_pem, "NEW-KEY");
  EXPECT_EQ(renewed.cert_pem, "NEW-CERT");
  EXPECT_EQ(renewed.ca_pem, "NEW-CA");
  EXPECT_EQ(renewed.bundle_signing_pub_pem, "NEW-BUNDLE-KEY");
  EXPECT_EQ(renewed.mtls_server_cert_pem, "NEW-MTLS-CERT");
  EXPECT_EQ(renewed.server_url, "https://api.example.com");
  EXPECT_EQ(renewed.mtls_url, "https://mtls.example.com");
}

TEST(SyncRenew, MissingFieldThrows) {
  EXPECT_THROW(onboarding::parse_renew_response("{\"cert_pem\": \"C\"}", onboarding::identity(), onboarding::enrolled_identity()),
               onboarding::onboarding_error);
}

// --- report / metrics payloads ----------------------------------------------

TEST(SyncReport, BuildStateReport) {
  std::vector<onboarding::installed_bundle> bundles;
  bundles.push_back({"b1", "1.0"});
  std::map<std::string, std::string> tags;
  tags["os"] = "windows";
  const json::object root = json::parse(onboarding::build_state_report(std::string("h1"), bundles, {"oops"}, tags)).as_object();
  EXPECT_EQ(root.at("applied_state_hash").as_string(), "h1");
  EXPECT_EQ(root.at("bundles_installed").as_array().at(0).as_object().at("id").as_string(), "b1");
  EXPECT_EQ(root.at("errors").as_array().at(0).as_string(), "oops");
  EXPECT_EQ(root.at("reported_tags").as_object().at("os").as_string(), "windows");
}

TEST(SyncReport, OmitsHashAfterFailedApply) {
  const json::object root = json::parse(onboarding::build_state_report(boost::none, {}, {}, {})).as_object();
  EXPECT_EQ(root.if_contains("applied_state_hash"), nullptr);
}

TEST(SyncReport, BuildMetrics) {
  std::vector<onboarding::metric_sample> samples;
  onboarding::metric_sample with_ts;
  with_ts.key = "uptime_seconds";
  with_ts.value = 86400;
  with_ts.ts = 1753789200;
  samples.push_back(with_ts);
  onboarding::metric_sample without_ts;
  without_ts.key = "cpu.load";
  without_ts.value = 0.42;
  samples.push_back(without_ts);
  const json::object root = json::parse(onboarding::build_metrics(samples)).as_object();
  const json::array &list = root.at("samples").as_array();
  ASSERT_EQ(list.size(), 2u);
  EXPECT_EQ(list.at(0).as_object().at("ts").as_int64(), 1753789200);
  EXPECT_EQ(list.at(1).as_object().if_contains("ts"), nullptr);
}
