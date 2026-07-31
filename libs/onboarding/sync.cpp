// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <map>
#include <onboarding/sync.hpp>
#include <sstream>

#include "json_util.hpp"

namespace json = boost::json;

namespace {

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

// A scalar (or array element) rendered as an INI value.
std::string format_ini_value(const json::value &value) {
  if (value.is_string()) {
    return std::string(value.as_string().c_str());
  }
  if (value.is_bool()) {
    return value.as_bool() ? "true" : "false";
  }
  return json::serialize(value);
}

typedef std::map<std::string, std::map<std::string, std::string>> ini_sections;

void collect_ini(const json::object &object, const std::string &path, ini_sections &sections) {
  for (const auto &member : object) {
    const std::string key(member.key());
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
      sections[path.empty() ? "/" : path][key] = joined;
    } else {
      sections[path.empty() ? "/" : path][key] = format_ini_value(value);
    }
  }
}

}  // namespace

onboarding::desired_state onboarding::parse_desired_state(const std::string &body) {
  const json::object root = parse_object(body, "desired state");
  desired_state result;
  result.state_hash = detail::require_string(root, "state_hash", "Desired state");
  result.next_poll_in_seconds = static_cast<unsigned long>(optional_int(root, "next_poll_in_seconds", 60));
  const json::value *merged = root.if_contains("merged_config_json");
  result.merged_config_json = merged != nullptr ? json::serialize(*merged) : "{}";
  const json::value *bundles = root.if_contains("bundles");
  if (bundles != nullptr && bundles->is_array()) {
    for (const json::value &entry : bundles->as_array()) {
      if (!entry.is_object()) {
        throw onboarding_error("Desired state bundle entry is not an object", false);
      }
      const json::object &b = entry.as_object();
      bundle_info info;
      info.id = detail::require_string(b, "id", "Bundle");
      info.sha256 = detail::require_string(b, "sha256", "Bundle");
      info.signature = detail::require_string(b, "signature", "Bundle");
      info.url = detail::require_string(b, "url", "Bundle");
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
    if (seconds < 0) {
      return boost::none;
    }
    return static_cast<unsigned long>(seconds);
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
