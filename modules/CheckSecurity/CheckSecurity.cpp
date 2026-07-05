/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include "CheckSecurity.h"

#include "check_antivirus.hpp"
#include "check_bitlocker.hpp"
#include "check_certificate.hpp"
#include "check_firewall.hpp"
#include "check_nla.hpp"
#include "check_secureboot.hpp"
#include "check_users.hpp"

CheckSecurity::CheckSecurity() {}

bool CheckSecurity::loadModuleEx(std::string, NSCAPI::moduleLoadMode) { return true; }

bool CheckSecurity::unloadModule() { return true; }

void CheckSecurity::check_certificate(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_certificate_command::check(request, response);
}

void CheckSecurity::check_firewall(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_firewall_command::check(request, response);
}

void CheckSecurity::check_users(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_users_command::check(request, response);
}

void CheckSecurity::check_nla(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_nla_command::check(request, response);
}

void CheckSecurity::check_antivirus(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_antivirus_command::check(request, response);
}

void CheckSecurity::check_bitlocker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_bitlocker_command::check(request, response);
}

void CheckSecurity::check_secureboot(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_secureboot_command::check(request, response);
}
