// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <algorithm>
#include <boost/json.hpp>
#include <bytes/base64.hpp>
#include <cctype>
#include <map>
#include <memory>
#include <onboarding/bundle_crypto.hpp>
#include <onboarding/sync.hpp>
#include <string>
#include <vector>

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

// A key of the wrong algorithm, for the "not an Ed25519 key" cases. Built the
// portable way so it also works against OpenSSL 1.1.1.
pkey_ptr generate_rsa() {
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  EVP_PKEY *key = nullptr;
  EVP_PKEY_keygen_init(ctx);
  EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048);
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

// A minimal self-signed certificate expiring `seconds` from now (negative for
// one that has already expired).
std::string make_cert_expiring_in_seconds(const long seconds) {
  const pkey_ptr key = generate_ed25519();
  const std::unique_ptr<X509, x509_deleter> cert(X509_new());
  X509_set_version(cert.get(), 2);
  ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
  X509_gmtime_adj(X509_getm_notBefore(cert.get()), seconds < 0 ? seconds - 3600 : -3600);
  X509_gmtime_adj(X509_getm_notAfter(cert.get()), seconds);
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

// A minimal self-signed certificate expiring `days` from now.
std::string make_cert_expiring_in(const long days) { return make_cert_expiring_in_seconds(days * 24 * 3600); }

// A 64 character hex digest made of one repeated character - shape-valid
// without pretending to be a real digest.
std::string hex64(const char c) { return std::string(64, c); }

// The shape of a well-formed bundle entry; tests override one member at a time
// to send exactly one hostile field.
json::object valid_bundle() {
  json::object bundle;
  bundle["id"] = "b-1";
  bundle["name"] = "demo";
  bundle["version"] = "1.0";
  bundle["sha256"] = hex64('a');
  bundle["signature"] = "c2lnbmF0dXJl";
  bundle["url"] = "/agent/v1/bundles/b-1";
  bundle["priority"] = 100;
  return bundle;
}

json::object valid_state() {
  json::object root;
  root["state_hash"] = "h1";
  root["next_poll_in_seconds"] = 30;
  root["merged_config_json"] = json::object();
  root["bundles"] = json::array();
  return root;
}

/** A JSON string value; boost::json::value has no implicit std::string ctor
 * (and the strings here contain embedded nuls, so length matters). */
json::value jstr(const std::string &value) { return json::value(json::string_view(value.data(), value.size())); }

/** A desired-state body whose single bundle has `key` replaced by `value`. */
std::string bundle_with(const char *key, const json::value &value) {
  json::object bundle = valid_bundle();
  bundle[key] = value;
  json::object root = valid_state();
  root["bundles"] = json::array{bundle};
  return json::serialize(root);
}
std::string bundle_with(const char *key, const std::string &value) { return bundle_with(key, jstr(value)); }
std::string bundle_with(const char *key, const char *value) { return bundle_with(key, jstr(value)); }

/** A desired-state body with a top level `key` replaced by `value`. */
std::string state_with(const char *key, const json::value &value) {
  json::object root = valid_state();
  root[key] = value;
  return json::serialize(root);
}
std::string state_with(const char *key, const std::string &value) { return state_with(key, jstr(value)); }
std::string state_with(const char *key, const char *value) { return state_with(key, jstr(value)); }

/** The body must be rejected as fatal, and the error must not echo the hostile
 * value back (the message is logged and reported to the server). */
void expect_rejected(const std::string &body, const std::string &must_not_leak = std::string()) {
  try {
    onboarding::parse_desired_state(body);
    FAIL() << "expected onboarding_error for " << body;
  } catch (const onboarding::onboarding_error &e) {
    EXPECT_FALSE(e.retryable()) << e.what();
    if (!must_not_leak.empty()) {
      EXPECT_EQ(std::string(e.what()).find(must_not_leak), std::string::npos) << "hostile value leaked into: " << e.what();
    }
  }
}

}  // namespace

// --- desired state ----------------------------------------------------------

