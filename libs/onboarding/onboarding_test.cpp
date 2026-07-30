// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>
#include <onboarding/onboarding.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace fs = boost::filesystem;
namespace json = boost::json;

namespace {

struct evp_pkey_deleter {
  void operator()(EVP_PKEY *p) const { EVP_PKEY_free(p); }
};
struct x509_req_deleter {
  void operator()(X509_REQ *p) const { X509_REQ_free(p); }
};
typedef std::unique_ptr<EVP_PKEY, evp_pkey_deleter> pkey_ptr;
typedef std::unique_ptr<X509_REQ, x509_req_deleter> x509_req_ptr;

pkey_ptr parse_private_key(const std::string &pem) {
  BIO *bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
  EVP_PKEY *key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  return pkey_ptr(key);
}

x509_req_ptr parse_csr(const std::string &pem) {
  BIO *bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
  X509_REQ *req = PEM_read_bio_X509_REQ(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  return x509_req_ptr(req);
}

http::response make_response(const unsigned int code, const std::string &payload) {
  http::response response;
  response.status_code_ = code;
  response.payload_ = payload;
  return response;
}

onboarding::identity test_identity() {
  onboarding::identity id;
  id.private_key_pem = "PRIVATE-KEY";
  id.csr_pem = "CSR";
  return id;
}

onboarding::enrollment_request test_request() {
  onboarding::enrollment_request request;
  request.server_url = "https://fleet.example.com";
  request.bootstrap_token = "tok-123";
  request.hostname = "web-01";
  request.os = "linux";
  return request;
}

const std::string ok_body =
    "{"
    "\"cert_pem\": \"CERT\","
    "\"ca_pem\": \"CA\","
    "\"bundle_signing_pub_pem\": \"BUNDLE-KEY\","
    "\"server_url\": \"https://api.example.com\","
    "\"mtls_url\": \"https://mtls.example.com:8443\","
    "\"mtls_server_cert_pem\": \"MTLS-CERT\""
    "}";

onboarding::enrolled_identity test_state() {
  onboarding::enrolled_identity state;
  state.private_key_pem = "PRIVATE-KEY";
  state.cert_pem = "CERT";
  state.ca_pem = "CA";
  state.bundle_signing_pub_pem = "BUNDLE-KEY";
  state.server_url = "https://api.example.com";
  state.mtls_url = "https://mtls.example.com:8443";
  state.mtls_server_cert_pem = "MTLS-CERT";
  return state;
}

void no_sleep(unsigned long) {}

}  // namespace

TEST(OnboardingIdentity, GeneratesEd25519KeyAndValidCsr) {
  const onboarding::identity id = onboarding::generate_identity();

  const pkey_ptr key = parse_private_key(id.private_key_pem);
  ASSERT_TRUE(key) << "private key PEM should parse";
  EXPECT_EQ(EVP_PKEY_base_id(key.get()), EVP_PKEY_ED25519);

  const x509_req_ptr req = parse_csr(id.csr_pem);
  ASSERT_TRUE(req) << "CSR PEM should parse";

  const pkey_ptr csr_key(X509_REQ_get_pubkey(req.get()));
  ASSERT_TRUE(csr_key);
  EXPECT_EQ(X509_REQ_verify(req.get(), csr_key.get()), 1) << "CSR signature should verify with its own public key";
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EXPECT_EQ(EVP_PKEY_eq(csr_key.get(), key.get()), 1) << "CSR public key should match the private key";
#else
  EXPECT_EQ(EVP_PKEY_cmp(csr_key.get(), key.get()), 1) << "CSR public key should match the private key";
#endif
}

TEST(OnboardingIdentity, UsesRequestedCommonName) {
  const onboarding::identity id = onboarding::generate_identity("web-01");
  const x509_req_ptr req = parse_csr(id.csr_pem);
  ASSERT_TRUE(req);
  char buf[256] = {0};
  ASSERT_GT(X509_NAME_get_text_by_NID(X509_REQ_get_subject_name(req.get()), NID_commonName, buf, sizeof(buf)), 0);
  EXPECT_STREQ(buf, "web-01");
}

TEST(OnboardingIdentity, GeneratesUniqueKeys) {
  const onboarding::identity first = onboarding::generate_identity();
  const onboarding::identity second = onboarding::generate_identity();
  EXPECT_NE(first.private_key_pem, second.private_key_pem);
  EXPECT_NE(first.csr_pem, second.csr_pem);
}

TEST(OnboardingParse, MapsAllFields) {
  const onboarding::enrolled_identity result = onboarding::parse_enroll_response(ok_body, test_identity(), "https://fallback.example.com");
  EXPECT_EQ(result.private_key_pem, "PRIVATE-KEY");
  EXPECT_EQ(result.cert_pem, "CERT");
  EXPECT_EQ(result.ca_pem, "CA");
  EXPECT_EQ(result.bundle_signing_pub_pem, "BUNDLE-KEY");
  EXPECT_EQ(result.server_url, "https://api.example.com");
  EXPECT_EQ(result.mtls_url, "https://mtls.example.com:8443");
  EXPECT_EQ(result.mtls_server_cert_pem, "MTLS-CERT");
}

TEST(OnboardingParse, FallsBackToRequestServerUrl) {
  const std::string body =
      "{\"cert_pem\": \"CERT\", \"ca_pem\": \"CA\", \"bundle_signing_pub_pem\": \"BUNDLE-KEY\","
      "\"mtls_url\": \"https://mtls.example.com\", \"mtls_server_cert_pem\": \"MTLS-CERT\"}";
  const onboarding::enrolled_identity result = onboarding::parse_enroll_response(body, test_identity(), "https://fallback.example.com");
  EXPECT_EQ(result.server_url, "https://fallback.example.com");
}

TEST(OnboardingParse, MissingFieldThrows) {
  const std::string body = "{\"cert_pem\": \"CERT\"}";
  try {
    onboarding::parse_enroll_response(body, test_identity(), "");
    FAIL() << "expected onboarding_error";
  } catch (const onboarding::onboarding_error &e) {
    EXPECT_FALSE(e.retryable());
    EXPECT_NE(std::string(e.what()).find("ca_pem"), std::string::npos);
  }
}

TEST(OnboardingParse, GarbageThrows) {
  try {
    onboarding::parse_enroll_response("this is not json", test_identity(), "");
    FAIL() << "expected onboarding_error";
  } catch (const onboarding::onboarding_error &e) {
    EXPECT_FALSE(e.retryable());
  }
}

TEST(OnboardingEnroll, PostsTokenAndCsrToEnrollEndpoint) {
  std::vector<std::string> urls;
  std::vector<std::string> payloads;
  // Trailing slash on the server url must not produce a double slash.
  onboarding::enrollment_request request = test_request();
  request.server_url = "https://fleet.example.com/";
  const onboarding::enrolled_identity result = onboarding::enroll(
      request, test_identity(),
      [&](const std::string &url, const std::string &payload) {
        urls.push_back(url);
        payloads.push_back(payload);
        return make_response(200, ok_body);
      },
      no_sleep);

  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0], "https://fleet.example.com/enroll/v1");
  const json::object body = json::parse(payloads[0]).as_object();
  EXPECT_EQ(body.at("bootstrap_token").as_string(), "tok-123");
  EXPECT_EQ(body.at("csr_pem").as_string(), "CSR");
  EXPECT_EQ(body.at("hostname").as_string(), "web-01");
  EXPECT_EQ(body.at("os").as_string(), "linux");
  EXPECT_EQ(result.cert_pem, "CERT");
  EXPECT_EQ(result.private_key_pem, "PRIVATE-KEY");
}

