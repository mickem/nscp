// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>

namespace check_net {

// Parse a nsclient REST query JSON body, extracting the nagios result code and
// the first line's message. Returns false when the body is not the expected
// shape. Split out for unit testing.
bool parse_nsclient_web_online_result(const std::string &json_body, int &result_code, std::string &message);

void check_nsclient_web_online(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                   PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_net