TEST(SyncDesiredState, ParsesAndSortsBundlesByPriority) {
  const std::string body =
      "{\"state_hash\": \"h1\", \"next_poll_in_seconds\": 30, \"merged_config_json\": {},"
      "\"bundles\": ["
      "{\"id\": \"b2\", \"name\": \"second\", \"version\": \"2.0\", \"sha256\": \"" +
      hex64('b') +
      "\", \"signature\": \"sig2\", \"url\": \"/agent/v1/bundles/b2\", \"priority\": 200},"
      "{\"id\": \"b1\", \"name\": \"first\", \"version\": \"1.0\", \"sha256\": \"" +
      hex64('d') +
      "\", \"signature\": \"sig1\", \"url\": \"/agent/v1/bundles/b1\", \"priority\": 100}"
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

// --- desired state: hostile / malformed payloads -----------------------------
//
// The desired state is only as trustworthy as the channel it arrives on, and
// its fields are used in URLs, as file names and as OpenSSL inputs. Everything
// below is a value a compromised (or simply buggy) server could send.

TEST(SyncDesiredStateHostile, StateHashCannotSmuggleARequestLine) {
  // The hash goes back in "?current_hash=<hash>": a CR/LF would let the server
  // append whatever it likes to our request.
  expect_rejected(state_with("state_hash", "h1\r\nX-Injected: 1"), "X-Injected");
  expect_rejected(state_with("state_hash", "h1\nh2"));
  expect_rejected(state_with("state_hash", "h1 h2"));
  expect_rejected(state_with("state_hash", "h1&admin=1"), "admin");
  expect_rejected(state_with("state_hash", "h1#frag"));
  expect_rejected(state_with("state_hash", "h1%0aX"));
}

TEST(SyncDesiredStateHostile, StateHashIsLengthBounded) {
  EXPECT_NO_THROW(onboarding::parse_desired_state(state_with("state_hash", std::string(128, 'a'))));
  expect_rejected(state_with("state_hash", std::string(129, 'a')));
}

TEST(SyncDesiredStateHostile, StateHashAcceptsTheFormsAServerActuallyUses) {
  // hex, "sha256:<hex>", base64 and base64url all have to keep working.
  for (const std::string hash : {hex64('f'), "sha256:" + hex64('0'), std::string("Zm9vYmFy+/8="), std::string("Zm9vYmFy-_8")}) {
    const onboarding::desired_state state = onboarding::parse_desired_state(state_with("state_hash", hash));
    EXPECT_EQ(state.state_hash, hash);
  }
}

TEST(SyncDesiredStateHostile, StateHashMustBePresentAndAString) {
  expect_rejected("{\"state_hash\": \"\"}");
  expect_rejected("{\"state_hash\": 42}");
  expect_rejected("{\"state_hash\": null}");
  expect_rejected("{\"state_hash\": [\"h\"]}");
}

TEST(SyncDesiredStateHostile, BundleIdCannotEscapeTheCacheDirectory) {
  // The id becomes "<cache>/<id>-<sha16>.zip"; a separator or ".." in it would
  // write verified-but-attacker-chosen bytes anywhere on disk.
  expect_rejected(bundle_with("id", "../../../../etc/cron.d/x"), "cron.d");
  expect_rejected(bundle_with("id", "..\\..\\windows\\system32\\x"));
  expect_rejected(bundle_with("id", "sub/dir"));
  expect_rejected(bundle_with("id", "C:evil"));
  expect_rejected(bundle_with("id", "a..b"));
  expect_rejected(bundle_with("id", ".."));
  expect_rejected(bundle_with("id", "."));
  expect_rejected(bundle_with("id", "id with space"));
  expect_rejected(bundle_with("id", "id\nnewline"));
  expect_rejected(bundle_with("id", std::string("id\0nul", 6)));
  expect_rejected(bundle_with("id", std::string(129, 'b')));
  expect_rejected(bundle_with("id", ""));
}

TEST(SyncDesiredStateHostile, BundleIdAcceptsTheFormsAServerActuallyUses) {
  for (const std::string &id : std::vector<std::string>{"b-good", "1f3c9e4a-7b21-4d5e-9c8a-0b1d2e3f4a5b", "demo_bundle.v2", std::string(128, 'x')}) {
    const onboarding::desired_state state = onboarding::parse_desired_state(bundle_with("id", id));
    ASSERT_EQ(state.bundles.size(), 1u);
    EXPECT_EQ(state.bundles[0].id, id);
  }
}

TEST(SyncDesiredStateHostile, Sha256MustBeExactlySixtyFourHexCharacters) {
  // A short digest used to reach substr(0, 16) and throw std::out_of_range
  // from deep inside the apply path instead of failing the parse cleanly.
  expect_rejected(bundle_with("sha256", "beef"));
  expect_rejected(bundle_with("sha256", hex64('a') + "a"));
  expect_rejected(bundle_with("sha256", std::string(63, 'a')));
  expect_rejected(bundle_with("sha256", std::string(64, 'z')));
  expect_rejected(bundle_with("sha256", std::string(63, 'a') + " "));
  expect_rejected(bundle_with("sha256", ""));
  expect_rejected(bundle_with("sha256", 42));
}

TEST(SyncDesiredStateHostile, Sha256AcceptsEitherCase) {
  const std::string upper = "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD";
  const onboarding::desired_state state = onboarding::parse_desired_state(bundle_with("sha256", upper));
  ASSERT_EQ(state.bundles.size(), 1u);
  EXPECT_EQ(state.bundles[0].sha256, upper) << "case is preserved; verify_bundle compares case-insensitively";
}

TEST(SyncDesiredStateHostile, SignatureMustLookLikeBase64AndBeBounded) {
  expect_rejected(bundle_with("signature", "sig with space"));
  expect_rejected(bundle_with("signature", "sig\nsig"));
  expect_rejected(bundle_with("signature", std::string(513, 'A')));
  expect_rejected(bundle_with("signature", ""));
  EXPECT_NO_THROW(onboarding::parse_desired_state(bundle_with("signature", "YWJjZGVm+/==")));
}

TEST(SyncDesiredStateHostile, BundleUrlMustBeAServerRelativePath) {
  // The url is appended to the pinned mtls base url, so an absolute url (or a
  // CRLF) must not get through.
  expect_rejected(bundle_with("url", "https://evil.example.com/payload.zip"), "evil.example.com");
  expect_rejected(bundle_with("url", "agent/v1/bundles/b-1"));
  expect_rejected(bundle_with("url", "/bundles/b\r\nHost: evil"), "evil");
  expect_rejected(bundle_with("url", "/bundles/b 1"));
  expect_rejected(bundle_with("url", "/bundles/b#frag"));
  expect_rejected(bundle_with("url", "/bundles/\\b"));
  expect_rejected(bundle_with("url", "/bundles/b\x7f"));
  expect_rejected(bundle_with("url", "/" + std::string(1024, 'u')));
  expect_rejected(bundle_with("url", ""));
  EXPECT_NO_THROW(onboarding::parse_desired_state(bundle_with("url", "/agent/v1/bundles/b-1?token=abc%20def")));
}

TEST(SyncDesiredStateHostile, MalformedBundleListFailsInsteadOfLookingEmpty) {
  // "no bundles" is a legitimate desired state that wipes the managed config,
  // so a bundles field we cannot understand must never degrade into one.
  expect_rejected(state_with("bundles", json::object()));
  expect_rejected(state_with("bundles", "all of them"));
  expect_rejected(state_with("bundles", 3));
  expect_rejected(state_with("bundles", json::array{json::value("b1")}));
  expect_rejected(state_with("bundles", json::array{json::value(json::object()), json::value(1)}));
}

TEST(SyncDesiredStateHostile, AbsentOrNullBundlesIsAnEmptySet) {
  EXPECT_TRUE(onboarding::parse_desired_state("{\"state_hash\": \"h\"}").bundles.empty());
  EXPECT_TRUE(onboarding::parse_desired_state(state_with("bundles", json::value())).bundles.empty());
}

TEST(SyncDesiredStateHostile, MergedConfigMustBeAnObject) {
  expect_rejected(state_with("merged_config_json", "{\"modules\": {}}"));
  expect_rejected(state_with("merged_config_json", json::array()));
  expect_rejected(state_with("merged_config_json", 7));
  EXPECT_EQ(onboarding::parse_desired_state(state_with("merged_config_json", json::value())).merged_config_json, "{}");
  EXPECT_EQ(onboarding::parse_desired_state("{\"state_hash\": \"h\"}").merged_config_json, "{}");
}

TEST(SyncDesiredStateHostile, PollIntervalIsClampedToSomethingSurvivable) {
  // A negative or absurd interval used to become a near-infinite sleep once
  // cast to unsigned - a single response could park the agent for good.
  EXPECT_EQ(onboarding::parse_desired_state(state_with("next_poll_in_seconds", json::value(-1))).next_poll_in_seconds, 60u);
  EXPECT_EQ(onboarding::parse_desired_state(state_with("next_poll_in_seconds", json::value(0))).next_poll_in_seconds, 60u);
  EXPECT_EQ(onboarding::parse_desired_state(state_with("next_poll_in_seconds", json::value(-9223372036854775807LL))).next_poll_in_seconds, 60u);
  EXPECT_EQ(onboarding::parse_desired_state(state_with("next_poll_in_seconds", json::value(9007199254740992LL))).next_poll_in_seconds, 86400u);
  EXPECT_EQ(onboarding::parse_desired_state(state_with("next_poll_in_seconds", json::value(86401))).next_poll_in_seconds, 86400u);
  EXPECT_EQ(onboarding::parse_desired_state(state_with("next_poll_in_seconds", json::value(86400))).next_poll_in_seconds, 86400u);
  EXPECT_EQ(onboarding::parse_desired_state(state_with("next_poll_in_seconds", json::value(1))).next_poll_in_seconds, 1u);
  EXPECT_EQ(onboarding::parse_desired_state(state_with("next_poll_in_seconds", "30")).next_poll_in_seconds, 60u) << "a non-number is not a hint";
}

TEST(SyncDesiredStateHostile, ParseNextPollIsClampedToo) {
  EXPECT_FALSE(onboarding::parse_next_poll("{\"next_poll_in_seconds\": -5}"));
  EXPECT_FALSE(onboarding::parse_next_poll("{\"next_poll_in_seconds\": 0}"));
  EXPECT_FALSE(onboarding::parse_next_poll("{\"next_poll_in_seconds\": \"30\"}"));
  EXPECT_EQ(onboarding::parse_next_poll("{\"next_poll_in_seconds\": 999999999}").value_or(0), 86400u);
  EXPECT_FALSE(onboarding::parse_next_poll("[]")) << "a non-object body is not a hint";
  EXPECT_FALSE(onboarding::parse_next_poll(""));
}

TEST(SyncDesiredStateHostile, GarbageBodiesAreFatalNotRetryable) {
  expect_rejected("");
  expect_rejected("not json at all");
  expect_rejected("[]");
  expect_rejected("null");
  expect_rejected("\"h1\"");
  expect_rejected("{\"state_hash\": \"h1\"");  // truncated
}

TEST(SyncDesiredStateHostile, PriorityIsBestEffortAndOrdersTheApply) {
  // A missing or non-numeric priority is not fatal (it only decides order),
  // but it must not disturb the ordering of the bundles that do have one.
  json::object low = valid_bundle();
  low["id"] = "low";
  low["priority"] = -50;
  json::object none = valid_bundle();
  none["id"] = "none";
  none.erase("priority");
  json::object text = valid_bundle();
  text["id"] = "text";
  text["priority"] = "high";
  json::object high = valid_bundle();
  high["id"] = "high";
  high["priority"] = 500;

  json::object root = valid_state();
  root["bundles"] = json::array{high, text, none, low};
  const onboarding::desired_state state = onboarding::parse_desired_state(json::serialize(root));
  ASSERT_EQ(state.bundles.size(), 4u);
  EXPECT_EQ(state.bundles[0].id, "low");
  // Ties (0) keep the server's order: "text" came before "none".
  EXPECT_EQ(state.bundles[1].id, "text");
  EXPECT_EQ(state.bundles[2].id, "none");
  EXPECT_EQ(state.bundles[3].id, "high");
}

TEST(SyncDesiredStateHostile, OptionalNameAndVersionAreNotTrustedForAnything) {
  // name/version are only ever reported back, so they stay permissive - but
  // they must not be able to break the report either (see SyncReport).
  const onboarding::desired_state state = onboarding::parse_desired_state(bundle_with("name", "../../etc\r\n\"quoted\""));
  ASSERT_EQ(state.bundles.size(), 1u);
  EXPECT_EQ(state.bundles[0].name, "../../etc\r\n\"quoted\"");
  EXPECT_EQ(onboarding::parse_desired_state(bundle_with("version", 5)).bundles[0].version, "") << "a non-string falls back to empty";
}

// --- Retry-After -------------------------------------------------------------
//
// A single bad header used to translate into a wait of 5 minutes (enrollment)
// or a whole day (sync): strtoul happily accepts "-5" as 18446744073709551611
// and reads "1e9" as 1.

TEST(SyncRetryAfter, AcceptsPlainDelaySeconds) {
  EXPECT_EQ(onboarding::parse_retry_after("0").value_or(999u), 0u);
  EXPECT_EQ(onboarding::parse_retry_after("1").value_or(0), 1u);
  EXPECT_EQ(onboarding::parse_retry_after("120").value_or(0), 120u);
  EXPECT_EQ(onboarding::parse_retry_after("  30  ").value_or(0), 30u) << "header whitespace is not part of the value";
  EXPECT_EQ(onboarding::parse_retry_after("\t7").value_or(0), 7u);
  EXPECT_EQ(onboarding::parse_retry_after("999999999").value_or(0), 999999999u);
}

TEST(SyncRetryAfter, RejectsEverythingItCannotActOn) {
  for (const char *header : {"",
                             " ",
                             "-5",
                             "-1",
                             "+5",
                             "1e9",
                             "1E9",
                             "5s",
                             "5 seconds",
                             "0x10",
                             "1.5",
                             "1,5",
                             " 5 5",
                             "Wed, 21 Oct 2026 07:28:00 GMT",
                             "now",
                             "١٢٣",                      // non-ASCII digits
                             "1000000000",               // ten digits: beyond any sane wait
                             "99999999999999999999"}) {  // would overflow
    EXPECT_FALSE(onboarding::parse_retry_after(header)) << "accepted \"" << header << "\"";
  }
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

// --- INI rendering: injection ------------------------------------------------
//
// The rendered file is included by the core, so a value that can start a new
// line can enable modules, define external commands or rewrite any setting -
// regardless of what the server meant to send.

TEST(SyncRenderIniInjection, NewlineInAValueCannotOpenANewSection) {
  json::object settings;
  settings["greeting"] = "hello\n[/settings/external scripts/scripts]\nevil=/bin/sh -c id";
  json::object root;
  root["settings"] = settings;
  EXPECT_THROW(onboarding::render_ini(root), onboarding::onboarding_error);
}

TEST(SyncRenderIniInjection, EveryFlavourOfLineBreakIsRejected) {
  for (const std::string &hostile :
       std::vector<std::string>{"a\nb", "a\rb", "a\r\nb", std::string("a\0b", 3), "a\x0b" "b", "a\x0c" "b", "a\x1b" "b", "a\x7f" "b"}) {
    json::object root;
    root["key"] = jstr(hostile);
    EXPECT_THROW(onboarding::render_ini(root), onboarding::onboarding_error) << "accepted a control character in a value";
  }
}

TEST(SyncRenderIniInjection, KeysAndSectionNamesAreCheckedToo) {
  {
    json::object root;
    root["key\nevil"] = "1";
    EXPECT_THROW(onboarding::render_ini(root), onboarding::onboarding_error);
  }
  {
    json::object inner;
    inner["ok"] = "1";
    json::object root;
    root["section\n[/modules]\nEvilModule"] = inner;
    EXPECT_THROW(onboarding::render_ini(root), onboarding::onboarding_error);
  }
  {
    json::object inner;
    inner["key\rx"] = "1";
    json::object root;
    root["settings"] = inner;
    EXPECT_THROW(onboarding::render_ini(root), onboarding::onboarding_error);
  }
}

TEST(SyncRenderIniInjection, ArrayElementsAreCheckedToo) {
  json::object root;
  root["levels"] = json::array{json::value("error"), json::value("warning\nEvilModule=enabled")};
  EXPECT_THROW(onboarding::render_ini(root), onboarding::onboarding_error);
}

TEST(SyncRenderIniInjection, HarmlessPunctuationSurvives) {
  // Only line breaks are dangerous: brackets, semicolons, equals signs, quotes
  // and non-ASCII text all have to render as-is or configuration breaks.
  json::object inner;
  inner["syntax"] = "${status}: ${list} [ok]; a=b \"q\" 'p' \\ /path/to é日本";
  inner["tabbed"] = "a\tb";
  json::object root;
  root["settings"] = inner;
  const std::string ini = onboarding::render_ini(root);
  EXPECT_NE(ini.find("syntax=${status}: ${list} [ok]; a=b \"q\" 'p' \\ /path/to é日本"), std::string::npos) << ini;
  EXPECT_NE(ini.find("tabbed=a\tb"), std::string::npos) << ini;
}

TEST(SyncRenderIni, ScalarTypesRenderWithoutQuotesOrLoss) {
  json::object inner;
  inner["int"] = 5666;
  inner["negative"] = -1;
  inner["big"] = 9007199254740993LL;
  inner["double"] = 0.5;
  inner["round"] = 100.0;
  inner["yes"] = true;
  inner["no"] = false;
  inner["empty"] = "";
  json::object root;
  root["settings"] = inner;
  const std::string ini = onboarding::render_ini(root);
  EXPECT_NE(ini.find("int=5666"), std::string::npos) << ini;
  EXPECT_NE(ini.find("negative=-1"), std::string::npos) << ini;
  EXPECT_NE(ini.find("big=9007199254740993"), std::string::npos) << ini;
  EXPECT_NE(ini.find("yes=true"), std::string::npos) << ini;
  EXPECT_NE(ini.find("no=false"), std::string::npos) << ini;
  EXPECT_NE(ini.find("empty=\n"), std::string::npos) << ini;
  // Doubles must be plain decimal: boost.json's own output would be "5E-1"
  // and "1E2", which nothing that reads this file understands.
  EXPECT_NE(ini.find("double=0.5\n"), std::string::npos) << ini;
  EXPECT_NE(ini.find("round=100\n"), std::string::npos) << ini;
  EXPECT_EQ(ini.find("E-"), std::string::npos) << "scientific notation leaked into the INI: " << ini;
  EXPECT_EQ(ini.find("1E2"), std::string::npos) << "scientific notation leaked into the INI: " << ini;
}

TEST(SyncRenderIni, EmptyAndNullOnlyConfigsRenderJustTheHeader) {
  const std::string header = onboarding::render_ini(json::object());
  EXPECT_NE(header.find("DO NOT EDIT"), std::string::npos);
  EXPECT_EQ(header.find('['), std::string::npos) << header;

  // Merge-patch deletion markers must not leak in as the string "null".
  json::object inner;
  inner["gone"] = json::value();
  json::object root;
  root["settings"] = inner;
  const std::string ini = onboarding::render_ini(root);
  EXPECT_EQ(ini.find("null"), std::string::npos) << ini;
  EXPECT_EQ(ini.find("gone"), std::string::npos) << ini;
}

TEST(SyncRenderIni, DeeplyNestedObjectsBecomeSlashPaths) {
  const json::value config = json::parse("{\"a\": {\"b\": {\"c\": {\"d\": {\"key\": 1}}}}}");
  EXPECT_NE(onboarding::render_ini(config).find("[/a/b/c/d]\nkey=1\n"), std::string::npos);
}

// --- bundle verification -----------------------------------------------------

TEST(SyncVerify, Sha256KnownVector) {
  EXPECT_EQ(onboarding::sha256_hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(SyncVerify, Sha256StreamMatchesOneShot) {
  onboarding::sha256_stream stream;
  stream.update("abc");
  EXPECT_EQ(stream.hex_final(), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(SyncVerify, Sha256StreamChunkingDoesNotChangeTheDigest) {
  // The content-hash caller feeds header and file content as separate
  // update()s: any chunking of the same bytes must produce the same digest.
  std::string payload = "ini:[/settings]\nkey=value";
  payload += std::string(100000, 'x');
  onboarding::sha256_stream stream;
  stream.update(payload.substr(0, 1));
  stream.update("");
  stream.update(payload.substr(1, 42));
  stream.update(payload.substr(43));
  EXPECT_EQ(stream.hex_final(), onboarding::sha256_hex(payload));
}

TEST(SyncVerify, Sha256StreamOfNothingIsTheEmptyDigest) {
  onboarding::sha256_stream stream;
  EXPECT_EQ(stream.hex_final(), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
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

// --- bundle verification: everything that must NOT verify ---------------------
//
// verify_bundle is the only thing standing between a downloaded zip and the
// configuration this host runs, so each way of getting it wrong gets a case.

TEST(SyncVerifyHostile, Sha256KnownVectors) {
  EXPECT_EQ(onboarding::sha256_hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(onboarding::sha256_hex(std::string("\0\0\0", 3)), "709e80c88487a2411e1ee4dfb9f22a861492d20c4765150c0c794abd70f8147c");
  EXPECT_EQ(onboarding::sha256_hex(std::string(1000, 'a')).size(), 64u);
}

TEST(SyncVerifyHostile, RejectsASingleFlippedByte) {
  const pkey_ptr key = generate_ed25519();
  std::string payload(256, 'x');
  const std::string digest = onboarding::sha256_hex(payload);
  const std::string signature = sign_bundle(key.get(), payload);
  payload[128] = 'y';
  std::string error;
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), payload, digest, signature, error));
  EXPECT_NE(error.find("checksum"), std::string::npos) << error;
}

TEST(SyncVerifyHostile, RejectsATruncatedBundle) {
  const pkey_ptr key = generate_ed25519();
  const std::string payload(256, 'x');
  std::string error;
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), payload.substr(0, 255), onboarding::sha256_hex(payload),
                                         sign_bundle(key.get(), payload), error));
}

TEST(SyncVerifyHostile, RejectsAnEmptyBundleAgainstANonEmptyDigest) {
  const pkey_ptr key = generate_ed25519();
  std::string error;
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), "", onboarding::sha256_hex("something"), sign_bundle(key.get(), "something"), error));
}

