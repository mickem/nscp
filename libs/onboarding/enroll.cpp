// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithm>
#include <boost/asio/ip/host_name.hpp>
#include <boost/json.hpp>
#include <cctype>
#include <chrono>
#include <net/http/client.hpp>
#include <onboarding/onboarding.hpp>
#include <random>
#include <str/xtos.hpp>
#include <thread>

#include "json_util.hpp"

namespace json = boost::json;

namespace {

const char *enroll_path = "/enroll/v1";

std::string default_os() {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#else
  return "linux";
#endif
}

std::string default_hostname() {
  boost::system::error_code ec;
  const std::string name = boost::asio::ip::host_name(ec);
  return ec ? std::string() : name;
}

unsigned long jitter_ms() {
  std::random_device rd;
  return rd() % 1000;
}

// Enrollment is interactive (installer / command line), so however patient the
// server asks us to be, one wait is bounded to five minutes.
const unsigned long max_retry_after_seconds = 300;

// Backoff for retryable failures: honor the server's Retry-After when given,
// otherwise exponential (2s, 4s, 8s, ...) capped at 64s, always with jitter so
// a fleet enrolling at the same time does not hammer the server in lockstep.
// The cap keeps the shift well-defined (and the wait sane) for large --retries.
unsigned long backoff_ms(const unsigned int attempt, const boost::optional<unsigned long> &retry_after_seconds) {
  if (retry_after_seconds) {
    return std::min(*retry_after_seconds, max_retry_after_seconds) * 1000UL + jitter_ms();
  }
  const unsigned int max_exponent = 6;
  return (1UL << std::min(attempt, max_exponent)) * 1000UL + jitter_ms();
}

boost::optional<unsigned long> get_retry_after(const http::response &response) {
  const auto it = response.headers_.find("retry-after");
  if (it == response.headers_.end()) {
    return boost::none;
  }
  // HTTP-date form (or garbage): none, and the caller falls back to
  // exponential backoff.
  return onboarding::parse_retry_after(it->second);
}

std::string build_enroll_url(const std::string &server_url) {
  std::string base = server_url;
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  return base + enroll_path;
}

std::string build_enroll_body(const onboarding::enrollment_request &request, const onboarding::identity &id) {
  json::object body;
  body["bootstrap_token"] = request.bootstrap_token;
  body["csr_pem"] = id.csr_pem;
  const std::string hostname = request.hostname.empty() ? default_hostname() : request.hostname;
  if (!hostname.empty()) {
    body["hostname"] = hostname;
  }
  const std::string os = request.os.empty() ? default_os() : request.os;
  if (!os.empty()) {
    body["os"] = os;
  }
  return json::serialize(body);
}

// A short single-line sample of the response body for error messages.
std::string error_snippet(const std::string &payload) {
  std::string snippet = payload;
  for (char &c : snippet) {
    if (c == '\r' || c == '\n') c = ' ';
  }
  const std::size_t max_snippet = 200;
  if (snippet.size() > max_snippet) {
    snippet.resize(max_snippet);
  }
  return snippet;
}

http::response default_post(const onboarding::enrollment_request &request, const std::string &url, const std::string &payload) {
  const http::parsed_url parsed = http::parse_url(url);
  // Verify by default. Enrollment is the one exchange that establishes who the
  // fleet server is: the response supplies mtls_server_cert_pem (the pin every
  // later call validates against) and bundle_signing_pub_pem (the key that
  // authorises executable bundles), and the request carries the bootstrap
  // token. Falling back to "none" whenever no CA happened to be supplied meant
  // an on-path attacker could hand over both trust anchors and keep the agent
  // permanently - after which pinning and bundle signature checks still pass,
  // against the attacker's material. Callers that genuinely want an unverified
  // enrollment now have to ask for it explicitly (nscp enroll --insecure).
  const std::string verify = request.verify_mode.empty() ? "certificate" : request.verify_mode;
  const http::http_client_options options(parsed.protocol, request.tls_version, verify, request.ca);
  http::request rq("POST", parsed.host, parsed.path);
  rq.add_post_payload("application/json", payload);
  rq.add_header("Accept", "application/json");
  http::simple_client client(options);
  return client.fetch(parsed.host, parsed.port, rq);
}

}  // namespace

