// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <map>
#include <onboarding/sync.hpp>
#include <sstream>
#include <str/xtos.hpp>

#include "json_util.hpp"

namespace json = boost::json;

namespace {

// Bounds and character sets for everything the server sends us. This is
// defense in depth: these values end up in request URLs (state_hash), as
// filesystem path components (bundle id) and in OpenSSL calls (sha256), so a
// hostile - or merely broken - server must not be able to smuggle a path
// traversal, an extra request line or an unbounded allocation through them.
const std::size_t max_hash_length = 128;
const std::size_t max_id_length = 128;
const std::size_t max_url_length = 1024;
const std::size_t max_signature_length = 512;
const unsigned long default_poll_seconds = 60;
const unsigned long max_poll_seconds = 86400;  // a day: anything longer is a stuck agent

// Opaque server tokens: hex, base64 and base64url all fit this set.
const char *token_extra_chars = "-._:=+/~";
// A bundle id becomes a file name in the cache directory: no separators, no
// drive letters, and (below) no "..".
const char *id_extra_chars = "-._";
const char *signature_extra_chars = "+/=-_";

bool is_allowed_char(const unsigned char c, const char *extra) {
  if (std::isalnum(c) != 0) return true;
  if (c == 0) return false;
  return std::strchr(extra, static_cast<char>(c)) != nullptr;
}

// Never echo a rejected value back into the message: it is attacker-influenced
// and the message goes to the log (and on to the server as an error report).
onboarding::onboarding_error bad_field(const char *context, const char *key, const std::string &why) {
  return {std::string(context) + " " + key + " " + why, false};
}

std::string require_token(const json::object &object, const char *key, const char *context, const char *extra, const std::size_t max_length) {
  const std::string value = onboarding::detail::require_string(object, key, context);
  if (value.size() > max_length) {
    throw bad_field(context, key, "is too long (" + str::xtos(value.size()) + " > " + str::xtos(max_length) + ")");
  }
  for (const char c : value) {
    if (!is_allowed_char(static_cast<unsigned char>(c), extra)) {
      throw bad_field(context, key, "contains an illegal character");
    }
  }
  return value;
}

// A bundle id is used to build the cache file name, so it must be a single,
// harmless path component.
std::string require_id(const json::object &object, const char *key, const char *context) {
  const std::string value = require_token(object, key, context, id_extra_chars, max_id_length);
  if (value.find("..") != std::string::npos) {
    throw bad_field(context, key, "must not contain '..'");
  }
  if (value == ".") {
    throw bad_field(context, key, "must not be '.'");
  }
  return value;
}

std::string require_sha256_hex(const json::object &object, const char *key, const char *context) {
  const std::string value = onboarding::detail::require_string(object, key, context);
  if (value.size() != 64) {
    throw bad_field(context, key, "must be a 64 character SHA-256 hex digest");
  }
  for (const char c : value) {
    if (std::isxdigit(static_cast<unsigned char>(c)) == 0) {
      throw bad_field(context, key, "must be a 64 character SHA-256 hex digest");
    }
  }
  return value;
}

// The download url is appended to the pinned mtls base url, so it must be a
// server-relative path with nothing in it that could break the request line
// (CR/LF, spaces) or cut it short (#).
std::string require_relative_url(const json::object &object, const char *key, const char *context) {
  const std::string value = onboarding::detail::require_string(object, key, context);
  if (value.size() > max_url_length) {
    throw bad_field(context, key, "is too long (" + str::xtos(value.size()) + " > " + str::xtos(max_url_length) + ")");
  }
  if (value[0] != '/') {
    throw bad_field(context, key, "must be a server-relative path starting with '/'");
  }
  for (const char c : value) {
    const auto u = static_cast<unsigned char>(c);
    if (u <= 0x20 || u >= 0x7f || std::strchr("\\\"<>^{}|`#", static_cast<char>(u)) != nullptr) {
      throw bad_field(context, key, "contains an illegal character");
    }
  }
  return value;
}

json::object parse_object(const std::string &body, const char *context) {
  try {
    return json::parse(body).as_object();
  } catch (const std::exception &e) {
    throw onboarding::onboarding_error(std::string("Failed to parse ") + context + ": " + e.what(), false);
  }
}

long long optional_int(const json::object &object, const char *key, const long long fallback) {
  const json::value *value = object.if_contains(key);
  if (value == nullptr || !value->is_number()) {
    return fallback;
  }
  return value->to_number<long long>();
}

// A poll interval we are willing to act on: nonsense (zero, negative) falls
// back to the default, and an absurdly large value is capped so a bad server
// response cannot park the agent forever (or overflow the sleep computation).
unsigned long clamp_poll_seconds(const long long seconds) {
  if (seconds <= 0) return default_poll_seconds;
  return static_cast<unsigned long>(std::min<long long>(seconds, static_cast<long long>(max_poll_seconds)));
}

// Anything that ends up on an INI line must not be able to start a new one:
// a newline in a key or value would let managed configuration inject whole
// sections (a module, an external command) that the server never sent.
void reject_control_chars(const std::string &value, const char *what) {
  for (const char c : value) {
    const auto u = static_cast<unsigned char>(c);
    if ((u < 0x20 && u != '\t') || u == 0x7f) {
      throw onboarding::onboarding_error(std::string("Managed configuration ") + what + " contains a control character", false);
    }
  }
}

// A scalar (or array element) rendered as an INI value.
std::string format_ini_value(const json::value &value) {
  if (value.is_string()) {
    return onboarding::detail::to_string(value.as_string());
  }
  if (value.is_bool()) {
    return value.as_bool() ? "true" : "false";
  }
  if (value.is_double()) {
    // boost.json serializes doubles in scientific notation (0.5 -> "5E-1",
    // 100.0 -> "1E2"); nothing that reads these files parses that. Render the
    // plain decimal form used everywhere else in NSCP.
    return str::xtos_non_sci(value.as_double());
  }
  return json::serialize(value);
}

typedef std::map<std::string, std::map<std::string, std::string>> ini_sections;

void collect_ini(const json::object &object, const std::string &path, ini_sections &sections) {
  for (const auto &member : object) {
    const std::string key(member.key());
    reject_control_chars(key, "key");
    const json::value &value = member.value();
    if (value.is_object()) {
      collect_ini(value.as_object(), path + "/" + key, sections);
    } else if (value.is_null()) {
      // Nulls are merge-patch deletion markers; a merged config should not
      // contain any, but skipping is safer than rendering the string "null".
    } else if (value.is_array()) {
      std::string joined;
      for (const json::value &element : value.as_array()) {
        if (!joined.empty()) joined += ",";
        joined += format_ini_value(element);
      }
      reject_control_chars(joined, "value");
      sections[path.empty() ? "/" : path][key] = joined;
    } else {
      const std::string rendered = format_ini_value(value);
      reject_control_chars(rendered, "value");
      sections[path.empty() ? "/" : path][key] = rendered;
    }
  }
}

}  // namespace