TEST(OnboardingEnroll, RejectedTokenIsFatalAndNotRetried) {
  int calls = 0;
  try {
    onboarding::enroll(
        test_request(), test_identity(),
        [&](const std::string &, const std::string &) {
          ++calls;
          return make_response(403, "token already used");
        },
        no_sleep);
    FAIL() << "expected onboarding_error";
  } catch (const onboarding::onboarding_error &e) {
    EXPECT_FALSE(e.retryable());
    EXPECT_NE(std::string(e.what()).find("new install command"), std::string::npos);
  }
  EXPECT_EQ(calls, 1);
}

TEST(OnboardingEnroll, RateLimitHonorsRetryAfterThenSucceeds) {
  int calls = 0;
  std::vector<unsigned long> sleeps;
  const onboarding::enrolled_identity result = onboarding::enroll(
      test_request(), test_identity(),
      [&](const std::string &, const std::string &) {
        if (++calls == 1) {
          http::response response = make_response(429, "slow down");
          response.add_header("Retry-After", "2");
          return response;
        }
        return make_response(200, ok_body);
      },
      [&](const unsigned long ms) { sleeps.push_back(ms); });

  EXPECT_EQ(calls, 2);
  ASSERT_EQ(sleeps.size(), 1u);
  // Retry-After of 2s plus up to 1s of jitter.
  EXPECT_GE(sleeps[0], 2000u);
  EXPECT_LT(sleeps[0], 3000u);
  EXPECT_EQ(result.cert_pem, "CERT");
}

TEST(OnboardingEnroll, ServerErrorsAreRetriedUntilAttemptsExhausted) {
  int calls = 0;
  std::vector<unsigned long> sleeps;
  onboarding::enrollment_request request = test_request();
  request.max_attempts = 3;
  try {
    onboarding::enroll(
        request, test_identity(),
        [&](const std::string &, const std::string &) {
          ++calls;
          return make_response(500, "boom");
        },
        [&](const unsigned long ms) { sleeps.push_back(ms); });
    FAIL() << "expected onboarding_error";
  } catch (const onboarding::onboarding_error &e) {
    EXPECT_TRUE(e.retryable());
  }
  EXPECT_EQ(calls, 3);
  EXPECT_EQ(sleeps.size(), 2u);
}

