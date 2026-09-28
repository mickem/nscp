// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <net/http/client.hpp>
#include <sstream>
#include <stdexcept>

#include "check_domain.hpp"
#include "check_http_internal.hpp"
#include "check_net_error.hpp"

namespace check_net {
namespace domain {
namespace {
constexpr std::size_t max_response = 1024 * 1024;

check_http_internal::parsed_url https_url(const std::string &url) {
  check_http_internal::parsed_url parsed;
  if (url.find_first_of("\r\n\t #\\") != std::string::npos || url.find('@') != std::string::npos || !check_http_internal::parse_url(url, parsed) ||
      parsed.protocol != "https")
    throw std::runtime_error("RDAP requires an HTTPS URL without credentials, whitespace or fragment");
  return parsed;
}

std::string redirected_url(const std::string &base, const std::string &target) {
  if (target.empty()) throw std::runtime_error("RDAP redirect has no Location");
  if (target.find("://") != std::string::npos) return target;
  if (target.compare(0, 2, "//") == 0) return "https:" + target;
  const auto u = https_url(base);
  const auto origin = "https://" + check_http_internal::host_header_value(u.host) + ":" + u.port;
  if (target.front() == '/') return origin + target;
  const auto path = u.path.substr(0, u.path.find('?'));
  if (target.front() == '?') return origin + path + target;
  return origin + path.substr(0, path.rfind('/') + 1) + target;
}

http::response get(const std::string &url, const lookup_options &options) {
  const auto u = https_url(url);
  http::http_client_options client_options(u.protocol, "tlsv1.2+", "peer", options.ca_file);
  client_options.timeout_seconds_ = static_cast<unsigned int>(options.timeout);
  client_options.max_response_bytes_ = max_response;
  http::simple_client client(client_options);
  http::request request("GET", check_http_internal::host_header_value(u.host) + ":" + u.port, u.path);
  // Some authoritative RDAP services reject HTTP/1.0 with HTTP 505.
  request.version_ = http::request::version::http_1_1;
  request.add_header("User-Agent", "NSClient++ check_domain");
  request.add_header("Accept", "application/rdap+json, application/json");
  request.add_header("Connection", "close");
  return client.fetch(u.host, u.port, request);
}

registration rdap(const std::string &name, std::string &url, const lookup_options &options) {
  for (int redirects = 0; redirects <= 5; ++redirects) {
    const auto response = get(url, options);
    const auto code = response.status_code_;
    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
      if (redirects == 5) throw std::runtime_error("RDAP redirect limit exceeded");
      const auto location = response.headers_.find("location");
      url = redirected_url(url, location == response.headers_.end() ? "" : location->second);
      continue;
    }
    if (code != 200) throw std::runtime_error("RDAP returned HTTP " + std::to_string(code));
    return parse_rdap(response.payload_, name);
  }
  throw std::runtime_error("RDAP redirect limit exceeded");
}

registration whois(const std::string &name, const lookup_options &options) {
  boost::asio::io_context io;
  http::tcp_socket socket(io);
  socket.set_timeouts(static_cast<unsigned int>(options.timeout));
  socket.connect(options.whois_server, std::to_string(options.whois_port));
  boost::asio::streambuf buffer(max_response + 1);
  std::ostream request(&buffer);
  request << name << "\r\n";
  socket.write(buffer);
  std::ostringstream body;
  for (;;) {
    boost::system::error_code error;
    const auto received = socket.read_some(buffer, error);
    if (static_cast<std::size_t>(body.tellp()) + buffer.size() > max_response) throw std::runtime_error("WHOIS response exceeds 1 MiB");
    if (buffer.size() > 0) body << &buffer;
    if (error == boost::asio::error::eof) break;
    // The shared socket reports read timeouts as error codes. Do not parse a
    // partial response as a successful WHOIS lookup after a failed read.
    if (error) throw boost::system::system_error(error);
    if (received == 0) throw std::runtime_error("WHOIS connection ended without EOF");
  }
  return parse_whois(body.str(), name);
}
}  // namespace

filter_obj lookup(const std::string &name, const lookup_options &options) {
  filter_obj out;
  out.domain = name;
  registration record;
  try {
    std::string url = options.rdap_url;
    const auto placeholder = url.find("{domain}");
    if (placeholder != std::string::npos) url.replace(placeholder, 8, name);
    record = rdap(name, url, options);
    out.source = "rdap:" + url;
    if (record.expiration_type != "registrar" && !record.registrar_url.empty() && record.registrar_url != url) {
      auto registrar_url = record.registrar_url;
      const auto registrar = rdap(name, registrar_url, options);
      if (registrar.expiration_type == "registrar") {
        record = registrar;
        out.source = "rdap:" + registrar_url;
      }
    }
    if (record.expiration.is_not_a_date_time()) throw std::runtime_error("No RDAP expiration event for domain");
  } catch (const std::exception &e) {
    if (!options.whois_fallback) throw;
    const auto rdap_error = check_net::format_exception_message(e);
    try {
      record = whois(name, options);
      out.source = "whois:" + options.whois_server + ":" + std::to_string(options.whois_port);
    } catch (const std::exception &fallback_error) {
      throw std::runtime_error("RDAP: " + rdap_error + "; WHOIS: " + check_net::format_exception_message(fallback_error));
    }
  }
  out.expiration = boost::posix_time::to_iso_extended_string(record.expiration) + "Z";
  out.expiration_type = record.expiration_type;
  out.expires_in = days_remaining(record.expiration, boost::posix_time::microsec_clock::universal_time());
  return out;
}
}  // namespace domain
}  // namespace check_net
