/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */
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