boost::optional<unsigned long> onboarding::parse_retry_after(const std::string &header_value) {
  const std::size_t first = header_value.find_first_not_of(" \t");
  if (first == std::string::npos) return boost::none;
  const std::size_t last = header_value.find_last_not_of(" \t");
  const std::string digits = header_value.substr(first, last - first + 1);
  // Nine digits is already 31 years; anything longer is a bug, not a hint.
  if (digits.size() > 9) return boost::none;
  for (const char c : digits) {
    if (std::isdigit(static_cast<unsigned char>(c)) == 0) return boost::none;
  }
  return static_cast<unsigned long>(std::stoul(digits));
}

onboarding::enrolled_identity onboarding::parse_enroll_response(const std::string &body, const identity &id, const std::string &fallback_server_url) {
  json::object root;
  try {
    root = json::parse(body).as_object();
  } catch (const std::exception &e) {
    throw onboarding_error(std::string("Failed to parse enrollment response: ") + e.what(), false);
  }
  enrolled_identity result;
  result.private_key_pem = id.private_key_pem;
  result.cert_pem = detail::require_string(root, "cert_pem", "Enrollment response");
  result.ca_pem = detail::require_string(root, "ca_pem", "Enrollment response");
  result.bundle_signing_pub_pem = detail::require_string(root, "bundle_signing_pub_pem", "Enrollment response");
  result.mtls_url = detail::require_string(root, "mtls_url", "Enrollment response");
  result.mtls_server_cert_pem = detail::require_string(root, "mtls_server_cert_pem", "Enrollment response");
  result.server_url = detail::optional_string(root, "server_url", fallback_server_url);
  return result;
}

onboarding::enrolled_identity onboarding::enroll(const enrollment_request &request, const identity &id, const post_function &post,
                                                 const sleep_function &sleep_ms) {
  if (request.server_url.empty() || http::parse_url(request.server_url).protocol.empty()) {
    throw onboarding_error("Invalid server url: " + request.server_url, false);
  }
  if (request.bootstrap_token.empty()) {
    throw onboarding_error("No bootstrap token given: generate an install command on the fleet server and pass its token", false);
  }
  const std::string url = build_enroll_url(request.server_url);
  const std::string body = build_enroll_body(request, id);
  const unsigned int attempts = request.max_attempts == 0 ? 1 : request.max_attempts;

  for (unsigned int attempt = 1;; ++attempt) {
    http::response response;
    std::string transient_error;
    boost::optional<unsigned long> retry_after;
    try {
      response = post(url, body);
    } catch (const std::exception &e) {
      transient_error = "Failed to contact " + url + ": " + e.what();
    }
    if (transient_error.empty()) {
      if (response.is_2xx()) {
        return parse_enroll_response(response.payload_, id, request.server_url);
      }
      if (response.status_code_ == 401 || response.status_code_ == 403) {
        // The nonce is burned server-side on first use so retrying can never
        // succeed: the user has to generate a new install command.
        throw onboarding_error("Enrollment rejected (" + str::xtos(response.status_code_) +
                                   "): the bootstrap token is invalid, expired or already used - generate a new install command on the fleet server",
                               false);
      }
      if (response.status_code_ == 429 || response.status_code_ >= 500 || response.status_code_ == 0) {
        transient_error = "Enrollment failed (" + str::xtos(response.status_code_) + "): " + error_snippet(response.payload_);
        retry_after = get_retry_after(response);
      } else {
        throw onboarding_error("Enrollment failed (" + str::xtos(response.status_code_) + "): " + error_snippet(response.payload_), false);
      }
    }
    if (attempt >= attempts) {
      throw onboarding_error(transient_error, true);
    }
    sleep_ms(backoff_ms(attempt, retry_after));
  }
}

onboarding::enrolled_identity onboarding::enroll(const enrollment_request &request) {
  const identity id = generate_identity();
  return enroll(request, id, [&request](const std::string &url, const std::string &payload) { return default_post(request, url, payload); },
                [](const unsigned long milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); });
}
