// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Parsing of the nsclient REST query JSON, kept in its own translation unit so
// it can be unit-tested without pulling in the HTTP/TLS client stack. The
// header-only Boost.JSON implementation is compiled here (once).

#include "check_nsclient_web_online.h"

#include <boost/json/src.hpp>

namespace json = boost::json;

namespace check_net {

bool parse_nsclient_web_online_result(const std::string &json_body, int &result_code, std::string &message) {
  boost::system::error_code ec;
  const json::value v = json::parse(json_body, ec);
  if (ec || !v.is_object()) return false;
  const json::object &obj = v.as_object();
  const json::value *result = obj.if_contains("result");
  if (result == nullptr || !result->is_int64()) return false;
  result_code = static_cast<int>(result->as_int64());

  message.clear();
  const json::value *lines = obj.if_contains("lines");
  if (lines != nullptr && lines->is_array()) {
    for (const json::value &line : lines->as_array()) {
      if (!line.is_object()) continue;
      const json::value *msg = line.as_object().if_contains("message");
      if (msg != nullptr && msg->is_string()) {
        if (!message.empty()) message += "\n";
        message.append(msg->as_string().c_str());
      }
    }
  }
  return true;
}

}  // namespace check_net
