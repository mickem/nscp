// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckActiveDirectory.h"

#include "check_ad_replication.hpp"
#include "check_kdc.hpp"
#include "check_secure_channel.hpp"

CheckActiveDirectory::CheckActiveDirectory() {}

bool CheckActiveDirectory::loadModuleEx(std::string, NSCAPI::moduleLoadMode) { return true; }

bool CheckActiveDirectory::unloadModule() { return true; }

void CheckActiveDirectory::check_ad_replication(const PB::Commands::QueryRequestMessage::Request &request,
                                                PB::Commands::QueryResponseMessage::Response *response) {
  check_ad_replication_command::check(request, response);
}

void CheckActiveDirectory::check_secure_channel(const PB::Commands::QueryRequestMessage::Request &request,
                                                PB::Commands::QueryResponseMessage::Response *response) {
  check_secure_channel_command::check(request, response);
}

void CheckActiveDirectory::check_kdc(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_kdc_command::check(request, response);
}