TEST(SyncVerifyHostile, AcceptsAnEmptyBundleSignedAsSuch) {
  // Degenerate but legal: an empty bundle has a digest and can be signed.
  const pkey_ptr key = generate_ed25519();
  std::string error;
  EXPECT_TRUE(onboarding::verify_bundle(public_key_pem(key.get()), "", onboarding::sha256_hex(""), sign_bundle(key.get(), ""), error)) << error;
}

TEST(SyncVerifyHostile, ExpectedDigestIsCaseInsensitiveButNothingElse) {
  const pkey_ptr key = generate_ed25519();
  const std::string payload = "bundle";
  const std::string signature = sign_bundle(key.get(), payload);
  std::string upper = onboarding::sha256_hex(payload);
  std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  std::string error;
  EXPECT_TRUE(onboarding::verify_bundle(public_key_pem(key.get()), payload, upper, signature, error)) << error;
  // Padding, whitespace or a prefix is not "the same digest".
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), payload, " " + upper, signature, error));
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), payload, upper + "\n", signature, error));
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), payload, upper.substr(0, 32), signature, error));
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), payload, "", signature, error));
}

TEST(SyncVerifyHostile, RejectsASignatureOverTheRawBytesInsteadOfTheDigest) {
  // Pins the protocol: the signature covers the 32-byte SHA-256 digest. An
  // implementation that signed the bundle bytes directly must not be accepted
  // (nor should ours drift into accepting it).
  const pkey_ptr key = generate_ed25519();
  const std::string payload = "bundle-bytes-here";
  const std::unique_ptr<EVP_MD_CTX, evp_md_ctx_deleter> ctx(EVP_MD_CTX_new());
  EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key.get());
  std::size_t sig_len = 0;
  EVP_DigestSign(ctx.get(), nullptr, &sig_len, reinterpret_cast<const unsigned char *>(payload.data()), payload.size());
  std::string signature(sig_len, '\0');
  EVP_DigestSign(ctx.get(), reinterpret_cast<unsigned char *>(&signature[0]), &sig_len, reinterpret_cast<const unsigned char *>(payload.data()),
                 payload.size());
  signature.resize(sig_len);

  std::string error;
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()), payload, onboarding::sha256_hex(payload), bytes::base64_encode(signature), error));
  EXPECT_NE(error.find("signature"), std::string::npos) << error;
}

