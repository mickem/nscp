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
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32
#include <fcntl.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstdlib>
#include <functional>
#endif

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

TEST(OnboardingIdentity, PrivateKeyIsAnUnencryptedPkcs8Pem) {
  // The agent has to be able to read this back unattended at boot, so it must
  // not be passphrase protected - and it must be a private key, not a keypair
  // dump that would also ship the public half around.
  const onboarding::identity id = onboarding::generate_identity();
  EXPECT_NE(id.private_key_pem.find("-----BEGIN PRIVATE KEY-----"), std::string::npos);
  EXPECT_EQ(id.private_key_pem.find("ENCRYPTED"), std::string::npos);
  EXPECT_EQ(id.private_key_pem.find("Proc-Type"), std::string::npos);
  EXPECT_NE(id.csr_pem.find("-----BEGIN CERTIFICATE REQUEST-----"), std::string::npos);
}

TEST(OnboardingIdentity, CsrIsAVersionOneRequestWithOnlyACommonName) {
  const onboarding::identity id = onboarding::generate_identity("web-01");
  const x509_req_ptr req = parse_csr(id.csr_pem);
  ASSERT_TRUE(req);
  EXPECT_EQ(X509_REQ_get_version(req.get()), 0L) << "PKCS#10 v1";
  const X509_NAME *name = X509_REQ_get_subject_name(req.get());
  EXPECT_EQ(X509_NAME_entry_count(name), 1) << "the server derives the real identity from the token, not from the subject";
}

TEST(OnboardingIdentity, AnUnusableCommonNameFailsCleanly) {
  // X.520 caps commonName at 64 characters. Whether this OpenSSL accepts a
  // longer one or not, the outcome must be an onboarding_error or a CSR with
  // the name intact - never a silently truncated subject or a raw OpenSSL
  // exception escaping the library.
  const std::string long_cn(300, 'n');
  try {
    const onboarding::identity id = onboarding::generate_identity(long_cn);
    const x509_req_ptr req = parse_csr(id.csr_pem);
    ASSERT_TRUE(req);
    std::vector<char> buffer(long_cn.size() + 64, 0);
    ASSERT_GT(X509_NAME_get_text_by_NID(X509_REQ_get_subject_name(req.get()), NID_commonName, buffer.data(), static_cast<int>(buffer.size())), 0);
    EXPECT_EQ(std::string(buffer.data()), long_cn);
  } catch (const onboarding::onboarding_error &e) {
    EXPECT_FALSE(e.retryable());
  }
}