onboarding::desired_state onboarding::parse_desired_state(const std::string &body) {
  const json::object root = parse_object(body, "desired state");
  desired_state result;
  // The hash is sent back to the server in a query string, so it may not carry
  // anything that could alter the request.
  result.state_hash = require_token(root, "state_hash", "Desired state", token_extra_chars, max_hash_length);
  result.next_poll_in_seconds = clamp_poll_seconds(optional_int(root, "next_poll_in_seconds", static_cast<long long>(default_poll_seconds)));
  const json::value *merged = root.if_contains("merged_config_json");
  if (merged != nullptr && !merged->is_null() && !merged->is_object()) {
    throw onboarding_error("Desired state merged_config_json is not an object", false);
  }
  result.merged_config_json = (merged != nullptr && merged->is_object()) ? json::serialize(*merged) : "{}";
  const json::value *bundles = root.if_contains("bundles");
  if (bundles != nullptr && !bundles->is_null()) {
    // A malformed bundle list must fail the cycle: silently treating it as
    // "no bundles" would apply an empty configuration and delete every script.
    if (!bundles->is_array()) {
      throw onboarding_error("Desired state bundles is not an array", false);
    }
    for (const json::value &entry : bundles->as_array()) {
      if (!entry.is_object()) {
        throw onboarding_error("Desired state bundle entry is not an object", false);
      }
      const json::object &b = entry.as_object();
      bundle_info info;
      // The id becomes a cache file name and the url is appended to the pinned
      // base url: both are validated, not merely copied.
      info.id = require_id(b, "id", "Bundle");
      info.sha256 = require_sha256_hex(b, "sha256", "Bundle");
      info.signature = require_token(b, "signature", "Bundle", signature_extra_chars, max_signature_length);
      info.url = require_relative_url(b, "url", "Bundle");
      info.name = detail::optional_string(b, "name", "");
      info.version = detail::optional_string(b, "version", "");
      info.priority = optional_int(b, "priority", 0);
      result.bundles.push_back(info);
    }
  }
  // Apply order is ascending priority; stable so the server's order breaks ties.
  std::stable_sort(result.bundles.begin(), result.bundles.end(), [](const bundle_info &a, const bundle_info &b) { return a.priority < b.priority; });
  return result;
}

boost::optional<unsigned long> onboarding::parse_next_poll(const std::string &body) {
  try {
    const json::object root = json::parse(body).as_object();
    const json::value *value = root.if_contains("next_poll_in_seconds");
    if (value == nullptr || !value->is_number()) {
      return boost::none;
    }
    const long long seconds = value->to_number<long long>();
    if (seconds <= 0) {
      // Nonsense (or "poll immediately"): keep the interval we already have
      // rather than spinning on the server.
      return boost::none;
    }
    return static_cast<unsigned long>(std::min<long long>(seconds, static_cast<long long>(max_poll_seconds)));
  } catch (...) {
    return boost::none;
  }
}