TEST(OnboardingEnroll, NetworkFailureIsRetryable) {
  int calls = 0;
  try {
    onboarding::enroll(
        test_request(), test_identity(),
        [&](const std::string &, const std::string &) -> http::response {
          ++calls;
          throw std::runtime_error("connection refused");
        },
        no_sleep);
    FAIL() << "expected onboarding_error";
  } catch (const onboarding::onboarding_error &e) {
    EXPECT_TRUE(e.retryable());
    EXPECT_NE(std::string(e.what()).find("connection refused"), std::string::npos);
  }
  EXPECT_EQ(calls, 3);
}

TEST(OnboardingEnroll, OtherClientErrorsAreFatal) {
  int calls = 0;
  try {
    onboarding::enroll(
        test_request(), test_identity(),
        [&](const std::string &, const std::string &) {
          ++calls;
          return make_response(400, "bad request");
        },
        no_sleep);
    FAIL() << "expected onboarding_error";
  } catch (const onboarding::onboarding_error &e) {
    EXPECT_FALSE(e.retryable());
  }
  EXPECT_EQ(calls, 1);
}

TEST(OnboardingEnroll, MissingTokenFailsWithoutCallingServer) {
  int calls = 0;
  onboarding::enrollment_request request = test_request();
  request.bootstrap_token = "";
  EXPECT_THROW(onboarding::enroll(
                   request, test_identity(),
                   [&](const std::string &, const std::string &) {
                     ++calls;
                     return make_response(200, ok_body);
                   },
                   no_sleep),
               onboarding::onboarding_error);
  EXPECT_EQ(calls, 0);
}

TEST(OnboardingEnroll, InvalidServerUrlFailsWithoutCallingServer) {
  int calls = 0;
  onboarding::enrollment_request request = test_request();
  request.server_url = "not-a-url";
  EXPECT_THROW(onboarding::enroll(
                   request, test_identity(),
                   [&](const std::string &, const std::string &) {
                     ++calls;
                     return make_response(200, ok_body);
                   },
                   no_sleep),
               onboarding::onboarding_error);
  EXPECT_EQ(calls, 0);
}

class OnboardingStateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = fs::temp_directory_path() / fs::unique_path("nscp-onboarding-%%%%-%%%%");
    fs::create_directories(dir_);
    path_ = (dir_ / "agent-state.json").string();
  }
  void TearDown() override {
    boost::system::error_code ignored;
    fs::remove_all(dir_, ignored);
  }

  fs::path dir_;
  std::string path_;
};

TEST_F(OnboardingStateTest, RoundTrip) {
  const onboarding::enrolled_identity saved = test_state();
  onboarding::save_state(saved, path_);

  EXPECT_FALSE(fs::exists(path_ + ".tmp")) << "temp file should be renamed away";
  const boost::optional<onboarding::enrolled_identity> loaded = onboarding::load_state(path_);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->private_key_pem, saved.private_key_pem);
  EXPECT_EQ(loaded->cert_pem, saved.cert_pem);
  EXPECT_EQ(loaded->ca_pem, saved.ca_pem);
  EXPECT_EQ(loaded->bundle_signing_pub_pem, saved.bundle_signing_pub_pem);
  EXPECT_EQ(loaded->server_url, saved.server_url);
  EXPECT_EQ(loaded->mtls_url, saved.mtls_url);
  EXPECT_EQ(loaded->mtls_server_cert_pem, saved.mtls_server_cert_pem);
}

TEST_F(OnboardingStateTest, SaveOverwritesExistingState) {
  onboarding::save_state(test_state(), path_);
  onboarding::enrolled_identity renewed = test_state();
  renewed.cert_pem = "NEW-CERT";
  onboarding::save_state(renewed, path_);

  const boost::optional<onboarding::enrolled_identity> loaded = onboarding::load_state(path_);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->cert_pem, "NEW-CERT");
}

TEST_F(OnboardingStateTest, MissingFileReturnsNone) { EXPECT_FALSE(onboarding::load_state(path_)); }

TEST_F(OnboardingStateTest, CorruptFileThrows) {
  std::ofstream out(path_.c_str(), std::ios::binary);
  out << "this is not json";
  out.close();
  try {
    onboarding::load_state(path_);
    FAIL() << "expected onboarding_error";
  } catch (const onboarding::onboarding_error &e) {
    EXPECT_FALSE(e.retryable());
  }
}

TEST_F(OnboardingStateTest, IncompleteFileThrows) {
  std::ofstream out(path_.c_str(), std::ios::binary);
  out << "{\"version\": 1, \"cert_pem\": \"CERT\"}";
  out.close();
  EXPECT_THROW(onboarding::load_state(path_), onboarding::onboarding_error);
}

TEST_F(OnboardingStateTest, UnsupportedVersionThrows) {
  std::ofstream out(path_.c_str(), std::ios::binary);
  out << "{\"version\": 99}";
  out.close();
  EXPECT_THROW(onboarding::load_state(path_), onboarding::onboarding_error);
}

#ifndef WIN32
TEST_F(OnboardingStateTest, StateFileIsOwnerOnly) {
  onboarding::save_state(test_state(), path_);
  const fs::perms permissions = fs::status(path_).permissions();
  EXPECT_EQ(permissions & fs::perms_mask, fs::owner_read | fs::owner_write);
}
#endif