TEST(OnboardingIdentity, AnEmptyCommonNameDoesNotCrash) {
  try {
    const onboarding::identity id = onboarding::generate_identity("");
    EXPECT_FALSE(id.csr_pem.empty());
    EXPECT_TRUE(parse_csr(id.csr_pem));
  } catch (const onboarding::onboarding_error &e) {
    EXPECT_FALSE(e.retryable());
  }
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

// --- enrollment: hostile / awkward server behaviour --------------------------

TEST(OnboardingEnrollHostile, RetryAfterIsBoundedSoTheInstallerCannotHang) {
  // Enrollment runs in an installer: honouring an hour long Retry-After would
  // look like a hang. Five minutes is the cap.
  std::vector<unsigned long> sleeps;
  int calls = 0;
  onboarding::enroll(
      test_request(), test_identity(),
      [&](const std::string &, const std::string &) {
        if (++calls == 1) {
          http::response response = make_response(429, "slow down");
          response.add_header("Retry-After", "100000");
          return response;
        }
        return make_response(200, ok_body);
      },
      [&](const unsigned long ms) { sleeps.push_back(ms); });
  ASSERT_EQ(sleeps.size(), 1u);
  EXPECT_GE(sleeps[0], 300u * 1000u);
  EXPECT_LT(sleeps[0], 301u * 1000u) << "Retry-After was not clamped";
}

TEST(OnboardingEnrollHostile, UnusableRetryAfterFallsBackToExponentialBackoff) {
  for (const char *header : {"Wed, 21 Oct 2026 07:28:00 GMT", "soon", "", "-5", "1e9", "9999999999999999999999"}) {
    std::vector<unsigned long> sleeps;
    int calls = 0;
    onboarding::enroll(
        test_request(), test_identity(),
        [&](const std::string &, const std::string &) {
          if (++calls == 1) {
            http::response response = make_response(429, "slow down");
            response.add_header("Retry-After", header);
            return response;
          }
          return make_response(200, ok_body);
        },
        [&](const unsigned long ms) { sleeps.push_back(ms); });
    ASSERT_EQ(sleeps.size(), 1u) << header;
    // First retry: 2s of exponential backoff plus up to 1s of jitter, and in
    // no case an hour derived from a number we could not parse.
    EXPECT_GE(sleeps[0], 2000u) << header;
    EXPECT_LT(sleeps[0], 3000u) << header;
  }
}

TEST(OnboardingEnrollHostile, BackoffStaysBoundedForALargeRetryCount) {
  // The exponent used to be unbounded: 1UL << attempt is undefined behaviour
  // once attempt reaches the width of the type, and the sleeps before that are
  // already absurd (2^20 seconds is twelve days).
  std::vector<unsigned long> sleeps;
  onboarding::enrollment_request request = test_request();
  request.max_attempts = 40;
  EXPECT_THROW(onboarding::enroll(
                   request, test_identity(), [&](const std::string &, const std::string &) { return make_response(503, "nope"); },
                   [&](const unsigned long ms) { sleeps.push_back(ms); }),
               onboarding::onboarding_error);
  ASSERT_EQ(sleeps.size(), 39u);
  for (const unsigned long sleep : sleeps) {
    EXPECT_GE(sleep, 2000u);
    EXPECT_LT(sleep, 65u * 1000u) << "backoff exceeded the 64s cap";
  }
  EXPECT_LT(sleeps[0], sleeps[3]) << "backoff must still grow before it caps";
}

TEST(OnboardingEnrollHostile, TransportlessResponsesAreRetryable) {
  // status 0 is what the client reports when it never got a response.
  int calls = 0;
  EXPECT_THROW(onboarding::enroll(
                   test_request(), test_identity(),
                   [&](const std::string &, const std::string &) {
                     ++calls;
                     return make_response(0, "");
                   },
                   no_sleep),
               onboarding::onboarding_error);
  EXPECT_EQ(calls, 3);
}

TEST(OnboardingEnrollHostile, RedirectsAndOtherOddStatusesAreFatal) {
  for (const unsigned int status : {301u, 302u, 304u, 402u, 404u, 405u, 409u, 418u, 451u}) {
    int calls = 0;
    EXPECT_THROW(onboarding::enroll(
                     test_request(), test_identity(),
                     [&](const std::string &, const std::string &) {
                       ++calls;
                       return make_response(status, "no");
                     },
                     no_sleep),
                 onboarding::onboarding_error)
        << status;
    EXPECT_EQ(calls, 1) << "status " << status << " must not be retried";
  }
}

TEST(OnboardingEnrollHostile, EveryFiveHundredIsRetriedAndFourOhOneIsNot) {
  for (const unsigned int status : {500u, 502u, 503u, 504u, 429u}) {
    int calls = 0;
    EXPECT_THROW(onboarding::enroll(
                     test_request(), test_identity(),
                     [&](const std::string &, const std::string &) {
                       ++calls;
                       return make_response(status, "no");
                     },
                     no_sleep),
                 onboarding::onboarding_error);
    EXPECT_EQ(calls, 3) << "status " << status << " should have been retried";
  }
  for (const unsigned int status : {401u, 403u}) {
    int calls = 0;
    EXPECT_THROW(onboarding::enroll(
                     test_request(), test_identity(),
                     [&](const std::string &, const std::string &) {
                       ++calls;
                       return make_response(status, "no");
                     },
                     no_sleep),
                 onboarding::onboarding_error);
    EXPECT_EQ(calls, 1) << "a burned token must never be retried (status " << status << ")";
  }
}

TEST(OnboardingEnrollHostile, ABrokenSuccessBodyIsFatalAndNotRetried) {
  // 2xx with a body we cannot use is a server bug, not a transient failure:
  // retrying burns the one-time token for nothing.
  for (const std::string body : {std::string(""), std::string("not json"), std::string("[]"), std::string("{}"),
                                 std::string("{\"cert_pem\": \"C\"}"), std::string("{\"cert_pem\": \"\", \"ca_pem\": \"CA\"}")}) {
    int calls = 0;
    try {
      onboarding::enroll(
          test_request(), test_identity(),
          [&](const std::string &, const std::string &) {
            ++calls;
            return make_response(200, body);
          },
          no_sleep);
      FAIL() << "accepted " << body;
    } catch (const onboarding::onboarding_error &e) {
      EXPECT_FALSE(e.retryable()) << body;
    }
    EXPECT_EQ(calls, 1) << body;
  }
}

TEST(OnboardingEnrollHostile, ANonJsonSuccessBodyIsNotEchoedRaw) {
  // The failure message ends up in logs and on the console; a body full of
  // control characters must not go there verbatim.
  try {
    onboarding::enroll(
        test_request(), test_identity(),
        [&](const std::string &, const std::string &) { return make_response(500, "line1\r\nline2\nline3" + std::string(500, 'x')); }, no_sleep);
    FAIL() << "expected onboarding_error";
  } catch (const onboarding::onboarding_error &e) {
    const std::string message = e.what();
    EXPECT_EQ(message.find('\n'), std::string::npos) << message;
    EXPECT_EQ(message.find('\r'), std::string::npos) << message;
    EXPECT_LT(message.size(), 400u) << "the body snippet is not bounded";
  }
}

TEST(OnboardingEnrollHostile, SuccessOnTheFirstAttemptNeverSleeps) {
  std::vector<unsigned long> sleeps;
  onboarding::enroll(
      test_request(), test_identity(), [&](const std::string &, const std::string &) { return make_response(200, ok_body); },
      [&](const unsigned long ms) { sleeps.push_back(ms); });
  EXPECT_TRUE(sleeps.empty());
}

TEST(OnboardingEnrollHostile, ZeroRetriesStillMeansOneAttempt) {
  int calls = 0;
  onboarding::enrollment_request request = test_request();
  request.max_attempts = 0;
  const onboarding::enrolled_identity result = onboarding::enroll(
      request, test_identity(),
      [&](const std::string &, const std::string &) {
        ++calls;
        return make_response(200, ok_body);
      },
      no_sleep);
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(result.cert_pem, "CERT");
}

TEST(OnboardingEnrollHostile, TheTokenIsJsonEncodedNotConcatenated) {
  std::string payload;
  onboarding::enrollment_request request = test_request();
  request.bootstrap_token = "tok\", \"admin\": true, \"x\": \"\n\\ é";
  request.hostname = "host\"\n";
  onboarding::enroll(
      request, test_identity(),
      [&](const std::string &, const std::string &body) {
        payload = body;
        return make_response(200, ok_body);
      },
      no_sleep);
  json::object sent;
  ASSERT_NO_THROW(sent = json::parse(payload).as_object()) << payload;
  EXPECT_EQ(sent.at("bootstrap_token").as_string(), request.bootstrap_token);
  EXPECT_EQ(sent.at("hostname").as_string(), request.hostname);
  EXPECT_EQ(sent.if_contains("admin"), nullptr) << "the token escaped its string";
}

TEST(OnboardingEnrollHostile, HostnameAndOsAreFilledInWhenNotGiven) {
  std::string payload;
  onboarding::enrollment_request request = test_request();
  request.hostname = "";
  request.os = "";
  onboarding::enroll(
      request, test_identity(),
      [&](const std::string &, const std::string &body) {
        payload = body;
        return make_response(200, ok_body);
      },
      no_sleep);
  const json::object sent = json::parse(payload).as_object();
  ASSERT_NE(sent.if_contains("os"), nullptr);
  EXPECT_FALSE(sent.at("os").as_string().empty());
#if defined(_WIN32)
  EXPECT_EQ(sent.at("os").as_string(), "windows");
#elif defined(__APPLE__)
  EXPECT_EQ(sent.at("os").as_string(), "macos");
#else
  EXPECT_EQ(sent.at("os").as_string(), "linux");
#endif
}

TEST(OnboardingEnrollHostile, TheEnrollUrlIsBuiltFromTheServerUrlWithoutSurprises) {
  const std::pair<std::string, std::string> cases[] = {
      {"https://fleet.example.com", "https://fleet.example.com/enroll/v1"},
      {"https://fleet.example.com/", "https://fleet.example.com/enroll/v1"},
      {"https://fleet.example.com///", "https://fleet.example.com/enroll/v1"},
      {"https://fleet.example.com:8443", "https://fleet.example.com:8443/enroll/v1"},
      {"https://fleet.example.com/base", "https://fleet.example.com/base/enroll/v1"},
      {"http://fleet.example.com", "http://fleet.example.com/enroll/v1"},
  };
  for (const auto &item : cases) {
    std::string seen;
    onboarding::enrollment_request request = test_request();
    request.server_url = item.first;
    onboarding::enroll(
        request, test_identity(),
        [&](const std::string &url, const std::string &) {
          seen = url;
          return make_response(200, ok_body);
        },
        no_sleep);
    EXPECT_EQ(seen, item.second);
  }
}

TEST(OnboardingEnrollHostile, RefusesToTalkToSomethingThatIsNotAUrl) {
  for (const std::string url : {std::string(""), std::string("fleet.example.com"), std::string("//fleet.example.com"), std::string("not a url"),
                                std::string("/enroll")}) {
    int calls = 0;
    onboarding::enrollment_request request = test_request();
    request.server_url = url;
    EXPECT_THROW(onboarding::enroll(
                     request, test_identity(),
                     [&](const std::string &, const std::string &) {
                       ++calls;
                       return make_response(200, ok_body);
                     },
                     no_sleep),
                 onboarding::onboarding_error)
        << url;
    EXPECT_EQ(calls, 0) << "contacted " << url;
  }
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

// --- adopt_owner ------------------------------------------------------------
// The handoff that makes a sudo enrollment readable by an unprivileged service.
// The chown itself only happens as root, so the portable cases below pin the
// contract everyone else relies on: it must never fail the enrollment, and it
// must never touch anything when there is nothing to hand over.

TEST_F(OnboardingStateTest, AdoptOwnerSucceedsWhenTheReferenceDoesNotExist) {
  onboarding::save_state(test_state(), path_);
  std::string error;
  // A from-source install that never created a state directory: leaving
  // ownership alone is the right answer, and it is not an enrollment failure.
  EXPECT_TRUE(onboarding::adopt_owner(path_, (dir_ / "no-such-directory").string(), error));
  EXPECT_TRUE(error.empty());
}

TEST_F(OnboardingStateTest, AdoptOwnerLeavesTheStateFileReadable) {
  onboarding::save_state(test_state(), path_);
  std::string error;
  EXPECT_TRUE(onboarding::adopt_owner(path_, dir_.string(), error)) << error;
  // Whatever it did with ownership, the identity must still load - and on POSIX
  // it must still be 0600, since the file holds the private key.
  EXPECT_TRUE(static_cast<bool>(onboarding::load_state(path_)));
#ifndef WIN32
  EXPECT_EQ(fs::status(path_).permissions() & fs::perms_mask, fs::owner_read | fs::owner_write);
#endif
}

#ifndef WIN32
TEST_F(OnboardingStateTest, AdoptOwnerIsANoOpForAnUnprivilegedProcess) {
  if (::geteuid() == 0) {
    GTEST_SKIP() << "running as root: this covers the non-root contract";
  }
  onboarding::save_state(test_state(), path_);
  struct stat file_stat = {};
  ASSERT_EQ(::stat(path_.c_str(), &file_stat), 0);
  std::string error;
  // Cannot chown as a normal user, so this must report success rather than
  // failing an enrollment that otherwise worked.
  EXPECT_TRUE(onboarding::adopt_owner(path_, "/", error)) << error;
  struct stat after = {};
  ASSERT_EQ(::stat(path_.c_str(), &after), 0);
  EXPECT_EQ(after.st_uid, file_stat.st_uid);
}

TEST_F(OnboardingStateTest, AdoptOwnerGivesTheFileToTheReferenceOwner) {
  if (::geteuid() != 0) {
    GTEST_SKIP() << "needs root to hand a file to another account";
  }
  // Model the packaged layout: ${data-path} owned by the service account, with
  // the identity written by root inside it. dir_ plays ${data-path} (the
  // reference); path_ is the state file within it, exactly as the real
  // ${data-path}/security/agent-state.json sits under ${data-path}.
  const fs::path reference = dir_;
  const uid_t service_uid = 12345;
  const gid_t service_gid = 12345;
  ASSERT_EQ(::chown(reference.string().c_str(), service_uid, service_gid), 0);
  onboarding::save_state(test_state(), path_);

  std::string error;
  ASSERT_TRUE(onboarding::adopt_owner(path_, reference.string(), error)) << error;

  struct stat after = {};
  ASSERT_EQ(::stat(path_.c_str(), &after), 0);
  EXPECT_EQ(after.st_uid, service_uid);
  EXPECT_EQ(after.st_gid, service_gid);
  // Still secret, still loadable.
  EXPECT_EQ(fs::status(path_).permissions() & fs::perms_mask, fs::owner_read | fs::owner_write);
  EXPECT_TRUE(static_cast<bool>(onboarding::load_state(path_)));
}

TEST_F(OnboardingStateTest, AdoptOwnerRecursesIntoADirectory) {
  if (::geteuid() != 0) {
    GTEST_SKIP() << "needs root to hand a directory to another account";
  }
  // The fleet managed directory (${data-path}/fleet): the sync rewrites
  // everything under it. dir_ is ${data-path} (the reference); managed is fleet
  // within it.
  const fs::path reference = dir_;
  const fs::path managed = dir_ / "fleet";
  fs::create_directories(managed / "cache");
  std::ofstream(( managed / "fleet.ini").string().c_str()) << "; managed";
  std::ofstream((managed / "cache" / "bundle.zip").string().c_str()) << "zip";
  ASSERT_EQ(::chown(reference.string().c_str(), 12345, 12345), 0);

  std::string error;
  ASSERT_TRUE(onboarding::adopt_owner(managed.string(), reference.string(), error)) << error;

  for (const char *relative : {"", "fleet.ini", "cache", "cache/bundle.zip"}) {
    struct stat entry = {};
    const std::string target = relative[0] == '\0' ? managed.string() : (managed / relative).string();
    ASSERT_EQ(::stat(target.c_str(), &entry), 0) << target;
    EXPECT_EQ(entry.st_uid, 12345u) << target;
  }
}

// The tree adopt_owner walks is owned by the unprivileged service account, so
// its contents are attacker-controlled in the only threat model that matters
// here: an agent whose service account has been compromised. Running as root
// over paths that account can replace is the classic setup for
// chown-follows-symlink, so these pin that it does not.

TEST_F(OnboardingStateTest, AdoptOwnerRefusesASymlinkedTarget) {
  if (::geteuid() != 0) {
    GTEST_SKIP() << "the chown only happens as root, so there is nothing to refuse otherwise";
  }
  // `rm -rf fleet && ln -s /etc fleet` as the service account. Following this
  // would hand the link's target to nsclient:nsclient.
  const fs::path reference = dir_;
  const fs::path victim = dir_ / "victim";
  const fs::path managed = dir_ / "fleet";
  fs::create_directories(victim);
  std::ofstream((victim / "shadow").string().c_str()) << "root:x:";
  ASSERT_EQ(::chown(reference.string().c_str(), 12345, 12345), 0);
  fs::create_directory_symlink(victim, managed);

  std::string error;
  EXPECT_FALSE(onboarding::adopt_owner(managed.string(), reference.string(), error));
  EXPECT_FALSE(error.empty());

  struct stat after = {};
  ASSERT_EQ(::stat((victim / "shadow").string().c_str(), &after), 0);
  EXPECT_EQ(after.st_uid, 0u) << "ownership was changed through a symlink";
}

TEST_F(OnboardingStateTest, AdoptOwnerDoesNotFollowASymlinkInsideTheTree) {
  if (::geteuid() != 0) {
    GTEST_SKIP() << "needs root to hand a directory to another account";
  }
  // `ln -s /etc/shadow ${fleet-folder}/x`, the escalation this exists to stop.
  // The real file must keep its owner; the link itself may be retargeted, which
  // is harmless.
  const fs::path reference = dir_;
  const fs::path managed = dir_ / "fleet";
  const fs::path outside = dir_ / "outside.txt";
  fs::create_directories(managed);
  std::ofstream(outside.string().c_str()) << "not ours";
  std::ofstream((managed / "fleet.ini").string().c_str()) << "; managed";
  ASSERT_EQ(::chown(reference.string().c_str(), 12345, 12345), 0);
  fs::create_symlink(outside, managed / "x");

  std::string error;
  ASSERT_TRUE(onboarding::adopt_owner(managed.string(), reference.string(), error)) << error;

  struct stat victim = {};
  ASSERT_EQ(::stat(outside.string().c_str(), &victim), 0);
  EXPECT_EQ(victim.st_uid, 0u) << "the symlink was followed out of the tree";
  // The rest of the handoff still happened.
  struct stat managed_file = {};
  ASSERT_EQ(::stat((managed / "fleet.ini").string().c_str(), &managed_file), 0);
  EXPECT_EQ(managed_file.st_uid, 12345u);
}

TEST_F(OnboardingStateTest, AdoptOwnerLeavesHardlinkedFilesAlone) {
  if (::geteuid() != 0) {
    GTEST_SKIP() << "needs root to hand a directory to another account";
  }
  // A hardlink is a second name for one file, so chowning it changes the owner
  // everywhere it is named - a symlink check alone would not catch this.
  // Nothing we write is ever hardlinked, so an extra link means someone else
  // made it.
  const fs::path reference = dir_;
  const fs::path managed = dir_ / "fleet";
  const fs::path outside = dir_ / "outside.txt";
  fs::create_directories(managed);
  std::ofstream(outside.string().c_str()) << "not ours";
  ASSERT_EQ(::chown(reference.string().c_str(), 12345, 12345), 0);
  boost::system::error_code ec;
  fs::create_hard_link(outside, managed / "x", ec);
  if (ec) {
    GTEST_SKIP() << "could not create a hardlink: " << ec.message();
  }

  std::string error;
  ASSERT_TRUE(onboarding::adopt_owner(managed.string(), reference.string(), error)) << error;

  struct stat victim = {};
  ASSERT_EQ(::stat(outside.string().c_str(), &victim), 0);
  EXPECT_EQ(victim.st_uid, 0u) << "a hardlinked file outside the tree changed owner";
}

TEST_F(OnboardingStateTest, AdoptOwnerDoesNotDescendThroughASymlinkedIntermediate) {
  if (::geteuid() != 0) {
    GTEST_SKIP() << "needs root to hand a file to another account";
  }
  // The variant the earlier lchown-by-path fix missed: not the target and not a
  // leaf, but an *intermediate* directory swapped for a symlink. Target is
  // ${data-path}/security/agent-state.json; the service account has replaced
  // `security` with a symlink to a directory it does not own. Resolving the
  // path by string would chown the victim's agent-state.json; descending with
  // openat(O_NOFOLLOW) refuses at the symlinked component instead.
  const fs::path reference = dir_;
  const fs::path victim = dir_ / "victim";
  fs::create_directories(victim);
  std::ofstream((victim / "agent-state.json").string().c_str()) << "root-owned";
  ASSERT_EQ(::chown(reference.string().c_str(), 12345, 12345), 0);
  fs::create_directory_symlink(victim, dir_ / "security");

  std::string error;
  const fs::path target = dir_ / "security" / "agent-state.json";
  EXPECT_FALSE(onboarding::adopt_owner(target.string(), reference.string(), error));
  EXPECT_FALSE(error.empty());

  struct stat after = {};
  ASSERT_EQ(::stat((victim / "agent-state.json").string().c_str(), &after), 0);
  EXPECT_EQ(after.st_uid, 0u) << "ownership was changed through a symlinked intermediate directory";
}

TEST_F(OnboardingStateTest, AdoptOwnerRefusesATargetOutsideTheReference) {
  if (::geteuid() != 0) {
    GTEST_SKIP() << "the anchor check runs only on the root chown path";
  }
  // The anchor only makes a path safe if the path is actually under it. A target
  // elsewhere is refused rather than resolved without the anchor's protection.
  ASSERT_EQ(::chown(dir_.string().c_str(), 12345, 12345), 0);  // a non-root reference, so we reach the check
  const fs::path outside = fs::temp_directory_path() / fs::unique_path("nscp-elsewhere-%%%%");
  fs::create_directories(outside);
  std::ofstream((outside / "x").string().c_str()) << "x";
  std::string error;
  EXPECT_FALSE(onboarding::adopt_owner((outside / "x").string(), dir_.string(), error));
  EXPECT_FALSE(error.empty());
  fs::remove_all(outside);
}
#endif

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

TEST_F(OnboardingStateTest, SavingOverAWorldReadableFileTightensThePermissions) {
  // Writing through a temp file + rename is what makes this true: the mode of
  // the file we replace must not survive, or a key can end up world readable.
  {
    std::ofstream out(path_.c_str(), std::ios::binary);
    out << "{}";
  }
  fs::permissions(path_, fs::owner_all | fs::group_read | fs::others_read);
  onboarding::save_state(test_state(), path_);
  EXPECT_EQ(fs::status(path_).permissions() & fs::perms_mask, fs::owner_read | fs::owner_write);
}
#endif

// The state file IS this host's identity. Anything we cannot fully understand
// has to be a hard error: a half-loaded identity would have the agent hand
// OpenSSL an empty key or call an empty url, and "not enrolled" (none) would
// silently disable fleet sync instead of telling the operator.

TEST_F(OnboardingStateTest, EmptyIdentityFieldsAreRejected) {
  for (const char *field : {"private_key_pem", "cert_pem", "ca_pem", "bundle_signing_pub_pem", "mtls_url", "mtls_server_cert_pem"}) {
    json::object root = json::parse(json::serialize(json::object())).as_object();
    root["version"] = 1;
    root["private_key_pem"] = "KEY";
    root["cert_pem"] = "CERT";
    root["ca_pem"] = "CA";
    root["bundle_signing_pub_pem"] = "BUNDLE-KEY";
    root["server_url"] = "https://api.example.com";
    root["mtls_url"] = "https://mtls.example.com";
    root["mtls_server_cert_pem"] = "MTLS-CERT";
    root[field] = "";
    std::ofstream out(path_.c_str(), std::ios::binary);
    out << json::serialize(root);
    out.close();
    EXPECT_THROW(onboarding::load_state(path_), onboarding::onboarding_error) << "accepted an empty " << field;
  }
}

TEST_F(OnboardingStateTest, AnEmptyPublicApiUrlIsAllowed) {
  // server_url is only informational and enrollment may not have learned one.
  onboarding::enrolled_identity state = test_state();
  state.server_url = "";
  onboarding::save_state(state, path_);
  const boost::optional<onboarding::enrolled_identity> loaded = onboarding::load_state(path_);
  ASSERT_TRUE(loaded);
  EXPECT_TRUE(loaded->server_url.empty());
}

TEST_F(OnboardingStateTest, MalformedFilesThrowInsteadOfLookingUnenrolled) {
  const std::string bodies[] = {
      "",                                    // empty file
      "[]",                                  // array root
      "null",                                // null root
      "\"a string\"",                        // string root
      "42",                                  // number root
      "{\"version\": 1}",                    // no material at all
      "{\"version\": \"1\", ...}",            // version as a string
      "{\"version\": 1.0}",                  // version as a double
      "{\"version\": 2}",                    // a future version we cannot read
      "{\"private_key_pem\": \"K\"}",        // no version
      "{\"version\": 1, \"private_key_pem\": 5, \"cert_pem\": \"C\"}",  // wrong type
      "{\"version\": 1, \"private_key_pem\": null}",
      "{\"version\": 1, \"private_key_pem\": {\"pem\": \"K\"}}",
  };
  for (const std::string &body : bodies) {
    std::ofstream out(path_.c_str(), std::ios::binary);
    out << body;
    out.close();
    EXPECT_THROW(onboarding::load_state(path_), onboarding::onboarding_error) << "accepted " << body;
  }
}

TEST_F(OnboardingStateTest, TruncatedFileThrows) {
  const std::string full = [&] {
    onboarding::save_state(test_state(), path_);
    std::ifstream in(path_.c_str(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }();
  ASSERT_FALSE(full.empty());
  std::ofstream out(path_.c_str(), std::ios::binary | std::ios::trunc);
  out << full.substr(0, full.size() / 2);
  out.close();
  EXPECT_THROW(onboarding::load_state(path_), onboarding::onboarding_error);
}

TEST_F(OnboardingStateTest, ExtraFieldsAreIgnored) {
  // Forward compatibility: a newer server writing more fields (at the same
  // version) must not brick an older agent.
  onboarding::save_state(test_state(), path_);
  std::ifstream in(path_.c_str(), std::ios::binary);
  json::object root = json::parse(std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>())).as_object();
  in.close();
  root["future_field"] = "whatever";
  std::ofstream out(path_.c_str(), std::ios::binary | std::ios::trunc);
  out << json::serialize(root);
  out.close();
  const boost::optional<onboarding::enrolled_identity> loaded = onboarding::load_state(path_);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->cert_pem, "CERT");
}

TEST_F(OnboardingStateTest, RealPemMaterialSurvivesTheRoundTrip) {
  onboarding::enrolled_identity state = test_state();
  state.private_key_pem = onboarding::generate_identity().private_key_pem;
  state.cert_pem = "-----BEGIN CERTIFICATE-----\r\nMIIB\r\n-----END CERTIFICATE-----\r\n";
  state.ca_pem = "-----BEGIN CERTIFICATE-----\n\n\nMIIB+/=\n-----END CERTIFICATE-----\n";
  state.mtls_url = "https://mtls.example.com:8443/tenant/ä-1";
  onboarding::save_state(state, path_);
  const boost::optional<onboarding::enrolled_identity> loaded = onboarding::load_state(path_);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->private_key_pem, state.private_key_pem);
  EXPECT_EQ(loaded->cert_pem, state.cert_pem) << "CRLF inside a PEM must not be rewritten";
  EXPECT_EQ(loaded->ca_pem, state.ca_pem);
  EXPECT_EQ(loaded->mtls_url, state.mtls_url);
}

TEST_F(OnboardingStateTest, StringsAreNotTruncatedAtAnEmbeddedNul) {
  // as_string().c_str() would cut here, which is how a hostile value can look
  // harmless after validation.
  json::object root;
  root["version"] = 1;
  root["private_key_pem"] = json::string_view("K\0EY", 4);
  root["cert_pem"] = "CERT";
  root["ca_pem"] = "CA";
  root["bundle_signing_pub_pem"] = "BUNDLE-KEY";
  root["server_url"] = "https://api.example.com";
  root["mtls_url"] = "https://mtls.example.com";
  root["mtls_server_cert_pem"] = "MTLS-CERT";
  std::ofstream out(path_.c_str(), std::ios::binary);
  out << json::serialize(root);
  out.close();
  const boost::optional<onboarding::enrolled_identity> loaded = onboarding::load_state(path_);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->private_key_pem.size(), 4u);
}

