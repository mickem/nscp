// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ElasticClient.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <bytes/base64.hpp>
#include <cmath>
#include <cstdint>
#include <limits>
#include <net/http/client.hpp>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <nsclient/logger/logger_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <str/format.hpp>
#include <str/utils.hpp>

#include "elastic_bulk.hpp"

namespace json = boost::json;
namespace sh = nscapi::settings_helper;

/**
 * Default c-tor
 * @return
 */
ElasticClient::ElasticClient() : started(false), timeout(30) {}

/**
 * Default d-tor
 * @return
 */
ElasticClient::~ElasticClient() {}

bool ElasticClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  try {
    std::string events;

    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias("elastic", alias, "client");

    settings.alias()
        .add_key_to_settings()
        .add_string("hostname", sh::string_key(&hostname_, "auto"), "HOSTNAME",
                    "The host name of the monitored computer.\nSet this to auto (default) to use the windows name of the computer.\n\n"
                    "auto\tHostname\n"
                    "${host}\tHostname\n"
                    "${host_lc}\tHostname in lowercase\n"
                    "${host_uc}\tHostname in uppercase\n"
                    "${domain}\tDomainname\n"
                    "${domain_lc}\tDomainname in lowercase\n"
                    "${domain_uc}\tDomainname in uppercase\n"
                    "${address_ipv4}\tIPv4 address of the computer\n"
                    "${address_ipv6}\tIPv6 address of the computer (lowercase, compressed)\n"
                    "${address_ipv6_lc}\tIPv6 address in lowercase (compressed)\n"
                    "${address_ipv6_uc}\tIPv6 address in uppercase (compressed)\n"
                    "${address_ipv6_lc_comp}\tIPv6 address in lowercase, compressed (2001:db8::7)\n"
                    "${address_ipv6_lc_uncomp}\tIPv6 address in lowercase, uncompressed (2001:0db8:0000:0000:0000:0000:0000:0007)\n"
                    "${address_ipv6_uc_comp}\tIPv6 address in uppercase, compressed\n"
                    "${address_ipv6_uc_uncomp}\tIPv6 address in uppercase, uncompressed\n")

        .add_string("events", sh::string_key(&events, "eventlog:*,logfile:*"), "Event", "The events to subscribe to such as eventlog:* or logfile:mylog.")

        .add_string("address", sh::string_key(&address), "Elastic address", "The address to send data to (http://127.0.0.1:9200/_bulk).")

        .add_string("user", sh::string_key(&user, ""), "Elastic user",
                    "The username used to authenticate against Elasticsearch (basic authentication). Leave empty to send no credentials.")
        .add_password("password", sh::string_key(&password, ""), "Elastic password",
                      "The password used to authenticate against Elasticsearch (basic authentication).")
        .add_password("api key", sh::string_key(&api_key, ""), "Elastic API key",
                      "An Elasticsearch API key (the base64 encoded id:key value as returned when the key is created), sent as 'Authorization: ApiKey ...'. "
                      "Takes precedence over user/password when both are set.")

        .add_string("tls version", sh::string_key(&tls_version, "1.2+"), "TLS version",
                    "The TLS version to use when connecting over https (1.0, 1.1, 1.2, 1.2+ or 1.3).")
        .add_string("verify mode", sh::string_key(&verify_mode, "peer"), "TLS verify mode",
                    "How to verify the Elasticsearch server certificate when connecting over https. 'peer' (the default) validates the certificate chain and "
                    "hostname against the configured CA. Set to 'none' to disable verification - this is insecure and lets an on-path attacker read the "
                    "submitted data and any configured credentials.")
        .add_string("ca", sh::path_key(&ca, "${ca-path}"), "Certificate authority",
                    "The certificate authority bundle used to verify the Elasticsearch server certificate (used when 'verify mode' is not 'none').")

        .add_int("timeout", sh::int_key(&timeout, 30), "Timeout",
                 "Timeout (in seconds) for each connect, read and write when talking to Elasticsearch. 0 waits forever.")

        .add_string("event index", sh::string_key(&event_index, "nsclient_event-%(date)"), "Elastic index used for events",
                    "The elastic index to use for events (log messages).")
        .add_string("event type", sh::string_key(&event_type, ""), "Elastic type used for events",
                    "The elastic type to use for events (log messages). Only set this for Elasticsearch 6.x or older: mapping types were removed in "
                    "Elasticsearch 8, which rejects requests that carry a type.")

        .add_string("metrics index", sh::string_key(&metrics_index, "nsclient_metrics-%(date)"), "Elastic index used for metrics",
                    "The elastic index to use for metrics.")
        .add_string("metrics type", sh::string_key(&metrics_type, ""), "Elastic type used for metrics",
                    "The elastic type to use for metrics. Only set this for Elasticsearch 6.x or older: mapping types were removed in Elasticsearch 8, "
                    "which rejects requests that carry a type.")

        .add_string("nsclient log index", sh::string_key(&nsclient_index, "nsclient_log-%(date)"), "Elastic index used for the nsclient log",
                    "The elastic index to use for the NSClient++ log.")
        .add_string("nsclient log type", sh::string_key(&nsclient_type, ""), "Elastic type used for the nsclient log",
                    "The elastic type to use for the NSClient++ log. Only set this for Elasticsearch 6.x or older: mapping types were removed in "
                    "Elasticsearch 8, which rejects requests that carry a type.")

        ;

    settings.register_all();
    settings.notify();

    if (timeout < 0) {
      NSC_LOG_ERROR("Invalid elastic timeout (negative): " + str::xtos(timeout) + ", using 30 seconds");
      timeout = 30;
    }

    hostname_ = socket_helpers::expand_hostname(hostname_);

    nscapi::core_helper ch(get_core(), get_id());
    ch.register_event(events);

    if (mode == NSCAPI::normalStart || mode == NSCAPI::reloadStart) {
      started = true;
    }

  } catch (nsclient::nsclient_exception &e) {
    NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
    return false;
  } catch (std::exception &e) {
    NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("NSClient API exception: ");
    return false;
  }
  return true;
}