TEST(SyncVerifyHostile, RejectsSignaturesOfTheWrongShape) {
  const pkey_ptr key = generate_ed25519();
  const std::string payload = "bundle-bytes-here";
  const std::string digest = onboarding::sha256_hex(payload);
  const std::string pub = public_key_pem(key.get());
  std::string error;

  EXPECT_FALSE(onboarding::verify_bundle(pub, payload, digest, "", error)) << "empty signature";
  EXPECT_FALSE(onboarding::verify_bundle(pub, payload, digest, "=", error));
  EXPECT_FALSE(onboarding::verify_bundle(pub, payload, digest, bytes::base64_encode("short"), error)) << "not 64 bytes";
  EXPECT_FALSE(onboarding::verify_bundle(pub, payload, digest, bytes::base64_encode(std::string(64, '\0')), error)) << "all zero signature";
  EXPECT_FALSE(onboarding::verify_bundle(pub, payload, digest, bytes::base64_encode(std::string(1024, 'A')), error)) << "oversized signature";

  // A valid signature with one flipped bit is still a forgery.
  std::string tampered_raw;
  {
    const std::string valid = sign_bundle(key.get(), payload);
    tampered_raw = valid;
    tampered_raw[10] = (tampered_raw[10] == 'A') ? 'B' : 'A';
  }
  EXPECT_FALSE(onboarding::verify_bundle(pub, payload, digest, tampered_raw, error));
}