TEST_F(OnboardingStateTest, LoadingADirectoryThrows) { EXPECT_THROW(onboarding::load_state(dir_.string()), onboarding::onboarding_error); }

TEST_F(OnboardingStateTest, SavingWhereWeCannotWriteThrowsAndLeavesNothingBehind) {
  const std::string missing = (dir_ / "no-such-dir" / "agent-state.json").string();
  EXPECT_THROW(onboarding::save_state(test_state(), missing), onboarding::onboarding_error);
  EXPECT_FALSE(fs::exists(missing));
  EXPECT_FALSE(fs::exists(missing + ".tmp"));
}

TEST_F(OnboardingStateTest, AFailedSaveKeepsThePreviousIdentity) {
  onboarding::save_state(test_state(), path_);
  // A directory where the temp file wants to be: the write fails, the existing
  // (working) identity must still be loadable.
  fs::create_directories(path_ + ".tmp");
  EXPECT_THROW(onboarding::save_state(test_state(), path_), onboarding::onboarding_error);
  fs::remove_all(path_ + ".tmp");
  const boost::optional<onboarding::enrolled_identity> loaded = onboarding::load_state(path_);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->cert_pem, "CERT");
}

TEST_F(OnboardingStateTest, SavingOntoADirectoryThrowsAndRemovesTheTempFile) {
  // The temp file writes fine; the rename onto the target cannot work. The
  // failure must surface as an onboarding_error and must not strand the temp
  // file (which holds the private key) next to it.
  fs::create_directories(path_);
  EXPECT_THROW(onboarding::save_state(test_state(), path_), onboarding::onboarding_error);
  EXPECT_FALSE(fs::exists(path_ + ".tmp"));
}

