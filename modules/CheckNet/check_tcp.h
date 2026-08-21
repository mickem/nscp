// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <cctype>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

#include "check_ssh_internal.hpp"

namespace check_net {

// A named service preset: default port, an optional payload to send, a regular
// expression the peer's greeting/response must match, and whether the transport
// is wrapped in TLS.
struct service_preset {
  const char *name;
  unsigned short port;
  const char *send;
  const char *expect_regex;
  bool tls;
};

// Look up a service preset by name (case-insensitive). Returns nullptr for an
// unknown service.
inline const service_preset *find_service_preset(const std::string &name) {
  // SSH/FTP/SMTP/POP/IMAP all send a greeting on connect, so no payload is
  // sent — we just read and match the greeting. The S-prefixed variants are the
  // implicit-TLS ports (POP3S/IMAPS/SMTPS).
  static const service_preset presets[] = {
      {"FTP", 21, "", "^220", false},   {"POP", 110, "", "^\\+OK", false},  {"IMAP", 143, "", "^\\* OK", false},
      {"SMTP", 25, "", "^220", false},  {"SSH", 22, "", "^SSH-", false},    {"SPOP", 995, "", "^\\+OK", true},
      {"SIMAP", 993, "", "^\\* OK", true}, {"SSMTP", 465, "", "^220", true},
  };
  std::string upper = name;
  for (char &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  for (const service_preset &p : presets)
    if (upper == p.name) return &p;
  return nullptr;
}

namespace check_tcp_filter {

struct filter_obj {
  std::string host;
  long long port;
  long long time;
  std::string result;
  std::string response;
  bool connected;

  // TLS peer certificate, populated only when the connection was wrapped in
  // TLS and the peer actually presented one. `has_certificate` is the guard:
  // ssl_expiry_days is legitimately negative for an expired certificate, so the
  // -1 it carries otherwise cannot be told apart from "expired yesterday".
  bool has_certificate = false;
  long long ssl_expiry_days = -1;

  // Registered form of ssl_expiry_days: optional — no certificate, no value.
  // An expired certificate keeps its (negative) day count; only the absence
  // of a certificate is 'no certificate'.
  boost::optional<long long> get_ssl_expiry_days_opt() const {
    if (!has_certificate) return boost::none;
    return ssl_expiry_days;
  }

  filter_obj() : port(0), time(0), connected(false) {}
  virtual ~filter_obj() = default;

  std::string show() const { return host + ":" + std::to_string(port) + " (" + result + ")"; }

  std::string get_host() const { return host; }
  long long get_port() const { return port; }
  long long get_time() const { return time; }
  std::string get_result() const { return result; }
  std::string get_response() const { return response; }
  long long get_connected() const { return connected ? 1 : 0; }
  long long get_has_certificate() const { return has_certificate ? 1 : 0; }
  long long get_ssl_expiry_days() const { return ssl_expiry_days; }

  // Called once the peer's response has been read, so a specialised check can
  // derive extra fields from it (check_ssh parses the identification string).
  virtual void post_read() {}
};

// The keywords every TCP-style check shares. Templated on the registry so
// check_ssh can register the same set for its own (derived) filter_obj instead
// of duplicating them.
template <typename Registry>
void register_common_keywords(Registry &registry) {
  registry.add_string_var("host", &filter_obj::get_host, "Host the check connected to");
  registry.add_string_var("result", &filter_obj::get_result, "Textual result of the check (ok, refused, timeout, no_match, resolve_failed, ...)");
  registry.add_string_var("response", &filter_obj::get_response, "The data received from the peer (use with 'like'/'regexp' for custom matching)");
  registry.add_int_var("port", parsers::where::type_int, &filter_obj::get_port, "TCP port the check connected to");
  registry.add_int_var("time", parsers::where::type_int, &filter_obj::get_time, "Connection time in milliseconds").add_int_perf("ms");
  registry.add_int_var("connected", parsers::where::type_int, &filter_obj::get_connected, "1 when the connection succeeded, 0 otherwise");
}

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_tcp_filter

namespace check_ssh_filter {

// check_ssh is check_tcp against the SSH preset plus the parsed identification
// string, so its object is the TCP one with the banner fields added.
struct filter_obj : public check_tcp_filter::filter_obj {
  check_ssh_internal::ssh_banner banner;

  std::string get_banner() const { return banner.banner; }
  std::string get_protocol() const { return banner.protocol; }
  long long get_protocol_major() const { return banner.protocol_major; }
  long long get_protocol_minor() const { return banner.protocol_minor; }
  std::string get_version() const { return banner.version; }
  std::string get_software() const { return banner.software; }
  std::string get_software_version() const { return banner.software_version; }
  std::string get_comments() const { return banner.comments; }

  void post_read() override { check_ssh_internal::parse_ssh_banner(response, banner); }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_ssh_filter

void check_tcp(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
void check_ssh(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check_net