TEST(SyncVerifyHostile, RejectsKeysThatAreNotAnEd25519PublicKey) {
  const pkey_ptr key = generate_ed25519();
  const std::string payload = "bundle-bytes-here";
  const std::string digest = onboarding::sha256_hex(payload);
  const std::string signature = sign_bundle(key.get(), payload);
  std::string error;

  EXPECT_FALSE(onboarding::verify_bundle("", payload, digest, signature, error)) << "empty key";
  EXPECT_NE(error.find("public key"), std::string::npos) << error;
  EXPECT_FALSE(onboarding::verify_bundle("not a pem at all", payload, digest, signature, error));
  EXPECT_FALSE(onboarding::verify_bundle("-----BEGIN PUBLIC KEY-----\nnot base64\n-----END PUBLIC KEY-----\n", payload, digest, signature, error));
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(key.get()).substr(0, 40), payload, digest, signature, error)) << "truncated PEM";

  // A private key is not a public key, and another algorithm is not Ed25519.
  const std::unique_ptr<BIO, bio_deleter> bio(BIO_new(BIO_s_mem()));
  PEM_write_bio_PrivateKey(bio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr);
  char *data = nullptr;
  const long len = BIO_get_mem_data(bio.get(), &data);
  EXPECT_FALSE(onboarding::verify_bundle(std::string(data, static_cast<std::size_t>(len)), payload, digest, signature, error)) << "private key PEM";

  const pkey_ptr rsa = generate_rsa();
  ASSERT_TRUE(rsa);
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(rsa.get()), payload, digest, signature, error)) << "RSA key with an Ed25519 signature";
}

TEST(SyncVerifyHostile, TheWrongKeyOfTheRightTypeIsStillTheWrongKey) {
  // The bundle signing key is per tenant: a valid bundle from another tenant
  // (or a re-signed one) must not verify here.
  const pkey_ptr ours = generate_ed25519();
  const pkey_ptr theirs = generate_ed25519();
  const std::string payload = "bundle-bytes-here";
  std::string error;
  EXPECT_FALSE(onboarding::verify_bundle(public_key_pem(ours.get()), payload, onboarding::sha256_hex(payload), sign_bundle(theirs.get(), payload), error));
  EXPECT_TRUE(onboarding::verify_bundle(public_key_pem(theirs.get()), payload, onboarding::sha256_hex(payload), sign_bundle(theirs.get(), payload), error))
      << error;
}

