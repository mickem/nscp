// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_http_fetch.hpp"

#include <net/http/client.hpp>

namespace check_net {
namespace check_http_fetch {

fetch_result fetch_status_page(const std::string &url, const fetch_options &opt) {
  fetch_result out;
  check_http_internal::parsed_url u;
  if (!check_http_internal::parse_url(url, u)) {
    out.result = "invalid_url";
    return out;
  }
  out.host = u.host;
  try {
    out.port = std::stoll(u.port);
    http::http_client_options options(u.protocol, opt.tls_version, opt.verify_mode, opt.ca_file);
    // add_fetch_options rejects non-positive timeouts; guard the cast anyway
    // so a future caller cannot turn -1 into a ~136-year unsigned timeout.
    options.timeout_seconds_ = static_cast<unsigned int>(opt.timeout > 0 ? opt.timeout : 1);
    http::simple_client client(options);

    http::request rq("GET", check_http_internal::host_header_value(u.host), u.path);
    rq.add_header("User-Agent", "NSClient++");
    add_basic_auth(rq, opt.username, opt.password);
    if (!opt.post_body.empty()) rq.add_post_payload(opt.post_content_type, opt.post_body);

    const http::response resp = client.fetch(u.host, u.port, rq);
    out.code = resp.status_code_;
    out.body = resp.payload_;
    out.result = classify_status(resp.status_code_, 300);
  } catch (const std::exception &e) {
    out.result = error_result(e);
  }
  return out;
}

}  // namespace check_http_fetch
}  // namespace check_net