#ifndef WIN32
TEST_F(OnboardingStateTest, AWriteErrorRemovesTheTempFileAndThrows) {
  // Model a full disk with RLIMIT_FSIZE: the open succeeds, the write cannot
  // complete. A partial state file is worse than none (it holds half a key),
  // so the temp file has to go and the caller has to hear about it.
  struct rlimit old_limit = {};
  ASSERT_EQ(::getrlimit(RLIMIT_FSIZE, &old_limit), 0);
  // Exceeding the limit raises SIGXFSZ (default: kill); ignore it so write()
  // fails with EFBIG instead.
  struct sigaction ignore_action = {};
  ignore_action.sa_handler = SIG_IGN;
  struct sigaction old_action = {};
  ASSERT_EQ(::sigaction(SIGXFSZ, &ignore_action, &old_action), 0);
  struct rlimit tiny = old_limit;
  tiny.rlim_cur = 4;
  ASSERT_EQ(::setrlimit(RLIMIT_FSIZE, &tiny), 0);

  try {
    onboarding::save_state(test_state(), path_);
    ::setrlimit(RLIMIT_FSIZE, &old_limit);
    ::sigaction(SIGXFSZ, &old_action, nullptr);
    FAIL() << "expected onboarding_error";
  } catch (const onboarding::onboarding_error &) {
    ::setrlimit(RLIMIT_FSIZE, &old_limit);
    ::sigaction(SIGXFSZ, &old_action, nullptr);
  }
  EXPECT_FALSE(fs::exists(path_ + ".tmp")) << "a torn temp file was left behind";
  EXPECT_FALSE(fs::exists(path_));
}