TEST(SyncVerifyHostile, VerifiesLargeBundlesToo) {
  const pkey_ptr key = generate_ed25519();
  const std::string payload(4u * 1024u * 1024u, 'z');
  std::string error;
  EXPECT_TRUE(onboarding::verify_bundle(public_key_pem(key.get()), payload, onboarding::sha256_hex(payload), sign_bundle(key.get(), payload), error))
      << error;
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

TEST(SyncTransportErrors, ClassificationIsCaseInsensitiveAndEmptySafe) {
  EXPECT_EQ(onboarding::classify_transport_error("TLSV13 ALERT CERTIFICATE REQUIRED").kind, onboarding::transport_error_kind::tls_identity);
  EXPECT_EQ(onboarding::classify_transport_error("Certificate Verify Failed").kind, onboarding::transport_error_kind::tls_server_trust);
  EXPECT_EQ(onboarding::classify_transport_error("Connection Timed Out").kind, onboarding::transport_error_kind::network);
  const onboarding::transport_error_info empty = onboarding::classify_transport_error("");
  EXPECT_EQ(empty.kind, onboarding::transport_error_kind::unknown);
  EXPECT_TRUE(empty.advice.empty());
}

TEST(SyncTransportErrors, AnIdentityProblemOutranksTheConnectionWrapper) {
  // Every TLS failure arrives wrapped in "Failed to connect to host:port: ..."
  // which itself matches the network patterns; the actionable classification
  // has to win or the operator is told to "wait for the network".
  EXPECT_EQ(onboarding::classify_transport_error("Failed to connect to fleet:8443: timed out, tlsv13 alert certificate required").kind,
            onboarding::transport_error_kind::tls_identity);
  EXPECT_EQ(onboarding::classify_transport_error("Failed to connect to fleet:8443: connection reset, certificate verify failed").kind,
            onboarding::transport_error_kind::tls_server_trust);
  EXPECT_EQ(onboarding::classify_transport_error("Failed to connect to fleet:8443: connection refused (handshake never started)").kind,
            onboarding::transport_error_kind::tls_other)
      << "a message naming the handshake is a TLS problem, not a plain network one";
}

TEST(SyncTransportErrors, EveryClassifiedKindCarriesActionableAdvice) {
  for (const std::string message : {"tlsv13 alert certificate required", "certificate verify failed", "alert handshake failure", "connection refused"}) {
    const onboarding::transport_error_info info = onboarding::classify_transport_error(message);
    EXPECT_NE(info.kind, onboarding::transport_error_kind::unknown) << message;
    EXPECT_FALSE(info.advice.empty()) << message;
  }
}

// --- certificate lifecycle ---------------------------------------------------

TEST(SyncCert, DaysUntilExpiry) {
  const long days = onboarding::days_until_expiry(make_cert_expiring_in(30));
  EXPECT_GE(days, 29);
  EXPECT_LE(days, 30);
}

TEST(SyncCert, ExpiredCertIsNegative) { EXPECT_LT(onboarding::days_until_expiry(make_cert_expiring_in(-10)), 0); }

TEST(SyncCert, ACertExpiredLessThanADayAgoIsStillNegative) {
  // ASN1_TIME_diff reports this as days=0 with negative seconds. Returning the
  // day count as-is made it 0 - the same answer as "expires later today" - so
  // an already-expired certificate read as merely urgent, and any "< 0" test
  // stayed false for the whole first day of expiry.
  EXPECT_LT(onboarding::days_until_expiry(make_cert_expiring_in_seconds(-3600)), 0) << "expired an hour ago";
  EXPECT_LT(onboarding::days_until_expiry(make_cert_expiring_in_seconds(-60)), 0) << "expired a minute ago";
}

TEST(SyncCert, ACertValidForLessThanADayIsNotNegative) {
  // The other side of the same rounding: still valid must never read as expired.
  EXPECT_EQ(onboarding::days_until_expiry(make_cert_expiring_in_seconds(23 * 3600)), 0);
  EXPECT_EQ(onboarding::days_until_expiry(make_cert_expiring_in_seconds(60)), 0);
}

TEST(SyncCert, GarbagePemThrows) { EXPECT_THROW(onboarding::days_until_expiry("not a pem"), onboarding::onboarding_error); }

// days_until_expiry drives renewal: a certificate it cannot read must be a
// hard error (the caller logs and skips renewal) and never a "0 days left"
// that triggers a renewal storm, nor a huge number that never renews.
TEST(SyncCertHostile, UnreadableCertificatesThrowRatherThanGuess) {
  EXPECT_THROW(onboarding::days_until_expiry(""), onboarding::onboarding_error);
  EXPECT_THROW(onboarding::days_until_expiry("   "), onboarding::onboarding_error);
  EXPECT_THROW(onboarding::days_until_expiry("-----BEGIN CERTIFICATE-----\nnot base64\n-----END CERTIFICATE-----\n"), onboarding::onboarding_error);
  EXPECT_THROW(onboarding::days_until_expiry("-----BEGIN CERTIFICATE-----\n-----END CERTIFICATE-----\n"), onboarding::onboarding_error);
  EXPECT_THROW(onboarding::days_until_expiry(make_cert_expiring_in(30).substr(0, 60)), onboarding::onboarding_error) << "truncated PEM";
  // A private key is a valid PEM, just not a certificate.
  const pkey_ptr key = generate_ed25519();
  const std::unique_ptr<BIO, bio_deleter> bio(BIO_new(BIO_s_mem()));
  PEM_write_bio_PrivateKey(bio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr);
  char *data = nullptr;
  const long len = BIO_get_mem_data(bio.get(), &data);
  EXPECT_THROW(onboarding::days_until_expiry(std::string(data, static_cast<std::size_t>(len))), onboarding::onboarding_error);
}

TEST(SyncCertHostile, ExpiryBoundariesAroundTheRenewalThreshold) {
  // The sync loop renews at <= 14 days, so the sign and the boundary matter.
  EXPECT_LE(onboarding::days_until_expiry(make_cert_expiring_in(13)), 13);
  EXPECT_GE(onboarding::days_until_expiry(make_cert_expiring_in(13)), 12);
  EXPECT_GE(onboarding::days_until_expiry(make_cert_expiring_in(3650)), 3649) << "a long lived cert must not look expiring";
  EXPECT_LT(onboarding::days_until_expiry(make_cert_expiring_in(-3650)), 0);
  // Expiring within the hour is "0 days", not "already expired".
  EXPECT_EQ(onboarding::days_until_expiry(make_cert_expiring_in(0)), 0);
}

TEST(SyncCertHostile, IgnoresTrailingGarbageAfterTheCertificate) {
  // PEM readers stop at END CERTIFICATE; a server appending anything after it
  // must not change what we read (or make us throw on a good certificate).
  const std::string cert = make_cert_expiring_in(30);
  EXPECT_GE(onboarding::days_until_expiry(cert + "trailing junk\n"), 29);
}

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

// A renewal response replaces this host's whole identity, so a partial or
// malformed one must be rejected outright - the caller then keeps the existing
// (still valid) certificate rather than saving something unusable.
TEST(SyncRenewHostile, EveryRequiredFieldIsRequired) {
  json::object full;
  full["cert_pem"] = "NEW-CERT";
  full["ca_pem"] = "NEW-CA";
  full["bundle_signing_pub_pem"] = "NEW-BUNDLE-KEY";
  full["mtls_server_cert_pem"] = "NEW-MTLS-CERT";
  ASSERT_NO_THROW(onboarding::parse_renew_response(json::serialize(full), onboarding::identity(), onboarding::enrolled_identity()));

  for (const char *field : {"cert_pem", "ca_pem", "bundle_signing_pub_pem", "mtls_server_cert_pem"}) {
    json::object missing = full;
    missing.erase(field);
    EXPECT_THROW(onboarding::parse_renew_response(json::serialize(missing), onboarding::identity(), onboarding::enrolled_identity()),
                 onboarding::onboarding_error)
        << "accepted a response without " << field;

    for (const json::value bad : {json::value(""), json::value(), json::value(42), json::value(json::object())}) {
      json::object wrong = full;
      wrong[field] = bad;
      EXPECT_THROW(onboarding::parse_renew_response(json::serialize(wrong), onboarding::identity(), onboarding::enrolled_identity()),
                   onboarding::onboarding_error)
          << "accepted " << json::serialize(bad) << " as " << field;
    }
  }
}

TEST(SyncRenewHostile, MalformedBodiesThrow) {
  for (const std::string body : {std::string(""), std::string("not json"), std::string("[]"), std::string("null"), std::string("{")}) {
    EXPECT_THROW(onboarding::parse_renew_response(body, onboarding::identity(), onboarding::enrolled_identity()), onboarding::onboarding_error);
  }
}

TEST(SyncRenewHostile, TheServerCannotMoveUsToAnotherServer) {
  // Only certificate material is renewable: a renewal response that tries to
  // redirect the agent (server_url / mtls_url) must be ignored, or a
  // compromised renewal endpoint could hand the host to someone else.
  onboarding::enrolled_identity current;
  current.private_key_pem = "OLD-KEY";
  current.server_url = "https://api.example.com";
  current.mtls_url = "https://mtls.example.com";
  onboarding::identity fresh;
  fresh.private_key_pem = "NEW-KEY";
  const std::string body =
      "{\"cert_pem\": \"C\", \"ca_pem\": \"CA\", \"bundle_signing_pub_pem\": \"K\", \"mtls_server_cert_pem\": \"M\","
      " \"server_url\": \"https://evil.example.com\", \"mtls_url\": \"https://evil.example.com\", \"private_key_pem\": \"ATTACKER-KEY\"}";
  const onboarding::enrolled_identity renewed = onboarding::parse_renew_response(body, fresh, current);
  EXPECT_EQ(renewed.server_url, "https://api.example.com");
  EXPECT_EQ(renewed.mtls_url, "https://mtls.example.com");
  EXPECT_EQ(renewed.private_key_pem, "NEW-KEY") << "the key must come from our own CSR, never from the response";
}

// --- state report payload ---------------------------------------------------

TEST(SyncReport, BuildStateReport) {
  std::vector<onboarding::installed_bundle> bundles;
  bundles.push_back({"b1", "1.0"});
  std::map<std::string, std::string> tags;
  tags["os"] = "windows";
  const json::object root = json::parse(onboarding::build_state_report(std::string("h1"), bundles, {"oops"}, tags, false)).as_object();
  EXPECT_EQ(root.at("applied_state_hash").as_string(), "h1");
  EXPECT_EQ(root.at("bundles_installed").as_array().at(0).as_object().at("id").as_string(), "b1");
  EXPECT_EQ(root.at("errors").as_array().at(0).as_string(), "oops");
  EXPECT_EQ(root.at("reported_tags").as_object().at("os").as_string(), "windows");
}

TEST(SyncReport, OmitsHashAfterFailedApply) {
  const json::object root = json::parse(onboarding::build_state_report(boost::none, {}, {}, {}, false)).as_object();
  EXPECT_EQ(root.if_contains("applied_state_hash"), nullptr);
}

TEST(SyncReport, ReportsWhetherLocalConfigurationOutranksTheFleet) {
  // Always present, both ways round: the server distinguishes "no local
  // overrides" from an older agent that says nothing at all.
  const json::object without = json::parse(onboarding::build_state_report(boost::none, {}, {}, {}, false)).as_object();
  ASSERT_NE(without.if_contains("local_config_present"), nullptr);
  EXPECT_FALSE(without.at("local_config_present").as_bool());

  const json::object with = json::parse(onboarding::build_state_report(boost::none, {}, {}, {}, true)).as_object();
  EXPECT_TRUE(with.at("local_config_present").as_bool());
}

TEST(SyncReport, LocalConfigFlagCarriesNoConfigurationContent) {
  // The point of the flag is that the server learns a host is partly
  // self-managed without the agent uploading configuration that routinely
  // holds passwords. Guard the payload, not just the boolean.
  std::map<std::string, std::string> tags;
  tags["os"] = "linux";
  const std::string payload = onboarding::build_state_report(std::string("h1"), {}, {}, tags, true);
  const json::object root = json::parse(payload).as_object();
  // Exactly the members the report is allowed to have.
  for (const auto &member : root) {
    const std::string name(member.key());
    EXPECT_TRUE(name == "applied_state_hash" || name == "bundles_installed" || name == "errors" || name == "reported_tags" ||
                name == "local_config_present")
        << "unexpected member in the state report: " << name;
  }
  EXPECT_TRUE(root.at("local_config_present").as_bool());
}

// build_state_report takes strings from outside (bundle names and versions
// from the server, error text from anywhere, tags from any module), so it has
// to produce valid JSON no matter what is in them.

TEST(SyncReportHostile, ErrorTextAndTagsCannotBreakTheJson) {
  std::vector<onboarding::installed_bundle> bundles;
  bundles.push_back({"b\"1", "1.0\n[evil]"});
  std::map<std::string, std::string> tags;
  tags["hostname"] = "web-01\", \"admin\": true, \"x\": \"";
  tags["quote\""] = "\\";
  std::vector<std::string> errors;
  errors.push_back("failed: \"quoted\" \\ backslash\nnewline\ttab");
  errors.push_back(std::string("with a nul\0inside", 17));
  errors.push_back("unicode é日本 \xC3\xA9");

  const std::string payload = onboarding::build_state_report(std::string("h\"1"), bundles, errors, tags, false);
  json::object root;
  ASSERT_NO_THROW(root = json::parse(payload).as_object()) << payload;
  EXPECT_EQ(root.at("applied_state_hash").as_string(), "h\"1");
  EXPECT_EQ(root.at("errors").as_array().size(), 3u);
  EXPECT_EQ(root.at("errors").as_array().at(0).as_string(), errors[0]) << "error text must round-trip, escaped not mangled";
  EXPECT_EQ(root.at("reported_tags").as_object().at("hostname").as_string(), tags["hostname"]);
  // The injected "admin" is data inside a string, not a member of the report.
  EXPECT_EQ(root.if_contains("admin"), nullptr);
}

TEST(SyncReportHostile, EmptyReportIsStillWellFormed) {
  const json::object root = json::parse(onboarding::build_state_report(boost::none, {}, {}, {}, false)).as_object();
  EXPECT_TRUE(root.at("bundles_installed").as_array().empty());
  EXPECT_TRUE(root.at("errors").as_array().empty());
  EXPECT_TRUE(root.at("reported_tags").as_object().empty()) << "the server relies on the keys existing";
}

// --- encrypted bundles ("enc-v1") ---------------------------------------------
//
// The envelope is produced by the fleet server's browser code and its Rust
// reference implementation; the agent must open it byte for byte. The fixed
// vector below is the server's pinned key (bytes 00..1f) sealed with a fixed
// nonce by an independent AES-256-GCM implementation, so a drift in framing,
// AAD encoding or tag placement fails here rather than in the field.

namespace {

std::string from_hex(const std::string &hex) {
  std::string out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2) out.push_back(static_cast<char>(std::stoul(hex.substr(i, 2), nullptr, 16)));
  return out;
}

const char *pinned_key_b64 = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=";
const char *pinned_fingerprint = "630dcd2966c43366";
const char *pinned_blob_hex = "4e53454231630dcd2966c43366000102030405060708090a0b3167b56faa976318e145332dec299b8f64c8fa5351ed";

std::string pinned_key() {
  std::string raw, error;
  EXPECT_TRUE(onboarding::parse_bundle_key(pinned_key_b64, raw, error)) << error;
  return raw;
}

std::string other_key() { return std::string(32, 'k'); }

const std::string fixed_nonce = from_hex("000102030405060708090a0b");

}  // namespace