boost::json::value onboarding::json_merge_patch(const boost::json::value &target, const boost::json::value &patch) {
  // RFC 7396: a non-object patch replaces the target wholesale.
  if (!patch.is_object()) {
    return patch;
  }
  json::object result = target.is_object() ? target.as_object() : json::object();
  for (const auto &member : patch.as_object()) {
    if (member.value().is_null()) {
      result.erase(member.key());
    } else {
      const json::value *existing = result.if_contains(member.key());
      result[member.key()] = json_merge_patch(existing != nullptr ? *existing : json::value(), member.value());
    }
  }
  return result;
}

std::string onboarding::render_ini(const boost::json::value &config) {
  if (!config.is_object()) {
    throw onboarding_error("Managed configuration must be a JSON object", false);
  }
  ini_sections sections;
  collect_ini(config.as_object(), "", sections);
  std::ostringstream out;
  out << "; Managed by the fleet sync - DO NOT EDIT, changes are overwritten on the next sync.\n";
  for (const ini_sections::value_type &section : sections) {
    out << "\n[" << section.first << "]\n";
    for (const auto &entry : section.second) {
      out << entry.first << "=" << entry.second << "\n";
    }
  }
  return out.str();
}

onboarding::transport_error_info onboarding::classify_transport_error(const std::string &message) {
  std::string lower = message;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  const auto contains_any = [&lower](std::initializer_list<const char *> needles) {
    for (const char *needle : needles) {
      if (lower.find(needle) != std::string::npos) return true;
    }
    return false;
  };

  transport_error_info info;
  // TLS alerts are wrapped in "Failed to connect to host:port: <alert>" by the
  // HTTP client, so certificate patterns must be checked before generic
  // connection patterns.
  if (contains_any({"certificate required", "bad certificate", "unknown ca", "unknown_ca", "certificate unknown", "certificate revoked", "access denied"})) {
    info.kind = transport_error_kind::tls_identity;
    info.advice =
        "The fleet server did not accept this host's client certificate. "
        "If the server was reinstalled or this host's certificate was revoked, re-enroll with a new install command (nscp enroll ... --force).";
  } else if (contains_any({"certificate verify failed", "self-signed", "self signed", "certificate has expired"})) {
    info.kind = transport_error_kind::tls_server_trust;
    info.advice =
        "The fleet server's certificate does not match the one pinned at enrollment. "
        "If the server's certificate was rotated outside a renewal, re-enroll with a new install command (nscp enroll ... --force).";
  } else if (contains_any({"handshake", "ssl routines", "tls"})) {
    info.kind = transport_error_kind::tls_other;
    info.advice = "TLS negotiation with the fleet server failed; verify the server's mTLS endpoint and TLS version settings.";
  } else if (contains_any({"resolve", "refused", "timed out", "timeout", "host not found", "connection reset", "no route", "unreachable"})) {
    info.kind = transport_error_kind::network;
    info.advice = "The fleet server is unreachable; will keep retrying with backoff.";
  }
  return info;
}

std::string onboarding::build_state_report(const boost::optional<std::string> &applied_state_hash, const std::vector<installed_bundle> &bundles_installed,
                                           const std::vector<std::string> &errors, const std::map<std::string, std::string> &reported_tags) {
  json::object root;
  if (applied_state_hash) {
    root["applied_state_hash"] = *applied_state_hash;
  }
  json::array bundles;
  for (const installed_bundle &bundle : bundles_installed) {
    json::object entry;
    entry["id"] = bundle.id;
    entry["version"] = bundle.version;
    bundles.push_back(entry);
  }
  root["bundles_installed"] = bundles;
  json::array error_list;
  for (const std::string &error : errors) {
    error_list.push_back(json::value(error));
  }
  root["errors"] = error_list;
  json::object tags;
  for (const auto &tag : reported_tags) {
    tags[tag.first] = tag.second;
  }
  root["reported_tags"] = tags;
  return json::serialize(root);
}

std::string onboarding::build_metrics(const std::vector<metric_sample> &samples) {
  json::array list;
  for (const metric_sample &sample : samples) {
    // A non-finite gauge (a counter that divided by zero, an uninitialized
    // sensor) has no JSON representation: boost.json turns NaN into null and
    // infinity into 1e99999, either of which makes the server reject the whole
    // batch. Drop the bad sample instead of losing every good one with it.
    if (!std::isfinite(sample.value)) {
      continue;
    }
    json::object entry;
    entry["key"] = sample.key;
    entry["value"] = sample.value;
    if (sample.ts) {
      entry["ts"] = *sample.ts;
    }
    list.push_back(entry);
  }
  json::object root;
  root["samples"] = list;
  return json::serialize(root);
}

onboarding::enrolled_identity onboarding::parse_renew_response(const std::string &body, const identity &fresh_identity, const enrolled_identity &current) {
  const json::object root = parse_object(body, "renewal response");
  enrolled_identity result = current;
  result.private_key_pem = fresh_identity.private_key_pem;
  result.cert_pem = detail::require_string(root, "cert_pem", "Renewal response");
  result.ca_pem = detail::require_string(root, "ca_pem", "Renewal response");
  result.bundle_signing_pub_pem = detail::require_string(root, "bundle_signing_pub_pem", "Renewal response");
  result.mtls_server_cert_pem = detail::require_string(root, "mtls_server_cert_pem", "Renewal response");
  return result;
}