TEST_F(OnboardingStateTest, AnUnreadableStateFileThrows) {
  if (::geteuid() == 0) {
    GTEST_SKIP() << "root can read anything, so there is no unreadable file to test";
  }
  onboarding::save_state(test_state(), path_);
  fs::permissions(path_, fs::no_perms);
  // The file exists, so "not enrolled" (none) would be a lie; the operator has
  // to hear that the identity is there but cannot be read.
  EXPECT_THROW(onboarding::load_state(path_), onboarding::onboarding_error);
  fs::permissions(path_, fs::owner_read | fs::owner_write);
}

// --- adopt_owner under a fake root ------------------------------------------
// The chown handoff itself only runs as root (geteuid() == 0), which the tests
// above skip. A user namespace gives us that root: fork a child, map the
// current user to uid 0 in a fresh namespace, and every rule of the handoff
// becomes testable - the anchor check, the O_NOFOLLOW descent, the symlink
// refusals and the recursive chown (to the only ids the namespace maps, which
// is exactly what CAP_CHOWN in a user namespace allows).

namespace {

// Exit code meaning "this kernel/sandbox does not allow user namespaces";
// tests skip rather than fail on it.
constexpr int kFakeRootUnavailable = 101;

bool write_proc_file(const char *proc_path, const std::string &content) {
  const int fd = ::open(proc_path, O_WRONLY | O_CLOEXEC);
  if (fd < 0) return false;
  const ssize_t written = ::write(fd, content.c_str(), content.size());
  ::close(fd);
  return written == static_cast<ssize_t>(content.size());
}

// Run `body` in a forked child inside a new user namespace where the current
// user is uid 0 and the current group is gid `in_ns_gid`. Returns the child's
// exit code (0 = the body was happy), kFakeRootUnavailable when namespaces are
// not permitted, -1 when the child died abnormally.
int run_as_fake_root(const gid_t in_ns_gid, const std::function<int()> &body) {
  const uid_t uid = ::getuid();
  const gid_t gid = ::getgid();
  const pid_t pid = ::fork();
  if (pid < 0) return -1;
  if (pid == 0) {
    if (::unshare(CLONE_NEWUSER) != 0) ::_exit(kFakeRootUnavailable);
    if (!write_proc_file("/proc/self/setgroups", "deny") || !write_proc_file("/proc/self/uid_map", "0 " + std::to_string(uid) + " 1") ||
        !write_proc_file("/proc/self/gid_map", std::to_string(in_ns_gid) + " " + std::to_string(gid) + " 1")) {
      ::_exit(kFakeRootUnavailable);
    }
    // exit() rather than _exit() so gcov data is flushed.
    ::exit(body());
  }
  int status = 0;
  if (::waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) return -1;
  return WEXITSTATUS(status);
}

// Inside the namespace our own files appear as uid 0 - the reference must not
// look root:root (that is the "everything runs as root" early-out), so map the
// group to a non-zero gid.
constexpr gid_t kServiceGid = 5;

}  // namespace