TEST(BundleKey, ParsesTheOperatorForm) {
  std::string raw, error;
  ASSERT_TRUE(onboarding::parse_bundle_key(pinned_key_b64, raw, error)) << error;
  ASSERT_EQ(raw.size(), 32u);
  for (std::size_t i = 0; i < raw.size(); ++i) EXPECT_EQ(static_cast<unsigned char>(raw[i]), i);
  EXPECT_EQ(onboarding::bundle_key_fingerprint(raw), pinned_fingerprint);
}

TEST(BundleKey, TrimsSurroundingWhitespace) {
  std::string raw, error;
  EXPECT_TRUE(onboarding::parse_bundle_key(std::string("  ") + pinned_key_b64 + "\r\n", raw, error)) << error;
  EXPECT_EQ(onboarding::bundle_key_fingerprint(raw), pinned_fingerprint);
}

TEST(BundleKey, RejectsAnythingButThirtyTwoBase64Bytes) {
  std::string raw, error;
  EXPECT_FALSE(onboarding::parse_bundle_key("", raw, error));
  EXPECT_FALSE(onboarding::parse_bundle_key("   ", raw, error));
  EXPECT_FALSE(onboarding::parse_bundle_key("not base64!", raw, error));
  EXPECT_FALSE(onboarding::parse_bundle_key("AAECAwQFBgcICQoLDA0ODw==", raw, error)) << "16 bytes";
  EXPECT_FALSE(onboarding::parse_bundle_key(std::string(pinned_key_b64) + "AAAA", raw, error)) << "35 bytes";
  EXPECT_FALSE(onboarding::parse_bundle_key("630dcd2966c43366", raw, error)) << "a fingerprint is not a key";
  // The error is logged and reported; it must not carry the value.
  EXPECT_FALSE(onboarding::parse_bundle_key("secretsecretsecretsecretsecretsecretsecr", raw, error));
  EXPECT_EQ(error.find("secret"), std::string::npos);
}

TEST(BundleKey, RejectsPaddingThatIsNotTrailingPadding) {
  // '=' is padding, so it only means anything at the end. A paste error that
  // drops one into the middle keeps the length a multiple of four, so without
  // the alphabet check the operator would be told the key is the wrong length
  // (it is not) instead of that it is not base64.
  std::string mangled = pinned_key_b64;
  mangled[8] = '=';
  std::string raw, error;
  EXPECT_FALSE(onboarding::parse_bundle_key(mangled, raw, error));
  EXPECT_NE(error.find("base64"), std::string::npos) << error;
  // A whole quantum of padding is not a key either.
  EXPECT_FALSE(onboarding::parse_bundle_key("====", raw, error));
  EXPECT_NE(error.find("base64"), std::string::npos) << error;
  EXPECT_FALSE(onboarding::parse_bundle_key(std::string(pinned_key_b64).substr(0, 40) + "A===", raw, error));
}

TEST(BundleKey, SplitsAnInstallerList) {
  const std::vector<std::string> keys = onboarding::split_bundle_keys(" a1==, b2== ;c3==\nd4== ");
  ASSERT_EQ(keys.size(), 4u);
  EXPECT_EQ(keys[0], "a1==");
  EXPECT_EQ(keys[1], "b2==");
  EXPECT_EQ(keys[2], "c3==");
  EXPECT_EQ(keys[3], "d4==");
  EXPECT_TRUE(onboarding::split_bundle_keys("").empty());
  EXPECT_TRUE(onboarding::split_bundle_keys(" ,; ").empty());
}

TEST(BundleCrypto, OpensTheServersFixedVector) {
  const std::string blob = from_hex(pinned_blob_hex);
  ASSERT_EQ(blob.size(), 47u);
  EXPECT_TRUE(onboarding::is_encrypted_bundle(blob));
  EXPECT_EQ(onboarding::encrypted_bundle_fingerprint(blob), pinned_fingerprint);
  std::string plaintext, error;
  ASSERT_EQ(onboarding::decrypt_bundle(pinned_key(), "pinned", "0.1", blob, plaintext, error), onboarding::unseal_status::ok) << error;
  EXPECT_EQ(plaintext, "vector");
}

