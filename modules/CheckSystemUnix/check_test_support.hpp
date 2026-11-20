// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// What a test that drives a whole check command needs: a request built the way
// REST builds one (every argument a single `key=value` token), and the pieces
// of the response worth asserting on.

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

namespace check_test {

typedef PB::Commands::QueryResponseMessage::Response response_type;

inline PB::Commands::QueryRequestMessage::Request make_request(const std::string &command, const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command(command);
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

// Every line's message, joined by newlines.
inline std::string message_of(const response_type &response) {
  std::string out;
  for (int i = 0; i < response.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += response.lines(i).message();
  }
  return out;
}

// Whether any perf entry's alias contains the given text.
inline bool has_perf_like(const response_type &response, const std::string &text) {
  for (int i = 0; i < response.lines_size(); ++i) {
    for (const auto &p : response.lines(i).perf()) {
      if (p.alias().find(text) != std::string::npos) return true;
    }
  }
  return false;
}

inline bool contains(const std::string &haystack, const std::string &needle) { return haystack.find(needle) != std::string::npos; }

}  // namespace check_test