#define SKIP_WITHOUT_FAKE_ROOT(rc)                                               \
  if ((rc) == kFakeRootUnavailable) {                                            \
    GTEST_SKIP() << "user namespaces are not permitted in this environment";     \
  }

TEST_F(OnboardingStateTest, FakeRootHandsOverTheWholeReferenceTree) {
  // target == reference: the whole ${data-path} tree changes hands, recursing
  // into subdirectories, chowning regular files, and skipping symlinks.
  fs::create_directories(dir_ / "fleet" / "cache");
  std::ofstream((dir_ / "fleet" / "fleet.ini").string().c_str()) << "; managed";
  std::ofstream((dir_ / "fleet" / "cache" / "bundle.zip").string().c_str()) << "zip";
  std::ofstream((dir_ / "outside.txt").string().c_str()) << "x";
  fs::create_symlink(dir_ / "outside.txt", dir_ / "fleet" / "link");
  onboarding::save_state(test_state(), path_);

  const int rc = run_as_fake_root(kServiceGid, [&]() -> int {
    std::string error;
    if (!onboarding::adopt_owner(dir_.string(), dir_.string(), error)) return 1;
    if (!error.empty()) return 2;
    return 0;
  });
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
  EXPECT_TRUE(static_cast<bool>(onboarding::load_state(path_)));
}

