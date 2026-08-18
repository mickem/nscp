// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>

namespace check_net {

// Web/application server status-page checks: each command fetches the vendor's
// machine-readable status endpoint over HTTP(S) and exposes the parsed values
// as filter keywords.
void check_apache_status(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                         PB::Commands::QueryResponseMessage::Response *response);
void check_nginx_status(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                        PB::Commands::QueryResponseMessage::Response *response);
void check_phpfpm_status(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                         PB::Commands::QueryResponseMessage::Response *response);
void check_tomcat_status(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                         PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_net
