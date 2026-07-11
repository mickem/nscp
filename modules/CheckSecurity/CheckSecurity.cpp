// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckSecurity.h"

#include "check_antivirus.hpp"
#include "check_bitlocker.hpp"
#include "check_certificate.hpp"
#include "check_defender.hpp"
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

void CheckSecurity::check_defender(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_defender_command::check(request, response);
}
