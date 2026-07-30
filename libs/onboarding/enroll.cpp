// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/asio/ip/host_name.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <net/http/client.hpp>
#include <onboarding/onboarding.hpp>
#include <random>
#include <str/xtos.hpp>
#include <thread>

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

// Backoff for retryable failures: honor the server's Retry-After when given,
// otherwise exponential (2s, 4s, 8s, ...), always with jitter so a fleet
// enrolling at the same time does not hammer the server in lockstep.
unsigned long backoff_ms(const unsigned int attempt, const boost::optional<unsigned long> &retry_after_seconds) {
  if (retry_after_seconds) {
    return *retry_after_seconds * 1000UL + jitter_ms();
  }
  return (1UL << attempt) * 1000UL + jitter_ms();
}

boost::optional<unsigned long> get_retry_after(const http::response &response) {
  const auto it = response.headers_.find("retry-after");
  if (it == response.headers_.end()) {
    return boost::none;
  }
  try {
    return static_cast<unsigned long>(std::stoul(it->second));
  } catch (...) {
    // HTTP-date form (or garbage): fall back to exponential backoff.
    return boost::none;
  }
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

std::string get_required_string(const json::object &object, const char *key) {
  const json::value *value = object.if_contains(key);
  if (value == nullptr || !value->is_string() || value->as_string().empty()) {
    throw onboarding::onboarding_error(std::string("Enrollment response is missing ") + key, false);
  }
  return std::string(value->as_string().c_str());
}

std::string get_optional_string(const json::object &object, const char *key, const std::string &fallback) {
  const json::value *value = object.if_contains(key);
  if (value == nullptr || !value->is_string() || value->as_string().empty()) {
    return fallback;
  }
  return std::string(value->as_string().c_str());
}

http::response default_post(const onboarding::enrollment_request &request, const std::string &url, const std::string &payload) {
  const http::parsed_url parsed = http::parse_url(url);
  const std::string verify = request.verify_mode.empty() ? (request.ca.empty() ? "none" : "certificate") : request.verify_mode;
  const http::http_client_options options(parsed.protocol, request.tls_version, verify, request.ca);
  http::request rq("POST", parsed.host, parsed.path);
  rq.add_post_payload("application/json", payload);
  rq.add_header("Accept", "application/json");
  http::simple_client client(options);
  return client.fetch(parsed.host, parsed.port, rq);
}

}  // namespace

onboarding::enrolled_identity onboarding::parse_enroll_response(const std::string &body, const identity &id, const std::string &fallback_server_url) {
  json::object root;
  try {
    root = json::parse(body).as_object();
  } catch (const std::exception &e) {
    throw onboarding_error(std::string("Failed to parse enrollment response: ") + e.what(), false);
  }
  enrolled_identity result;
  result.private_key_pem = id.private_key_pem;
  result.cert_pem = get_required_string(root, "cert_pem");
  result.ca_pem = get_required_string(root, "ca_pem");
  result.bundle_signing_pub_pem = get_required_string(root, "bundle_signing_pub_pem");
  result.mtls_url = get_required_string(root, "mtls_url");
  result.mtls_server_cert_pem = get_required_string(root, "mtls_server_cert_pem");
  result.server_url = get_optional_string(root, "server_url", fallback_server_url);
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