TEST_F(OnboardingStateTest, FakeRootHandsOverAFileInsideASubdirectory) {
  // The packaged shape: ${data-path}/security/agent-state.json. The descent
  // walks `security` with O_NOFOLLOW and chowns the leaf through its parent's
  // descriptor.
  fs::create_directories(dir_ / "security");
  const std::string target = (dir_ / "security" / "agent-state.json").string();
  onboarding::save_state(test_state(), target);

  const int rc = run_as_fake_root(kServiceGid, [&]() -> int {
    std::string error;
    if (!onboarding::adopt_owner(target, dir_.string(), error)) return 1;
    return 0;
  });
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
}

TEST_F(OnboardingStateTest, FakeRootHandsOverADirectoryTarget) {
  fs::create_directories(dir_ / "fleet");
  std::ofstream((dir_ / "fleet" / "fleet.ini").string().c_str()) << "; managed";

  const int rc = run_as_fake_root(kServiceGid, [&]() -> int {
    std::string error;
    if (!onboarding::adopt_owner((dir_ / "fleet").string(), dir_.string(), error)) return 1;
    return 0;
  });
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
}

TEST_F(OnboardingStateTest, FakeRootRefusesASymlinkedTarget) {
  // `rm -rf fleet && ln -s victim fleet` by the service account: refusing is
  // the whole point of the fstatat(AT_SYMLINK_NOFOLLOW) check.
  fs::create_directories(dir_ / "victim");
  fs::create_directory_symlink(dir_ / "victim", dir_ / "fleet");

  const int rc = run_as_fake_root(kServiceGid, [&]() -> int {
    std::string error;
    if (onboarding::adopt_owner((dir_ / "fleet").string(), dir_.string(), error)) return 1;
    if (error.empty()) return 2;
    return 0;
  });
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
}