/**
 * Unload (terminate) module.
 * Stop accepting events so nothing is sent during/after shutdown.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool ElasticClient::unloadModule() {
  started = false;
  return true;
}

void ElasticClient::send_to_elastic(const std::string &index, const std::string &type, const std::vector<std::string> &payloads, bool log_errors) const {
  if (payloads.empty()) {
    return;
  }
  const std::string payload = elastic_bulk::build_payload(index, type, payloads);

  if (log_errors) {
    NSC_TRACE_ENABLED() { NSC_TRACE_MSG(payload); }
  }

  try {
    const http::parsed_url parsed = http::parse_url(address);
    if (parsed.host.empty()) {
      if (log_errors) {
        NSC_LOG_ERROR("Invalid elastic address: " + address);
      }
      return;
    }
    http::http_client_options opts(parsed.protocol, tls_version, verify_mode, ca);
    opts.timeout_seconds_ = static_cast<unsigned int>(timeout < 0 ? 0 : timeout);
    http::request rq("POST", parsed.host, parsed.path, payload);
    rq.add_header("Content-Type", "application/x-ndjson");
    rq.add_header("Content-Length", str::xtos(payload.size()));
    // Never log the Authorization header value: the API key is a bearer
    // credential and the basic form is base64(user:password), trivially
    // reversible.
    if (!api_key.empty()) {
      rq.add_header("Authorization", "ApiKey " + api_key);
    } else if (!user.empty()) {
      rq.add_header("Authorization", "Basic " + bytes::base64_encode(user + ":" + password));
    }
    http::simple_client c(opts);
    const http::response r = c.fetch(parsed.host, parsed.port, rq);

    if (log_errors) {
      NSC_TRACE_ENABLED() {
        NSC_TRACE_MSG("code: " + str::xtos(r.status_code_));
        for (const http::response::header_type::value_type &v : r.headers_) {
          NSC_TRACE_MSG(v.first + " = " + v.second);
        }
        NSC_TRACE_MSG(r.payload_);
      }
    }

    if (!r.is_2xx()) {
      if (log_errors) {
        std::string details = elastic_bulk::extract_errors(r.payload_);
        if (details.empty()) {
          details = r.payload_.substr(0, 200);
        }
        NSC_LOG_ERROR("Failed to send to elastic (HTTP " + str::xtos(r.status_code_) + "): " + details);
      }
      return;
    }
    const std::string errors = elastic_bulk::extract_errors(r.payload_);
    if (!errors.empty() && log_errors) {
      NSC_LOG_ERROR("Failed to send to elastic: " + errors);
    }
  } catch (const socket_helpers::socket_exception &e) {
    if (log_errors) {
      NSC_LOG_ERROR("Failed to send to elastic (connection error): " + e.reason());
    }
  } catch (const std::exception &e) {
    if (log_errors) {
      NSC_LOG_ERROR_EXR("Failed to send to elastic: ", e);
    }
  }
}

void ElasticClient::onEvent(const PB::Commands::EventMessage &request, const std::string &buffer) {
  if (!started || address.empty()) {
    return;
  }
  const std::string now = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time());

  std::vector<std::string> payloads;
  for (const ::PB::Commands::EventMessage::Request &line : request.payload()) {
    std::string time = now;
    json::object node;
    for (const PB::Common::KeyValue &e : line.data()) {
      if (e.key() == "written_str") {
        time = e.value();
      } else if (e.key() != "xml" && e.key() != "written") {
        node[e.key()] = e.value();
      }
    }
    node["@timestamp"] = time;
    node["hostname"] = hostname_;
    payloads.push_back(json::serialize(node));
  }
  send_to_elastic(event_index, event_type, payloads, true);
}

namespace {
json::value gauge_to_json(double v) {
  if (std::trunc(v) == v && v >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
      v <= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
    return json::value(static_cast<std::int64_t>(v));
  }
  return json::value(v);
}
}  // namespace

void build_metrics(json::object &metrics, const std::string trail, const PB::Metrics::MetricsBundle &b) {
  json::object node;
  for (const PB::Metrics::MetricsBundle &b2 : b.children()) {
    build_metrics(node, trail + boost::replace_all_copy(b.alias(), ".", "_"), b2);
  }
  for (const PB::Metrics::Metric &v : b.value()) {
    std::string key = trail.empty() ? boost::replace_all_copy(v.key(), ".", "_") : trail + "_" + boost::replace_all_copy(v.key(), ".", "_");
    if (v.has_gauge_value())
      node.insert(json::object::value_type(key, gauge_to_json(v.gauge_value().value())));
    else if (v.has_string_value())
      node.insert(json::object::value_type(key, v.string_value().value()));
  }
  metrics.insert(json::object::value_type(b.key(), node));
}
void ElasticClient::submitMetrics(const PB::Metrics::MetricsMessage &response) {
  if (!started || address.empty()) {
    return;
  }
  json::object metrics;
  metrics["@timestamp"] = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time());
  metrics["hostname"] = hostname_;
  for (const PB::Metrics::MetricsMessage::Response &p : response.payload()) {
    for (const PB::Metrics::MetricsBundle &b : p.bundles()) {
      build_metrics(metrics, "", b);
    }
  }

  std::vector<std::string> payloads;
  payloads.push_back(json::serialize(metrics));
  send_to_elastic(metrics_index, metrics_type, payloads, true);
}

void ElasticClient::handleLogMessage(const PB::Log::LogEntry::Entry &message) {
  if (!started || address.empty()) {
    return;
  }

  json::object node;
  node["sender"] = message.sender();
  node["message"] = message.message();
  node["file"] = message.file();
  node["line"] = message.line();
  node["level"] = nsclient::logging::logger_helper::render_log_level_long(message.level());
  node["@timestamp"] = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time());
  node["hostname"] = hostname_;

  // Never log errors for our own log messages: a failing send would otherwise
  // log an error, which is forwarded here again, forever.
  bool log = message.sender() != "elastic";
  std::vector<std::string> payloads;
  payloads.push_back(json::serialize(node));
  send_to_elastic(nsclient_index, nsclient_type, payloads, log);
}