TEST(BundleCrypto, SealsExactlyLikeTheServer) {
  // Same key, nonce, AAD and plaintext as the vector: the bytes must match.
  const std::string blob = onboarding::encrypt_bundle(pinned_key(), fixed_nonce, "pinned", "0.1", "vector");
  EXPECT_EQ(blob, from_hex(pinned_blob_hex));
}

TEST(BundleCrypto, RoundTripsABundleSizedPayload) {
  std::string payload(200000, '\0');
  for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>(i * 7);
  const std::string blob = onboarding::encrypt_bundle(other_key(), fixed_nonce, "big", "3.2.1", payload);
  std::string plaintext, error;
  ASSERT_EQ(onboarding::decrypt_bundle(other_key(), "big", "3.2.1", blob, plaintext, error), onboarding::unseal_status::ok) << error;
  EXPECT_EQ(plaintext, payload);
}

TEST(BundleCrypto, NameAndVersionAreBoundIntoTheSeal) {
  const std::string blob = onboarding::encrypt_bundle(pinned_key(), fixed_nonce, "secrets", "1.0.0", "zip");
  std::string plaintext, error;
  EXPECT_EQ(onboarding::decrypt_bundle(pinned_key(), "secrets", "1.0.1", blob, plaintext, error), onboarding::unseal_status::failed);
  EXPECT_EQ(onboarding::decrypt_bundle(pinned_key(), "other", "1.0.0", blob, plaintext, error), onboarding::unseal_status::failed);
  // The NUL separator: a shifted split of the same characters is not the
  // same additional data.
  EXPECT_EQ(onboarding::decrypt_bundle(pinned_key(), "secrets1", ".0.0", blob, plaintext, error), onboarding::unseal_status::failed);
  EXPECT_EQ(onboarding::decrypt_bundle(pinned_key(), "secrets", "1.0.0", blob, plaintext, error), onboarding::unseal_status::ok);
}

TEST(BundleCrypto, TamperingIsDetected) {
  const std::string blob = onboarding::encrypt_bundle(pinned_key(), fixed_nonce, "demo", "1.0", "zip-bytes");
  std::string plaintext, error;
  for (const std::size_t at : {std::size_t(13), std::size_t(25), blob.size() - 1}) {
    std::string tampered = blob;
    tampered[at] = static_cast<char>(tampered[at] ^ 0x01);
    EXPECT_EQ(onboarding::decrypt_bundle(pinned_key(), "demo", "1.0", tampered, plaintext, error), onboarding::unseal_status::failed) << "byte " << at;
  }
}

TEST(BundleCrypto, ReportsAnotherKeyAndTruncation) {
  const std::string blob = onboarding::encrypt_bundle(pinned_key(), fixed_nonce, "demo", "1.0", "zip-bytes");
  std::string plaintext, error;
  EXPECT_EQ(onboarding::decrypt_bundle(other_key(), "demo", "1.0", blob, plaintext, error), onboarding::unseal_status::wrong_key);
  EXPECT_EQ(onboarding::decrypt_bundle(pinned_key(), "demo", "1.0", "NSEB1short", plaintext, error), onboarding::unseal_status::corrupt);
  EXPECT_EQ(onboarding::decrypt_bundle(pinned_key(), "demo", "1.0", blob.substr(0, 40), plaintext, error), onboarding::unseal_status::corrupt);
  EXPECT_EQ(onboarding::decrypt_bundle(pinned_key(), "demo", "1.0", "PK\x03\x04plain zip", plaintext, error), onboarding::unseal_status::not_encrypted);
  EXPECT_EQ(onboarding::encrypted_bundle_fingerprint("NSEB1short"), "");
  EXPECT_FALSE(onboarding::is_encrypted_bundle("PK\x03\x04"));
  EXPECT_FALSE(onboarding::is_encrypted_bundle("NSEB"));
}

TEST(BundleOpen, PassesPlainBundlesThroughUnlessRequired) {
  const std::string zip = "PK\x03\x04 a plain zip";
  std::string out, error;
  EXPECT_TRUE(onboarding::open_bundle({}, "demo", "1.0", zip, false, out, error)) << error;
  EXPECT_EQ(out, zip);
  EXPECT_TRUE(onboarding::open_bundle({pinned_key_b64}, "demo", "1.0", zip, false, out, error)) << error;
  EXPECT_FALSE(onboarding::open_bundle({pinned_key_b64}, "demo", "1.0", zip, true, out, error));
  EXPECT_NE(error.find("requires encrypted"), std::string::npos);
}

TEST(BundleOpen, PicksTheKeyByFingerprintAndSkipsTheOthers) {
  const std::string blob = onboarding::encrypt_bundle(pinned_key(), fixed_nonce, "demo", "1.0", "zip-bytes");
  const std::string other_b64 = "a2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2s=";  // 32 x 'k'
  std::string out, error;
  EXPECT_TRUE(onboarding::open_bundle({other_b64, pinned_key_b64}, "demo", "1.0", blob, true, out, error)) << error;
  EXPECT_EQ(out, "zip-bytes");
}

TEST(BundleOpen, ExplainsAMissingKeyWithTheFingerprint) {
  const std::string blob = onboarding::encrypt_bundle(pinned_key(), fixed_nonce, "demo", "1.0", "zip-bytes");
  const std::string other_b64 = "a2tra2tra2tra2tra2tra2tra2tra2tra2tra2tra2s=";
  std::string out, error;
  EXPECT_FALSE(onboarding::open_bundle({}, "demo", "1.0", blob, false, out, error));
  EXPECT_NE(error.find(pinned_fingerprint), std::string::npos) << error;
  EXPECT_NE(error.find("no bundle key"), std::string::npos) << error;
  EXPECT_FALSE(onboarding::open_bundle({other_b64}, "demo", "1.0", blob, false, out, error));
  EXPECT_NE(error.find(pinned_fingerprint), std::string::npos) << error;
  EXPECT_EQ(error.find(pinned_key_b64), std::string::npos) << "no key material in the message";
}

TEST(BundleOpen, AMatchingKeyThatFailsToAuthenticateIsFatal) {
  // Served under another version: the right key is present, the seal does
  // not open. That is a substitution, not a reason to try the next key.
  const std::string blob = onboarding::encrypt_bundle(pinned_key(), fixed_nonce, "demo", "1.0", "zip-bytes");
  std::string out, error;
  EXPECT_FALSE(onboarding::open_bundle({pinned_key_b64, pinned_key_b64}, "demo", "2.0", blob, false, out, error));
  EXPECT_NE(error.find("authentication failed"), std::string::npos) << error;
}

TEST(BundleOpen, AnUnparsableConfiguredKeyIsAnError) {
  const std::string blob = onboarding::encrypt_bundle(pinned_key(), fixed_nonce, "demo", "1.0", "zip-bytes");
  std::string out, error;
  EXPECT_FALSE(onboarding::open_bundle({"garbage"}, "demo", "1.0", blob, false, out, error));
  EXPECT_NE(error.find("invalid"), std::string::npos) << error;
}

TEST(BundleOpen, DesiredStateCarriesTheAdvisoryFormat) {
  const onboarding::desired_state state = onboarding::parse_desired_state(bundle_with("format", "enc-v1"));
  ASSERT_EQ(state.bundles.size(), 1u);
  EXPECT_EQ(state.bundles[0].format, "enc-v1");
  const onboarding::desired_state plain = onboarding::parse_desired_state(bundle_with("priority", 100));
  EXPECT_EQ(plain.bundles[0].format, "");
}