TEST_F(OnboardingStateTest, FakeRootRefusesASymlinkedIntermediateDirectory) {
  // The service account swapped an *intermediate* component for a symlink;
  // the openat(O_NOFOLLOW) descent must refuse at that component.
  fs::create_directories(dir_ / "victim");
  std::ofstream((dir_ / "victim" / "agent-state.json").string().c_str()) << "x";
  fs::create_directory_symlink(dir_ / "victim", dir_ / "security");

  const int rc = run_as_fake_root(kServiceGid, [&]() -> int {
    std::string error;
    if (onboarding::adopt_owner((dir_ / "security" / "agent-state.json").string(), dir_.string(), error)) return 1;
    if (error.empty()) return 2;
    return 0;
  });
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
}

TEST_F(OnboardingStateTest, FakeRootRefusesATargetOutsideTheReference) {
  // The anchor only protects paths under it; anything else must be refused
  // rather than resolved unanchored.
  const fs::path outside = fs::temp_directory_path() / fs::unique_path("nscp-elsewhere-%%%%");
  fs::create_directories(outside);
  std::ofstream((outside / "x").string().c_str()) << "x";

  const int rc = run_as_fake_root(kServiceGid, [&]() -> int {
    std::string error;
    if (onboarding::adopt_owner((outside / "x").string(), dir_.string(), error)) return 1;
    if (error.empty()) return 2;
    return 0;
  });
  fs::remove_all(outside);
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
}

TEST_F(OnboardingStateTest, FakeRootTreatsAMissingTargetAsHandedOver) {
  // Nothing at the target means nothing to hand over: success, not an error.
  const int rc = run_as_fake_root(kServiceGid, [&]() -> int {
    std::string error;
    if (!onboarding::adopt_owner((dir_ / "not-there.json").string(), dir_.string(), error)) return 1;
    return 0;
  });
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
}

TEST_F(OnboardingStateTest, FakeRootLeavesOwnershipAloneWhenTheReferenceIsMissing) {
  // stat() on the reference fails: a from-source install without the state
  // directory. Leaving ownership alone is the safe answer even for root.
  onboarding::save_state(test_state(), path_);

  const int rc = run_as_fake_root(kServiceGid, [&]() -> int {
    std::string error;
    if (!onboarding::adopt_owner(path_, (dir_ / "no-such-dir").string(), error)) return 1;
    return 0;
  });
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
}

TEST_F(OnboardingStateTest, FakeRootLeavesARootOwnedReferenceAlone) {
  // Map the group to 0 as well, so the reference looks root:root inside the
  // namespace: an install that genuinely runs everything as root, where there
  // is nothing to hand over.
  onboarding::save_state(test_state(), path_);

  const int rc = run_as_fake_root(0, [&]() -> int {
    std::string error;
    if (!onboarding::adopt_owner(path_, dir_.string(), error)) return 1;
    return 0;
  });
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
}

TEST_F(OnboardingStateTest, FakeRootLeavesAFifoAlone) {
  // Nothing we write is ever a fifo or a device, so one at the target means
  // someone else put it there: not ours to hand over, and not an error.
  ASSERT_EQ(::mkfifo((dir_ / "pipe").string().c_str(), 0600), 0);

  const int rc = run_as_fake_root(kServiceGid, [&]() -> int {
    std::string error;
    if (!onboarding::adopt_owner((dir_ / "pipe").string(), dir_.string(), error)) return 1;
    if (!error.empty()) return 2;
    return 0;
  });
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
}

TEST_F(OnboardingStateTest, FakeRootLeavesASymlinkedReferenceAlone) {
  // The anchor itself cannot be opened O_NOFOLLOW when it is a symlink; with
  // no trusted anchor the handoff declines to touch anything - and that is a
  // success, not a failed enrollment.
  fs::create_directories(dir_ / "real");
  fs::create_directory_symlink(dir_ / "real", dir_ / "alias");

  const int rc = run_as_fake_root(kServiceGid, [&]() -> int {
    std::string error;
    if (!onboarding::adopt_owner((dir_ / "alias" / "x").string(), (dir_ / "alias").string(), error)) return 1;
    return 0;
  });
  SKIP_WITHOUT_FAKE_ROOT(rc);
  EXPECT_EQ(rc, 0);
}
#endif
