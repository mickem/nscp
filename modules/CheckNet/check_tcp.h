// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cctype>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

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

  filter_obj() : port(0), time(0), connected(false) {}

  std::string show() const { return host + ":" + std::to_string(port) + " (" + result + ")"; }

  std::string get_host() const { return host; }
  long long get_port() const { return port; }
  long long get_time() const { return time; }
  std::string get_result() const { return result; }
  std::string get_response() const { return response; }
  long long get_connected() const { return connected ? 1 : 0; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_tcp_filter

void check_tcp(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
void check_ssh(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check_net
